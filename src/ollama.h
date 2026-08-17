/* vim:tw=78:ts=4:sw=4:sts=4:et:set ft=c:  */
/*
    Ollama HTTP opponent for cboard (XBoard-facing bridge).
*/
#ifndef CBOARD_OLLAMA_H
#define CBOARD_OLLAMA_H

#include "engine.h"

/* Defaults when the user has not configured Connect yet. */
#define OLLAMA_DEFAULT_URL "http://127.0.0.1:11434"
#define OLLAMA_DEFAULT_MODEL "llama3.2"

/*
 * Fork an XBoard-speaking child that translates go/moves into Ollama chat
 * HTTP (native /api/chat or OpenAI-compatible /v1/chat/completions).
 * Fills g->data->engine fds/pid like a local engine.
 * Returns 0 on success, non-zero on failure (message already shown).
 */
int start_ollama_engine(GAME g);

/* True if the active engine is the Ollama bridge (not gnuchess/etc). */
int engine_is_ollama(GAME g);

/*
 * Show saved connections (or New… if none).  Successful connects are stored
 * under ~/.cboard/ollama_connections.
 */
void do_global_connect_ollama(void);

/* Drop Ollama URL binding and stop the opponent engine if it is Ollama. */
void do_global_disconnect_ollama(void);

/* Set/clear the PGN Black tag from the current Ollama model. */
void ollama_set_black_tag(GAME g);
void ollama_clear_black_tag(GAME g);

/* Load/save helpers (also used from rcfile). */
void ollama_conn_load(void);
void ollama_conn_save(void);
int ollama_conn_add(const char *name, const char *url, const char *model);
/* Parse "name url model" or "url model" from a config line value. */
void ollama_conn_add_from_config(const char *val);

#endif
