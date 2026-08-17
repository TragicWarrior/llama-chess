/* vim:tw=78:ts=4:sw=4:sts=4:et:set ft=c:  */
/*
    Llama Chess title screen (full terminal, resizes with the window).

    Block / half-block / box-drawing only.  Classic 16-color palette.
    Layout matches design/intro_screen_example.jpg.
*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <ncurses.h>
#include <vdk.h>

#include "ui_screen.h"
#include "splash.h"

/* Interior of the splash frame (updated on each paint / resize). */
static int sw, sh;
static vk_frame_t *frame;
static vk_widget_t *body;
static WINDOW *cv;

/*
 * Logical inks (not ncurses pair numbers).  Resolved through VDK so the
 * splash works on 8-color terms (COLOR_PAIRS == 64).  High init_pair()
 * ids (240+) are out of range there and leave the screen uncolored.
 */
#define P_BLUE 0
#define P_CBLUE 1
#define P_CYAN 2
#define P_CCYAN 3
#define P_WHITE 4
#define P_GRAY 5
#define P_DGRAY 6
#define P_YELLOW 7
#define P_BROWN 8
#define P_RED 9
#define P_BLACK 10
#define P_ONRED 11
#define P_REDFILL 12
#define P_ONYEL 13
#define P_GRAYFILL 14

static void
geom_sync(void)
{
    int w = 0, h = 0;

    cv = body ? vk_widget_get_canvas(body) : NULL;
    if (cv)
        getmaxyx(cv, h, w);
    sw = w > 0 ? w : 1;
    sh = h > 0 ? h : 1;
}

/* Color 8 (bright black) exists only when the terminal has 16+ colors. */
static short
alt_black(void)
{
    return (COLORS >= 16) ? 8 : COLOR_BLACK;
}

static short
ink_fg(int ink)
{
    switch (ink)
    {
    case P_BLUE:
    case P_CBLUE:
        return COLOR_BLUE;
    case P_CYAN:
    case P_CCYAN:
        return COLOR_CYAN;
    case P_WHITE:
    case P_GRAY:
    case P_ONRED:
        return COLOR_WHITE;
    case P_YELLOW:
    case P_ONYEL:
    case P_BROWN:
        return COLOR_YELLOW;
    case P_RED:
    case P_REDFILL:
        return COLOR_RED;
    case P_DGRAY:
    case P_BLACK:
    case P_GRAYFILL:
        return (ink == P_GRAYFILL) ? alt_black() : COLOR_BLACK;
    default:
        return COLOR_WHITE;
    }
}

static short
ink_bg(int ink)
{
    switch (ink)
    {
    case P_ONRED:
    case P_REDFILL:
        return COLOR_RED;
    case P_ONYEL:
    case P_GRAYFILL:
        return alt_black();
    default:
        return COLOR_BLACK;
    }
}

static attr_t
attr_for(int ink)
{
    switch (ink)
    {
    case P_CBLUE:
    case P_CCYAN:
    case P_WHITE:
    case P_YELLOW:
    case P_ONRED:
    case P_ONYEL:
    case P_GRAY:
        return A_BOLD;
    case P_DGRAY:
        return A_DIM;
    default:
        return A_NORMAL;
    }
}

/*
 * VDK only preloads the 8×8 matrix (pairs 0..63) unless COLORS >= 256.
 * On a 16-color term, color 8 is legal but those pairs are not installed
 * yet — install just the ones we need, above the 8×8 block.
 */
static void
ensure_pair(short fg, short bg)
{
    short pair;

    if (fg < 0 || bg < 0)
        return;
    if (fg <= 7 && bg <= 7)
        return;
    pair = vdk_color_pair(fg, bg);
    if (pair <= 0 || pair >= COLOR_PAIRS)
        return;
    init_pair(pair, fg, bg);
}

static void
pairs_init(void)
{
    short gray = alt_black();

    ensure_pair(COLOR_YELLOW, gray);
    ensure_pair(gray, gray);
}

static short
pair_for(int ink)
{
    short fg = ink_fg(ink);
    short bg = ink_bg(ink);
    short pair;

    if (fg >= COLORS)
        fg = COLOR_WHITE;
    if (bg >= COLORS)
        bg = COLOR_BLACK;

    pair = vdk_color_pair(fg, bg);
    if (pair < 0 || pair >= COLOR_PAIRS)
    {
        if (fg > 7)
            fg = COLOR_WHITE;
        if (bg > 7)
            bg = COLOR_BLACK;
        pair = vdk_color_pair(fg, bg);
    }
    return pair;
}

static void
cell(int y, int x, const char *ch, int pid)
{
    if (cv == NULL || y < 0 || y >= sh || x < 0 || x >= sw)
        return;
    wattr_set(cv, attr_for(pid), pair_for(pid), NULL);
    mvwaddstr(cv, y, x, ch);
}

static void
fill_row(int y, int x0, int x1, const char *ch, int pid)
{
    int x;

    for (x = x0; x <= x1; x++)
        cell(y, x, ch, pid);
}

/* 5x5 glyphs.  # = white █. */
static const char *glyph_L[5] = {"#    ", "#    ", "#    ", "#    ", "#### "};
static const char *glyph_A[5] = {" ### ", "#   #", "#####", "#   #", "#   #"};
static const char *glyph_M[5] = {"#   #", "## ##", "# # #", "#   #", "#   #"};
static const char *glyph_C[5] = {" ####", "#    ", "#    ", "#    ", " ####"};
static const char *glyph_H[5] = {"#   #", "#   #", "#####", "#   #", "#   #"};
static const char *glyph_E[5] = {"#####", "#    ", "#### ", "#    ", "#####"};
static const char *glyph_S[5] = {" ####", "#    ", " ### ", "    #", "#### "};

static const char **
glyph_for(char c)
{
    switch (c)
    {
    case 'L':
        return glyph_L;
    case 'A':
        return glyph_A;
    case 'M':
        return glyph_M;
    case 'C':
        return glyph_C;
    case 'H':
        return glyph_H;
    case 'E':
        return glyph_E;
    case 'S':
        return glyph_S;
    default:
        return NULL;
    }
}

static void
draw_glyph(int y0, int x0, char letter)
{
    const char **g = glyph_for(letter);
    int r, c;

    if (!g)
        return;
    for (r = 0; r < 5; r++)
    {
        for (c = 0; c < 5; c++)
        {
            if (g[r][c] != '#')
                continue;
            cell(y0 + r, x0 + c, "█", P_WHITE);
            if (c == 4 || g[r][c + 1] != '#')
                cell(y0 + r, x0 + c + 1, "▌", P_CCYAN);
        }
    }
    for (c = 0; c < 5; c++)
    {
        if (g[4][c] == '#')
            cell(y0 + 5, x0 + c, "▀", P_CCYAN);
    }
}

static int
title_width(void)
{
    /* 5 letters * 6 cols + 2-col word gap + 5 letters * 6 cols */
    return 5 * 6 + 2 + 5 * 6;
}

static void
draw_title(int y)
{
    const char *word = "LLAMA CHESS";
    int i, cx = (sw - title_width()) / 2;

    for (i = 0; word[i]; i++)
    {
        if (word[i] == ' ')
        {
            cx += 2;
            continue;
        }
        draw_glyph(y, cx, word[i]);
        cx += 6;
    }
}

/*
 * BIG_BOARD 7x3 art from print_piece() / f_pieces[].  Queen crown uses
 * ASCII (the in-game "•°" line is double-width on some terminals).
 */
static const char *big_art[5][3] = {
    {" |-|-| ", "  ] [  ", " /___\\ "}, /* R */
    {"  /?M  ", " (@/)) ", "  /__))" }, /* N */
    {"   O   ", "  (+)  ", "  /_\\  "}, /* B */
    {" oOoOo ", " \\\\|// ", " |___| "}, /* Q */
    {" __+__ ", "(__|__)", " |___| "}, /* K */
};
static const int rank_which[8] = {0, 1, 2, 3, 4, 2, 1, 0};

static void
draw_rank(int y)
{
    const int tw = 7;
    const int th = 3;
    const int n = 8;
    int x0 = (sw - n * tw) / 2;
    int i, r, c, x, red, ink;

    for (i = 0; i < n; i++)
    {
        x = x0 + i * tw;
        red = (i % 2) == 0;
        ink = red ? P_ONRED : P_ONYEL;

        for (r = 0; r < th; r++)
        {
            const char *line = big_art[rank_which[i]][r];

            for (c = 0; c < tw; c++)
            {
                char buf[8];

                if (line[c] == ' ')
                {
                    cell(y + r, x + c, " ", red ? P_REDFILL : P_GRAYFILL);
                    continue;
                }
                buf[0] = line[c];
                buf[1] = '\0';
                cell(y + r, x + c, buf, ink);
            }
        }
    }
}

static void
draw_centered(int y, const char *s, int pid)
{
    int len = (int) strlen(s);
    int x = (sw - len) / 2;

    if (x < 0)
        x = 0;
    if (cv == NULL || y < 0 || y >= sh || x >= sw)
        return;
    wattr_set(cv, attr_for(pid), pair_for(pid), NULL);
    mvwaddstr(cv, y, x, s);
}

static void
paint(void)
{
    const int title_h = 6;
    const int rank_h = 3;
    const int cluster = title_h + 2 + rank_h + 2 + 1 + 1 + 1;
    int top;

    geom_sync();
    if (cv == NULL || frame == NULL)
        return;

    werase(cv);

    top = (sh - cluster) / 2;
    if (top < 0)
        top = 0;

    draw_title(top);
    draw_rank(top + title_h + 2);
    draw_centered(top + title_h + 2 + rank_h + 2, "Press any key to start",
                  P_YELLOW);
    draw_centered(top + title_h + 2 + rank_h + 2 + 2, "v1.0.0", P_CBLUE);

    vk_frame_update(frame);
    if (cboard_ui_screen())
        vk_screen_refresh(cboard_ui_screen());
}

static int
frame_build(int width, int height)
{
    if (width < 3)
        width = 3;
    if (height < 3)
        height = 3;

    frame = vk_frame_create(width, height);
    if (frame == NULL)
        return -1;

    vk_frame_set_border_style(frame, VK_BORDER_DOUBLE);
    vk_frame_set_border_colors(frame, COLOR_BLUE, COLOR_BLACK);
    vk_frame_set_border_attrs(frame, A_BOLD);
    vk_widget_set_colors(VK_WIDGET(frame), COLOR_WHITE, COLOR_BLACK);

    body = vk_widget_create(width - 2, height - 2);
    if (body == NULL)
    {
        vk_frame_destroy(frame);
        frame = NULL;
        return -1;
    }

    vk_widget_set_colors(body, COLOR_WHITE, COLOR_BLACK);
    vk_widget_set_expand(body);
    vk_frame_set_child(frame, body);

    cboard_ui_widget_attach((cboard_widget_t *) frame, 0, 0);
    return 0;
}

static void
frame_teardown(void)
{
    if (frame)
    {
        cboard_ui_widget_detach((cboard_widget_t *) frame);
        vk_frame_destroy(frame);
        frame = NULL;
    }
    if (body)
    {
        vk_widget_destroy(body);
        body = NULL;
    }
    cv = NULL;
}

static void
frame_fit(void)
{
    int w = COLS;
    int h = LINES;

    if (frame == NULL)
        return;
    if (w < 3)
        w = 3;
    if (h < 3)
        h = 3;
    vk_widget_resize(VK_WIDGET(frame), w, h);
}

void
llama_chess_splash(void)
{
    wint_t k;
    MEVENT mev;
    int last_w, last_h;

    /* cboard_ui_init() already owns the vk_screen; do not newterm again. */
    if (cboard_ui_screen() == NULL)
        return;

    pairs_init();
    curs_set(0);
    wtimeout(stdscr, 100);

    if (frame_build(COLS, LINES) != 0)
        return;

    paint();
    last_w = COLS;
    last_h = LINES;
    flushinp();

    for (;;)
    {
        int r = cboard_ui_poll_event(&k, &mev);
        struct winsize ws;
        int nw = last_w, nh = last_h;

        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0)
        {
            nw = ws.ws_col;
            nh = ws.ws_row;
        }
        else
        {
            nw = COLS;
            nh = LINES;
        }

        if ((r == 1 && k == KEY_RESIZE) || nw != last_w || nh != last_h)
        {
            cboard_ui_resize();
            cboard_ui_input_rearm();
            frame_fit();
            paint();
            last_w = COLS;
            last_h = LINES;
            if (r == 1 && k == KEY_RESIZE)
                continue;
        }
        if (r == 1 && k && k != KEY_RESIZE)
            break;
        if (r == 2)
            break;
    }

    frame_teardown();
    if (cboard_ui_screen())
        vk_screen_refresh(cboard_ui_screen());
}
