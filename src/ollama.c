/* vim:tw=78:ts=4:sw=4:sts=4:et:set ft=c:  */
/*
    Ollama opponent: child process speaks a minimal XBoard dialect on a pipe
    and turns "go" into an HTTP chat completion against an Ollama server.

    Wire protocol (cboard → bridge), subset of XBoard/CECP:
      xboard / protover N / new / force / quit  — ignored or handled
      setboard <FEN>                            — position for next go
      usermove <uci> | <uci>                    — human move (optional history)
      go                                        — request a move now

    Bridge → cboard (parsed by parse_xboard_line):
      move e2e4
      move e7e8q
*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "common.h"
#include "conf.h"
#include "misc.h"
#include "window.h"
#include "message.h"
#include "engine.h"
#include "input.h"
#include "menu.h"
#include "ui_screen.h"
#include "ollama.h"

#ifndef AI_NAME
#define AI_NAME "Ollama"
#endif

/* First-connect handshake must answer within this many seconds.
 * Large local models (30B+) often need longer than 15s to load + reply. */
#define OLLAMA_HANDSHAKE_TIMEOUT_SEC 60

#define OLLAMA_CONN_FILE "ollama_connections"
#define OLLAMA_CONN_MAX 64

/* Saved endpoints the user can pick from (persisted under ~/.cboard). */
struct ollama_conn
{
    char *name;
    char *url;
    char *model;
};

static struct ollama_conn conns[OLLAMA_CONN_MAX];
static int nconns;
static int conns_loaded;
/* Menu mode: 0 = pick/connect, 1 = remove. */
static int conn_menu_mode;
/* After the connections menu closes: open new prompt or connect by index. */
static int pending_new_conn;
static int pending_connect_idx = -1;

/* ---- URL helpers ------------------------------------------------------- */

struct http_url
{
    char host[256];
    char path[512];
    int port;
    int use_tls; /* unsupported; we only do plain HTTP */
};

static int
parse_http_url(const char *url, struct http_url *u)
{
    const char *p, *host, *path, *colon;
    size_t host_len;

    if (!url || !u)
        return -1;
    memset(u, 0, sizeof(*u));
    u->port = 80;
    snprintf(u->path, sizeof(u->path), "/");

    p = url;
    if (strncmp(p, "https://", 8) == 0)
    {
        u->use_tls = 1;
        p += 8;
        u->port = 443;
    }
    else if (strncmp(p, "http://", 7) == 0)
        p += 7;
    else
        return -1;

    host = p;
    path = strchr(p, '/');
    colon = strchr(p, ':');
    if (colon && (!path || colon < path))
    {
        host_len = (size_t) (colon - host);
        u->port = atoi(colon + 1);
    }
    else if (path)
        host_len = (size_t) (path - host);
    else
        host_len = strlen(host);

    if (host_len == 0 || host_len >= sizeof(u->host))
        return -1;
    memcpy(u->host, host, host_len);
    u->host[host_len] = '\0';

    if (path && path[0])
    {
        size_t plen;

        snprintf(u->path, sizeof(u->path), "%s", path);
        /* Drop trailing slashes so /v1/ and /v1 match. */
        plen = strlen(u->path);
        while (plen > 1 && u->path[plen - 1] == '/')
            u->path[--plen] = '\0';
    }

    return 0;
}

/*
 * Map a base URL path to the chat endpoint.
 *
 *   http://host:11434          → /api/chat              (native Ollama)
 *   http://host:11434/v1       → /v1/chat/completions   (OpenAI-compatible,
 *                                                        as used by OpenCode etc.)
 *   …/api/chat or …/chat/completions → used as-is
 *   other prefix               → {prefix}/api/chat
 *
 * Pasting an OpenAI baseURL (…/v1) used to become /v1/api/chat → HTTP 404.
 */
static void
ollama_chat_endpoint(const struct http_url *u, char *path, size_t pathsz)
{
    const char *base = u && u->path[0] ? u->path : "/";
    size_t blen;

    if (!path || pathsz < 8)
        return;

    if (!base[0] || strcmp(base, "/") == 0)
    {
        snprintf(path, pathsz, "/api/chat");
        return;
    }

    blen = strlen(base);

    /* Already a full chat path. */
    if (blen >= 9 && strcmp(base + blen - 9, "/api/chat") == 0)
    {
        snprintf(path, pathsz, "%s", base);
        return;
    }
    if (blen >= 18 && strcmp(base + blen - 18, "/chat/completions") == 0)
    {
        snprintf(path, pathsz, "%s", base);
        return;
    }

    /* OpenAI-compatible root: /v1 or …/v1 */
    if (strcmp(base, "/v1") == 0
        || (blen >= 3 && strcmp(base + blen - 3, "/v1") == 0))
    {
        snprintf(path, pathsz, "%s/chat/completions", base);
        return;
    }

    /* Custom reverse-proxy prefix → native Ollama under that prefix. */
    snprintf(path, pathsz, "%s/api/chat", base);
}

/* Escape for JSON string (minimal). */
static char *
json_escape(const char *s)
{
    size_t n = 0, i, j;
    char *o;

    if (!s)
        s = "";
    for (i = 0; s[i]; i++)
    {
        if (s[i] == '"' || s[i] == '\\' || s[i] == '\n' || s[i] == '\r'
            || s[i] == '\t')
            n += 2;
        else
            n++;
    }
    o = Calloc(1, n + 1);
    for (i = 0, j = 0; s[i]; i++)
    {
        if (s[i] == '"' || s[i] == '\\')
        {
            o[j++] = '\\';
            o[j++] = s[i];
        }
        else if (s[i] == '\n')
        {
            o[j++] = '\\';
            o[j++] = 'n';
        }
        else if (s[i] == '\r')
        {
            o[j++] = '\\';
            o[j++] = 'r';
        }
        else if (s[i] == '\t')
        {
            o[j++] = '\\';
            o[j++] = 't';
        }
        else
            o[j++] = s[i];
    }
    o[j] = '\0';
    return o;
}

/*
 * POST path on host:port with JSON body.  Returns malloc'd response body
 * (not including headers), or NULL on failure.  Optional err is filled on
 * failure for bridge Error lines.  timeout_sec > 0 applies SO_RCV/SNDTIMEO
 * (handshake uses 15s; move requests pass 0 for no socket timeout).
 */
static char *
http_post_json(const char *host, int port, const char *path,
               const char *json_body, char *err, size_t errsz,
               int timeout_sec)
{
    struct addrinfo hints, *res = NULL, *rp;
    char portstr[16];
    int fd = -1;
    char *req = NULL;
    char *resp = NULL;
    size_t body_len, req_len, cap = 0, len = 0;
    ssize_t n;
    char buf[4096];
    char *hdr_end;
    long content_len = -1;
    char *body;
    int http_status = 0;
    int read_errno = 0;
    int conn_errno = 0;

    if (err && errsz)
        err[0] = '\0';

    if (!host || !path || !json_body)
    {
        if (err && errsz)
            snprintf(err, errsz, "internal: bad args");
        return NULL;
    }

    body_len = strlen(json_body);
    snprintf(portstr, sizeof(portstr), "%d", port);

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    if (getaddrinfo(host, portstr, &hints, &res) != 0)
    {
        if (err && errsz)
            snprintf(err, errsz, "DNS failed for %s", host);
        return NULL;
    }

    for (rp = res; rp; rp = rp->ai_next)
    {
        int cr;

        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0)
        {
            conn_errno = errno;
            continue;
        }
        if (timeout_sec > 0)
        {
            struct timeval tv;

            tv.tv_sec = timeout_sec;
            tv.tv_usec = 0;
            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        }
        /*
         * Play clocks use ITIMER_REAL every 100ms (SIGALRM, no SA_RESTART).
         * connect() is almost always interrupted if a game is already live.
         */
        do
            cr = connect(fd, rp->ai_addr, rp->ai_addrlen);
        while (cr < 0 && errno == EINTR);
        if (cr == 0)
            break;
        conn_errno = errno;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    if (fd < 0)
    {
        if (err && errsz)
        {
            if (timeout_sec > 0
                && (conn_errno == EAGAIN || conn_errno == EWOULDBLOCK
                    || conn_errno == ETIMEDOUT))
                snprintf(err, errsz, "connect timed out (%ds)", timeout_sec);
            else
                snprintf(err, errsz, "connect %s:%d failed: %s", host, port,
                         conn_errno ? strerror(conn_errno) : "unknown");
        }
        return NULL;
    }

    req_len = body_len + 512;
    req = Malloc(req_len);
    snprintf(req, req_len,
             "POST %s HTTP/1.0\r\n"
             "Host: %s\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             path, host, body_len, json_body);

    {
        const char *p = req;
        size_t left = strlen(req);

        while (left)
        {
            n = write(fd, p, left);
            if (n < 0)
            {
                if (errno == EINTR)
                    continue;
                close(fd);
                free(req);
                if (err && errsz)
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK
                        || errno == ETIMEDOUT)
                        snprintf(err, errsz, "send timed out (%ds)",
                                 timeout_sec);
                    else
                        snprintf(err, errsz, "send failed: %s",
                                 strerror(errno));
                }
                return NULL;
            }
            p += n;
            left -= (size_t) n;
        }
    }
    free(req);

    for (;;)
    {
        n = read(fd, buf, sizeof(buf));
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            break;
        if (len + (size_t) n + 1 > cap)
        {
            cap = (cap ? cap * 2 : 8192);
            while (cap < len + (size_t) n + 1)
                cap *= 2;
            resp = Realloc(resp, cap);
        }
        memcpy(resp + len, buf, (size_t) n);
        len += (size_t) n;
        resp[len] = '\0';
    }
    if (n < 0)
        read_errno = errno;
    close(fd);

    if (!resp || len == 0)
    {
        free(resp);
        if (err && errsz)
        {
            if (read_errno == EAGAIN || read_errno == EWOULDBLOCK
                || read_errno == ETIMEDOUT)
                snprintf(err, errsz,
                         "no response within %d seconds "
                         "(is Ollama running / model loaded?)",
                         timeout_sec > 0 ? timeout_sec
                                         : OLLAMA_HANDSHAKE_TIMEOUT_SEC);
            else
                snprintf(err, errsz, "empty reply from %s", host);
        }
        return NULL;
    }

    hdr_end = strstr(resp, "\r\n\r\n");
    if (!hdr_end)
    {
        free(resp);
        if (err && errsz)
            snprintf(err, errsz, "malformed HTTP reply");
        return NULL;
    }
    *hdr_end = '\0';
    if (sscanf(resp, "HTTP/%*s %d", &http_status) == 1 && http_status >= 400)
    {
        if (err && errsz)
        {
            /* Body often has {"error":"model 'x' not found"} for 404. */
            char *be = strstr(hdr_end + 4, "\"error\"");
            char detail[96] = {0};

            if (be)
            {
                char *q1 = strchr(be + 7, '"');

                if (q1)
                {
                    char *q2;

                    q1 = strchr(q1 + 1, '"');
                    if (q1)
                    {
                        q1++;
                        q2 = strchr(q1, '"');
                        if (q2 && (size_t) (q2 - q1) < sizeof(detail) - 1)
                        {
                            memcpy(detail, q1, (size_t) (q2 - q1));
                            detail[q2 - q1] = '\0';
                        }
                    }
                }
            }
            if (detail[0])
                snprintf(err, errsz, "HTTP %d POST %s: %s", http_status, path,
                         detail);
            else if (http_status == 404)
                snprintf(err, errsz,
                         "HTTP 404 POST %s (wrong path or model not found; "
                         "use http://host:11434 or …/v1)",
                         path);
            else
                snprintf(err, errsz, "HTTP %d POST %s", http_status, path);
        }
        free(resp);
        return NULL;
    }
    {
        char *cl = strstr(resp, "Content-Length:");

        if (!cl)
            cl = strstr(resp, "content-length:");
        if (cl)
        {
            cl = strchr(cl, ':');
            if (cl)
                content_len = strtol(cl + 1, NULL, 10);
        }
    }
    body = hdr_end + 4;
    if (content_len > 0 && (size_t) content_len < strlen(body))
        body[content_len] = '\0';

    {
        char *out = strdup(body);

        free(resp);
        return out;
    }
}

/* Pull first UCI-like move e2e4 or e7e8q from free text. */
static int
extract_uci_move(const char *text, char *out, size_t outsz)
{
    const char *p;
    size_t i;

    if (!text || !out || outsz < 6)
        return 0;

    /* Prefer explicit "move e2e4" (XBoard style). */
    p = strstr(text, "move ");
    if (p)
    {
        p += 5;
        while (*p == ' ' || *p == '\t')
            p++;
        for (i = 0; i < outsz - 1 && p[i] && !isspace((unsigned char) p[i])
                    && p[i] != '"' && p[i] != '\\';
             i++)
            out[i] = p[i];
        out[i] = '\0';
        if (i >= 4)
            return 1;
    }

    /* Scan for [a-h][1-8][a-h][1-8][qrbnQRBN]? */
    for (p = text; *p; p++)
    {
        if (p[0] >= 'a' && p[0] <= 'h' && p[1] >= '1' && p[1] <= '8'
            && p[2] >= 'a' && p[2] <= 'h' && p[3] >= '1' && p[3] <= '8')
        {
            size_t n = 4;

            if (strchr("qrbnQRBN", p[4]))
                n = 5;
            if (n >= outsz)
                n = outsz - 1;
            memcpy(out, p, n);
            out[n] = '\0';
            /* lower-case promotion */
            if (n == 5 && out[4] >= 'A' && out[4] <= 'Z')
                out[4] = (char) (out[4] - 'A' + 'a');
            return 1;
        }
    }
    return 0;
}

/*
 * From chat JSON, get assistant message content (naive, no full JSON parser).
 * Prefers OpenAI choices[0].message.content, then Ollama message.content,
 * then any "content":"…" string value.
 */
static char *
ollama_json_message_content(const char *json)
{
    const char *p, *q, *start = NULL;
    char *out;
    size_t n;

    if (!json)
        return NULL;

    /* OpenAI-compatible: "choices":[{"message":{"content":"…" */
    p = strstr(json, "\"choices\"");
    if (p)
    {
        const char *msg = strstr(p, "\"message\"");

        if (msg)
        {
            const char *c = strstr(msg, "\"content\"");

            if (c)
                start = c;
        }
    }
    /* Native Ollama: "message":{"role":"assistant","content":"…" */
    if (!start)
    {
        p = strstr(json, "\"message\"");
        if (p)
        {
            const char *c = strstr(p, "\"content\"");

            if (c)
                start = c;
        }
    }
    if (!start)
        start = strstr(json, "\"content\"");
    if (!start)
        return NULL;

    p = strchr(start + 9, ':');
    if (!p)
        return NULL;
    p++;
    while (*p == ' ' || *p == '\t')
        p++;
    if (*p != '"')
        return NULL;
    p++;
    q = p;
    while (*q)
    {
        if (*q == '\\' && q[1])
        {
            q += 2;
            continue;
        }
        if (*q == '"')
            break;
        q++;
    }
    n = (size_t) (q - p);
    out = Malloc(n + 1);
    {
        size_t i, j;

        for (i = 0, j = 0; i < n; i++)
        {
            if (p[i] == '\\' && i + 1 < n)
            {
                i++;
                if (p[i] == 'n')
                    out[j++] = '\n';
                else if (p[i] == 'r')
                    out[j++] = '\r';
                else if (p[i] == 't')
                    out[j++] = '\t';
                else
                    out[j++] = p[i];
            }
            else
                out[j++] = p[i];
        }
        out[j] = '\0';
    }
    return out;
}

/* Build a non-streaming chat JSON body (works for both Ollama and OpenAI APIs). */
static char *
ollama_build_chat_json(const char *model, const char *sys_msg,
                       const char *user_msg)
{
    char *sys_esc, *user_esc, *model_esc, *json;
    size_t jlen;

    sys_esc = json_escape(sys_msg ? sys_msg : "");
    user_esc = json_escape(user_msg ? user_msg : "");
    model_esc = json_escape(model && model[0] ? model : OLLAMA_DEFAULT_MODEL);
    jlen = strlen(sys_esc) + strlen(user_esc) + strlen(model_esc) + 256;
    json = Malloc(jlen);
    snprintf(json, jlen,
             "{\"model\":\"%s\",\"stream\":false,"
             "\"messages\":["
             "{\"role\":\"system\",\"content\":\"%s\"},"
             "{\"role\":\"user\",\"content\":\"%s\"}"
             "]}",
             model_esc, sys_esc, user_esc);
    free(sys_esc);
    free(user_esc);
    free(model_esc);
    return json;
}

/* Progress line for the parent Game Status (must end with \n, line-buffered). */
static void
ollama_progress(const char *fmt, ...)
{
    va_list ap;

    fputs("# ollama: ", stdout);
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
    fputc('\n', stdout);
    fflush(stdout);
}

static int
ollama_request_move(const char *base_url, const char *model, const char *fen,
                    const char *history, char *move_out, size_t move_out_sz,
                    char *err, size_t errsz)
{
    struct http_url u;
    char path[640];
    char *json = NULL, *resp = NULL;
    char *content = NULL;
    char user_msg[2048];
    int ok = 0;

    if (err && errsz)
        err[0] = '\0';

    if (parse_http_url(base_url, &u) != 0)
    {
        if (err && errsz)
            snprintf(err, errsz, "bad URL");
        return 0;
    }
    if (u.use_tls)
    {
        if (err && errsz)
            snprintf(err, errsz, "HTTPS not supported");
        return 0;
    }

    ollama_chat_endpoint(&u, path, sizeof(path));
    ollama_progress("POST %s on %s:%d", path, u.host, u.port);

    snprintf(user_msg, sizeof(user_msg),
             "You are playing chess as an XBoard engine.\n"
             "Position (FEN): %s\n"
             "Recent moves (UCI): %s\n"
             "Reply with exactly one line in XBoard form: move e2e4\n"
             "Use UCI coordinates only (e.g. move e7e8q for promotion).\n"
             "No other words.",
             fen && fen[0] ? fen : "startpos",
             history && history[0] ? history : "(none)");

    json = ollama_build_chat_json(
        model,
        "You are a chess engine compatible with the XBoard/CECP protocol. "
        "When asked to move, answer with a single line: move <uci>.",
        user_msg);

    ollama_progress("waiting for %s",
                    model && model[0] ? model : OLLAMA_DEFAULT_MODEL);

    /* No socket timeout on moves — models can think longer than the handshake. */
    resp = http_post_json(u.host, u.port, path, json, err, errsz, 0);
    free(json);

    if (!resp)
    {
        if (err && errsz && !err[0])
            snprintf(err, errsz, "HTTP request failed");
        return 0;
    }

    ollama_progress("parsing reply");

    content = ollama_json_message_content(resp);
    free(resp);
    if (!content)
    {
        if (err && errsz)
            snprintf(err, errsz, "no message in JSON");
        return 0;
    }

    ok = extract_uci_move(content, move_out, move_out_sz);
    free(content);
    if (!ok && err && errsz)
        snprintf(err, errsz, "no UCI move in model reply");
    return ok;
}

/* Case-insensitive substring (for READY-TO-PLAY in free-form replies). */
static int
str_contains_ci(const char *hay, const char *needle)
{
    size_t nlen, i, j;

    if (!hay || !needle || !*needle)
        return 0;
    nlen = strlen(needle);
    for (i = 0; hay[i]; i++)
    {
        for (j = 0; j < nlen; j++)
        {
            unsigned char a = (unsigned char) hay[i + j];
            unsigned char b = (unsigned char) needle[j];

            if (!a || tolower(a) != tolower(b))
                break;
        }
        if (j == nlen)
            return 1;
    }
    return 0;
}

/*
 * Connect-time handshake: ask the model to confirm READY-TO-PLAY within
 * OLLAMA_HANDSHAKE_TIMEOUT_SEC.  Returns 0 on success, non-zero on failure
 * (err filled when provided).
 */
static int
ollama_handshake(const char *base_url, const char *model, char *err,
                 size_t errsz)
{
    struct http_url u;
    char path[640];
    char *json = NULL, *resp = NULL;
    char *content = NULL;
    const char *sys_msg =
        "You are a chess partner. When asked to confirm you are ready to "
        "play via the XBoard protocol, reply with the single token "
        "READY-TO-PLAY and nothing else.";
    const char *user_msg =
        "I want to play a chess game with you. We will use the Xboard "
        "protocol. Confirm with a response of READY-TO-PLAY";

    if (err && errsz)
        err[0] = '\0';

    if (parse_http_url(base_url, &u) != 0)
    {
        if (err && errsz)
            snprintf(err, errsz, "bad URL");
        return 1;
    }
    if (u.use_tls)
    {
        if (err && errsz)
            snprintf(err, errsz, "HTTPS is not supported");
        return 1;
    }

    ollama_chat_endpoint(&u, path, sizeof(path));

    json = ollama_build_chat_json(model, sys_msg, user_msg);
    resp = http_post_json(u.host, u.port, path, json, err, errsz,
                          OLLAMA_HANDSHAKE_TIMEOUT_SEC);
    free(json);

    if (!resp)
    {
        if (err && errsz && !err[0])
            snprintf(err, errsz,
                     "no response within %d seconds "
                     "(is anyone listening?)",
                     OLLAMA_HANDSHAKE_TIMEOUT_SEC);
        return 1;
    }

    content = ollama_json_message_content(resp);
    free(resp);
    if (!content)
    {
        if (err && errsz)
            snprintf(err, errsz, "empty/invalid JSON from model");
        return 1;
    }

    if (!str_contains_ci(content, "READY-TO-PLAY"))
    {
        if (err && errsz)
            snprintf(err, errsz,
                     "model did not confirm READY-TO-PLAY "
                     "(got: \"%.40s%s\")",
                     content, strlen(content) > 40 ? "…" : "");
        free(content);
        return 1;
    }

    free(content);
    return 0;
}

/* ---- XBoard bridge child ---------------------------------------------- */

static void
ollama_bridge_main(const char *url, const char *model)
{
    char line[1024];
    char fen[256] = {0};
    char history[1024] = {0};
    char move[16];

    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stdin, NULL, _IOLBF, 0);

    /* Minimal CECP greeting so a picky parent is happy. */
    printf("feature done=1\n");
    fflush(stdout);

    while (fgets(line, sizeof(line), stdin))
    {
        char *p = line;
        size_t L;

        while (*p == ' ' || *p == '\t')
            p++;
        L = strlen(p);
        while (L && (p[L - 1] == '\n' || p[L - 1] == '\r'))
            p[--L] = '\0';

        if (!L)
            continue;

        if (strcmp(p, "xboard") == 0 || strncmp(p, "protover", 8) == 0
            || strcmp(p, "new") == 0 || strcmp(p, "force") == 0
            || strcmp(p, "hard") == 0 || strcmp(p, "easy") == 0
            || strcmp(p, "post") == 0 || strcmp(p, "nopost") == 0
            || strncmp(p, "level", 5) == 0 || strncmp(p, "time", 4) == 0
            || strncmp(p, "otim", 4) == 0 || strcmp(p, "random") == 0)
            continue;

        if (strcmp(p, "quit") == 0)
            break;

        if (strncmp(p, "setboard ", 9) == 0)
        {
            snprintf(fen, sizeof(fen), "%s", p + 9);
            history[0] = '\0';
            continue;
        }

        if (strncmp(p, "usermove ", 9) == 0)
        {
            char uci[16];

            if (extract_uci_move(p + 9, uci, sizeof(uci)))
            {
                if (history[0])
                    strncat(history, " ", sizeof(history) - strlen(history) - 1);
                strncat(history, uci, sizeof(history) - strlen(history) - 1);
            }
            /*
             * Like gnuchess under CECP: after the opponent's move, think.
             * cboard protocol 1 only sends the UCI move (no explicit go).
             */
            goto do_go;
        }

        /* Bare UCI from cboard (engine_protocol 1). */
        if (extract_uci_move(p, move, sizeof(move))
            && (p[0] >= 'a' && p[0] <= 'h'))
        {
            if (history[0])
                strncat(history, " ", sizeof(history) - strlen(history) - 1);
            strncat(history, move, sizeof(history) - strlen(history) - 1);
            goto do_go;
        }

        if (strcmp(p, "go") == 0)
        {
        do_go:
            {
                char err[128];

                ollama_progress("requesting move");
                if (ollama_request_move(url, model, fen, history, move,
                                        sizeof(move), err, sizeof(err)))
                {
                    printf("move %s\n", move);
                    if (history[0])
                        strncat(history, " ",
                                sizeof(history) - strlen(history) - 1);
                    strncat(history, move,
                            sizeof(history) - strlen(history) - 1);
                }
                else
                {
                    printf("Error (ollama): %s\n",
                           err[0] ? err : "no move");
                }
                fflush(stdout);
            }
            continue;
        }
    }
}

/* ---- Parent-side start / connect -------------------------------------- */

int
engine_is_ollama(GAME g)
{
    struct userdata_s *d;

    if (!g || !g->data)
        return 0;
    d = g->data;
    return d->engine && d->engine->backend == ENGINE_BACKEND_OLLAMA;
}

int
start_ollama_engine(GAME g)
{
    struct userdata_s *d = g->data;
    int to_child[2], from_child[2];
    pid_t pid;
    const char *url = config.ollama_url;
    const char *model = config.ollama_model;

    if (!d)
        return 1;
    if (d->engine && d->engine->status != ENGINE_OFFLINE)
        return -1;

    if (!url || !url[0])
        url = OLLAMA_DEFAULT_URL;
    if (!model || !model[0])
        model = OLLAMA_DEFAULT_MODEL;

    if (pipe(to_child) < 0 || pipe(from_child) < 0)
    {
        message(ERROR_STR, ANY_KEY_STR, "%s",
                _("Could not create pipes for Ollama bridge."));
        return 1;
    }

    pid = fork();
    if (pid < 0)
    {
        close(to_child[0]);
        close(to_child[1]);
        close(from_child[0]);
        close(from_child[1]);
        message(ERROR_STR, ANY_KEY_STR, "%s",
                _("Could not start Ollama bridge process."));
        return 1;
    }

    if (pid == 0)
    {
        const char *u = url;
        const char *m = model;

        /* Child: stdin = commands from parent, stdout = xboard replies. */
        close(to_child[1]);
        close(from_child[0]);
        if (dup2(to_child[0], STDIN_FILENO) < 0
            || dup2(from_child[1], STDOUT_FILENO) < 0)
            _exit(127);
        close(to_child[0]);
        close(from_child[1]);
        /* Don't hold the TUI's other FDs. */
        ollama_bridge_main(u, m);
        _exit(0);
    }

    close(to_child[0]);
    close(from_child[1]);

    if (!d->engine)
        d->engine = Calloc(1, sizeof(struct engine_s));

    d->engine->fd[ENGINE_IN_FD] = from_child[0];
    d->engine->fd[ENGINE_OUT_FD] = to_child[1];
    d->engine->pid = pid;
    d->engine->backend = ENGINE_BACKEND_OLLAMA;
    d->engine->status = ENGINE_READY;

    if (fcntl(d->engine->fd[ENGINE_IN_FD], F_SETFL, O_NONBLOCK) == -1
        || fcntl(d->engine->fd[ENGINE_OUT_FD], F_SETFL, O_NONBLOCK) == -1)
    {
        free_engine(g);
        message(ERROR_STR, ANY_KEY_STR, "%s",
                _("Could not configure Ollama bridge pipes."));
        return 1;
    }

    return 0;
}

/* ---- Saved connections ------------------------------------------------ */

static char *
ollama_conn_path(void)
{
    static char path[PATH_MAX];

    if (!config.datadir)
        return NULL;
    snprintf(path, sizeof(path), "%s/%s", config.datadir, OLLAMA_CONN_FILE);
    return path;
}

/* Derive a short label from the host part of a URL. */
static void
ollama_name_from_url(const char *url, char *out, size_t outsz)
{
    struct http_url u;

    if (parse_http_url(url, &u) == 0 && u.host[0])
    {
        snprintf(out, outsz, "%s", u.host);
        return;
    }
    snprintf(out, outsz, "%s", "ollama");
}

/* Keep only printable ASCII so a bad free cannot poison the save file. */
static void
ollama_sanitize_name(char *s)
{
    char *r, *w;

    if (!s)
        return;
    for (r = w = s; *r; r++)
    {
        unsigned char c = (unsigned char) *r;

        if (c >= 32 && c < 127 && c != '|')
            *w++ = (char) c;
    }
    *w = '\0';
}

static void
ollama_conn_clear(void)
{
    int i;

    for (i = 0; i < nconns; i++)
    {
        free(conns[i].name);
        free(conns[i].url);
        free(conns[i].model);
        conns[i].name = conns[i].url = conns[i].model = NULL;
    }
    nconns = 0;
}

void
ollama_conn_load(void)
{
    FILE *fp;
    char *path;
    char line[1024];
    int dirty = 0;

    /* Always re-read from disk so a bad in-memory update cannot stick. */
    ollama_conn_clear();
    conns_loaded = 1;
    path = ollama_conn_path();
    if (!path)
        return;
    fp = fopen(path, "r");
    if (!fp)
        return;

    while (fgets(line, sizeof(line), fp) && nconns < OLLAMA_CONN_MAX)
    {
        char *p = line, *name, *url, *model;
        char nbuf[128];

        while (*p == ' ' || *p == '\t')
            p++;
        if (!*p || *p == '#' || *p == '\n')
            continue;
        /* name|url|model */
        name = p;
        url = strchr(p, '|');
        if (!url)
            continue;
        *url++ = '\0';
        model = strchr(url, '|');
        if (!model)
            continue;
        *model++ = '\0';
        {
            char *nl = strchr(model, '\n');

            if (nl)
                *nl = '\0';
        }
        name = trim(name);
        url = trim(url);
        model = trim(model);
        if (!url[0] || !model[0])
            continue;
        if (!name[0])
        {
            ollama_name_from_url(url, nbuf, sizeof(nbuf));
            name = nbuf;
        }
        else
        {
            /* Detect binary / non-printable garbage names from older bugs. */
            char *t;
            int bad = 0;

            for (t = name; *t; t++)
            {
                unsigned char c = (unsigned char) *t;

                if (c < 32 || c > 126)
                {
                    bad = 1;
                    break;
                }
            }
            if (bad)
            {
                ollama_name_from_url(url, nbuf, sizeof(nbuf));
                name = nbuf;
                dirty = 1;
            }
        }
        conns[nconns].name = strdup(name);
        conns[nconns].url = strdup(url);
        conns[nconns].model = strdup(model);
        if (!conns[nconns].name || !conns[nconns].url || !conns[nconns].model)
        {
            free(conns[nconns].name);
            free(conns[nconns].url);
            free(conns[nconns].model);
            continue;
        }
        ollama_sanitize_name(conns[nconns].name);
        if (!conns[nconns].name[0])
        {
            free(conns[nconns].name);
            ollama_name_from_url(url, nbuf, sizeof(nbuf));
            conns[nconns].name = strdup(nbuf);
            if (!conns[nconns].name)
            {
                free(conns[nconns].url);
                free(conns[nconns].model);
                continue;
            }
        }
        nconns++;
    }
    fclose(fp);
    if (dirty)
        ollama_conn_save();
}

void
ollama_conn_save(void)
{
    FILE *fp;
    char *path;
    int i;

    path = ollama_conn_path();
    if (!path)
        return;
    fp = fopen(path, "w");
    if (!fp)
        return;
    fprintf(fp, "# cboard Ollama connections - name|url|model\n");
    for (i = 0; i < nconns; i++)
        fprintf(fp, "%s|%s|%s\n", conns[i].name, conns[i].url,
                conns[i].model);
    fclose(fp);
}

/*
 * Add or update a saved connection.  name may be NULL (derived from host).
 * Returns index, or -1 if full.
 *
 * IMPORTANT: copy arguments first.  Callers often pass conns[i].name/url/model
 * pointers; updating that slot free()s them before strdup would run (UAF),
 * which previously wrote binary garbage into the save file and broke reconnect.
 */
int
ollama_conn_add(const char *name, const char *url, const char *model)
{
    char nbuf[128];
    char *name_c = NULL, *url_c = NULL, *model_c = NULL;
    int i;

    if (!url || !url[0] || !model || !model[0])
        return -1;

    ollama_conn_load();

    url_c = strdup(url);
    model_c = strdup(model);
    if (!url_c || !model_c)
    {
        free(url_c);
        free(model_c);
        return -1;
    }
    if (!name || !name[0])
    {
        ollama_name_from_url(url_c, nbuf, sizeof(nbuf));
        name_c = strdup(nbuf);
        if (!name_c)
        {
            free(url_c);
            free(model_c);
            return -1;
        }
    }
    else
    {
        name_c = strdup(name);
        if (!name_c)
        {
            free(url_c);
            free(model_c);
            return -1;
        }
        ollama_sanitize_name(name_c);
        if (!name_c[0])
        {
            free(name_c);
            ollama_name_from_url(url_c, nbuf, sizeof(nbuf));
            name_c = strdup(nbuf);
            if (!name_c)
            {
                free(url_c);
                free(model_c);
                return -1;
            }
        }
    }

    /* Update existing same name or same url+model. */
    for (i = 0; i < nconns; i++)
    {
        if (strcmp(conns[i].name, name_c) == 0
            || (strcmp(conns[i].url, url_c) == 0
                && strcmp(conns[i].model, model_c) == 0))
        {
            free(conns[i].name);
            free(conns[i].url);
            free(conns[i].model);
            conns[i].name = name_c;
            conns[i].url = url_c;
            conns[i].model = model_c;
            ollama_conn_save();
            return i;
        }
    }

    if (nconns >= OLLAMA_CONN_MAX)
    {
        free(name_c);
        free(url_c);
        free(model_c);
        return -1;
    }

    conns[nconns].name = name_c;
    conns[nconns].url = url_c;
    conns[nconns].model = model_c;
    nconns++;
    ollama_conn_save();
    return nconns - 1;
}

/* Config line: ollama_connection = name url model  (or url model). */
void
ollama_conn_add_from_config(const char *val)
{
    char buf[768];
    char *name = NULL, *url = NULL, *model = NULL, *p, *sp;

    if (!val || !val[0])
        return;
    snprintf(buf, sizeof(buf), "%s", val);
    p = trim(buf);
    if (strncmp(p, "http://", 7) == 0 || strncmp(p, "https://", 8) == 0)
    {
        url = p;
        sp = strchr(p, ' ');
        if (sp)
        {
            *sp = '\0';
            model = trim(sp + 1);
        }
    }
    else
    {
        name = p;
        sp = strchr(p, ' ');
        if (!sp)
            return;
        *sp = '\0';
        url = trim(sp + 1);
        sp = strchr(url, ' ');
        if (sp)
        {
            *sp = '\0';
            model = trim(sp + 1);
        }
    }
    if (!url || !url[0])
        return;
    if (!model || !model[0])
        model = OLLAMA_DEFAULT_MODEL;
    ollama_conn_add(name, url, model);
}

static void
ollama_conn_remove_at(int idx)
{
    int i;

    if (idx < 0 || idx >= nconns)
        return;
    free(conns[idx].name);
    free(conns[idx].url);
    free(conns[idx].model);
    for (i = idx; i < nconns - 1; i++)
        conns[i] = conns[i + 1];
    nconns--;
    conns[nconns].name = conns[nconns].url = conns[nconns].model = NULL;
    ollama_conn_save();
}

/* ---- Connect UI ------------------------------------------------------- */

/* Activate url/model: handshake, save list entry, start bridge. */
static int
ollama_connect_to(const char *name, const char *url, const char *model)
{
    struct userdata_s *d;
    char herr[192];
    /* Own copies: callers often pass pointers into conns[] or config. */
    char *url_c = NULL, *model_c = NULL, *name_c = NULL;

    if (!gp || !gp->data || !url || !url[0])
        return 1;

    url_c = strdup(url);
    model_c = strdup(model && model[0] ? model : OLLAMA_DEFAULT_MODEL);
    if (name && name[0])
        name_c = strdup(name);

    if (strncmp(url_c, "http://", 7) != 0 && strncmp(url_c, "https://", 8) != 0)
    {
        message(ERROR_STR, ANY_KEY_STR, "%s",
                _("URL must start with http:// (HTTPS is not supported yet)."));
        free(url_c);
        free(model_c);
        free(name_c);
        return 1;
    }
    if (strncmp(url_c, "https://", 8) == 0)
    {
        message(ERROR_STR, ANY_KEY_STR, "%s",
                _("HTTPS is not supported yet; use http:// on a local Ollama."));
        free(url_c);
        free(model_c);
        free(name_c);
        return 1;
    }

    free(config.ollama_url);
    config.ollama_url = strdup(url_c);
    free(config.ollama_model);
    config.ollama_model = strdup(model_c);

    d = gp->data;

    /* Drop any prior bridge (including one left on an older game). */
    free_engine(gp);

    update_status_notify(gp,
                         _("Handshaking with Ollama (%ds timeout)…"),
                         OLLAMA_HANDSHAKE_TIMEOUT_SEC);
    update_status_window(gp);
    cboard_ui_refresh();

    if (ollama_handshake(config.ollama_url, config.ollama_model, herr,
                         sizeof(herr)) != 0)
    {
        free(config.ollama_url);
        config.ollama_url = NULL;
        free(config.ollama_model);
        config.ollama_model = NULL;
        SET_FLAG(d->flags, CF_HUMAN);
        CLEAR_FLAG(d->flags, CF_ENGINE_LOOP);
        message(ERROR_STR, ANY_KEY_STR,
                _("Ollama handshake failed:\n%s\n\n"
                  "URL: %s\nModel: %s\n\n"
                  "Check that Ollama is running and the model name is correct."),
                herr[0] ? herr : _("unknown error"), url_c, model_c);
        update_status_notify(gp, "%s",
                             _("Ollama handshake failed; back to human/human."));
        update_all(gp);
        free(url_c);
        free(model_c);
        free(name_c);
        return 1;
    }

    /* Remember successful endpoints for next time (safe vs conns[] aliases). */
    ollama_conn_add(name_c, url_c, model_c);

    CLEAR_FLAG(d->flags, CF_HUMAN);
    CLEAR_FLAG(d->flags, CF_ENGINE_LOOP);

    if (start_ollama_engine(gp) != 0)
    {
        free(url_c);
        free(model_c);
        free(name_c);
        return 1;
    }

    {
        char *fen = pgn_game_to_fen(gp, d->b);

        add_engine_command(gp, ENGINE_READY, "xboard\n");
        add_engine_command(gp, ENGINE_READY, "protover 2\n");
        add_engine_command(gp, ENGINE_READY, "new\n");
        add_engine_command(gp, ENGINE_READY, "setboard %s\n", fen);
        free(fen);
        if (gp->turn != gp->side)
            add_engine_command(gp, ENGINE_THINKING, "go\n");
        else
            d->engine->status = ENGINE_READY;
    }

    update_status_notify(gp,
                         _("Connected to Ollama at %s (%s) — READY-TO-PLAY"),
                         config.ollama_url, config.ollama_model);
    update_all(gp);
    free(url_c);
    free(model_c);
    free(name_c);
    return 0;
}

static void
connect_ollama_new_finalize(WIN *win)
{
    struct input_data_s *in = win->data;
    char *url, *model, *name, *sp;
    char *buf;

    if (!in || !in->str || !in->str[0])
    {
        if (in)
        {
            free(in->str);
            free(in);
        }
        return;
    }

    buf = in->str;
    name = NULL;
    url = buf;
    while (*url == ' ' || *url == '\t')
        url++;

    /*
     * Formats:
     *   http://host:port [model]
     *   name http://host:port [model]
     */
    if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0)
    {
        name = url;
        sp = strchr(url, ' ');
        if (!sp)
        {
            message(ERROR_STR, ANY_KEY_STR, "%s",
                    _("Expected: [name] http://host:port [model]"));
            free(in->str);
            free(in);
            return;
        }
        *sp = '\0';
        url = sp + 1;
        while (*url == ' ' || *url == '\t')
            url++;
    }

    model = NULL;
    sp = strchr(url, ' ');
    if (sp)
    {
        *sp = '\0';
        model = sp + 1;
        while (*model == ' ' || *model == '\t')
            model++;
        if (!*model)
            model = NULL;
    }

    ollama_connect_to(name, url,
                      model && model[0] ? model : OLLAMA_DEFAULT_MODEL);
    free(in->str);
    free(in);
}

static void
prompt_new_ollama_connection(void)
{
    struct input_data_s *id;
    char init[512];

    snprintf(init, sizeof(init), "%s %s",
             config.ollama_url && config.ollama_url[0]
                 ? config.ollama_url
                 : OLLAMA_DEFAULT_URL,
             config.ollama_model && config.ollama_model[0]
                 ? config.ollama_model
                 : OLLAMA_DEFAULT_MODEL);

    id = Calloc(1, sizeof(struct input_data_s));
    id->efunc = connect_ollama_new_finalize;
    construct_input(_("New Ollama connection"), init, 1, 1,
                    _("[name] URL [model]  e.g. ghosthall "
                      "http://172.16.0.75:11434 gemma4:26b\n"
                      "OpenAI base OK: http://host:11434/v1 model"),
                    NULL, NULL, 0, id, -1, NULL, -1);
}

/* Index of first real connection in the menu (after action rows). */
#define CONN_MENU_ACTIONS 2

static struct menu_item_s **
get_ollama_conn_items(WIN *win)
{
    struct menu_input_s *m = win->data;
    struct menu_item_s **items = NULL;
    int i, n = 0;
    char label[256];

    ollama_conn_load();

    /* Land on a saved opponent, not New/Remove (easy to overshoot). */
    if (!m->items && conn_menu_mode == 0 && nconns > 0)
        m->selected = CONN_MENU_ACTIONS;

    /* Free previous items if rebuild. */
    if (m->items)
    {
        for (i = 0; m->items[i]; i++)
        {
            free(m->items[i]->name);
            free(m->items[i]->value);
            free(m->items[i]);
        }
        free(m->items);
        m->items = NULL;
    }

    if (conn_menu_mode == 0)
    {
        items = Realloc(items, (n + 2) * sizeof(*items));
        items[n] = Calloc(1, sizeof(**items));
        items[n]->name = strdup(_("New connection…"));
        items[n]->value = strdup(_("type URL and model"));
        n++;

        items = Realloc(items, (n + 2) * sizeof(*items));
        items[n] = Calloc(1, sizeof(**items));
        items[n]->name = strdup(_("Remove connection…"));
        items[n]->value = nconns
                              ? strdup(_("pick one to delete"))
                              : strdup(_("(list empty)"));
        n++;

        for (i = 0; i < nconns; i++)
        {
            items = Realloc(items, (n + 2) * sizeof(*items));
            items[n] = Calloc(1, sizeof(**items));
            items[n]->name = strdup(conns[i].name);
            snprintf(label, sizeof(label), "%s  %s", conns[i].model,
                     conns[i].url);
            items[n]->value = strdup(label);
            n++;
        }
    }
    else
    {
        /* Remove mode: only saved entries + cancel. */
        for (i = 0; i < nconns; i++)
        {
            items = Realloc(items, (n + 2) * sizeof(*items));
            items[n] = Calloc(1, sizeof(**items));
            items[n]->name = strdup(conns[i].name);
            snprintf(label, sizeof(label), "%s  %s", conns[i].model,
                     conns[i].url);
            items[n]->value = strdup(label);
            n++;
        }
        items = Realloc(items, (n + 2) * sizeof(*items));
        items[n] = Calloc(1, sizeof(**items));
        items[n]->name = strdup(_("Cancel"));
        items[n]->value = strdup(_("back to list"));
        n++;
    }

    items = Realloc(items, (n + 1) * sizeof(*items));
    items[n] = NULL;
    m->items = items;
    m->total = n;
    m->nofree = 0;
    return items;
}

static void
do_ollama_conn_help(struct menu_input_s *m)
{
    (void) m;
    message(_("Ollama Connections"), ANY_KEY_STR, "%s",
            _("    UP/DOWN - move\n"
              "      ENTER - connect / choose action\n"
              "          d - delete selected connection\n"
              "     ESCAPE - cancel\n\n"
              "Successful connects are saved under ~/.cboard/\n"
              "ollama_connections for next time."));
}

static void
do_ollama_conn_abort(struct menu_input_s *m)
{
    (void) m;
    conn_menu_mode = 0;
    pending_new_conn = 0;
    pending_connect_idx = -1;
    pushkey = -1;
}

static void
do_ollama_conn_delete_key(struct menu_input_s *m)
{
    int idx;

    if (conn_menu_mode != 0)
        return;
    if (!m->items || m->selected < CONN_MENU_ACTIONS)
        return;
    idx = m->selected - CONN_MENU_ACTIONS;
    if (idx < 0 || idx >= nconns)
        return;
    ollama_conn_remove_at(idx);
    update_status_notify(gp, "%s", _("Removed saved Ollama connection."));
}

static void
do_ollama_conn_activate(struct menu_input_s *m)
{
    if (!m->items || m->selected < 0 || m->selected >= m->total)
        return;

    if (conn_menu_mode == 1)
    {
        /* Last item is Cancel. */
        if (m->selected >= nconns)
        {
            conn_menu_mode = 0;
            return;
        }
        ollama_conn_remove_at(m->selected);
        conn_menu_mode = 0;
        update_status_notify(gp, "%s", _("Removed saved Ollama connection."));
        return;
    }

    if (m->selected == 0)
    {
        /* Close menu first; exit handler opens the input dialog. */
        pending_new_conn = 1;
        pending_connect_idx = -1;
        pushkey = -1;
        return;
    }
    if (m->selected == 1)
    {
        if (nconns == 0)
        {
            message(NULL, ANY_KEY_STR, "%s",
                    _("No saved connections to remove."));
            return;
        }
        conn_menu_mode = 1;
        return;
    }

    {
        int idx = m->selected - CONN_MENU_ACTIONS;

        if (idx < 0 || idx >= nconns)
            return;
        pending_new_conn = 0;
        pending_connect_idx = idx;
        pushkey = -1;
    }
}

static void
ollama_conn_menu_exit(WIN *win)
{
    int idx;
    char *url = NULL, *model = NULL, *name = NULL;

    (void) win;
    conn_menu_mode = 0;

    if (pending_new_conn)
    {
        pending_new_conn = 0;
        pending_connect_idx = -1;
        prompt_new_ollama_connection();
        return;
    }

    idx = pending_connect_idx;
    pending_connect_idx = -1;
    if (idx < 0 || idx >= nconns)
        return;

    /*
     * Snapshot before connect: ollama_conn_add may free/replace conns[]
     * slots during the handshake success path.
     */
    name = strdup(conns[idx].name);
    url = strdup(conns[idx].url);
    model = strdup(conns[idx].model);
    ollama_connect_to(name, url, model);
    free(name);
    free(url);
    free(model);
}

void
do_global_connect_ollama(void)
{
    struct menu_key_s **keys = NULL;

    ollama_conn_load();
    conn_menu_mode = 0;
    pending_new_conn = 0;
    pending_connect_idx = -1;

    /* No saved entries yet — go straight to the new-connection prompt. */
    if (nconns == 0)
    {
        prompt_new_ollama_connection();
        return;
    }

    add_menu_help_key(&keys, do_ollama_conn_help);
    add_menu_key(&keys, KEY_ESCAPE, do_ollama_conn_abort);
    /* vterm / some hosts deliver CR or KEY_ENTER, not LF. Space is
     * the same select key as on the board and always comes through. */
    add_menu_key(&keys, '\n', do_ollama_conn_activate);
    add_menu_key(&keys, '\r', do_ollama_conn_activate);
    add_menu_key(&keys, KEY_ENTER, do_ollama_conn_activate);
    add_menu_key(&keys, ' ', do_ollama_conn_activate);
    add_menu_key(&keys, 'd', do_ollama_conn_delete_key);
    add_menu_key(&keys, 'D', do_ollama_conn_delete_key);
    construct_menu(0, 0, -1, -1, _("Ollama Connections"), 0,
                   get_ollama_conn_items, keys, NULL, NULL,
                   ollama_conn_menu_exit, NULL);
}

void
do_global_disconnect_ollama(void)
{
    struct userdata_s *d;

    if (gp && gp->data && engine_is_ollama(gp))
        free_engine(gp);

    free(config.ollama_url);
    config.ollama_url = NULL;
    free(config.ollama_model);
    config.ollama_model = NULL;

    if (gp && gp->data)
    {
        d = gp->data;
        SET_FLAG(d->flags, CF_HUMAN);
        CLEAR_FLAG(d->flags, CF_ENGINE_LOOP);
        update_status_notify(gp, "%s", _("Disconnected Ollama; human/human."));
        update_all(gp);
    }
}
