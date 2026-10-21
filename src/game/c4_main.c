/* Catacomb Abyss Source Code
 * Copyright (C) 1993-2014 Flat Rock Software
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * C4_MAIN.C - Sega Master System port.  Saved games: save.c (cartridge RAM).
 */
#include "def.h"
GameDiff restartgame;
boolean loadedgame,abortgame,ingame;
boolean quitgame;

char        str[80],str2[20];
gametype    gamestate;
exittype    playstate;
s16         starting_level;
boolean     EASYMODEON;
u16         Flags;
boolean     restore_requested;

/* ------------------------------------------------------------ randomness */

static const u8 rndtable[256] = {
      0,   8, 109, 220, 222, 241, 149, 107,  75, 248, 254, 140,  16,  66,
     74,  21, 211,  47,  80, 242, 154,  27, 205, 128, 161,  89,  77,  36,
     95, 110,  85,  48, 212, 140, 211, 249,  22,  79, 200,  50,  28, 188,
     52, 140, 202, 120,  68, 145,  62,  70, 184, 190,  91, 197, 152, 224,
    149, 104,  25, 178, 252, 182, 202, 182, 141, 197,   4,  81, 181, 242,
    145,  42,  39, 227, 156, 198, 225, 193, 219,  93, 122, 175, 249,   0,
    175, 143,  70, 239,  46, 246, 163,  53, 163, 109, 168, 135,   2, 235,
     25,  92,  20, 145, 138,  77,  69, 166,  78, 176, 173, 212, 166, 113,
     94, 161,  41,  50, 239,  49, 111, 164,  70,  60,   2,  37, 171,  75,
    136, 156,  11,  56,  42, 146, 138, 229,  73, 146,  77,  61,  98, 196,
    135, 106,  63, 197, 195,  86,  96, 203, 113, 101, 170, 247, 181, 113,
     80, 250, 108,   7, 255, 237, 129, 226,  79, 107, 112, 166, 103, 241,
     24, 223, 239, 120, 198,  58,  60,  82, 128,   3, 184,  66, 143, 224,
    145, 224,  81, 206, 163,  45,  63,  90, 168, 114,  59,  33, 159,  95,
     28, 139, 123,  98, 125, 196,  15,  70, 194, 253,  54,  14, 109, 226,
     71,  17, 161,  93, 186,  87, 244, 138,  20,  52, 123, 251,  26,  36,
     17,  46,  52, 231, 232,  76,  31, 221,  84,  37, 216, 165, 212, 106,
    197, 242,  98,  43,  39, 175, 254, 145, 190,  84, 118, 222, 187, 136,
    120, 163, 236, 249 };
static u16 rndindex;

void US_InitRndT (boolean randomize) B
{
    rndindex = randomize ? (random16() & 0xff) : 0;
}

u16 US_RndT (void) B
{
    rndindex = (rndindex + 1) & 0xff;
    return rndtable[rndindex];
}

s16 rnd (s16 n) B
{
    if (n <= 0)
        return 0;
    return (s16)(((u32)random16() * (u16)n) >> 16);
}

/* ----------------------------------------------------------------------- */

void NewGame (void) B
{
    if (!loadedgame)
    {
        memset (&gamestate,0,sizeof(gamestate));
        gamestate.mapon = starting_level;
        gamestate.body = MAXBODY;
    }

    BGFLAGS = 0;
    Flags &= FL_CLEAR;

    boltsleft = bolttimer = 0;
    FreezeTime = 0;
}

void ResetGame (void) B
{
    NewGame ();
}

/* ----------------------------------------------------------------------- */

void InitGame (void) B
{
    static boolean done;
    if (!done)
    {
        done = TRUE;
        SD_Startup ();
        save_init();
    }
    RadarXY[0][2] = -1;
    lasttext = -1;
    vid_set_palette_black();
    vid_init_game();
    fontcolor = WHITE;
    fontxor = TRUE;
}

/* main game cycle, started by the intro with the chosen difficulty;
 * returns FALSE when a restore from the title screen was cancelled */
boolean DemoLoop (void) B
{
    InitGame();
    loadedgame = false;
    playstate = ex_stillplaying;

    if (restore_requested)
    {
        restore_requested = FALSE;
        PreFullDisplay();
        if (!GE_LoadGame())
            return FALSE;
        loadedgame = true;
        playstate = ex_loadedgame;
        vid_fullscreen(false);
        vid_set_palette_black();
    }

    restartgame = gd_Normal;
    NewGame();
    GameLoop();
    VW_FadeOut();
    return TRUE;
}
