/* gfx.c - 2-D drawing for the SMS port (banked): the ID_VW / ID_US text and
 * window routines on tile surfaces in VRAM.
 *
 * A surface is a column major block of tiles (tile = tile0 + tx*th + ty).
 * Drawing reads a tile from VRAM, changes it and writes it back (paced, so
 * it works with the display on).  Text is first rendered into a 1 bit per
 * pixel line buffer, then merged into each tile it touches once.
 * The 3-D view surface shares its tiles with the renderer: a cell drawn on
 * gets its own tile (cell contents copied from whatever the nametable
 * showed there) and the renderer is told to redraw everything next frame.
 */
#include "game/def.h"
#include "render.h"

Surf viewsurf, msgsurf, smsgsurf, fullsurf, facesurf, radarsurf;
Surf *cursurf;
s16  px, py;
u8   fontcolor;
u8   fontxor;
s16  PrintX, PrintY, WindowX, WindowY, WindowW, WindowH;
s16  PPT_LeftEdge, PPT_RightEdge;

#define TXT_BYTES 34                    /* 272 pixels */
static u8 txt[FONT_H][TXT_BYTES];
static u8 tb[32];                       /* tile being changed */
static u16 t_vaddr;
static u8 view_touched;

#define STATUSCOLOR 8
#define TEXTCOLOR   14

static void surf_set(Surf *s, u16 tile0, u8 th, u8 tw, u8 view)
{
    s->tile0 = tile0;
    s->th = th;
    s->tw = tw;
    s->view = view;
}

void gfx_init(void) B
{
    surf_set(&viewsurf, 0, VIEW_TH, VIEW_TW, 1);
    surf_set(&msgsurf, HUD_MSG_TILE, 1, HUD_MSG_W, 0);
    surf_set(&smsgsurf, HUD_SMSG_TILE, 1, HUD_SMSG_W, 0);
    surf_set(&fullsurf, 0, 16, 28, 0);
    surf_set(&facesurf, HUD_FACE_TILE, 5, 5, 0);
    surf_set(&radarsurf, HUD_RADAR_TILE, 5, 7, 0);
    cursurf = &viewsurf;
    fontcolor = WHITE;
    fontxor = 1;
}

/* ------------------------------------------------------------- tiles */

/* load cell (tx,ty) into tb; full: contents will be replaced, skip read */
static void tload(Surf *s, u8 tx, u8 ty, u8 full)
{
    u16 t = s->tile0 + (u16)tx * s->th + ty;
    t_vaddr = t << 5;
    if (s->view)
    {
        u16 nta = VRAM_NAMETABLE + ((((u16)(VIEW_TY0 + ty) << 5) + VIEW_TX0 + tx) << 1);
        u16 cur;
        if (!full)
        {
            vram_read(nta, (u8 *)&cur, 2);
            vram_read32((cur & 0x1FF) << 5, tb);
        }
        vram_put_word(nta, t);
        view_touched = 1;
    }
    else if (!full)
        vram_read32(t_vaddr, tb);
}

static void tstore(void)
{
    vram_write32(t_vaddr, tb);
}

/* plane fill bytes of a colour: 0xFF where the colour has the plane bit */
static u8 pc[4];
static void set_pc(u8 c)
{
    pc[0] = (c & 1) ? 0xFF : 0;
    pc[1] = (c & 2) ? 0xFF : 0;
    pc[2] = (c & 4) ? 0xFF : 0;
    pc[3] = (c & 8) ? 0xFF : 0;
}

/* rows of tb: pixels of mrow[r] get the colour of set_pc (xr: xored) */
static const u8 *pr_m;
static u8 *pr_p;
static u8 pr_n;
static void paint_rows_set(void) __naked
{
    __asm
        ld hl,(_pr_p)
        ld de,(_pr_m)
        ld a,(_pr_n)
        ld b,a
00001$:
        ld a,(de)
        inc de
        or a,a
        jr z,00002$
        ld c,a
        ld a,(_pc+0)
        xor a,(hl)
        and a,c
        xor a,(hl)
        ld (hl),a
        inc hl
        ld a,(_pc+1)
        xor a,(hl)
        and a,c
        xor a,(hl)
        ld (hl),a
        inc hl
        ld a,(_pc+2)
        xor a,(hl)
        and a,c
        xor a,(hl)
        ld (hl),a
        inc hl
        ld a,(_pc+3)
        xor a,(hl)
        and a,c
        xor a,(hl)
        ld (hl),a
        inc hl
        djnz 00001$
        ret
00002$:
        inc hl
        inc hl
        inc hl
        inc hl
        djnz 00001$
        ret
    __endasm;
}

static void paint_rows_xor(void) __naked
{
    __asm
        ld hl,(_pr_p)
        ld de,(_pr_m)
        ld a,(_pr_n)
        ld b,a
00001$:
        ld a,(de)
        inc de
        or a,a
        jr z,00002$
        ld c,a
        ld a,(_pc+0)
        and a,c
        xor a,(hl)
        ld (hl),a
        inc hl
        ld a,(_pc+1)
        and a,c
        xor a,(hl)
        ld (hl),a
        inc hl
        ld a,(_pc+2)
        and a,c
        xor a,(hl)
        ld (hl),a
        inc hl
        ld a,(_pc+3)
        and a,c
        xor a,(hl)
        ld (hl),a
        inc hl
        djnz 00001$
        ret
00002$:
        inc hl
        inc hl
        inc hl
        inc hl
        djnz 00001$
        ret
    __endasm;
}

static void paint_rows(const u8 *mrow, u8 r0, u8 n, u8 xr)
{
    pr_m = mrow + r0;
    pr_p = tb + (r0 << 2);
    pr_n = n;
    if (xr)
        paint_rows_xor();
    else
        paint_rows_set();
}

/* tb = solid colour of set_pc */
static void solid_tb(void) __naked
{
    __asm
        ld de,(_pc)
        ld bc,(_pc+2)
        ld hl,#_tb
        ld a,#8
00001$:
        ld (hl),e
        inc hl
        ld (hl),d
        inc hl
        ld (hl),c
        inc hl
        ld (hl),b
        inc hl
        dec a
        jr nz,00001$
        ret
    __endasm;
}

static void view_done(void)
{
    if (view_touched)
    {
        view_touched = 0;
        render_invalidate();
    }
}

/* -------------------------------------------------------------- bars */

void VW_Bar(s16 x, s16 y, s16 w, s16 h, u8 color) B
{
    Surf *s = cursurf;
    s16 sw = s->tw << 3, sh = s->th << 3;
    s16 x1, y1;
    u8 tx, ty, tx0, tx1, ty0, ty1, solid = 0;
    static u8 mrow[8];
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > sw) w = sw - x;
    if (y + h > sh) h = sh - y;
    if (w <= 0 || h <= 0)
        return;
    x1 = x + w - 1;
    y1 = y + h - 1;
    tx0 = x >> 3; tx1 = x1 >> 3;
    ty0 = y >> 3; ty1 = y1 >> 3;
    set_pc(color);
    for (tx = tx0; tx <= tx1; tx++)
    {
        u8 m = 0xFF;
        if (tx == tx0) m &= 0xFF >> (x & 7);
        if (tx == tx1) m &= 0xFF << (7 - (x1 & 7));
        for (ty = ty0; ty <= ty1; ty++)
        {
            u8 r0 = ty == ty0 ? (y & 7) : 0;
            u8 r1 = ty == ty1 ? (y1 & 7) : 7;
            if (m == 0xFF && r0 == 0 && r1 == 7)
            {
                /* whole tile: no read back */
                if (!solid)
                {
                    solid_tb();
                    solid = 1;
                }
                tload(s, tx, ty, 1);
            }
            else
            {
                u8 r;
                tload(s, tx, ty, 0);
                for (r = r0; r <= r1; r++)
                    mrow[r] = m;
                paint_rows(mrow, r0, r1 - r0 + 1, 0);
                solid = 0;
            }
            tstore();
        }
    }
    view_done();
}

void VW_Plot(s16 x, s16 y, u8 color) B
{
    VW_Bar(x, y, 1, 1, color);
}

void VW_ClearSurface(u8 color) B
{
    VW_Bar(0, 0, cursurf->tw << 3, cursurf->th << 3, color);
}

/* ------------------------------------------------------------- text */

/* width of a string; squeeze > 0 removes up to that many pixels (spaces
 * first, then the blank column of glyphs) */
static u8 sq_space, sq_every;
static void squeeze_plan(const char *s, u16 w, u16 maxw)
{
    u16 extra, n = 0, sp = 0;
    const char *p;
    sq_space = 0;
    sq_every = 0;
    if (!maxw || w <= maxw)
        return;
    extra = w - maxw;
    for (p = s; *p; p++)
    {
        n++;
        if (*p == ' ') sp++;
    }
    if (sp)
    {
        u16 per = (extra + sp - 1) / sp;
        if (per > 2) per = 2;
        sq_space = per;
        extra = extra > per * sp ? extra - per * sp : 0;
    }
    if (extra && n)
    {
        u16 k = n / extra;
        sq_every = k ? (k > 255 ? 255 : k) : 1;
    }
}

static u16 measure(const char *s)
{
    u16 w = 0;
    while (*s)
        w += font_widths[(u8)*s++ & 0x7F];
    return w;
}

/* OR a glyph (FONT_H rows of rg_bpr bytes) into txt at rg_row, shifted
 * right by rg_sh pixels */
static const u8 *rg_d;
static u8 *rg_row;
static u8 rg_bpr, rg_sh;
static void glyph_or(void) __naked
{
    __asm
        push ix
        ld ix,(_rg_row)
        ld de,(_rg_d)
        ld c,#FONT_H
00001$:
        ld a,(_rg_bpr)
        ld b,a
        push ix
00002$:
        ld a,(de)
        inc de
        or a,a
        jr z,00004$
        ld h,a
        ld l,#0
        ld a,(_rg_sh)
        or a,a
        jr z,00005$
00003$:
        srl h
        rr l
        dec a
        jr nz,00003$
00005$:
        ld a,0 (ix)
        or a,h
        ld 0 (ix),a
        ld a,1 (ix)
        or a,l
        ld 1 (ix),a
00004$:
        inc ix
        djnz 00002$
        pop ix
        push bc
        ld bc,#TXT_BYTES
        add ix,bc
        pop bc
        dec c
        jr nz,00001$
        pop ix
        ret
    __endasm;
}

/* render s into txt (cleared first); returns width */
static u8 rt_dry;          /* only measure (with squeezing) */
static u16 rt_x0;          /* first pixel */
static u16 render_txt(const char *s, u16 maxw)
{
    u16 x = rt_x0, w;
    u8 n = 0;
    if (!rt_dry)
        memset(txt, 0, sizeof(txt));
    w = measure(s);
    squeeze_plan(s, w, maxw);
    while (*s)
    {
        u8 ch = (u8)*s++ & 0x7F;
        u8 cw = font_widths[ch];
        const u8 *d = font_data + font_locs[ch];
        u8 bpr = (cw + 7) >> 3;
        u8 sh, xb;
        if (x + cw > TXT_BYTES * 8)
            break;
        if (!rt_dry)
        {
            xb = (u8)(x >> 3);
            sh = (u8)x & 7;
            rg_d = d;
            rg_row = &txt[0][xb];
            rg_bpr = bpr;
            rg_sh = sh;
            glyph_or();
        }
        if (ch == ' ' && sq_space)
            cw -= sq_space;
        else if (sq_every && ++n >= sq_every && cw > 1)
        {
            n = 0;
            cw--;
        }
        x += cw;
    }
    return x - rt_x0;
}

/* the 8 row masks of txt for the tile at surface pixel (px0,py0) when txt
 * is at (x,y); returns nonzero when any pixel is set */
static u8 tmask[8];
static const u8 *tm_p;
static u8 *tm_out;
static u8 tm_n, tm_sh, tm_aok, tm_bok, tm_any;
static void tm_rows(void) __naked
{
    __asm
        ld de,(_tm_p)
        ld a,(_tm_n)
        ld b,a
        ld c,#0
00001$:
        push bc
        ld h,#0
        ld a,(_tm_aok)
        or a,a
        jr z,00002$
        ld a,(de)
        ld h,a
00002$:
        ld l,#0
        ld a,(_tm_bok)
        or a,a
        jr z,00003$
        inc de
        ld a,(de)
        dec de
        ld l,a
00003$:
        ld a,(_tm_sh)
        or a,a
        jr z,00005$
        ld b,a
00004$:
        add hl,hl
        djnz 00004$
00005$:
        ld a,h
        ld hl,(_tm_out)
        ld (hl),a
        inc hl
        ld (_tm_out),hl
        pop bc
        or a,c
        ld c,a
        ld hl,#TXT_BYTES
        add hl,de
        ex de,hl
        djnz 00001$
        ld a,c
        ld (_tm_any),a
        ret
    __endasm;
}

static u8 txt_masks(s16 px0, s16 py0, s16 x, s16 y)
{
    s16 sr0 = py0 - y, o = px0 - x, bi;
    s8 rf, rl;
    tmask[0] = tmask[1] = tmask[2] = tmask[3] = 0;
    tmask[4] = tmask[5] = tmask[6] = tmask[7] = 0;
    rf = sr0 < 0 ? -sr0 : 0;
    rl = FONT_H - sr0 < 8 ? FONT_H - sr0 : 8;
    if (rf >= rl || rf >= 8)
        return 0;
    bi = o >> 3;
    if (bi < -1 || bi >= TXT_BYTES)
        return 0;
    tm_sh = (u8)o & 7;
    tm_aok = bi >= 0;
    tm_bok = bi + 1 < TXT_BYTES && tm_sh;
    tm_p = &txt[sr0 + rf][0] + bi;
    tm_out = tmask + rf;
    tm_n = rl - rf;
    tm_rows();
    return tm_any;
}

/* merge txt (w pixels) at surface position (x,y) */
static void blit_txt(s16 x, s16 y, u16 w)
{
    Surf *s = cursurf;
    s16 sw = s->tw << 3, sh = s->th << 3;
    s16 x1 = x + (s16)w - 1, y1 = y + FONT_H - 1;
    s16 tx, ty, tx0, tx1, ty0, ty1;
    if (!w) return;
    if (x1 >= sw) x1 = sw - 1;
    if (y1 >= sh) y1 = sh - 1;
    tx0 = x < 0 ? 0 : x >> 3;
    ty0 = y < 0 ? 0 : y >> 3;
    tx1 = x1 >> 3;
    ty1 = y1 >> 3;
    set_pc(fontcolor & 15);
    for (tx = tx0; tx <= tx1; tx++)
        for (ty = ty0; ty <= ty1; ty++)
        {
            if (!txt_masks(tx << 3, ty << 3, x, y))
                continue;
            tload(s, tx, ty, 0);
            paint_rows(tmask, 0, 8, fontxor);
            tstore();
        }
}

static void draw_str(const char *s, u16 maxw)
{
    u16 w = render_txt(s, maxw);
    blit_txt(px, py, w);
    px += w;
    view_done();
}

void VW_DrawPropStringR(const char *s) B
{
    draw_str(s, 0);
}

u16 VW_MeasurePropStringR(const char *s) B
{
    return measure(s);
}

void US_PrintR(const char *s) B
{
    char buf[128];
    while (*s)
    {
        u8 n = 0;
        while (s[n] && s[n] != '\n' && n < 127) { buf[n] = s[n]; n++; }
        buf[n] = 0;
        px = PrintX;
        py = PrintY;
        draw_str(buf, 0);
        s += n;
        if (*s == '\n')
        {
            s++;
            PrintX = WindowX;
            PrintY += FONT_H;
        }
        else
            PrintX = px;
    }
}

void US_PrintUnsigned(u32 n) B
{
    char buf[12];
    u8 i = 11;
    buf[11] = 0;
    do
    {
        buf[--i] = '0' + (u8)(n % 10);
        n /= 10;
    } while (n);
    US_PrintR(buf + i);
}

void US_PrintCenteredR(const char *s) B
{
    u16 w = measure(s);
    px = WindowX + ((WindowW - (s16)w) / 2);
    py = WindowY + ((WindowH - FONT_H) / 2);
    draw_str(s, WindowW);
}

/* centred line; lines wider than the window are word wrapped */
void US_CPrintLineR(const char *s) B
{
    char buf[128];
    u16 w = measure(s);
    while (w > (u16)WindowW)
    {
        u8 n = 0, cut = 0;
        u16 x = 0;
        while (s[n] && n < 127)
        {
            x += font_widths[(u8)s[n] & 0x7F];
            if (x > (u16)WindowW) break;
            if (s[n] == ' ') cut = n;
            n++;
        }
        if (!cut) cut = n ? n : 1;
        memcpy(buf, s, cut);
        buf[cut] = 0;
        px = WindowX + ((WindowW - (s16)measure(buf)) / 2);
        py = PrintY;
        if (py + FONT_H <= WindowY + WindowH)     /* (clipped to the window) */
            draw_str(buf, WindowW);
        PrintY += FONT_H;
        s += cut;
        while (*s == ' ') s++;
        w = measure(s);
    }
    px = WindowX + ((WindowW - (s16)w) / 2);
    py = PrintY;
    if (py + FONT_H <= WindowY + WindowH)
        draw_str(s, 0);
    PrintY += FONT_H;
}

void US_CPrintR(const char *s) B
{
    char buf[128];
    while (*s)
    {
        u8 n = 0;
        while (s[n] && s[n] != '\n' && n < 127) { buf[n] = s[n]; n++; }
        buf[n] = 0;
        US_CPrintLineR(buf);
        s += n;
        if (*s) s++;
    }
}

/* ------------------------------------------------------------ windows */

static void frame_tile(s16 tx, s16 ty, u8 piece)
{
    Surf *s = cursurf;
    const u8 *fr = frame_rows[piece];
    u8 r;
    u8 *p = tb;
    if (tx < 0 || ty < 0 || tx >= s->tw || ty >= s->th)
        return;
    tload(s, tx, ty, 0);
    r = 8;
    do
    {
        u8 k = fr[0];
        p[0] = (p[0] & k) | fr[1];
        p[1] = (p[1] & k) | fr[2];
        p[2] = (p[2] & k) | fr[3];
        p[3] = (p[3] & k) | fr[4];
        p += 4;
        fr += 5;
    } while (--r);
    tstore();
}

void US_ClearWindow(void) B
{
    VW_Bar(WindowX, WindowY, WindowW, WindowH, LT_GREY);
    PrintX = WindowX;
    PrintY = WindowY;
}

void US_DrawWindow(u16 x, u16 y, u16 w, u16 h) B
{
    s16 i;
    s16 x0 = (s16)x - 1, y0 = (s16)y - 1, x1 = x + w, y1 = y + h;

    WindowX = x * 8;
    WindowY = y * 8;
    WindowW = w * 8;
    WindowH = h * 8;
    PrintX = WindowX;
    PrintY = WindowY;

    US_ClearWindow();

    frame_tile(x0, y0, 0);
    frame_tile(x0, y1, 6);
    for (i = x0 + 1; i < x1; i++)
    {
        frame_tile(i, y0, 1);
        frame_tile(i, y1, 7);
    }
    frame_tile(x1, y0, 2);
    frame_tile(x1, y1, 8);
    for (i = y0 + 1; i < y1; i++)
    {
        frame_tile(x0, i, 3);
        frame_tile(x1, i, 5);
    }
    view_done();
}

static void center_in(u16 w, u16 h)
{
    u8 tw = cursurf->tw - 2, th = cursurf->th - 2;
    if (w > tw) w = tw;
    if (h > th) h = th;
    US_DrawWindow((cursurf->tw - w) / 2, (cursurf->th - h) / 2, w, h);
}

void US_CenterWindow(u16 w, u16 h) B
{
    center_in(w, h);
}

/* window in the 3-D view (C4_PLAY CenterWindow) */
void CenterWindow(u16 w, u16 h) B
{
    cursurf = &viewsurf;
    center_in(w, h);
}

/* ------------------------------------------------------ status lines */

static void text_at(s16 x, s16 y, const char *s, u8 color, u16 maxw)
{
    u8 ox = fontxor, oc = fontcolor;
    fontxor = 0;
    fontcolor = color;
    px = x;
    py = y;
    draw_str(s, maxw);
    fontxor = ox;
    fontcolor = oc;
}

extern u8 hud_freeze_on;
void hud_freeze_hide(void) B;

/* a status line (one tile row, no 3-D view): composed in RAM, every tile
 * written once without reading VRAM back */
static void status_line(Surf *s, const char *text)
{
    u8 tx, r, any;
    u8 has = text && *text;
    if (has)
    {
        /* text rendered at its final (centred) pixel position: txt byte
         * tx is tile tx, rows 1..8 (FONT_H 10) are the tile rows */
        u16 maxw = s->tw << 3;
        s16 x;
        rt_dry = 1;
        rt_x0 = 0;
        x = ((s16)maxw - (s16)render_txt(text, maxw)) / 2;
        rt_dry = 0;
        rt_x0 = x < 0 ? 0 : x;
        render_txt(text, maxw);
        rt_x0 = 0;
    }
    for (tx = 0; tx < s->tw; tx++)
    {
        set_pc(STATUSCOLOR);
        solid_tb();
        if (has)
        {
            const u8 *p = &txt[1][tx];
            any = 0;
            for (r = 0; r < 8; r++, p += TXT_BYTES)
                any |= (tmask[r] = *p);
            if (any)
            {
                set_pc(TEXTCOLOR);
                paint_rows(tmask, 0, 8, 0);
            }
        }
        vram_write32((s->tile0 + tx) << 5, tb);
    }
}

char DisplayMsgR(const char *text, const char *choices) B
{
    if (choices)
        return MenuChoiceR(text, choices);
    status_line(&msgsurf, text);
    return 1;
}

char DisplaySMsgR(const char *text, const char *choices) B
{
    if (hud_freeze_on)
        hud_freeze_hide();
    status_line(&smsgsurf, text);
    if (choices)
        return MenuChoiceR(text, choices);
    return 1;
}

static const char *choice_label(char c)
{
    switch (c)
    {
    case 'Y': return "Yes";
    case 'N': return "No";
    case 'S': return "Save";
    case 'R': return "Restore";
    case 'W': return "New";
    case 'Q': return "Quit";
    case 'H': return "Help";
    case 'O': return "Sound";
    case 'L': return "Scrolls";
    }
    return "?";
}

/* choice on the message line: LEFT/RIGHT, 1 picks, 2 cancels */
char MenuChoiceR(const char *prompt, const char *choices) B
{
    u8 n = strlen(choices), i, sel = 0;
    u16 total, gap, bw = msgsurf.tw << 3;
    s16 x0;
    Surf *old = cursurf;
    char lab[10];
    u8 hasp = prompt && *prompt;

    IN_ClearKeysDown();
    for (;;)
    {
        cursurf = &msgsurf;
        VW_Bar(0, 0, bw, 8, STATUSCOLOR);
        total = hasp ? measure(prompt) : 0;
        for (i = 0; i < n; i++)
            total += measure(choice_label(choices[i]));
        gap = (bw - 8 - total) / (n + hasp);
        if (gap > 20) gap = 20;
        if (gap < 6) gap = 6;
        total += gap * (n - 1 + hasp);
        x0 = ((s16)bw - (s16)total) / 2;
        if (x0 < 0) x0 = 0;
        if (hasp)
        {
            text_at(x0, -1, prompt, TEXTCOLOR, 0);
            x0 += measure(prompt) + gap;
        }
        for (i = 0; i < n; i++)
        {
            u16 w;
            strcpy(lab, choice_label(choices[i]));
            w = measure(lab);
            if (i == sel)
            {
                VW_Bar(x0 - 2, 0, w + 4, 8, TEXTCOLOR);
                text_at(x0, -1, lab, 0, 0);
            }
            else
                text_at(x0, -1, lab, 7, 0);
            x0 += w + gap;
        }
        {
            u8 p = IN_WaitPress();
            if (p & (PAD_LEFT | PAD_UP))
                sel = sel ? sel - 1 : n - 1;
            else if (p & (PAD_RIGHT | PAD_DOWN))
                sel = (sel + 1) % n;
            else if (p & PAD_OK)
            {
                IN_ClearKeysDown();
                cursurf = old;
                return choices[sel];
            }
            else if (p & (PAD_CANCEL | PAD_PAUSE))
            {
                IN_ClearKeysDown();
                cursurf = old;
                return 0;
            }
        }
    }
}

/* vertical list in the current window (two columns when it doesn't fit) */
s16 MenuWindowR(const char *title, u16 count, const char * const *items, s16 sel) B
{
    u16 i, top, rows, cols, colw;
    if (sel < 0) sel = 0;
    PrintY = WindowY;
    if (title)
        US_CPrintR(title);
    top = PrintY;
    rows = (WindowY + WindowH - top) / 10;
    if (!rows) rows = 1;
    cols = count > rows ? 2 : 1;
    if (cols == 2) rows = (count + 1) / 2;
    colw = WindowW / cols;
    IN_ClearKeysDown();
    for (;;)
    {
        for (i = 0; i < count; i++)
        {
            u16 c = i / rows, r = i % rows;
            u16 w = measure(items[i]);
            s16 cx = WindowX + c * colw;
            s16 x, y = top + r * 10;
            if (w > colw - 4) w = colw - 4;
            x = cx + (colw - w) / 2;
            VW_Bar(cx, y + 1, colw, 10, LT_GREY);
            if (i == (u16)sel)
            {
                VW_Bar(x - 2, y + 1, w + 4, 10, 4);
                text_at(x, y, items[i], 15, colw - 4);
            }
            else
                text_at(x, y, items[i], 8, colw - 4);
        }
        if (screenfaded)
            VW_FadeIn();
        {
            u8 p = IN_WaitPress();
            if (p & PAD_UP)
                sel = sel ? sel - 1 : count - 1;
            else if (p & PAD_DOWN)
                sel = (sel + 1) % count;
            else if ((p & PAD_LEFT) && cols == 2 && (u16)sel >= rows)
                sel -= rows;
            else if ((p & PAD_RIGHT) && cols == 2 && (u16)sel + rows < count)
                sel += rows;
            else if (p & PAD_OK)
            {
                IN_ClearKeysDown();
                return sel;
            }
            else if (p & (PAD_CANCEL | PAD_PAUSE))
            {
                IN_ClearKeysDown();
                return -1;
            }
        }
    }
}

/* text from RAM drawn at (x,y) of the current surface, colour c, no xor */
void gfx_text(s16 x, s16 y, const char *s, u8 c) B
{
    text_at(x, y, s, c, 0);
}
