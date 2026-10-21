/* render.h - SMS 3-D view */
#ifndef RENDER_H
#define RENDER_H
#include "hw.h"

#define VIEW_TX0   2        /* screen tile column of the view */
#define VIEW_TY0   1        /* screen tile row of the view */
#define VIEW_TW    28
#define VIEW_TH    10
#define VIEW_W     224
#define VIEW_CY    40

#define TILE_SKY    280
#define TILE_GROUND 281
#define TILE_BLANK  282
#define TILE_FREE   283     /* first tile not used by the view */

#define MAXVISIBLE  32

typedef struct {
    s16 x0;             /* left pixel */
    u16 width;          /* pixels */
    u8  s;              /* half height */
    u8  shape;          /* shape index (pic - FIRSTSCALEPIC) */
} VisSprite;

extern u16 r_viewx, r_viewy;    /* 8.8 camera position */
extern u16 r_fine;              /* fine view angle 0..3599 */
extern u8  r_animframe;
extern u8  r_skycolor, r_groundcolor;
extern u8  r_handrows;          /* visible hand rows (0..HAND_ROWS) */
extern u8  vis_stamp;           /* spotvis[x][y] == vis_stamp: seen */
extern u8  ray_s[];             /* z buffer (half heights) */
extern VisSprite vissprites[MAXVISIBLE];
extern u8  numvis;
extern u8  render_cast_dirty;      /* set when the tile map changes */
extern u8  nt_hold;             /* nonzero: render_draw leaves the nametable alone */
extern u8  nt_state[];          /* per view cell: 0 sky, 1 ground, 2 own tile */

void render_init(void);         /* view tiles / nametable, call with display off */
void render_invalidate(void);   /* force full redraw of the nametable state */
void render_set_colors(u8 sky, u8 ground);
void render_cast(void);         /* rays, spotvis (cart RAM must be in slot 2) */
void render_draw(void);         /* draw view from rays + vissprites */
u16  mulhi16(u16 a, u16 b);     /* (a*b) >> 16 */

#endif
