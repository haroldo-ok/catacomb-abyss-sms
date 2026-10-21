/* hud.c - the status area below the 3-D view (banked).  Static tiles and
 * the nametable come from tools/hud.py; numbers, keys and scrolls are
 * nametable writes; the face and the radar are dynamic tile blocks. */
#include "game/def.h"
#include "render.h"

u8 hud_freeze_on;
static u8 face_cur;
static u8 radar_old[HUD_RADAR_W * HUD_RADAR_H];     /* tiles holding blips */
static u8 radar_new[HUD_RADAR_W * HUD_RADAR_H];
static u8 rt[32];
#define MAX_BLIPS MAX_RADAR_BLIPS
static u8 blipcell[MAX_BLIPS];
static s8 last_blips[MAX_BLIPS][3];
static s16 last_nx, last_ny;
static s16 health_last;

static void nt_put(u8 x, u8 y, u16 tile)
{
    vram_put_word(VRAM_NAMETABLE + ((((u16)y << 5) + x) << 1), tile);
}

void hud_invalidate(void) B
{
    face_cur = 0xFF;
    memset(radar_old, 1, sizeof(radar_old));
    last_nx = -999;
    health_last = -1;
}

/* tiles + nametable rows 11..23 (and the frame rows 0, 1..10 edges) */
void hud_load(void) B
{
    u16 i;
    u8 buf[34];
    u16 a = HUD_TILES_ADDR;
    for (i = 0; i < HUD_NTILES; i++, a += 34)
    {
        u16 idx;
        rom_copy(HUD_TILES_BANK, (const void *)a, buf, 34);
        idx = buf[0] | (buf[1] << 8);
        vram_write32(idx << 5, buf + 2);
    }
    /* nametable: every cell that isn't the 3-D view */
    {
        u8 y;
        u16 row[32];
        for (y = 0; y < 24; y++)
        {
            rom_copy(HUD_NT_BANK, (const void *)(HUD_NT_ADDR + y * 64), row, 64);
            if (y >= VIEW_TY0 && y < VIEW_TY0 + VIEW_TH)
            {
                vram_write_nt(VRAM_NAMETABLE + (y << 6), row, VIEW_TX0);
                vram_write_nt(VRAM_NAMETABLE + (y << 6) + ((VIEW_TX0 + VIEW_TW) << 1),
                              row + VIEW_TX0 + VIEW_TW, 32 - VIEW_TX0 - VIEW_TW);
            }
            else
                vram_write_nt(VRAM_NAMETABLE + (y << 6), row, 32);
        }
    }
    /* dynamic blocks: blank status lines, radar base */
    {
        Surf *o = cursurf;
        cursurf = &msgsurf;
        VW_ClearSurface(8);
        cursurf = &smsgsurf;
        VW_ClearSurface(8);
        cursurf = o;
    }
    rom_to_vram_safe(RADAR_BANK, (const void *)RADAR_ADDR, HUD_RADAR_TILE << 5,
                     HUD_RADAR_W * HUD_RADAR_H * 32);
    memset(radar_old, 0, sizeof(radar_old));
    last_nx = -999;
    health_last = -1;
    face_cur = 0xFF;
    hud_freeze_on = 0;
}

void hud_char(u8 x, u8 y, u8 ch) B
{
    nt_put(x, y, hud_char_tile[ch]);
}

void hud_num(u8 x, u8 y, s16 value, u8 digits) B
{
    char s[6];
    u8 n = 0, i;
    u16 v = value < 0 ? 0 : value;
    /* the health is redrawn every frame: only when it changed */
    if (x == HUD_HEALTH_X && y == HUD_HEALTH_Y)
    {
        if (value == health_last)
            return;
        health_last = value;
    }
    do
    {
        s[n++] = v % 10;
        v /= 10;
    } while (v && n < 5);
    for (i = n; i < digits; i++)
        nt_put(x++, y, hud_char_tile[BLANKCHAR]);
    while (n)
        nt_put(x++, y, hud_char_tile[NUMBERCHARS + s[--n]]);
}

void hud_face(u8 n) B
{
    if (n == face_cur)
        return;
    face_cur = n;
    rom_to_vram_safe(FACES_BANK, (const void *)(FACES_ADDR + n * 800), HUD_FACE_TILE << 5, 800);
}

void hud_scroll(u8 n, u8 owned) B
{
    nt_put(HUD_SCROLL_X + (n & 3), HUD_SCROLL_Y + (n >> 2),
           owned ? hud_char_tile[SCROLLCHARS + n] : HUD_WHITE_TILE);
}

void hud_gems(const s16 *gems) B
{
    u8 i, j;
    for (i = 0; i < 5; i++)
        for (j = 0; j < 2; j++)
            nt_put(HUD_RADAR_X + 1 + i, HUD_RADAR_Y + 5 + j,
                   hud_gem_tile[gems[i] ? 1 : 0][i * 2 + j]);
}

/* time stop percentage over the small message line */
void hud_freeze(s16 percent) B
{
    if (percent < 0)
    {
        hud_freeze_hide();
        return;
    }
    hud_freeze_on = 1;
    hud_num(HUD_FREEZE_X, HUD_FREEZE_Y, percent, 3);
}

void hud_freeze_hide(void) B
{
    u8 i;
    hud_freeze_on = 0;
    for (i = 0; i < 3; i++)
        nt_put(HUD_FREEZE_X + i, HUD_FREEZE_Y,
               HUD_SMSG_TILE + HUD_FREEZE_X - HUD_SMSG_X + i);
}

/* ------------------------------------------------------------- radar */

static const u8 bitm[8] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};

/* pixel (rp_x,rp_y) (0..7) of the cell in rt gets colour rp_c */
static u8 rp_x, rp_y, rp_c;
static void rplot_a(void) __naked
{
    __asm
        ld a,(_rp_y)
        add a,a
        add a,a
        ld e,a
        ld d,#0
        ld hl,#_rt
        add hl,de
        ld a,(_rp_x)
        ld e,a
        push hl
        ld hl,#_bitm
        add hl,de
        ld c,(hl)
        pop hl
        ld a,(_rp_c)
        ld b,#4
00001$:
        rrca
        ld e,a
        ld a,c
        jr c,00002$
        cpl
        and a,(hl)
        jr 00003$
00002$:
        or a,(hl)
00003$:
        ld (hl),a
        inc hl
        ld a,e
        djnz 00001$
        ret
    __endasm;
}

#define RPLOT(x, y, c) do { s16 x_ = (x), y_ = (y); \
    if ((u16)x_ < 8 && (u16)y_ < 8) { rp_x = x_; rp_y = y_; rp_c = (c); rplot_a(); } } while (0)

static u8 mark(s16 x, s16 y)
{
    u8 c;
    if (x < 0 || y < 0 || x >= HUD_RADAR_W * 8 || y >= HUD_RADAR_H * 8)
        return 0xFF;
    c = ((u8)x >> 3) * HUD_RADAR_H + ((u8)y >> 3);
    radar_new[c] = 1;
    return c;
}

/* blips: x,y relative to the radar centre, colour; list ends with
 * colour -1.  nx,ny: top left of the north icon (radar coordinates) */
void hud_radar(const s8 (*blips)[3], s16 nx, s16 ny) B
{
    u8 c, n, i;

    /* nothing moved: nothing to draw */
    for (n = 0; blips[n][2] != -1 && n < MAX_BLIPS - 1; n++)
        ;
    if (nx == last_nx && ny == last_ny && !memcmp(blips, last_blips, (n + 1) * 3))
        return;
    memcpy(last_blips, blips, (n + 1) * 3);
    last_nx = nx;
    last_ny = ny;

    memset(radar_new, 0, sizeof(radar_new));
    for (i = 0; i < n; i++)
        blipcell[i] = mark(RADAR_XCENTER + blips[i][0], RADAR_YCENTER + blips[i][1]);
    mark(nx, ny);
    mark(nx + 7, ny);
    mark(nx, ny + 4);
    mark(nx + 7, ny + 4);
    for (c = 0; c < HUD_RADAR_W * HUD_RADAR_H; c++)
    {
        s16 cx, cy;
        if (!radar_new[c] && !radar_old[c])
            continue;
        rom_copy(RADAR_BANK, (const void *)(RADAR_ADDR + c * 32), rt, 32);
        cx = (c / HUD_RADAR_H) * 8;
        cy = (c % HUD_RADAR_H) * 8;
        if (radar_new[c])
        {
            s16 ox = nx - cx, oy = ny - cy;
            for (i = 0; i < n; i++)
                if (blipcell[i] == c)
                    RPLOT(RADAR_XCENTER + blips[i][0] - cx, RADAR_YCENTER + blips[i][1] - cy, blips[i][2]);
            /* the north icon (8x5) where it overlaps this cell */
            if (ox > -8 && ox < 8 && oy > -5 && oy < 8)
            {
                s8 iy, ix, iy0 = oy < 0 ? -oy : 0, iy1 = oy > 3 ? 8 - oy : 5;
                s8 ix0 = ox < 0 ? -ox : 0, ix1 = ox > 0 ? 8 - ox : 8;
                for (iy = iy0; iy < iy1; iy++)
                {
                    const u8 *ic = north_icon + (iy << 3);
                    for (ix = ix0; ix < ix1; ix++)
                    {
                        u8 p = ic[ix];
                        if (p != 0xFF)
                            RPLOT(ox + ix, oy + iy, p);
                    }
                }
            }
        }
        vram_write32((HUD_RADAR_TILE + c) << 5, rt);
        radar_old[c] = radar_new[c];
    }
}
