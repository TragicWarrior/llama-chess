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
#include <ctype.h>
#include <errno.h>
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
#include "ui_screen.h"
#include "ollama.h"

#ifndef AI_NAME
#define AI_NAME "Ollama"
#endif

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
        snprintf(u->path, sizeof(u->path), "%s", path);

    return 0;
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
 * (not including headers), or NULL on failure.
 */
static char *
http_post_json(const char *host, int port, const char *path,
               const char *json_body)
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

    if (!host || !path || !json_body)
        return NULL;

    body_len = strlen(json_body);
    snprintf(portstr, sizeof(portstr), "%d", port);

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    if (getaddrinfo(host, portstr, &hints, &res) != 0)
        return NULL;

    for (rp = res; rp; rp = rp->ai_next)
    {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0)
            continue;
        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0)
            break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    if (fd < 0)
        return NULL;

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
                return NULL;
            }
            p += n;
            left -= (size_t) n;
        }
    }
    free(req);

    while ((n = read(fd, buf, sizeof(buf))) > 0)
    {
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
    close(fd);

    if (!resp || len == 0)
    {
        free(resp);
        return NULL;
    }

    hdr_end = strstr(resp, "\r\n\r\n");
    if (!hdr_end)
    {
        free(resp);
        return NULL;
    }
    *hdr_end = '\0';
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

/* From Ollama chat JSON, get message.content (naive, no full JSON parser). */
static char *
ollama_json_message_content(const char *json)
{
    const char *p, *q;
    char *out;
    size_t n;

    if (!json)
        return NULL;
    p = strstr(json, "\"content\"");
    if (!p)
        return NULL;
    p = strchr(p + 9, ':');
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

static int
ollama_request_move(const char *base_url, const char *model, const char *fen,
                    const char *history, char *move_out, size_t move_out_sz)
{
    struct http_url u;
    char path[640];
    char *sys_esc, *user_esc, *model_esc, *json = NULL, *resp = NULL;
    char *content = NULL;
    char user_msg[2048];
    int ok = 0;
    size_t jlen;

    if (parse_http_url(base_url, &u) != 0 || u.use_tls)
        return 0;

    /* path = base path + /api/chat */
    if (strcmp(u.path, "/") == 0)
        snprintf(path, sizeof(path), "/api/chat");
    else
        snprintf(path, sizeof(path), "%s/api/chat", u.path);

    snprintf(user_msg, sizeof(user_msg),
             "You are playing chess as an XBoard engine.\n"
             "Position (FEN): %s\n"
             "Recent moves (UCI): %s\n"
             "Reply with exactly one line in XBoard form: move e2e4\n"
             "Use UCI coordinates only (e.g. move e7e8q for promotion).\n"
             "No other words.",
             fen && fen[0] ? fen : "startpos",
             history && history[0] ? history : "(none)");

    sys_esc = json_escape(
        "You are a chess engine compatible with the XBoard/CECP protocol. "
        "When asked to move, answer with a single line: move <uci>.");
    user_esc = json_escape(user_msg);
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

    resp = http_post_json(u.host, u.port, path, json);
    free(json);
    free(sys_esc);
    free(user_esc);
    free(model_esc);

    if (!resp)
        return 0;

    content = ollama_json_message_content(resp);
    free(resp);
    if (!content)
        return 0;

    ok = extract_uci_move(content, move_out, move_out_sz);
    free(content);
    return ok;
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
            continue;
        }

        /* Bare UCI / SAN-like line from cboard (protocol 1). */
        if (extract_uci_move(p, move, sizeof(move))
            && (p[0] >= 'a' && p[0] <= 'h'))
        {
            if (history[0])
                strncat(history, " ", sizeof(history) - strlen(history) - 1);
            strncat(history, move, sizeof(history) - strlen(history) - 1);
            /* Human moved; cboard may send go next or expect auto reply.
               We wait for go to keep one request per turn. */
            continue;
        }

        if (strcmp(p, "go") == 0)
        {
            if (ollama_request_move(url, model, fen, history, move,
                                    sizeof(move)))
            {
                printf("move %s\n", move);
                if (history[0])
                    strncat(history, " ", sizeof(history) - strlen(history) - 1);
                strncat(history, move, sizeof(history) - strlen(history) - 1);
            }
            else
            {
                /* Tell the parent something went wrong in a way it can log. */
                printf("Error (ollama): no move\n");
            }
            fflush(stdout);
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

/* ---- Connect UI ------------------------------------------------------- */

static void
connect_ollama_finalize(WIN *win)
{
    struct input_data_s *in = win->data;
    char *url, *model, *sp;
    char *buf;
    struct userdata_s *d;

    if (!in || !in->str || !in->str[0] || !gp || !gp->data)
    {
        if (in)
        {
            free(in->str);
            free(in);
        }
        return;
    }

    buf = in->str;

    /* Format: http://host:port  or  http://host:port modelname */
    url = buf;
    while (*url == ' ' || *url == '\t')
        url++;
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

    if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0)
    {
        message(ERROR_STR, ANY_KEY_STR, "%s",
                _("URL must start with http:// (HTTPS is not supported yet)."));
        free(in->str);
        free(in);
        return;
    }
    if (strncmp(url, "https://", 8) == 0)
    {
        message(ERROR_STR, ANY_KEY_STR, "%s",
                _("HTTPS is not supported yet; use http:// on a local Ollama."));
        free(in->str);
        free(in);
        return;
    }

    free(config.ollama_url);
    config.ollama_url = strdup(url);
    free(config.ollama_model);
    config.ollama_model =
        strdup(model && model[0] ? model : OLLAMA_DEFAULT_MODEL);

    free(in->str);
    free(in);

    d = gp->data;
    free_engine(gp);
    CLEAR_FLAG(d->flags, CF_HUMAN);
    CLEAR_FLAG(d->flags, CF_ENGINE_LOOP);

    if (start_ollama_engine(gp) != 0)
        return;

    {
        char *fen = pgn_game_to_fen(gp, d->b);

        add_engine_command(gp, ENGINE_READY, "xboard\n");
        add_engine_command(gp, ENGINE_READY, "protover 2\n");
        add_engine_command(gp, ENGINE_READY, "new\n");
        add_engine_command(gp, ENGINE_READY, "setboard %s\n", fen);
        free(fen);
        /* If it is the engine's turn, ask for a move immediately. */
        if (gp->turn != gp->side)
            add_engine_command(gp, ENGINE_THINKING, "go\n");
        else
            d->engine->status = ENGINE_READY;
    }

    update_status_notify(gp, _("Connected to Ollama at %s (%s)"),
                         config.ollama_url, config.ollama_model);
    update_all(gp);
}

void
do_global_connect_ollama(void)
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
    id->efunc = connect_ollama_finalize;
    construct_input(_("Connect to Ollama"), init, 1, 1,
                    _("URL [model] — e.g. http://127.0.0.1:11434 llama3.2\n"
                      "The bridge speaks XBoard; Ollama must return UCI moves "
                      "as: move e2e4"),
                    NULL, NULL, 0, id, -1, NULL, -1);
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
