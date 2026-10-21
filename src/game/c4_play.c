/* Catacomb Abyss Source Code
 * Copyright (C) 1993-2014 Flat Rock Software
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * C4_PLAY.C - Sega Master System port
 */
#include "def.h"
byte bcolor;
s16 skytimer,skytimer_reset;
s16 groundtimer,groundtimer_reset;
const u16 *skycolor,*groundcolor;
byte BGFLAGS,
      bgflag;

ControlInfo control;
boolean     running;

s16         bordertime;
objtype *new,*obj,*player,*lastobj,*objfreelist;

boolean     godmode;
status_flags status_flag;
s16         status_delay;

u16         mapwidth,mapheight,tics,realtics;
u16         maprow[MAPSIZEY];
u8          laststep;           /* last object loop sub-step of the frame */

s16         BeepTime;
status_flags status_hint;

objtype dummyobj;
s16 objectcount;

/* the F2 sound menu (no AdLib on the Master System) */
static const char * const sound_items[2] = {"SOUND OFF", "SOUND ON"};
static void SoundMenu(void)
{
    s16 c;
    CenterWindow(18,5);
    c = MenuWindow("\nSelect sound:\n", 2, sound_items,
        SoundMode == sdm_Off ? 0 : 1);
    if (c == 0) SD_SetSoundMode(sdm_Off);
    else if (c == 1) SD_SetSoundMode(sdm_PC);
}

/* F10 debug keys of C4_DEBUG.C, through a menu */

void CheckKeys (void) B
{
    if (screenfaded)
        return;

    if (gbuttons.debug)
    {
        gbuttons.debug = false;
        DebugKeys();
        lasttimecount = TimeCount;
        tics = realtics = 1;
        IN_ClearKeysDown();
    }

deadloop:;
    if (gbuttons.options || (Flags & FL_DEAD))
    {
        char ch;
        gbuttons.options = false;

        DisplaySMsg("Options", NULL);
        if ((status_flag != S_TIMESTOP) || (Flags & FL_DEAD))
            status_flag = S_NONE;

        if (Flags & FL_DEAD)
            ch = DisplayMsg("",  "RWQ");
        else
            ch = DisplayMsg("", "HOLSRWQ");
        DrawText(true);

        switch (ch)
        {
            case 'H':
                HelpScreens();
                break;

            case 'O':
                SoundMenu();
                break;

            case 'L':
                {
                    s16 scroll = MenuScroll();
                    if (scroll >= 0 && gamestate.scrolls[scroll])
                        ReadScroll(scroll);
                }
                break;

            case 'S':
                if (!(Flags & FL_DEAD))
                {
                    PreFullDisplay();
                    GE_SaveGame();
                    PostFullDisplay(true);
                }
            break;

            case 'R':
                PreFullDisplay();
                if (GE_LoadGame())
                {
                    loadedgame = true;
                    playstate = ex_loadedgame;
                    Flags &= ~FL_DEAD;
                    PostFullDisplay(false);
                }
                else
                if (playstate == ex_victorious)
                {
                    PostFullDisplay(false);
                    Victory(false);
                }
                else
                    PostFullDisplay(true);
            break;

            case 'W':
                DisplaySMsg("Starting anew", NULL);
                VW_WaitVBL(60);
                playstate = ex_resetgame;
                Flags &= ~FL_DEAD;
                status_flag = S_NONE;
            break;

            case 'Q':
                DisplaySMsg("FARE THEE WELL!", NULL);
                status_flag = S_NONE;
                VW_WaitVBL(120);
                VW_FadeOut();
                Flags &= ~FL_DEAD;
                playstate = ex_abort;
                abortgame = true;
            break;
        }
        tics = realtics = 1;
        lasttimecount = TimeCount;
        IN_ClearKeysDown();

        if (status_flag == S_TIMESTOP)
            DisplaySMsg("Time Stopped:     ",NULL);
    }

    if ((Flags & FL_DEAD) && playstate != ex_abort)
        goto deadloop;
}

void PreFullDisplay (void) B
{
    VW_FadeOut();
    vid_fullscreen(true);
    cursurf = &fullsurf;
}

void PostFullDisplay (boolean draw_view) B
{
    vid_fullscreen(false);
    RedrawStatusWindow();
    if (draw_view)
    {
        ThreeDRefresh();
        VW_FadeIn();
    }
}

/*
#############################################################################

                  The objlist data structure

#############################################################################
*/

void InitObjList (void) B
{
    s16 i;

    for (i=0;i<MAXACTORS;i++)
    {
        objlist[i].prev = &objlist[i+1];
        objlist[i].next = NULL;
    }

    objlist[MAXACTORS-1].prev = NULL;

    objfreelist = &objlist[0];
    lastobj = NULL;

    objectcount = 0;

    GetNewObj (false);
    player = new;
}

void GetNewObj (boolean usedummy) B
{
    if (!objfreelist)
    {
        if (usedummy)
        {
            new = &dummyobj;
            return;
        }
        Quit ("GetNewObj: No free spots in objlist!");
    }

    new = objfreelist;
    objfreelist = new->prev;
    memset (new,0,sizeof(*new));

    if (lastobj)
        lastobj->next = new;
    new->prev = lastobj;

    new->active = false;
    lastobj = new;

    objectcount++;
}

void RemoveObj (objtype *gone) B
{
    if (gone == player)
        Quit ("RemoveObj: Tried to remove the player!");

    if (gone == lastobj)
        lastobj = (objtype *)gone->prev;
    else
        gone->next->prev = gone->prev;

    gone->prev->next = gone->next;

    gone->prev = objfreelist;
    objfreelist = gone;
}

void PollControls (void) B
{
    IN_ReadControl(&control);
    IN_PollGameButtons();
    running = gbuttons.quickturn;
}

/*
===================
=
= PlayLoop
=
===================
*/

static const u16 radar_sq[RADAR_RADIUS + 1] = {
    0, 1, 4, 9, 16, 25, 36, 49, 64, 81, 100, 121, 144, 169, 196, 225, 256, 289
};

void PlayLoop (void) B
{
    static const s16 shot_color[3] = {4,9,14};

    static const s16 allgems[5]={GEM_DELAY_TIME,GEM_DELAY_TIME,GEM_DELAY_TIME,
                        GEM_DELAY_TIME,GEM_DELAY_TIME};

    s32 dx,dy;
    s8 rs6 = 0, rc6 = 0;
    s16 objnum;
    s32 px=0,py=0;
    s16 ptx=0,pty=0;
    static u16 framepage;

    thinkfn think;
    u16 alltics, allreal, remreal;

    ingame = true;
    playstate = 0;
    set_timecount(0);
    gamestate.shotpower = handheight = 0;

    switch (gamestate.mapon)
    {
        case 0:
            if (!(BGFLAGS & BGF_NIGHT))
            {
                InitBgChange(3*60,sky_daytonight,-1,NULL,BGF_NIGHT);
                groundcolor = &gnd_colors[0];
            }
            else
            {
                skycolor = &sky_colors[0];
                groundcolor = &gnd_colors[0];
            }
        break;

        default:
            skycolor = &sky_colors[gamestate.mapon];
            groundcolor = &gnd_colors[gamestate.mapon];
            skytimer = groundtimer = -1;
        break;
    }

    RedrawStatusWindow();
    ThreeDRefresh();
    if (screenfaded)
        VW_FadeIn();

    /* (the original fizzles again here, over the identical picture; on
     * the SMS the fizzle starts from black, so it's done once) */
    set_timecount(0);
    lasttimecount = lastnuke = 0;

    PollControls ();
    do
    {
        PollControls();

        /* at a low frame rate the objects move in steps of at most MAXTICS
         * (the collision code needs it), realtics follows each step */
        alltics = tics;
        allreal = remreal = realtics;
        do
        {
        tics = alltics > MAXTICS ? MAXTICS : alltics;
        alltics -= tics;
        realtics = alltics ? tics : remreal;
        laststep = !alltics;
        objnum=0;
        for (obj = player;obj;obj = obj->next)
        {
            /* inactive objects neither think nor show on the radar */
            if (obj->active < yes)
                continue;
            if ((obj->active >= yes) && (!(FreezeTime && (obj!=player))))
            {
                if (obj->ticcount)
                {
                    obj->ticcount-=tics;

                    while ( obj->ticcount <= 0)
                    {
                        think = obj->state->think;
                        if (think)
                        {
                            const statetype *oldstate=obj->state;

                            think_call (think, obj);
                            if (!obj->state)
                            {
                                RemoveObj (obj);
                                goto nextactor;
                            }

                            if (obj->state != oldstate)
                                break;
                        }

                        obj->state = obj->state->next;
                        if (!obj->state)
                        {
                            RemoveObj (obj);
                            goto nextactor;
                        }

                        if (!obj->state->tictime)
                        {
                            obj->ticcount = 0;
                            goto nextactor;
                        }

                        if (obj->state->tictime>0)
                            obj->ticcount += obj->state->tictime;
                    }
                }

                think = obj->state->think;

                if (think)
                {
                    think_call (think, obj);
                    if (!obj->state)
                        RemoveObj (obj);
                }
nextactor:;
            }

            if (playstate == ex_abort)
                break;

            /* keep a list of objects around the player for radar updates */
            if (obj == player)
            {
                px = player->x;
                py = player->y;
                rs6 = (s8)(((s16)(fsin(player->angle) >> 8)) >> 2);
                rc6 = (s8)(((s16)(fcos(player->angle) >> 8)) >> 2);
                ptx = player->tilex;
                pty = player->tiley;
            }

            if (objnum > MAX_RADAR_BLIPS-2)
                objnum = MAX_RADAR_BLIPS-2;

            /* the radar is drawn once a frame: blips from the last step.
             * IntSqrt(d) < RADAR_RADIUS is d < RADAR_RADIUS^2 (the
             * RADAR_WIDTH box around the player contains that circle);
             * the tile distance is a cheap first test */
            if (laststep)
            {
                s8 col = 0;
                switch (obj->obclass)
                {
                        case playerobj:
                            col=15;
                        break;

                        case pshotobj:
                        case bigpshotobj:
                            col=shot_color[framepage];
                        break;

                        case batobj:
                            if (obj->active == always)
                                col=8;
                        break;

                        case eyeobj:
                        case reddemonobj:
                            if (gamestate.gems[B_RGEM-B_RGEM])
                                if (obj->active == always)
                                    col=4;
                        break;

                        case mageobj:
                            if (gamestate.gems[B_RGEM-B_RGEM])
                                if (obj->active == always)
                                    col=12;
                        break;

                        case wetobj:
                            if (gamestate.gems[B_BGEM-B_RGEM])
                                if (obj->active == always)
                                    col=9;
                        break;

                        case demonobj:
                            if (gamestate.gems[B_BGEM-B_RGEM])
                                if (obj->active == always)
                                    col=1;
                        break;

                        case trollobj:
                            if (gamestate.gems[B_GGEM-B_RGEM])
                                if (obj->active == always)
                                    col=10;
                        break;

                        case orcobj:
                            if (gamestate.gems[B_GGEM-B_RGEM])
                                if (obj->active == always)
                                    col=2;
                        break;

                        case spookobj:
                            if (gamestate.gems[B_YGEM-B_RGEM])
                                if (obj->active == always)
                                    col=6;
                        break;

                        case skeletonobj:
                            if (gamestate.gems[B_YGEM-B_RGEM])
                                if (obj->active == always)
                                    col=14;
                        break;

                        case zombieobj:
                            if (gamestate.gems[B_PGEM-B_RGEM])
                                if (obj->active == always)
                                    col=13;
                        break;

                        case grelmobj:
                            if (!memcmp(gamestate.gems,allgems,sizeof(gamestate.gems)))
                                if (obj->active == always)
                                    col=15;
                        break;
                }
                /* (colour first: most objects never show) */
                if (col
                    && (u16)((s16)obj->tilex - ptx + RADAR_RADIUS + 1) <= 2 * RADAR_RADIUS + 2
                    && (u16)((s16)obj->tiley - pty + RADAR_RADIUS + 1) <= 2 * RADAR_RADIUS + 2)
                {
                    s16 ndx, ndy;
                    dx = px-obj->x;
                    dy = obj->y-py;
                    ndx = (s16)(dx >> 16);
                    ndy = (s16)(dy >> 16);

                    if (ndx > -RADAR_RADIUS - 1 && ndx < RADAR_RADIUS + 1
                        && ndy > -RADAR_RADIUS - 1 && ndy < RADAR_RADIUS + 1
                        && radar_sq[(u8)(ndx < 0 ? -ndx : ndx)] + radar_sq[(u8)(ndy < 0 ? -ndy : ndy)]
                            < RADAR_RADIUS * RADAR_RADIUS)
                    {
                        /* quarter-tile offsets (|d| < 18 tiles), cos/sin * 64 */
                        s8 dx4 = (s8)(((s16)(dx >> 8)) >> 6), dy4 = (s8)(((s16)(dy >> 8)) >> 6);
                        RadarXY[objnum][0]=(s8)(((s16)dy4 * rc6 - (s16)dx4 * rs6) >> 8);
                        RadarXY[objnum][1]=(s8)(((s16)dy4 * rs6 + (s16)dx4 * rc6) >> 8);
                        RadarXY[objnum][2]=col;
                        objnum++;
                    }
                }
            }
        }
        if (laststep)
            RadarXY[objnum][2]=-1;
        if (alltics)
            remreal -= tics;
        } while (alltics && !playstate);
        realtics = allreal;
        if (playstate == ex_abort)
            break;

        if (bordertime)
        {
            bordertime -= realtics;
            if (bordertime<=0)
            {
                bordertime = 0;
                VW_ColorBorder(0);
            }
        }

        /* random lightning? */
        if (BGFLAGS & (BGF_NIGHT|BGF_NOT_LIGHTNING))
            switch (gamestate.mapon)
            {
                case 0:
                case 1:
                case 3:
                    if (!rnd(120-realtics))
                    {
                        BGFLAGS &= ~BGF_NOT_LIGHTNING;
                        InitBgChange(1,sky_lightning,-1,NULL,BGF_NOT_LIGHTNING);
                    }
                break;
            }

        /* handle sky/ground color changes */
        if (skytimer != -1)
        {
            skytimer -= realtics;
            if (skytimer < 0)
            {
                skycolor++;
                if (*skycolor == 0xffff)
                {
                    skytimer = -1;
                    skycolor--;
                    if (groundtimer == -1)
                        BGFLAGS |= bgflag;
                }
                else
                    skytimer = skytimer_reset;
            }
        }

        if (groundtimer != -1)
        {
            groundtimer -= realtics;
            if (groundtimer < 0)
            {
                groundcolor++;
                if (*groundcolor == 0xffff)
                {
                    groundtimer = -1;
                    groundcolor--;
                    if (skytimer == -1)
                        BGFLAGS |= bgflag;
                }
                else
                    groundtimer = groundtimer_reset;
            }
        }

        /* Handle FreezeTime counter.. */
        if (FreezeTime)
        {
            if (FreezeTime<20*30)
                if ((BeepTime += realtics, (s16)BeepTime) >= 60)
                {
                    BeepTime -= 60;
                    SD_PlaySound(TICKSND);
                }

            if ((FreezeTime -= realtics, (s16)FreezeTime) <= 0)
            {
                FreezeTime=0;
                SD_PlaySound(TIMERETURNSND);
                DisplaySMsg(NULL,NULL);
                status_flag = S_NONE;
            }
        }

        /* refresh all */
        if (++framepage == 3)
            framepage = 0;
        ThreeDRefresh ();
        if (Flags & FL_DEAD)
        {
            SD_PlaySound (GAMEOVERSND);
            DisplaySMsg("DEAD",NULL);
            DrawHealth();
            if (gamestate.potions)
            {
                 CenterWindow(26,4);
                 US_CPrint("\nYou should use your Cure Potions wisely\n");
                 IN_Ack();
            }
        }

        if (playstate == ex_victorious)
        {
            Victory(true);
            Flags |= FL_DEAD;
        }

        DisplayStatus(&status_flag);
        CheckKeys();

    }while (!playstate);

    ingame = false;
    if (bordertime)
    {
        bordertime = 0;
        VW_ColorBorder(0);
    }

    if (abortgame)
        abortgame = false;
}

void InitBgChange (s16 stimer, const u16 *scolors, s16 gtimer, const u16 *gcolors, byte flag) B
{
    skytimer_reset = skytimer = stimer;
    if (scolors)
        skycolor = scolors;

    groundtimer_reset = groundtimer = gtimer;
    if (gcolors)
        groundcolor = gcolors;

    bgflag = flag;
}

#define MESSAGEDELAY  25
void DisplayStatus (status_flags *stat_flag) B
{
    status_flags temp_status;

    if (*stat_flag == S_TIMESTOP)
      return;

    if (status_delay > 0)
    {
        status_delay -= realtics;
        return;
    }
    else
        status_delay = 0;

    temp_status = S_VIEWING;

    if (control.button0)
        temp_status = S_MISSLE;

    if (status_hint)
        temp_status = status_hint;
    status_hint = S_NONE;

    if (control.x)
        temp_status = S_TURN;

    if (running && control.x)
        temp_status = S_QTURN;

    if (control.button1 && control.x)
        temp_status = S_SIDESTEP;

    if (control.y < 0)
        temp_status = S_ADVANCE;

    if (control.y > 0)
        temp_status = S_RETREAT;

    if (temp_status != *stat_flag)
    {
        *stat_flag = temp_status;

        switch (*stat_flag)
        {
            case S_MISSLE:
                DisplaySMsg("Magick Missile", NULL);
                status_delay = MESSAGEDELAY;
            break;

            case S_ZAPPER:
                if (gamestate.bolts)
                {
                    DisplaySMsg("Zapper", NULL);
                    status_delay = MESSAGEDELAY+10;
                }
            break;

            case S_XTER:
                if (gamestate.nukes)
                {
                    DisplaySMsg("Xterminator", NULL);
                    status_delay = MESSAGEDELAY+5;
                }
            break;

            case S_TURN:
                DisplaySMsg("Turning", NULL);
                status_delay = MESSAGEDELAY;
            break;

            case S_QTURN:
                DisplaySMsg("Quick Turning", NULL);
                status_delay = MESSAGEDELAY;
            break;

            case S_SIDESTEP:
                DisplaySMsg("Sidestepping", NULL);
                status_delay = MESSAGEDELAY;
            break;

            case S_ADVANCE:
                DisplaySMsg("Advancing", NULL);
                status_delay = MESSAGEDELAY;
            break;

            case S_RETREAT:
                DisplaySMsg("Retreating", NULL);
                status_delay = MESSAGEDELAY;
            break;

            case S_VIEWING:
                DisplaySMsg("Viewing", NULL);
            break;

            default:
            break;
        }
    }
}
