/* vid.c - screen modes, palette fades, border colour, fizzle fade
 * (banked) */
#include "game/def.h"
#include "render.h"
#include "screen.h"

u8 screenfaded;
static u8 pal_now[16];

#define FULL_ROW0   4           /* fullsurf: screen rows 4..19, columns 2..29 */
#define FULL_COL0   2
#define TILE_FBLANK 496

static void set_pal(const u8 *p)
{
    cram_load(p, 0, 16);
    cram_load(p, 16, 16);
}

static void fade_level(u8 k)
{
    u8 i;
    for (i = 0; i < 16; i++)
    {
        u8 c = ega_palette_sms[i], r = c & 3, g = (c >> 2) & 3, b = (c >> 4) & 3;
        r = r > k ? r - k : 0;
        g = g > k ? g - k : 0;
        b = b > k ? b - k : 0;
        pal_now[i] = r | (g << 2) | (b << 4);
    }
    set_pal(pal_now);
}

void vid_set_palette_black(void) B
{
    fade_level(3);
    screenfaded = 1;
}

void vid_set_palette_normal(void) B
{
    fade_level(0);
    screenfaded = 0;
}

void VW_WaitVBL(u16 n) B
{
    while (n--)
        wait_vblank();
}

void VW_FadeOut(void) B
{
    u8 k;
    for (k = 1; k <= 3; k++)
    {
        VW_WaitVBL(6);
        fade_level(k);
    }
    screenfaded = 1;
}

void VW_FadeIn(void) B
{
    s8 k;
    for (k = 2; k >= 0; k--)
    {
        VW_WaitVBL(6);
        fade_level(k);
    }
    screenfaded = 0;
}

/* overscan colour (VDP register 7 = sprite palette entry) */
void VW_ColorBorder(s16 color) B
{
    vdp_reg(7, 0xF0 | (color & 15));
}

static void nt_fill(u16 tile)
{
    u16 i;
    vdp_set_addr(VRAM_NAMETABLE);
    for (i = 0; i < 768; i++)
    {
        VDPData = tile & 0xFF;
        __asm__("nop\n nop");
        VDPData = tile >> 8;
    }
}

/* the play screen: HUD + 3-D view, display left off when screenfaded */
void vid_init_game(void) B
{
    vdp_display(0);
    vram_fill(TILE_BLANK << 5, 0, 32);
    vram_fill(TILE_FBLANK << 5, 0, 32);
    nt_fill(TILE_BLANK);
    render_init();
    hud_load();
    render_invalidate();
    VW_ColorBorder(0);
    cursurf = &viewsurf;
    vdp_display(1);
}

/* 224x128 surface for the help viewer and the menus (tiles 0..447,
 * which the HUD and the view share: leaving restores the play screen) */
void vid_fullscreen(u8 on) B
{
    if (on)
    {
        u8 x, y;
        vdp_display(0);
        vram_fill(0, 0, 448 * 32);
        vram_fill(TILE_FBLANK << 5, 0, 8 * 32);
        vram_fill((TILE_FBLANK + 9) << 5, 0, 7 * 32);
        nt_fill(TILE_FBLANK);
        for (x = 0; x < 28; x++)
            for (y = 0; y < 16; y++)
                vram_put_word(VRAM_NAMETABLE + ((((u16)(FULL_ROW0 + y) << 5) + FULL_COL0 + x) << 1),
                              (u16)x * 16 + y);
        cursurf = &fullsurf;
        vdp_display(1);
    }
    else
    {
        vid_init_game();
    }
}

/* full screen picture blob (u16 count, tiles, 768 nametable words) */
void vid_show_screen_rom(u8 bank, u16 addr) B
{
    u16 n, first;
    rom_copy(bank, (const void *)addr, &n, 2);
    vdp_display(0);
    addr += 2;
    first = n > 448 ? 448 : n;
    rom_to_vram(bank, (const void *)addr, 0, first * 32);
    addr += first * 32;
    if (n > 448)
    {
        rom_to_vram(bank, (const void *)addr, 0x3E00, (n - 448) * 32);
        addr += (n - 448) * 32;
    }
    rom_to_vram(bank, (const void *)addr, VRAM_NAMETABLE, 768 * 2);
    vdp_display(1);
}

/* FINALEPIC over the view (nametable rows 0..10) */
void vid_finale(void) B
{
    u16 n, addr = FINALE_ADDR;
    rom_copy(FINALE_BANK, (const void *)addr, &n, 2);
    vdp_display(0);
    addr += 2;
    rom_to_vram(FINALE_BANK, (const void *)addr, 0, n * 32);
    addr += n * 32;
    rom_to_vram(FINALE_BANK, (const void *)addr, VRAM_NAMETABLE, 32 * 11 * 2);
    vdp_display(1);
}

/* FizzleFade: the new view appears cell by cell in pseudo random order
 * (the original's pixel LFSR at tile resolution).  fizzle_begin() blanks
 * the view and stops the renderer from writing the nametable; after
 * render_draw, FizzleFade() reveals the cells nt_state says are right. */
static void view_cell(u8 x, u8 y, u16 tile)
{
    vram_put_word(VRAM_NAMETABLE + ((((u16)(VIEW_TY0 + y)) << 5) + VIEW_TX0 + x) * 2, tile);
}

void fizzle_begin(void) B
{
    u8 x, y;
    for (y = 0; y < VIEW_TH; y++)
        for (x = 0; x < VIEW_TW; x++)
            view_cell(x, y, TILE_BLANK);
    nt_hold = 1;
}

void FizzleFade(void) B
{
    u16 lfsr = 1, cell;
    u8 n = 0, x, y, st;
    nt_hold = 0;
    /* 9 bit maximal LFSR (x^9 + x^5 + 1): 511 states cover the 280 cells */
    do
    {
        u16 bit = (lfsr ^ (lfsr >> 4)) & 1;
        lfsr = (lfsr >> 1) | (bit << 8);
        cell = lfsr - 1;
        if (cell < VIEW_TW * VIEW_TH)
        {
            x = cell / VIEW_TH;
            y = cell % VIEW_TH;
            st = nt_state[cell];
            view_cell(x, y, st == 0 ? TILE_SKY : st == 1 ? TILE_GROUND : cell);
            if (++n == 12)
            {
                n = 0;
                wait_vblank();
            }
        }
    } while (lfsr != 1);
}
