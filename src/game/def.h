/* Catacomb Abyss Source Code
 * Copyright (C) 1993-2014 Flat Rock Software
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * DEF.H - Sega Master System port.
 *
 * Memory model: the game code lives in switchable banks 1..7 (slot 1) and
 * every function called from another file is __banked (SDCC far call
 * through ___sdcc_bcall_ehl).  Slot 2 is cartridge RAM for the whole game:
 * the map arrays and the object list are there (cartram.h); slot 0 holds
 * the platform layer (video, text, HUD, sound, input, saves), the state
 * table and the think dispatcher.  Strings handed to the platform layer
 * may live in the caller's bank: platform entry points copy them before
 * they map anything into slot 1.
 */
#ifndef DEF_H
#define DEF_H

#include "hw.h"
#include "assets.h"
#include "gfxe_abs.h"
#include "cartram.h"
#include "render.h"
#include <string.h>
#include <stdlib.h>

#define B __banked

typedef u8  boolean;
typedef u8  byte;
typedef u16 word;
typedef u32 longword;
typedef s32 fixed;

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
#define true 1
#define false 0
#define nil NULL

s16 rnd(s16 n) B;
u16 US_RndT(void) B;
void US_InitRndT(boolean randomize) B;
u16 random16(void);                 /* bank 0 */

#define FL_QUICK        0x01
#define FL_NOMEMCHECK   0x02
#define FL_CLEAR        (FL_QUICK|FL_NOMEMCHECK)
#define FL_DEAD         0x80

#define MAXBOLTS        10
#define MAXNUKES        10
#define MAXPOTIONS      10

/* radar (coordinates relative to the 56x56 radar surface) */
#define RADAR_WIDTH     51
#define RADAR_HEIGHT    51
#define RADAR_XCENTER   28
#define RADAR_YCENTER   22
#define MAX_RADAR_BLIPS 60
#define RADAR_RADIUS        17
#define RADAR_RADIUS_NSEW   15
#define RADAR_X_IRADIUS     (113/5)
#define RADAR_Y_IRADIUS     (113/7)
#define RADAR_ICON_CENTER   4

#define NAMESTART           180
#define REMOVED_DOOR_TILE   NAMESTART
#define NEXT_LEVEL_CODE     0xff
#define REMOVE_DOOR_CODE    0xfe
#define CANT_OPEN_CODE      0xfd
#define EXP_WALL_CODE       0xfc

#define EXPWALLSTART    8
#define NUMEXPWALLS     7
#define WALLEXP         15
#define WATEREXP        29
#define NUMFLOORS       71

#define FLASHCOLOR      12
#define FLASHTICS       4

#define NUMLEVELS       21

#define GLOBAL1         (1l<<16)
#define TILEGLOBAL      GLOBAL1
#define TILESHIFT       16l
#define MINDIST         (2*GLOBAL1/5)
#define FOCALLENGTH     (TILEGLOBAL)
#define ANGLES          360

#define MAXACTORS       100

#define NORTH   0
#define EAST    1
#define SOUTH   2
#define WEST    3

#define MAXBODY         100
#define MAXSHOTPOWER    56
#define PIXRADIUS       512

enum bonusnumbers {B_BOLT,B_NUKE,B_POTION,B_RKEY,B_YKEY,B_GKEY,B_BKEY,B_SCROLL1,
 B_SCROLL2,B_SCROLL3,B_SCROLL4,B_SCROLL5,B_SCROLL6,B_SCROLL7,B_SCROLL8,
 B_GOAL,B_CHEST,B_RGEM,B_GGEM,B_BGEM,B_YGEM,B_PGEM,B_RKEY2};

#define GEM_DELAY_TIME  (120*60)
#define ROTATE_SPEED    (6)
#define WALL_SKELETON_CODE 6
#define MAXREALTICS     (2*60)
#define MAXFREEZETIME   (100*30)
#define MAXTICS         6

enum {BLANKCHAR=9,BOLTCHAR,NUKECHAR,POTIONCHAR,KEYCHARS,SCROLLCHARS=17,
    NUMBERCHARS=25};

#define BLACK       0
#define WHITE       15
#define LT_GREY     7

#define tf_SOLID                0x01
#define tf_SPECIAL              0x02
#define tf_EMBEDDED_KEY_COLOR   0x04
extern const u8 tile_flags[256];
#define TILE_FLAGS(tile)        (tile_flags[(u8)(tile)])
#define GATE_KEY_COLOR(tile)    ((u8)(TILE_FLAGS(tile)>>4))

typedef struct {s16 x,y;} tilept;
typedef struct {fixed x,y;} globpt;

typedef enum
  {nothing,playerobj,bonusobj,orcobj,batobj,skeletonobj,trollobj,demonobj,
  mageobj,pshotobj,bigpshotobj,mshotobj,inertobj,bounceobj,grelmobj,
  gateobj,zombieobj,spookobj,wetobj,expobj,eyeobj,wallskelobj,eshotobj,
  gshotobj,reddemonobj,freezeobj,solidobj} classtype;

typedef enum {north,east,south,west,northeast,southeast,southwest,
          northwest,nodir} dirtype;

/* think routines are numbered (TH_xxx, generated states.c) and called
 * through think_call() in bank 0 */
typedef u8 thinkfn;

/* 5 bytes (bank 0 table, see gen/states.c): shapenum is relative to
 * FIRSTSCALEPIC (0 = nothing drawn) */
typedef struct statestruct
{
    u8  shapenum;
    s8  tictime;
    thinkfn think;
    const struct statestruct *next;
} statetype;

#define of_shootable    0x01
#define of_damagedone   0x02

enum {no,noalways,yes,always};

/* 64 bytes: objlist[] indexing is a shift */
typedef struct objstruct
{
    u8      active;
    u8      obclass;
    u8      flags;
    u8      dir;
    s16     ticcount;
    s16     angle;
    const statetype *state;
    s32     distance;
    fixed   x,y;
    u16     tilex,tiley;
    s16     viewx;
    u16     viewheight;
    s16     hitpoints;
    s16     temp1,temp2;
    s32     speed;
    u32     size;
    fixed   xl,xh,yl,yh;
    struct objstruct *next,*prev;
} objtype;

typedef enum {ex_stillplaying,ex_died,ex_warped,ex_resetgame
    ,ex_loadedgame,ex_victorious,ex_turning,ex_abort} exittype;

typedef enum {S_NONE,     S_HELP,    S_SND,     S_SAVING,   S_RESTORING,
            S_JOYSTICK, S_RETREAT, S_ADVANCE, S_SIDESTEP, S_QTURN,
            S_MISSLE,   S_ZAPPER,  S_XTER,    S_CURING,   S_READ,
            S_VIEWING,  S_ITEMDES,  S_DAMAGE,  S_TURN,    S_TIMESTOP} status_flags;

typedef enum {gd_Continue,gd_Easy,gd_Normal,gd_Hard} GameDiff;

typedef struct
{
    s16     difficulty;
    s16     mapon;
    s16     bolts,nukes,potions,keys[4],scrolls[8];
    s16     gems[5];
    s32     score;
    s16     body,shotpower;
} gametype;

/* ---------------------------------------------------------------- C4_MAIN */
extern char         str[80],str2[20];
extern gametype     gamestate;
extern exittype     playstate;
extern u16          Flags;
extern boolean      EASYMODEON;
extern GameDiff     restartgame;
extern boolean      loadedgame,abortgame,ingame;
extern boolean      quitgame;
extern boolean      restore_requested;
extern s16          starting_level;

void NewGame (void) B;
boolean SaveTheGame(u16 slot) B;
boolean LoadTheGame(u16 slot) B;
void ResetGame(void) B;
void InitGame (void) B;
boolean DemoLoop (void) B;

/* ---------------------------------------------------------------- C4_GAME */
void ScanInfoPlane (void) B;
void SetupGameLevel (void) B;
void Victory (boolean playsounds) B;
void DrawPlayScreen (void) B;
void FizzleOut (s16 showlevel) B;
void FreeUpMemory (void) B;
void GameLoop (void) B;
void DrawEnterScreen (void) B;
const char *LevelName(s16 n);           /* bank 0, RAM copy */

#define BGF_NIGHT           0x01
#define BGF_NOT_LIGHTNING   0x02

extern byte BGFLAGS,bcolor;
extern const u16 *skycolor,*groundcolor;

/* ---------------------------------------------------------------- C4_PLAY */
typedef struct
{
    boolean button0,button1;
    s16     x,y;
    s16     xaxis,yaxis;
    s16     dir;
} ControlInfo;

extern ControlInfo  control;
extern boolean      running;
extern s16          bordertime;

#define SEALTILE 0xFF
extern __at(CR_TILEMAP) u8 tilemap[MAPSIZE][MAPSIZEY];
extern __at(CR_ACTORAT) u8 actorat[MAPSIZE][MAPSIZEY];
extern __at(CR_PLANE0) byte mapplane0[MAP_PLANE_BYTES];
extern __at(CR_PLANE2) byte mapplane2[MAP_PLANE_BYTES];    /* high byte of plane 2 */
extern __at(CR_OBJLIST) objtype objlist[MAXACTORS];
extern u8 vis_stamp;
#define SPOTVIS(x,y)    (SPOTVIS_RAW(x,y) == vis_stamp)
extern u16      mapwidth,mapheight;

extern u16 maprow[MAPSIZEY];            /* y * mapwidth */
#define MAP0(x,y)   mapplane0[maprow[y]+(x)]
#define MAP2(x,y)   mapplane2[maprow[y]+(x)]
u16 map2full(u16 x, u16 y);             /* bank 0: 16 bit plane 2 from ROM */
#define MAP2FULL(x,y) map2full(x,y)
void map_read(u8 m, u16 ofs, u8 *dst, u16 n);   /* bank 0: map blob bytes */

#define ACTOROBJ(o)     ((u8)(0x80 + ((o) - objlist)))
#define ISACTOROBJ(v)   ((v) >= 0x80)
#define ACTORPTR(v)     (&objlist[(u8)((v)-0x80)])

extern objtype  *new,*obj,*player,*lastobj,*objfreelist;
extern objtype  dummyobj;

extern boolean  godmode;
extern s16      objectcount;
extern status_flags status_flag;
extern s16      status_delay;
extern s16      BeepTime;
extern status_flags status_hint;
extern u16      tics,realtics;
extern u8       laststep;

void CheckKeys (void) B;
void InitObjList (void) B;
void GetNewObj (boolean usedummy) B;
void RemoveObj (objtype *gone) B;
void PollControls (void) B;
void PlayLoop (void) B;
void InitBgChange(s16 stimer, const u16 *scolors, s16 gtimer, const u16 *gcolors, byte flag) B;
void DisplayStatus (status_flags *stat_flag) B;
s16  IntSqrt(s32 va);                   /* bank 0 */
void DebugKeys(void) B;

/* ---------------------------------------------------------------- C4_STATE */
void Internal_SpawnNewObj (u16 x, u16 y, const statetype *state, u32 size,boolean UseDummy) B;
void Internal_SpawnNewObjFrac (s32 x, s32 y, const statetype *state, u32 size,boolean UseDummy) B;

#define DSpawnNewObj(x, y, state, size)             Internal_SpawnNewObj(x,y,state,size,true)
#define SpawnNewObj(x, y, state, size)              Internal_SpawnNewObj(x,y,state,size,false)
#define SpawnNewObjFrac(x, y, state, size,Dummy)    Internal_SpawnNewObjFrac(x, y, state, size,false)
#define DSpawnNewObjFrac(x, y, state, size)         Internal_SpawnNewObjFrac(x, y, state, size,true)

boolean CheckHandAttack (objtype *ob) B;
void T_DoDamage (objtype *ob) B;
boolean Walk (objtype *ob) B;
void ChaseThink (objtype *obj, boolean diagonal) B;
void MoveObj (objtype *ob, s32 move) B;
boolean Chase (objtype *ob, boolean diagonal) B;
void ShootActor (objtype *ob, u16 damage) B;

extern const dirtype opposite[9];      /* c4_const.c (bank 0) */
extern const dirtype dirtable[9];
extern const s16 other_x[4], other_y[4];
extern const u16 sky_colors[NUMLEVELS], gnd_colors[NUMLEVELS];
extern const u16 sky_daytonight[5], sky_lightning[6];

/* ---------------------------------------------------------------- C4_DRAW */
fixed fsin(s16 angle);                  /* bank 0: sintable[angle] */
fixed fcos(s16 angle);                  /* bank 0: costable[angle] */
#define sintable_(a) fsin(a)

extern fixed    viewx,viewy,viewsin,viewcos;
extern s16      viewangle;
extern s32      lasttimecount;
extern boolean  fizzlein;

fixed FixedByFrac (fixed a, fixed b);   /* bank 0 */
void ThreeDRefresh (void) B;
void CalcTics (void) B;

/* ---------------------------------------------------------------- C4_WIZ */
#define MAXHANDHEIGHT   72
#define VIEWWIDTH       224
#define VIEWHEIGHT      80
extern u8 finale_shown;
void R_SealMap(void) B;


extern s32      lastnuke;
extern s16      lasttext;
extern s16      handheight;
extern s16      boltsleft,bolttimer;
extern s8       RadarXY[MAX_RADAR_BLIPS][3];
void DrawFreezeTime(void) B;
void GiveChest(void) B;
void GiveGoal(void) B;
extern s16      FreezeTime;
extern boolean  redraw_gems;
extern boolean  autofire;


void SpawnExplosion(fixed x, fixed y,s16 Delay) B;
void T_ExpThink(objtype *obj) B;
void SpawnBigExplosion(fixed x, fixed y, s16 Delay, fixed Range) B;

void RedrawStatusWindow (void) B;
void DrawHealth(void) B;
void DrawText (boolean force) B;
void TakeDamage (s16 points) B;
void GiveBolt (void) B;
void GiveNuke (void) B;
void GivePotion (void) B;
void GiveKey (s16 keytype) B;
void GiveScroll (s16 scrolltype,boolean show) B;
void ReadScroll (s16 scroll) B;
void StopTime(void) B;
void SpawnPlayer (s16 tilex, s16 tiley, s16 dir) B;
void Thrust (s16 angle, u32 speed) B;
void CalcBounds (objtype *ob);    /* bank 0, plat0.c */
boolean ShotClipMove (objtype *ob, s32 xmove, s32 ymove) B;
void DrinkPotion (void) B;
void CastNuke (void) B;
void CastBolt (void) B;
void DrawRadar (void) B;
void DrawNSEWIcons(void) B;

/* ---------------------------------------------------------------- C4_ACT1 */
s16 EasyHitPoints(s16 NrmHitPts) B;
s16 EasyDoDamage(s16 Damage) B;

extern s16 zombie_base_delay;

void SpawnBonus (s16 tilex, s16 tiley, s16 number) B;
void SpawnTombstone (s16 tilex, s16 tiley, s16 shape) B;
void SpawnFTime(s16 tilex, s16 tiley) B;
void ExplodeWall (s16 tilex, s16 tiley) B;
void SpawnWarp (s16 tilex, s16 tiley, s16 type) B;
void SpawnTroll (s16 tilex, s16 tiley) B;
void SpawnWetMan(s16 tilex, s16 tiley) B;
void SpawnZombie (s16 tilex, s16 tiley) B;
void SpawnSpook(s16 tilex, s16 tiley) B;
void SpawnWallSkeleton(s16 tilex, s16 tiley) B;
void SpawnSkeleton(s16 tilex, s16 tiley) B;
void SpawnEye(s16 tilex, s16 tiley) B;
void SpawnOrc (s16 tilex, s16 tiley) B;
void SpawnDemon (s16 tilex, s16 tiley) B;
void SpawnMage (s16 tilex, s16 tiley) B;
void SpawnRedDemon (s16 tilex, s16 tiley) B;
void SpawnGrelminar (s16 tilex, s16 tiley) B;
void SpawnBat (s16 tilex, s16 tiley) B;

#include "states.h"

/* ---------------------------------------------------------------- timing */
#define TimeCount get_timecount()      /* 70 Hz (hw.c); set with set_timecount() */

/* ---------------------------------------------------------------- platform */
#include "plat.h"

#endif
