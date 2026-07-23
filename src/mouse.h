/* vim:tw=78:ts=4:sw=4:sts=4:et:set ft=c:  */
/*
    Mouse dispatch for cboard — built on vk_kmio MEVENT + VDK hit tests.
*/
#ifndef CBOARD_MOUSE_H
#define CBOARD_MOUSE_H

#include <ncursesw/curses.h>

/*
 * Handle one mouse event from vk_kmio_fetch (KEY_MOUSE path).
 * Returns 1 if the event was consumed.
 */
int cboard_mouse_handle(const MEVENT *mev);

/* Board square hit-test (implemented in cboard.c). */
int cboard_board_mouse(int x, int y, mmask_t bstate);

#endif
