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

#include "ui_screen.h"
#include "splash.h"

/* Live terminal size (updated on each paint / resize). */
static int sw, sh;

/* Local pairs — well above VDK's usual 1..64 range. */
#define P_BLUE 240
#define P_CBLUE 241
#define P_CYAN 242
#define P_CCYAN 243
#define P_WHITE 244
#define P_GRAY 245
#define P_DGRAY 246
#define P_YELLOW 247
#define P_BROWN 248
#define P_RED 249
#define P_BLACK 250
#define P_ONRED 251
#define P_REDFILL 252
#define P_ONYEL 253
#define P_GRAYFILL 254

static void
geom_sync(void)
{
    sw = COLS > 2 ? COLS : 2;
    sh = LINES > 2 ? LINES : 2;
}

static void
pairs_init(void)
{
    init_pair(P_BLUE, COLOR_BLUE, COLOR_BLACK);
    init_pair(P_CBLUE, COLOR_BLUE, COLOR_BLACK);
    init_pair(P_CYAN, COLOR_CYAN, COLOR_BLACK);
    init_pair(P_CCYAN, COLOR_CYAN, COLOR_BLACK);
    init_pair(P_WHITE, COLOR_WHITE, COLOR_BLACK);
    init_pair(P_GRAY, COLOR_WHITE, COLOR_BLACK);
    init_pair(P_DGRAY, COLOR_BLACK, COLOR_BLACK);
    init_pair(P_YELLOW, COLOR_YELLOW, COLOR_BLACK);
    init_pair(P_BROWN, COLOR_YELLOW, COLOR_BLACK);
    init_pair(P_RED, COLOR_RED, COLOR_BLACK);
    init_pair(P_BLACK, COLOR_BLACK, COLOR_BLACK);
    init_pair(P_ONRED, COLOR_WHITE, COLOR_RED);
    init_pair(P_REDFILL, COLOR_RED, COLOR_RED);
    /* Color 8 = bright black / “alt black” on 16-color terminals. */
    {
        int gray = (COLORS >= 16) ? 8 : COLOR_BLACK;

        init_pair(P_ONYEL, COLOR_YELLOW, gray);
        init_pair(P_GRAYFILL, gray, gray);
    }
}

static int
pair_for(int id)
{
    switch (id)
    {
    case P_CBLUE:
    case P_CCYAN:
    case P_WHITE:
    case P_YELLOW:
        return id;
    default:
        return id;
    }
}

static int
attr_for(int id)
{
    switch (id)
    {
    case P_CBLUE:
    case P_CCYAN:
    case P_WHITE:
    case P_YELLOW:
    case P_ONRED:
    case P_ONYEL:
        return A_BOLD;
    case P_GRAY:
        return A_BOLD;
    case P_DGRAY:
        return A_DIM;
    case P_BROWN:
        return A_NORMAL; /* dark yellow = brown */
    default:
        return A_NORMAL;
    }
}

static void
cell(int y, int x, const char *ch, int pid)
{
    if (y < 0 || y >= sh || x < 0 || x >= sw)
        return;
    attr_set(attr_for(pid), pair_for(pid), NULL);
    mvaddstr(y, x, ch);
}

static void
fill_row(int y, int x0, int x1, const char *ch, int pid)
{
    int x;

    for (x = x0; x <= x1; x++)
        cell(y, x, ch, pid);
}

static void
draw_border(void)
{
    int x, y;

    attr_set(A_NORMAL, P_BLACK, NULL);
    for (y = 0; y < sh; y++)
        for (x = 0; x < sw; x++)
            cell(y, x, " ", P_BLACK);

    /* One-cell double pinstripe — follows the live terminal. */
    cell(0, 0, "╔", P_CBLUE);
    cell(0, sw - 1, "╗", P_CBLUE);
    cell(sh - 1, 0, "╚", P_CBLUE);
    cell(sh - 1, sw - 1, "╝", P_CBLUE);
    for (x = 1; x < sw - 1; x++)
    {
        cell(0, x, "═", P_CBLUE);
        cell(sh - 1, x, "═", P_CBLUE);
    }
    for (y = 1; y < sh - 1; y++)
    {
        cell(y, 0, "║", P_CBLUE);
        cell(y, sw - 1, "║", P_CBLUE);
    }
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

    if (x < 1)
        x = 1;
    attr_set(attr_for(pid), pair_for(pid), NULL);
    if (y >= 0 && y < sh && x >= 0 && x < sw)
        mvaddstr(y, x, s);
}

static void
paint(void)
{
    const int title_h = 6;
    const int rank_h = 3;
    const int cluster = title_h + 2 + rank_h + 2 + 1 + 1 + 1;
    int top;

    geom_sync();
    wbkgd(stdscr, COLOR_PAIR(P_BLACK) | ' ');
    erase();

    draw_border();

    top = (sh - cluster) / 2;
    if (top < 2)
        top = 2;

    draw_title(top);
    draw_rank(top + title_h + 2);
    draw_centered(top + title_h + 2 + rank_h + 2, "Press any key to start",
                  P_YELLOW);
    draw_centered(top + title_h + 2 + rank_h + 2 + 2, "v1.0.0", P_CBLUE);

    refresh();
}

void
llama_chess_splash(void)
{
    wint_t k;
    MEVENT mev;
    int last_w, last_h;

    pairs_init();
    curs_set(0);
    wtimeout(stdscr, 100);
    paint();
    last_w = sw;
    last_h = sh;
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
            paint();
            last_w = sw;
            last_h = sh;
            if (r == 1 && k == KEY_RESIZE)
                continue;
        }
        if (r == 1 && k && k != KEY_RESIZE)
            break;
        if (r == 2)
            break;
    }

    erase();
    refresh();
}
