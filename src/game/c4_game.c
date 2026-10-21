/* Catacomb Abyss Source Code
 * Copyright (C) 1993-2014 Flat Rock Software
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * C4_GAME.C - Sega Master System port
 */
#include "def.h"
u8 finale_shown;

void ScanInfoPlane (void) B
{
    u16 x,y;
    s16 tile;
    u8 row[MAPSIZE];
    u16 p2 = 2 + mapwidth * mapheight;

    InitObjList();

    for (y=0;y<mapheight;y++)
    {
        map_read(gamestate.mapon, p2 + maprow[y], row, mapwidth);
        for (x=0;x<mapwidth;x++)
        {
            tile = row[x];
            if (!tile)
                continue;

            switch (tile)
            {
            case 1:
            case 2:
            case 3:
            case 4:
                SpawnPlayer(x,y,NORTH+tile-1);
                break;

            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
            case 10:
            case 11:
                SpawnBonus(x,y,tile-5);
                break;

            case 29:
                SpawnBonus(x,y,B_RKEY2);
            break;

            case 58:
            case 59:
            case 60:
            case 61:
            case 62:
                SpawnBonus(x,y,tile-58+B_RGEM);
            break;

            case 12:
            case 13:
            case 14:
            case 15:
            case 16:
            case 17:
            case 18:
            case 19:
                SpawnBonus(x,y,B_SCROLL1+tile-12);
                break;

            case 21:
                SpawnBonus(x,y,B_CHEST);
            break;

            case 31:
            case 32:
            case 33:
            case 34:
            case 35:
                SpawnWarp (x,y,tile-30);
                break;

            case 41:
                if (gamestate.difficulty <gd_Hard)
                    break;
            case 36:
                if (gamestate.difficulty <gd_Normal)
                    break;
            case 22:
                SpawnTroll (x,y);
                break;

            case 42:
                if (gamestate.difficulty <gd_Hard)
                    break;
            case 37:
                if (gamestate.difficulty <gd_Normal)
                    break;
            case 23:
                SpawnOrc (x,y);
                break;

            case 43:
                if (gamestate.difficulty <gd_Hard)
                    break;
            case 38:
                if (gamestate.difficulty <gd_Normal)
                    break;
            case 25:
                SpawnBat (x,y);
                break;

            case 44:
                if (gamestate.difficulty <gd_Hard)
                    break;
            case 39:
                if (gamestate.difficulty <gd_Normal)
                    break;
            case 26:
                SpawnDemon (x,y);
                break;

            case 45:
                if (gamestate.difficulty <gd_Hard)
                    break;
            case 40:
                if (gamestate.difficulty <gd_Normal)
                    break;
            case 27:
                SpawnMage (x,y);
                break;

            case 28:
                SpawnGrelminar (x,y);
                break;

            case 46:
            case 47:
            case 48:
                SpawnTombstone(x,y,tile-46);
            break;

            case 54:
                SpawnWarp(x,y,0);
                break;

            case 53:
                if (gamestate.difficulty <gd_Normal)
                    break;
            case 52:
                SpawnZombie(x,y);
            break;

            case 51:
                if (gamestate.difficulty <gd_Hard)
                    break;
            case 50:
                if (gamestate.difficulty <gd_Normal)
                    break;
            case 49:
                SpawnSpook(x,y);
                break;

            case 57:
                SpawnFTime(x,y);
                break;

            case 56:
                if (gamestate.difficulty <gd_Normal)
                    break;
            case 55:
                SpawnSkeleton(x,y);
                break;

            case 65:
                if (gamestate.difficulty <gd_Hard)
                    break;
            case 64:
                if (gamestate.difficulty <gd_Normal)
                    break;
            case 63:
                SpawnWetMan(x,y);
                break;

            case 68:
                if (gamestate.difficulty <gd_Hard)
                    break;
            case 67:
                if (gamestate.difficulty <gd_Normal)
                    break;
            case 66:
                SpawnEye(x,y);
                break;

            case 71:
                if (gamestate.difficulty <gd_Hard)
                    break;
            case 70:
                if (gamestate.difficulty <gd_Normal)
                    break;
            case 69:
                SpawnWallSkeleton(x,y);
                break;

            case 20:
            case 24:
            case 30:
                SpawnRedDemon (x,y);
                break;
            }
        }
    }
}

void DrawEnterScreen (void) B
{
    s16 width;
    const char *name = LevelName(gamestate.mapon);

    cursurf = &viewsurf;
    VW_Bar(0,0,VIEWWIDTH,VIEWHEIGHT,0);
    width = strlen(name);
    if (width < 20)
        width = 20;
    CenterWindow(width,5);
    US_CPrint("\nYou have arrived at\n");
    US_CPrint(LevelName(gamestate.mapon));
}

void SetupGameLevel (void) B
{
    u16 x,y,ofs;
    u8 tile;
    u8 wh[2], row0[MAPSIZE], row2[MAPSIZE];
    u16 p2;

    US_InitRndT(true);

    if (gamestate.mapon >= NUMMAPDEFS)
        gamestate.mapon = 0;
    map_read(gamestate.mapon, 0, wh, 2);
    mapwidth = wh[0];
    mapheight = wh[1];
    for (y = 0, ofs = 0; y < MAPSIZEY; y++, ofs += mapwidth)
        maprow[y] = ofs;
    p2 = 2 + 2 * mapwidth * mapheight;     /* high bytes of plane 2 */

    memset (tilemap,0,sizeof(tilemap));
    memset (actorat,0,sizeof(actorat));
    for (y=0;y<mapheight;y++)
    {
        map_read(gamestate.mapon, 2 + maprow[y], row0, mapwidth);
        map_read(gamestate.mapon, p2 + maprow[y], row2, mapwidth);
        for (x=0;x<mapwidth;x++)
        {
            tile = row0[x];
            MAP0(x,y) = tile;
            MAP2(x,y) = row2[x];

            if (tile<NUMFLOORS)
            {
                tilemap[x][y] = tile;
                if (tile>0)
                    actorat[x][y] = tile;
            }
        }
    }

    R_SealMap();
    zombie_base_delay = 0;
    ScanInfoPlane ();
}

void Victory (boolean playsounds) B
{
    if (playsounds)
    {
        SD_PlaySound (GETBOLTSND);
        SD_WaitSoundDone ();
        SD_PlaySound (GETNUKESND);
        SD_WaitSoundDone ();
        SD_PlaySound (GETPOTIONSND);
        SD_WaitSoundDone ();
        SD_PlaySound (GETKEYSND);
        SD_WaitSoundDone ();
        SD_PlaySound (GETSCROLLSND);
        SD_WaitSoundDone ();
        SD_PlaySound (GETPOINTSSND);
    }

    if (!screenfaded)
        VW_FadeOut();

    vid_finale();
    finale_shown = 1;

    VW_FadeIn();
}

void DrawPlayScreen (void) B
{
    cursurf = &viewsurf;
    VW_Bar (0,0,VIEWWIDTH,VIEWHEIGHT,0);
    RedrawStatusWindow ();
}

void FizzleOut (s16 showlevel) B
{
    if (showlevel)
        DrawEnterScreen ();
}

void FreeUpMemory (void) B
{
}

void GameLoop (void) B
{
    boolean wait = false;
    DrawPlayScreen ();
    IN_ClearKeysDown();

restart:
    if (finale_shown)
    {
        /* the finale picture replaced the view tiles and the top border */
        finale_shown = 0;
        vid_init_game();
        RedrawStatusWindow();
    }
    if (!loadedgame)
    {
        gamestate.difficulty = restartgame;
        restartgame = gd_Continue;
        DrawEnterScreen ();
        if (gamestate.mapon != 8)
            fizzlein = true;
        wait = true;
    }

    do
    {
        playstate = ex_stillplaying;
        if (!loadedgame)
            SetupGameLevel ();
        else
            loadedgame = false;

        if (EASYMODEON)
            DisplaySMsg("*** NOVICE ***", NULL);
        else
            DisplaySMsg("*** WARRIOR ***", NULL);

        status_delay = 250;

        RedrawStatusWindow();
        if (wait)
        {
            VW_WaitVBL(120);
            wait = false;
        }

        PlayLoop ();
        switch (playstate)
        {
        case ex_abort:
            FreeUpMemory ();
            return;
        case ex_resetgame:
            NewGame();
        case ex_loadedgame:
        case ex_warped:
            goto restart;
        break;
        default:
        break;
        }

    } while (1);
}
