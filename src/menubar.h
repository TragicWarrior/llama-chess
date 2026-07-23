/* vim:tw=78:ts=4:sw=4:sts=4:et:set ft=c:  */
/*
    VWM-style top menubar for cboard (F10 activates).
*/
#ifndef MENUBAR_H
#define MENUBAR_H

#define CBOARD_MENUBAR_H 1

void cboard_menubar_init(void);
void cboard_menubar_shutdown(void);
void cboard_menubar_resize(void);
void cboard_menubar_refresh(void);

/* True while the bar has focus or a dropdown is open. */
int cboard_menubar_active(void);

/*
 * Handle a key while the menubar is active, or when F10 is pressed.
 * Returns 1 if the key was consumed (caller should not dispatch further).
 */
int cboard_menubar_key(wint_t c);

/*
 * Handle a screen-coordinate mouse event (MEVENT from vk_kmio_fetch).
 * Returns 1 if consumed.
 */
int cboard_menubar_mouse(int x, int y, mmask_t bstate);

#endif
