/* menu.c - joypad replacements for the keyboard prompts of the original
 * (banked): the scroll picker, the HELP.TXT viewer (DisplayText), the
 * save/restore dialogs (GE_SaveGame / GE_LoadGame) and the scroll reader.
 * Strings handed to the bank 0 entry points (MenuWindow, US_CPrint...) may
 * live in this bank. */
#include "game/def.h"

s16 MenuScroll(void) B
{
    const char *items[8];
    s16 idx[8];
    static char names[8][10];
    u16 n = 0, i;
    s16 r;
    for (i = 0; i < 8; i++)
        if (gamestate.scrolls[i])
        {
            strcpy(names[n], "Scroll  ");
            names[n][7] = '1' + i;
            items[n] = names[n];
            idx[n] = i;
            n++;
        }
    if (!n)
    {
        SD_PlaySound(NOITEMSND);
        return -1;
    }
    CenterWindow(16, n > 6 ? 8 : n + 2);
    r = MenuWindow("Read which scroll?", n, items, 0);
    lasttext = -1;
    if (r < 0)
        return -1;
    return idx[r];
}

/* ------------------------------------------------------------ help text */

#define HELP_LINES  11
#define HELP_LEFT   12          /* window interior starts at x 8 */
#define HELP_RIGHT  212
#define LINE_MAX    120

static char pb[LINE_MAX + 2];

/* one display line of a help page starting at pos: copies it to pb
 * (without the newline), returns the next position (pos when the page
 * has ended); *center set for '^' lines */
static u16 help_line(u16 page, u16 pos, u8 *center)
{
    char t[LINE_MAX + 2];
    u16 n = help_read(page, pos, t, LINE_MAX + 1);
    u16 length = 0, start = 0;
    s16 curx, maxx;
    u16 maxend;
    u8 tab = 0;

    if (!n)
        return pos;
    *center = 0;
    if (t[0] == '^')
    {
        *center = 1;
        start = 1;
    }
    {
        /* table lines (with a TAB) are never wrapped */
        u16 e = start;
        while (e < n && t[e] != '\n' && t[e] != '\t') e++;
        if (e < n && t[e] == '\t')
        {
            while (e < n && t[e] != '\n') e++;
            length = e - start;
            tab = 1;
        }
    }
    if (!tab)
    {
        const char *s = t + start;
        u16 m = n - start;
        curx = HELP_LEFT;
        while (length < m && s[length] != '\n' &&
               curx + font_widths[(u8)s[length] & 0x7F] < HELP_RIGHT)
            curx += font_widths[(u8)s[length++] & 0x7F];
        if (length < m && s[length] != '\n')
        {
            maxx = curx;
            maxend = length;
            while (length && s[length] != ' ')
                curx -= font_widths[(u8)s[length--] & 0x7F];
            if (!length)
                length = maxend, curx = maxx;
        }
    }
    memcpy(pb, t + start, length);
    pb[length] = 0;
    pos += start + length;
    /* skip the separator */
    if (start + length < n && (t[start + length] == ' ' || t[start + length] == '\n'))
        pos++;
    return pos;
}

static void help_print(u8 center)
{
    char *tab = strchr(pb, '\t');
    if (tab)
    {
        *tab = 0;
        PrintX = HELP_LEFT;
        US_PrintR(pb);
        PrintX = HELP_LEFT + HELP_TAB;
        US_PrintR(tab + 1);
        PrintX = WindowX;
        PrintY += FONT_H;
    }
    else if (center)
        US_CPrintLineR(pb);
    else
    {
        PrintX = HELP_LEFT;
        US_PrintR(pb);
        PrintX = WindowX;
        PrintY += FONT_H;
    }
}

static u16 help_count(u16 page)
{
    u16 pos = 0, next, lines = 0;
    u8 c;
    for (;;)
    {
        next = help_line(page, pos, &c);
        if (next == pos)
            break;
        pos = next;
        lines++;
    }
    return lines;
}

/* the "page x of y" strip under the window (tiles 497..511) */
static void help_strip(u16 page)
{
    char buf[24];
    Surf strip, *o = cursurf;
    u8 i;
    strip.tile0 = 497;
    strip.th = 1;
    strip.tw = 14;
    strip.view = 0;
    strcpy(buf, "Page ");
    intToStr(page + 1, buf + 5, 1);
    strcat(buf, " of ");
    intToStr(HELP_PAGES, buf + strlen(buf), 1);
    cursurf = &strip;
    /* tile 504 is the sprite table terminator: the strip skips it */
    for (i = 0; i < 14; i++)
        vram_put_word(VRAM_NAMETABLE + ((21u << 5) + 9 + i) * 2, 497 + i + (i >= 7));
    strip.tw = 7;
    VW_ClearSurface(0);
    strip.tile0 = 505;
    VW_ClearSurface(0);
    /* draw through both halves: render in two passes */
    {
        u16 w = VW_MeasurePropStringR(buf);
        s16 x = (112 - (s16)w) / 2;
        strip.tile0 = 497;
        px = x; py = -1;
        gfx_text(x, -1, buf, 7);
        strip.tile0 = 505;
        gfx_text(x - 56, -1, buf, 7);
    }
    cursurf = o;
}

void HelpScreens(void) B
{
    s16 page = 0, first = 0, lines = 0;
    boolean redraw = TRUE;

    VW_FadeOut();
    vid_fullscreen(true);
    cursurf = &fullsurf;
    PPT_LeftEdge = HELP_LEFT;
    PPT_RightEdge = HELP_RIGHT;

    for (;;)
    {
        if (redraw)
        {
            u16 pos = 0, next, line = 0;
            u8 c;
            lines = help_count(page);
            US_DrawWindow(1, 1, 26, 14);
            PrintY = WindowY + 2;
            for (;;)
            {
                next = help_line(page, pos, &c);
                if (next == pos || line >= first + HELP_LINES)
                    break;
                /* (a centred line may wrap: never print below the window) */
                if (line >= first && PrintY + FONT_H <= WindowY + WindowH)
                    help_print(c);
                pos = next;
                line++;
            }
            help_strip(page);
            if (screenfaded)
                VW_FadeIn();
            redraw = FALSE;
        }
        {
            u8 p = IN_WaitPress();
            if (p & (PAD_1 | PAD_2 | PAD_PAUSE))
                break;
            if ((p & PAD_LEFT) && page > 0) { page--; first = 0; redraw = TRUE; }
            if ((p & PAD_RIGHT) && page < HELP_PAGES - 1) { page++; first = 0; redraw = TRUE; }
            if ((p & PAD_UP) && first > 0) { first -= 5; if (first < 0) first = 0; redraw = TRUE; }
            if ((p & PAD_DOWN) && first + HELP_LINES < lines) { first += 5; redraw = TRUE; }
        }
    }
    IN_ClearKeysDown();
    VW_FadeOut();
    vid_fullscreen(false);
    if (ingame)
    {
        RedrawStatusWindow();
        DrawText(true);
        ThreeDRefresh();
        VW_FadeIn();
    }
    tics = realtics = 1;
}

/* --------------------------------------------------------- save / load */

static char slot_names[SAVE_SLOTS][40];

static s16 SlotMenu(const char *title)
{
    const char *items[SAVE_SLOTS];
    u8 i;
    for (i = 0; i < SAVE_SLOTS; i++)
    {
        char desc[32];
        slot_names[i][0] = '1' + i;
        slot_names[i][1] = ':';
        slot_names[i][2] = ' ';
        slot_names[i][3] = 0;
        if (save_slot_info(i, desc))
            strcat(slot_names[i], desc);
        else
            strcat(slot_names[i], "- empty -");
        items[i] = slot_names[i];
    }
    VW_ClearSurface(0);
    US_CenterWindow(26, 3 + SAVE_SLOTS + 2);
    return MenuWindow(title, SAVE_SLOTS, items, 0);
}

static void MessageBox(const char *l1, const char *l2)
{
    US_CenterWindow(26, 4);
    PrintY += 4;
    US_CPrintLine(l1);
    if (l2) US_CPrintLine(l2);
    if (screenfaded)
        VW_FadeIn();
    IN_Ack();
}

static const char * const yn[2] = {"Yes", "No"};

void GE_SaveGame(void) B
{
    s16 slot;
    char desc[32];

    for (;;)
    {
        cursurf = &fullsurf;
        slot = SlotMenu("\nChoose a slot to SAVE this game:\n");
        if (slot < 0)
            break;
        if (save_slot_info(slot, desc))
        {
            US_CenterWindow(24, 6);
            US_CPrintLine("That slot is in use...");
            if (MenuWindow("\nOverwrite it ????\n", 2, yn, 1) != 0)
                continue;
        }
        if (!SaveTheGame(slot))
            MessageBox("Not enough room in this slot,", "the game was NOT saved.");
        else
            MessageBox("Game saved.", NULL);
        break;
    }
    IN_ClearKeysDown();
    if (!screenfaded)
        VW_FadeOut();
}

boolean GE_LoadGame(void) B
{
    s16 slot;
    char desc[32];
    boolean rt = false;

    IN_ClearKeysDown();
    for (;;)
    {
        cursurf = &fullsurf;
        slot = SlotMenu("\nChoose the game to RESTORE:\n");
        if (slot < 0)
            break;
        if (!save_slot_info(slot, desc))
        {
            MessageBox("That slot is empty....", "Press a button to try again.");
            continue;
        }
        if (!LoadTheGame(slot))
        {
            MessageBox("This saved game is damaged.", "Press a button to try again.");
            continue;
        }
        rt = true;
        break;
    }
    IN_ClearKeysDown();
    if (!screenfaded)
        VW_FadeOut();
    return rt;
}

/* ------------------------------------------------------------ scrolls */

/* the scroll picture full screen (the original draws it over the view);
 * button 1, 2 or PAUSE goes back to the game */
void ShowScroll(u8 n) B
{
    VW_FadeOut();
    vid_show_screen_rom(scroll_bank[n], scroll_addr[n]);
    VW_FadeIn();
    IN_ClearKeysDown ();
    while (!(IN_WaitPress() & (PAD_1 | PAD_2 | PAD_PAUSE)))
        ;
    IN_ClearKeysDown ();
    VW_FadeOut();
    vid_init_game();
    RedrawStatusWindow();
    DrawText(true);
    ThreeDRefresh();
    VW_FadeIn();
}
