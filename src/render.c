/* render.c - SMS 3-D view: ray casting, wall/sprite column composition,
 * and bitplane conversion straight into the view's VRAM tiles.
 *
 * The view is 28x10 tiles; tile (tc, r) = tc*10 + r, so one tile column is
 * 320 contiguous VRAM bytes.  Each tile column holds two 4 pixel rays.
 * Cells that are entirely sky or ground point at two shared flat tiles
 * instead, so only the rows that hold walls, sprites or the hand are drawn.
 */
#include "hw.h"
#include "assets.h"
#include "cartram.h"
#include "render.h"
#include <string.h>

#define NUMFLOORS 71
#define RD8(a)  ((const u8 *)(a))
#define RD16(a) ((const u16 *)(a))

#define COLBUF ((u8 *)0xDA00)

u16 r_viewx, r_viewy, r_fine;
u8  r_animframe;
u8  r_skycolor, r_groundcolor;
u8  r_handrows;
u8  vis_stamp;
/* ray_s[] and ray_tbank[] live in render_a.s (adjacent, for rd_sig) */
VisSprite vissprites[MAXVISIBLE];
u8  numvis;

u8  ray_wall[NRAYS];     /* wall texture index, 0xFF: none */
u8  ray_u[NRAYS];
u16 ray_hit[NRAYS];
u8  ray_side[NRAYS];
extern u8 ray_tbank[];          /* texture bank / column address */
u16 ray_tex[NRAYS];

u8  scaler_bank0[MAXS + 1];
u16 scaler_addr0[MAXS + 1];
u8  sscaler_bank[MAXS + 1], sscaler_hi[MAXS + 1];
u8  spr_jp[3];                  /* jp nnnn trampoline (RAM) */
const u8 *spr_seg;
u8  spr_s;
void spr_ray(void);
void hand_rows(void);
u8  nt_state[VIEW_TW * VIEW_TH];
u8  nt_tc, nt_tr0, nt_tr1;
u8  nt_hold;             /* nonzero: don't write the nametable (fizzle) */
void nt_update(void);
u8  col_tr0[VIEW_TW], col_tr1[VIEW_TW];
u8  rd_tc, rd_hy0, rd_hrows;   /* snapshot of r_handrows for rd_cols */
void rd_cols(void);
u8  dr_c, dr_y0, dr_y1, dr_skyv, dr_gndv, colright;
void draw_ray(void);

/* asm interface */
u8  s1_rdbank;
u16 s2_vaddr, s2_hand;
u8  s2_y, s2_n;
u8   dda(void);
u16  mulfrac8(u16 d, u16 f);
u16  mq_a, mq_b;
u16  cr_p, cr_sx, cr_sX, cr_sY, cr_dX, cr_dY, cr_lim, cr_dist;
u8   cr_side, cr_ydir;
void s2_setaddr(void);
void s2_rows(void);

/* ------------------------------------------------------------ setup */

static void flat_tile(u16 tile, u8 color)
{
    u8 buf[32];
    u8 i;
    for (i = 0; i < 32; i++)
        buf[i] = (color >> (i & 3)) & 1 ? 0xFF : 0x00;
    vram_write_safe(tile * 32, buf, 32);
}

void render_set_colors(u8 sky, u8 ground)
{
    if (sky != r_skycolor)
    {
        r_skycolor = sky;
        flat_tile(TILE_SKY, sky);
    }
    if (ground != r_groundcolor)
    {
        r_groundcolor = ground;
        flat_tile(TILE_GROUND, ground);
    }
}

/* per tile column signature of everything that decides its pixels: when
 * it matches the previous frame the column is left alone.  col_sig holds
 * {s, tbank, tex lo, tex hi} per ray; rd_sig() compares and updates it and
 * fills col_info[tc] = {changed, ymin, ymax, min s} */
u8  col_sig[NRAYS * 4];
u8  col_info[VIEW_TW * 4];
void rd_sig(void);
void rd_ext(u8 chg);
static u16 col_spr[VIEW_TW], spr_new[VIEW_TW];
static u8  spr_any;
static u8  col_hand[HAND_TCOLS];
static u8  sig_colors = 0xFF;
static u8  sig_stale;
extern u8 render_cast_dirty;
u8 anim_tile[NUMFLOORS];    /* tile looks different in the two anim frames */

u8 col_hstat[HAND_TCOLS];    /* 1: hand fully raised already in VRAM */

void render_invalidate(void)
{
    memset(nt_state, 0xFF, sizeof(nt_state));
    memset(col_tr0, 0xFF, sizeof(col_tr0));
    memset(col_hstat, 0, sizeof(col_hstat));
    sig_colors = 0xFF;
    render_cast_dirty = 1;
}

void render_init(void)
{
    u8 old = MapSlot1;
    u8 i;
    MapSlot1 = RD_BANK;
    for (i = 0; i <= MAXS; i++)
    {
        scaler_bank0[i] = RD8(RD_SCALER_BANK)[i];
        scaler_addr0[i] = RD16(RD_SCALER_ADDR)[i];
        sscaler_bank[i] = RD8(RD_SSCALER_BANK)[i];
        sscaler_hi[i] = RD8(RD_SSCALER_HI)[i];
    }
    for (i = 0; i < NUMFLOORS; i++)
        anim_tile[i] = RD8(RD_WALLLIGHT1)[i] != RD8(RD_WALLLIGHT2)[i]
                    || RD8(RD_WALLDARK1)[i] != RD8(RD_WALLDARK2)[i];
    MapSlot1 = old;
    spr_jp[0] = 0xC3;
    s1_rdbank = RD_BANK;
    r_skycolor = 0xFF;
    r_groundcolor = 0xFF;
    render_set_colors(0, 0);
    flat_tile(TILE_BLANK, 0);
    render_invalidate();
}

/* ------------------------------------------------------------ casting */

u8  fxn, fyn;            /* camera position inside its tile (x/256) */
u8 *cast_p0;
const u8 *light_tab, *dark_tab;

u8 cr_c;
void cast_ray(void);
void set_tex_asm(void);

void cast_all(void);
u8 cast_anim;                       /* an animated tile was hit by the last cast */
static u8 cast_mode;                /* since the last draw: 0 rays unchanged, 1 some rays may
                                       have changed, 2 view moved (all changed) */

u8 render_cast_dirty = 1;
static u16 last_vx, last_vy, last_fine;
static u8 last_anim;

void render_cast(void)
{
    u8 old = MapSlot1;
    u8 cx = r_viewx >> 8, cy = r_viewy >> 8;

    /* same camera, same map: the rays (and spotvis) are still valid */
    if (!render_cast_dirty && r_viewx == last_vx && r_viewy == last_vy
        && r_fine == last_fine && (r_animframe == last_anim || !cast_anim))
        return;
    cast_mode = (r_viewx == last_vx && r_viewy == last_vy && r_fine == last_fine) ? 1 : 2;
    render_cast_dirty = 0;
    last_vx = r_viewx; last_vy = r_viewy; last_fine = r_fine; last_anim = r_animframe;

    MapSlot1 = RD_BANK;
    if (++vis_stamp == 0)
    {
        memset((u8 *)CR_SPOTVIS, 0, MAPSIZE * MAPSIZEY);
        vis_stamp = 1;
    }
    fxn = (u8)r_viewx;
    fyn = (u8)r_viewy;
    cast_p0 = (u8 *)(CR_TILEMAP + (u16)cx * 32 + cy);
    cast_p0[SPOTVIS_OFS] = vis_stamp;
    light_tab = RD8(r_animframe ? RD_WALLLIGHT2 : RD_WALLLIGHT1);
    dark_tab = RD8(r_animframe ? RD_WALLDARK2 : RD_WALLDARK1);

    /* coarse grid every 4th ray, refined where the face changes */
    cast_anim = 0;
    cast_all();
    MapSlot1 = old;
}

/* ------------------------------------------------------------ drawing */



/* one ray of a sprite (slot 1 = render data 2) */
static void draw_sprite_ray(const VisSprite *v, u8 px)
{
    const u8 *shape;
    u16 srccol, step;
    u8 sw, s = v->s;

    MapSlot1 = RD2_BANK;
    MapSlot2 = RD8(RD2_SHAPE_BANK)[v->shape];
    shape = (const u8 *)RD16(RD2_SHAPE_ADDR)[v->shape];
    step = RD16(RD2_STEPINV)[s];
    MapSlot1 = RD_BANK;                 /* mulhi16 needs the square tables */
    sw = shape[0];
    srccol = mulhi16((u16)((s16)px - v->x0) << 8, step);
    if (srccol >= sw) srccol = sw - 1;
    spr_seg = shape + (shape[1 + 2 * srccol] | (shape[2 + 2 * srccol] << 8));
    spr_s = s;
    spr_ray();
    MapSlot1 = RD_BANK;
}



/* sprite rays of tile column rd_tc (called from rd_cols) */
void rd_sprites(void)
{
    u8 i, c = rd_tc << 1, px = rd_tc << 3;
    VisSprite *v;
    for (i = 0, v = vissprites; i < numvis; i++, v++)
    {
        s16 l = (s16)px + 2 - v->x0;
        if (l >= 0 && l < (s16)v->width && ray_s[c] <= v->s)
        {
            colright = 0;
            draw_sprite_ray(v, px + 2);
        }
        l += 4;
        if (l >= 0 && l < (s16)v->width && ray_s[c + 1] <= v->s)
        {
            colright = 1;
            draw_sprite_ray(v, px + 6);
        }
    }
    MapSlot1 = RD_BANK;
}

void render_draw(void)
{
    u8 old1 = MapSlot1, old2 = slot2_cur;
    u8 tc, i, hy0, hc;
    u8 *ci;
    VisSprite *v;

    MapSlot1 = RD_BANK;
    slot2_map(TEX_BANK0);           /* cart RAM off: slot 2 = ROM */
    hy0 = VIEW_H - r_handrows;
    dr_skyv = 0x80 + (r_skycolor << 2);
    dr_gndv = 0x80 + (r_groundcolor << 2);
    i = (r_skycolor << 4) | r_groundcolor;
    if (i != sig_colors)
    {
        sig_colors = (r_skycolor << 4) | r_groundcolor;
        cast_mode = 2;
    }
    /* changed columns: everything after a move, the signature test when
     * only some rays may differ (animation, map change), none otherwise */
    if (cast_mode == 2)
    {
        sig_stale = 1;              /* cleared when next needed */
        rd_ext(1);
    }
    else if (cast_mode == 1)
    {
        if (sig_stale)
        {
            sig_stale = 0;
            memset(col_sig, 0xFF, sizeof(col_sig));
        }
        rd_sig();
    }
    else
        rd_ext(0);
    cast_mode = 0;

    /* sprites: one pass per sprite over the tile columns it covers */
    if (numvis || spr_any)
    {
        memset(spr_new, 0, sizeof(spr_new));
        for (i = 0, v = vissprites; i < numvis; i++, v++)
        {
            s16 a0 = v->x0 >> 3, a1 = (v->x0 + (s16)v->width - 1) >> 3;
            u8 s = v->s, ya, yb;
            u16 sig;
            if (a0 < 0) a0 = 0;
            if (a1 > VIEW_TW - 1) a1 = VIEW_TW - 1;
            if (a0 > a1) continue;
            ya = s > VIEW_CY ? 0 : VIEW_CY - s;
            yb = s > VIEW_CY ? VIEW_H : VIEW_CY + s;
            sig = (u16)v->x0 + ((u16)s << 6) + ((u16)v->shape << 9) + v->width + 1;
            ci = col_info + ((u8)a0 << 2);
            for (tc = (u8)a0; tc <= (u8)a1; tc++, ci += 4)
            {
                if (s >= ci[3])
                {
                    spr_new[tc] += sig;
                    if (ya < ci[1]) ci[1] = ya;
                    if (yb > ci[2]) ci[2] = yb;
                }
            }
        }
        spr_any = numvis;
        for (tc = 0, ci = col_info; tc < VIEW_TW; tc++, ci += 4)
        {
            if (spr_new[tc] != col_spr[tc])
            {
                col_spr[tc] = spr_new[tc];
                ci[0] = 1;
            }
        }
    }
    /* hand */
    hc = r_handrows;
    hy0 = VIEW_H - hc;
    for (i = 0, ci = col_info + HAND_TC0 * 4; i < HAND_TCOLS; i++, ci += 4)
    {
        if (col_hand[i] != hc)
        {
            col_hand[i] = hc;
            col_hstat[i] = 0;
            ci[0] = 1;
        }
        if (hc)
        {
            if (hy0 < ci[1]) ci[1] = hy0;
            ci[2] = VIEW_H;
        }
    }

    rd_hy0 = hy0;
    rd_hrows = hc;
    rd_cols();
    MapSlot1 = old1;
    slot2_map(old2);
}
