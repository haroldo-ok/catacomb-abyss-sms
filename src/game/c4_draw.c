#include <stddef.h>
/* Catacomb Abyss Source Code
 * Copyright (C) 1993-2014 Flat Rock Software
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * C4_DRAW.C - Sega Master System port: the game side of the 3-D view
 * (camera, object projection, timing).  The ray caster and the column
 * drawing are in render.c / render_a.s.
 */
#include "def.h"
#include "render.h"

fixed   viewx,viewy,viewsin,viewcos;
s16     viewangle;
s32     lasttimecount;
boolean fizzlein;

/* projection constants of tools/render_gen.py */
#define KV_HALF_256 38571L      /* 150.67 (half height 1 tile away) * 256 */
#define KH          301L        /* horizontal focal length, pixels */
#define CENTERX     112

void R_SealMap(void) B
{
    u16 x, y;
    memset((u8 *)CR_PAD, SEALTILE, 32);
    for (x = 0; x < MAPSIZE; x++)
        for (y = 0; y < MAPSIZEY; y++)
            if (x >= mapwidth || y >= mapheight)
                tilemap[x][y] = SEALTILE;
    render_cast_dirty = 1;
}

/* ------------------------------------------------------------- sprites */

/* object -> view: sets viewx (centre pixel) and viewheight (2 * half
 * height); FALSE when it can't be seen */
long smul16(int a, int b);     /* plat0.c (the library one is broken) */

static boolean TransformActor(objtype *ob)
{
    fixed gx,gy,nx,ny;
    s16 gx8, gy8, nx8, c14, s14, q;
    u16 s;
    u32 num;

    gx = ob->x-viewx;
    gy = ob->y-viewy;
    /* more than 127 tiles away: not visible */
    if (gx >= 0x7F0000L || gx <= -0x7F0000L || gy >= 0x7F0000L || gy <= -0x7F0000L)
        return FALSE;
    gx8 = (s16)(gx >> 8);           /* 8.8 */
    gy8 = (s16)(gy >> 8);
    c14 = (s16)(viewcos >> 2);      /* 2.14 */
    s14 = (s16)(viewsin >> 2);

    /* 8.8 * 2.14 = 10.22 */
    nx = smul16(gx8, c14) - smul16(gy8, s14) - ((fixed)ob->size << 6);
    if (nx < (0x100L << 14))
        nx8 = 1;
    else if (nx >= (KV_HALF_256 << 14))
        return FALSE;               /* scale 0 */
    else
        nx8 = (s16)(nx >> 14);
    ny = smul16(gx8, s14) + smul16(gy8, c14);
    q = (s16)(ny >> 14);            /* 8.8, |ny| < 2^23 */

    s = (u16)KV_HALF_256 / (u16)nx8;
    if (s > MAXS)
        s = MAXS;

    /* centre: 112 + ny * KH / nx, culled when more than 511 pixels off */
    num = (u32)(u16)(q < 0 ? -q : q) * (u32)KH;
    if ((u16)(num >> 9) >= (u16)nx8)
        return FALSE;
    if (q < 0)
        ob->viewx = CENTERX - (s16)udiv32_16(num, nx8);
    else
        ob->viewx = CENTERX + (s16)udiv32_16(num, nx8);
    ob->viewheight = s << 1;
    return TRUE;
}

/* is the object's spot seen (spotvis == vis_stamp), or one of the 8
 * spots around it seen and empty?  (objtype: tilex 22, tiley 24) */
typedef char ov_chk[(offsetof(objtype, tilex) == 22 && offsetof(objtype, tiley) == 24 && CR_SPOTVIS == 0x8820 && MAPSIZE == 64 && MAPSIZEY == 32) ? 1 : -1];

static u8 obj_vis(objtype *ob) __naked
{
    (void)ob;
    __asm
        ld de,#22
        add hl,de
        ld a,(hl)
        and a,#63
        ld c,a
        inc hl
        inc hl
        ld a,(hl)
        and a,#31
        ld b,a
        ld l,c
        ld h,#0
        add hl,hl
        add hl,hl
        add hl,hl
        add hl,hl
        add hl,hl
        ld a,l
        or a,b
        ld l,a
        ld de,#0x8820
        add hl,de
        ld a,(_vis_stamp)
        cp a,(hl)
        jr nz,00050$
        ld a,#1
        ret
00050$:
        ld e,a
        ld a,c
        dec a
        cp a,#62
        jr nc,00051$
        ld a,b
        dec a
        cp a,#30
        jr nc,00051$
        ld c,e
        ld de,#-33
        add hl,de
        ld a,(hl)
        cp a,c
        jr z,00010$
00030$:
        inc hl
        ld a,(hl)
        cp a,c
        jr z,00011$
00031$:
        inc hl
        ld a,(hl)
        cp a,c
        jr z,00012$
00032$:
        ld de,#30
        add hl,de
        ld a,(hl)
        cp a,c
        jr z,00013$
00033$:
        inc hl
        inc hl
        ld a,(hl)
        cp a,c
        jr z,00014$
00034$:
        ld de,#30
        add hl,de
        ld a,(hl)
        cp a,c
        jr z,00015$
00035$:
        inc hl
        ld a,(hl)
        cp a,c
        jr z,00016$
00036$:
        inc hl
        ld a,(hl)
        cp a,c
        jr z,00017$
00037$:
00051$:
        xor a,a
        ret
00010$:
        push hl
        ld de,#-0x800
        add hl,de
        ld a,(hl)
        pop hl
        or a,a
        jr nz,00030$
        inc a
        ret
00011$:
        push hl
        ld de,#-0x800
        add hl,de
        ld a,(hl)
        pop hl
        or a,a
        jr nz,00031$
        inc a
        ret
00012$:
        push hl
        ld de,#-0x800
        add hl,de
        ld a,(hl)
        pop hl
        or a,a
        jr nz,00032$
        inc a
        ret
00013$:
        push hl
        ld de,#-0x800
        add hl,de
        ld a,(hl)
        pop hl
        or a,a
        jr nz,00033$
        inc a
        ret
00014$:
        push hl
        ld de,#-0x800
        add hl,de
        ld a,(hl)
        pop hl
        or a,a
        jr nz,00034$
        inc a
        ret
00015$:
        push hl
        ld de,#-0x800
        add hl,de
        ld a,(hl)
        pop hl
        or a,a
        jr nz,00035$
        inc a
        ret
00016$:
        push hl
        ld de,#-0x800
        add hl,de
        ld a,(hl)
        pop hl
        or a,a
        jr nz,00036$
        inc a
        ret
00017$:
        push hl
        ld de,#-0x800
        add hl,de
        ld a,(hl)
        pop hl
        or a,a
        jp nz,00037$
        inc a
        ret
    __endasm;
}

/* the first object from ob on (ob may be NULL) that obj_vis finds
 * visible; the invisible ones passed lose their activity unless it's
 * always/noalways  (objtype: active 0, next 60) */
typedef char nv_chk[(offsetof(objtype, active) == 0 && offsetof(objtype, next) == 60 && always == 3 && noalways == 1 && no == 0) ? 1 : -1];
static objtype *next_vis(objtype *ob) __naked
{
    (void)ob;
    __asm
00001$:
        ld a,h
        or a,l
        jr z,00004$
        push hl
        call _obj_vis
        pop hl
        or a,a
        jr nz,00005$
        ld a,(hl)
        cp a,#3
        jr z,00003$
        cp a,#1
        jr z,00003$
        ld (hl),#0
00003$:
        ld de,#60
        add hl,de
        ld a,(hl)
        inc hl
        ld h,(hl)
        ld l,a
        jr 00001$
00004$:
        ld de,#0
        ret
00005$:
        ex de,hl
        ret
    __endasm;
}

static void DrawScaleds(void)
{
    objtype *obj;
    VisSprite *v;
    u8 i;

    numvis = 0;

    for (obj = next_vis(player->next); obj; obj = next_vis(obj->next))
    {
        {
            u8 sh, s;
            u16 w;
            s16 x0;
            if ((obj->active == noalways) || (obj->active == always))
                obj->active = always;
            else
                obj->active = yes;
            sh = obj->state->shapenum;
            if (!sh || !shape_width[sh])
                continue;
            if (!TransformActor (obj))
                continue;
            if (numvis == MAXVISIBLE)
                continue;
            s = obj->viewheight >> 1;
            w = ((u16)shape_width[sh] * s) >> 5;
            if (!w)
                continue;
            x0 = obj->viewx - (s16)(w >> 1);
            if (x0 >= VIEW_W || x0 + (s16)w <= 0)
                continue;

            /* keep the list sorted far to near (drawing order) */
            for (i = numvis; i && vissprites[i - 1].s > s; i--)
                vissprites[i] = vissprites[i - 1];
            v = &vissprites[i];
            v->x0 = x0;
            v->width = w;
            v->s = s;
            v->shape = sh;
            numvis++;
        }
    }
}

/* ------------------------------------------------------------- timing */

void CalcTics (void) B
{
    s32 newtime;

    if (lasttimecount > (s32)TimeCount)
        set_timecount(lasttimecount);

    newtime = TimeCount;
    while (newtime == lasttimecount)
    {
        wait_vblank();
        newtime = TimeCount;
    }
    realtics = tics = newtime-lasttimecount;
    lasttimecount = newtime;

    /* the object loop splits a frame into steps of at most MAXTICS
     * (c4_play.c); slower than that the game slows down */
    if (tics>MAXTICS*3)
    {
        set_timecount(TimeCount - (tics-MAXTICS*3));
        tics = MAXTICS*3;
    }

    if (realtics>MAXREALTICS)
        realtics = MAXREALTICS;
}

/* ------------------------------------------------------- ThreeDRefresh */

u16 game_frames;

void ThreeDRefresh (void) B
{
    game_frames++;
    viewangle = player->angle;
    viewsin = fsin(viewangle);
    viewcos = fcos(viewangle);
    viewx = player->x - FixedByFrac(FOCALLENGTH,viewcos);
    viewy = player->y + FixedByFrac(FOCALLENGTH,viewsin);

    r_viewx = (u16)(viewx >> 8);
    r_viewy = (u16)(viewy >> 8);
    r_fine = viewangle * 10;
    r_animframe = (TimeCount&8)>>3;
    render_set_colors(*skycolor & 15, *groundcolor & 15);

    render_cast();
    DrawScaleds();

    r_handrows = (u8)((handheight * 2) / 3);

    if (fizzlein)
        fizzle_begin();
    render_draw();
    if (fizzlein)
    {
        fizzlein = false;
        FizzleFade();
        lasttimecount = TimeCount;
    }

    CalcTics ();
}
