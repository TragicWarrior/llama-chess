/* vim:tw=78:ts=4:sw=4:sts=4:et:set ft=c:  */
#ifndef CBOARD_SPLASH_H
#define CBOARD_SPLASH_H

/*
 * Llama Chess 80x25 title screen.  Draws on stdscr, waits for a key or
 * mouse click, then erases so the board chrome can take over.
 */
void llama_chess_splash(void);

#endif
