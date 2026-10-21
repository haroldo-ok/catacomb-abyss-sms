/* Catacomb Abyss Source Code
 * Copyright (C) 1993-2014 Flat Rock Software
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

// C3_PLAY.C

#include "def.h"
/* forward declarations of every state in this file */












































































































































































































/*
=============================================================================

						 LOCAL CONSTANTS

=============================================================================
*/
void SpawnSkeleton (s16 tilex, s16 tiley) B;


#define MSHOTDAMAGE	2
#define MSHOTSPEED	10000

#define ESHOTDAMAGE	1
#define ESHOTSPEED	5000

#define RANDOM_ATTACK 20

/*
=============================================================================

						 GLOBAL VARIABLES

=============================================================================
*/

boolean ShootPlayer (objtype *ob, s16 obclass, s16 speed, const statetype *state) B;
boolean ShotClipMove (objtype *ob, s32 xmove, s32 ymove) B;
s16 AngleNearPlayer (objtype *ob) B;
void T_Grelm_DropKey (objtype *ob) B;
void T_ShootPlayer (objtype *ob) B;

s16 zombie_base_delay;




/*
=============================================================================

						 LOCAL VARIABLES

=============================================================================
*/






/*
=============================================================================

						  BONUS ITEMS

=============================================================================
*/




//const statetype s_goalbonus = {NEMESISPIC,0,NULL,&s_goalbonus};





/*
===============
=
= SpawnBonus
=
===============
*/

void SpawnBonus (s16 tilex, s16 tiley, s16 number) B
{

	const statetype *state;

	switch (number)
	{
		case B_BOLT:			state = &s_boltbonus;		break;
		case B_NUKE:			state = &s_nukebonus;		break;
		case B_POTION:			state = &s_potionbonus;		break;

		case B_RKEY:			state = &s_rkeybonus;		break;
		case B_YKEY:			state = &s_ykeybonus;		break;
		case B_GKEY:			state = &s_gkeybonus;		break;
		case B_BKEY:			state = &s_bkeybonus;		break;

		case B_RGEM:			state = &s_rgem1bonus;		break;
		case B_YGEM:			state = &s_ygem1bonus;		break;
		case B_GGEM:			state = &s_ggem1bonus;		break;
		case B_BGEM:			state = &s_bgem1bonus;		break;
		case B_PGEM:			state = &s_pgem1bonus;		break;

		case B_CHEST:
			if (gnd_colors[gamestate.mapon] == 0x0101)
				state = &s_waterchestbonus1;
			else
				state = &s_chestbonus;
		break;

		case B_SCROLL1:
		case B_SCROLL2:
		case B_SCROLL3:
		case B_SCROLL4:
		case B_SCROLL5:
		case B_SCROLL6:
		case B_SCROLL7:
		case B_SCROLL8:			state = &s_scrollbonus;		break;

		case B_RKEY2:			state = &s_rkey2bonus;		break;

		default:
			Quit("SpawnBonus(): INVALID BONUS");
		break;
	}

	SpawnNewObj (tilex,tiley,state,TILEGLOBAL/2);
//	new->tileobject = true;
	new->temp1 = number;
	new->obclass = bonusobj;

	switch (number)
	{
		case B_POTION:
		case B_CHEST:
		case B_BOLT:
		case B_NUKE:
			new->flags |= of_shootable;
		break;

		default:
			new->flags &= ~of_shootable;
		break;
	}
}


/*
===============
=
= SpawnTombstone
=
===============
*/


void SpawnTombstone (s16 tilex, s16 tiley, s16 shape) B
{
	const statetype *state=&s_tombs[shape];

	SpawnNewObj (tilex,tiley,state,TILEGLOBAL/2);
//	new->tileobject = true;
	new->obclass = solidobj;
	new->flags |= of_shootable;
}


/*
============================================================================

									FREEZE TIME OBJECT

============================================================================
*/


/*
===============
=
= SpawnFTime
=
===============
*/
void SpawnFTime (s16 tilex, s16 tiley) B
{
	SpawnNewObj(tilex,tiley,&s_ftimebonus,TILEGLOBAL/2);
//	new->tileobject = true;
	new->obclass = freezeobj;
	new->flags |= of_shootable;
}

/*
=============================================================================

					  EXPLODING WALL

=============================================================================
*/


void T_WallDie (objtype *ob) B;






/*
================
=
= ExplodeWall
=
================
*/

void ExplodeWall (s16 tilex, s16 tiley) B
{
	u16 tilenum;

	DSpawnNewObj (tilex,tiley,&s_walldie1,0);
	if (new == &dummyobj)
		return;
	new->obclass = inertobj;
	new->active = always;
	if (gnd_colors[gamestate.mapon] == 0x0101)
		tilenum = WATEREXP;
	else
		tilenum = WALLEXP;
	actorat[new->tilex][new->tiley] = tilemap[new->tilex][new->tiley] =
		MAP0(new->tilex,new->tiley) = tilenum;
	render_cast_dirty = 1;
	MAP2(new->tilex,new->tiley) = 0;
}


/*
================
=
= T_WallDie
=
================
*/

void T_WallDie (objtype *ob) B
{
	u16 tile,other,spot,x,y;

	if (++ob->temp1 == 3)
		tile = 0;
	else
		if (gnd_colors[gamestate.mapon] == 0x0101)
			tile = WATEREXP-1 + ob->temp1;
		else
			tile = WALLEXP-1 + ob->temp1;
	x = ob->tilex;
	y = ob->tiley;

	actorat[x][y] = tilemap[x][y] = MAP0(x,y) = tile;
	render_cast_dirty = 1;

	if (ob->temp1 == 1)
	{
	//
	// blow up nearby walls
	//
		spot = MAP2(x-1,y);
		if (spot == EXP_WALL_CODE)
			ExplodeWall (x-1,y);
		spot = MAP2(x+1,y);
		if (spot == EXP_WALL_CODE)
			ExplodeWall (x+1,y);
		spot = MAP2(x,y-1);
		if (spot == EXP_WALL_CODE)
			ExplodeWall (x,y-1);
		spot = MAP2(x,y+1);
		if (spot == EXP_WALL_CODE)
			ExplodeWall (x,y+1);
	}
}
/*
=============================================================================

								OBJ_WARP GATE

=============================================================================
*/

void T_Gate (objtype *ob) B;




//---------------------------------------------------------------------------
//	SpawnWarp()
//
// TYPE : Type param is the gate number (1-what ever) will link you to
//			 gate of that type.
//---------------------------------------------------------------------------
void SpawnWarp (s16 tilex, s16 tiley, s16 type) B
{

	if (type)
		SpawnNewObj (tilex,tiley,&s_obj_gate1,TILEGLOBAL/3);
	else
		SpawnNewObj (tilex,tiley,&s_pit,TILEGLOBAL/3);
	new->obclass = gateobj;
	new->temp1 = type;
}



/*
===============
=
= T_Gate
=
===============
*/

#define STATUSCOLOR	4

void T_Gate (objtype *ob) B
{
	objtype *check;
	u16	temp,spot;

	if (CheckHandAttack (ob) && !playstate)
	{
		// make
		//
//		spot = (*(mapsegs[2]+farmapylookup[ob->tiley+1]+ob->tilex)) >> 8;
//		if (spot--)
//			if (gamestate.keys[spot])
//				TakeKey(spot);
//			else
//				return;

		//
		// warp
		//

//		temp = bufferofs;
//		bufferofs = 0;
//		VW_Bar (26,4,232,9,STATUSCOLOR);		// clear text description
//		bufferofs = temp;

//		IN_ClearKeysDown ();
		if (ob->temp1)
		{
			//
			// teleport inside level
			//

			for (check=player->next;check;check=check->next)
				if (check->obclass==gateobj && check->temp1==ob->temp1 &&
					check != ob)
				{
					player->x = check->x;
					player->y = check->y;
					Thrust (player->angle,TILEGLOBAL/2);		// move forwards
					Thrust (player->angle,TILEGLOBAL/2);		// move forwards
					Thrust (player->angle,TILEGLOBAL/2);		// move forwards
					fizzlein=true;
					SD_PlaySound(WARPSND);
				}
		}
		else
		{
			//
			// teleport out of level
			//

			playstate = ex_warped;
			spot = MAP2(ob->tilex,ob->tiley+1);
			gamestate.mapon=spot;
			SD_PlaySound(WARPUPSND);
		}
	}
}



/*
=============================================================================

							TROLLS

=============================================================================
*/

void T_Troll (objtype *ob) B;








/*
===============
=
= SpawnTroll
=
===============
*/

void SpawnTroll (s16 tilex, s16 tiley) B
{
	SpawnNewObj(tilex,tiley,&s_troll1,35*PIXRADIUS);
	new->speed = 2500;
	new->obclass = trollobj;
	new->flags |= of_shootable;
	new->hitpoints = EasyHitPoints(14);
}


/*
===============
=
= T_Troll
=
===============
*/

void T_Troll (objtype *ob) B
{
	if (Chase(ob,true) || (rnd(1000)<RANDOM_ATTACK))
	{
		ob->state = &s_trollattack1;
		ob->ticcount = ob->state->tictime;
		return;
	}
}


void T_WetMan (objtype *ob) B;


//const statetype s_wet_pause = {WET_WALK1PIC,25,NULL,&s_wet_walk1};





//const statetype s_wet_rise1 = {WET_BUBBLE1PIC,14,NULL,&s_wet_rise2};























typedef enum {wt_BUBBLES,wt_WALK} WetManTypes;

#define WT_TIMEREMAIN 	(ob->temp1)
#define WT_STAGE			(ob->temp2)


/*
===============
=
= SpawnWetMan
=
===============
*/
void SpawnWetMan (s16 tilex, s16 tiley) B
{
	objtype *ob;
	SpawnNewObj(tilex,tiley,&s_wet_bubbles1,PIXRADIUS*35);
	ob=new;

	WT_STAGE = wt_BUBBLES;
	WT_TIMEREMAIN = 60*4+rnd(60*3);

	new->obclass = wetobj;
	new->speed = 1000;
	new->flags &= ~of_shootable;
	new->hitpoints = EasyHitPoints(18);
}


/*
===============
=
= T_WetMan
=
===============
*/

void T_WetMan (objtype *ob) B
{
	switch (WT_STAGE)
	{
		case wt_BUBBLES:
			ob->flags &= ~of_shootable;
			if (Chase(ob,true))
			{
				// RISE & GOTO WALK STAGE
				//

				WT_STAGE = wt_WALK;
				WT_TIMEREMAIN = 60*5+rnd(60*5);
				ob->state = &s_wet_rise1;
				ob->speed = 2200;
				ob->ticcount = ob->state->tictime;
			}
			else
			{
				// DEC COUNTER - And check for WALK
				//
				if ((WT_TIMEREMAIN -= realtics, (s16)WT_TIMEREMAIN) < 0)
				{
					// RISE & GOTO WALK STAGE
					//

					WT_STAGE = wt_WALK;
					WT_TIMEREMAIN = 60+rnd(60*2);
					ob->state = &s_wet_rise1;
					ob->speed = 2200;
					ob->ticcount = ob->state->tictime;
				}
				else
				if (rnd(1000)<5)
				{
					// RANDOM PEEK UP OUT OF WATER
					//

					ob->state=&s_wet_peek;
					ob->ticcount = ob->state->tictime;
				}
			}
			break;


		case wt_WALK:
			ob->flags |= of_shootable;
			if (Chase(ob,true) || (rnd(1000)<RANDOM_ATTACK))
			{
				ob->state = &s_wet_attack1;
				ob->ticcount = ob->state->tictime;
			}
			else
			{
				// DEC COUNTER - And check for SINK
				//
				if ((WT_TIMEREMAIN -= realtics, (s16)WT_TIMEREMAIN) < 0)
				{
					// SINK & GOTO BUBBLE STAGE
					//

					WT_STAGE = wt_BUBBLES;
					WT_TIMEREMAIN = 60*4+rnd(60*3);
					ob->state = &s_wet_sink1;
					ob->speed = 1200;
					ob->ticcount = ob->state->tictime;
					ob->flags &= ~of_shootable;
				}

			}
			break;
	}
}

/*
=============================================================================

							ZOMBIE

=============================================================================
*/






//extern const statetype s_zombie_attack1;



void T_Zombie (objtype *ob) B;
void T_ZombieRisen (objtype *obj) B;














//const statetype s_zombie_attack1 = {ZOMB_ATTACKPIC,15,NULL,&s_zombie_pause};





enum zombie_modes {zm_wait_for_dark,zm_wait_to_rise,zm_active};

#define zombie_mode	ob->temp1
#define zombie_delay	ob->temp2


//--------------------------------------------------------------------------
// SpawnZombie()
//--------------------------------------------------------------------------
void SpawnZombie (s16 tilex, s16 tiley) B
{
	objtype *ob;
	s16 current_zombie_delay;
	u16 tile;

	SpawnNewObj(tilex,tiley,&s_zombie_inground,35*PIXRADIUS);
	ob = new;
	zombie_mode = zm_wait_for_dark;

	tile = MAP2FULL(tilex,tiley+1);
	if (tile)
		zombie_delay = (tile>>8)*30;
	else
	{
		current_zombie_delay = (2*60)+rnd(4*60);
		zombie_delay = zombie_base_delay+current_zombie_delay;
		zombie_base_delay += current_zombie_delay;
		if (zombie_base_delay > 8*60)
			zombie_base_delay = 0;
	}

	new->speed = 2500;
	new->obclass = zombieobj;
	new->hitpoints = EasyHitPoints(5);
	new->active = yes;
	new->flags &= ~of_shootable;
}

//--------------------------------------------------------------------------
// T_Zombie()
//--------------------------------------------------------------------------
void T_Zombie (objtype *ob) B
{
	switch (zombie_mode)
	{
		case zm_wait_for_dark:
			if (gamestate.mapon == 0)
			{
				if (BGFLAGS & BGF_NIGHT)
					zombie_mode = zm_wait_to_rise;
			}
			else
				zombie_mode = zm_wait_to_rise;
		break;

		case zm_wait_to_rise:
			if (zombie_delay < 0)
			{
				if ((ob->tilex == player->tilex) && (ob->tiley == player->tiley))
					break;

				ob->active = always;
				ob->state = &s_zombie_rise1;
				ob->ticcount = ob->state->tictime;		// JIM Added
				zombie_mode = zm_active;
			}
			else
				zombie_delay -= tics;

		break;

		case zm_active:
			if (Chase (ob,true) || (rnd(1000)<RANDOM_ATTACK))
			{
				ob->state = &s_zombie_attack;
				ob->ticcount = ob->state->tictime;
				return;
			}
		break;
	}
}

//--------------------------------------------------------------------------
// T_ZombieRisen()
//--------------------------------------------------------------------------
void T_ZombieRisen (objtype *obj) B
{
	obj->flags |= of_shootable;
}



/*
=============================================================================

											SPOOKS

=============================================================================
*/

void T_Spook (objtype *ob) B;





















#define spook_mode	ob->temp1
#define spook_delay	ob->temp2

/*
===============
=
= SpawnSpook
=
===============
*/
void SpawnSpook (s16 tilex, s16 tiley) B
{
	objtype *ob;
	u16 tile;
	SpawnNewObj(tilex,tiley,&s_spook_wait,PIXRADIUS*35);
	ob = new;

	tile = MAP2FULL(tilex,tiley+1);
	if (tile)
		spook_delay = (tile>>8)*30;
	else
		spook_delay = 2*60+rnd(5*60);

	spook_mode = zm_wait_for_dark;

	new->active = yes;
	new->obclass = spookobj;
	new->speed = 1900;
	new->flags &= ~of_shootable;
	new->hitpoints = EasyHitPoints(5);
}


/*
===============
=
= T_Spook
=
===============
*/
void T_Spook (objtype *ob) B
{
	switch (zombie_mode)
	{
		case zm_wait_for_dark:
			if (!gamestate.mapon)
			{
				if (BGFLAGS & BGF_NIGHT)
					spook_mode = zm_wait_to_rise;
			}
			else
				spook_mode = zm_wait_to_rise;
		break;

		case zm_wait_to_rise:
			spook_delay -= tics;
			if (spook_delay < 0)
			{
				if ((ob->tilex == player->tilex) && (ob->tiley == player->tiley))
					break;

				ob->active = always;
				ob->flags |= of_shootable;
				ob->state = &s_spook0;
				ob->ticcount = ob->state->tictime;
				spook_mode = zm_active;
			}
			else
				spook_delay -= tics;
		break;

		case zm_active:
			if (Chase (ob,true) || (rnd(1000)<RANDOM_ATTACK))
			{
				ob->state = &s_spook_attack1;
				ob->ticcount = ob->state->tictime;
				return;
			}
		break;
	}
}


/*
=============================================================================

								SKELETON IN WALL

=============================================================================
*/

void T_WallSkeleton (objtype *ob) B;

enum wskel_modes {ws_wall1,ws_wall2,ws_wall3,ws_exit};

#define wskel_mode	ob->temp1
#define wskel_delay	ob->temp2
#define wskel_base	ob->angle
#define wskel_wallx	ob->hitpoints
#define wskel_wally	ob->speed

/*
===============
=
= SpawnWallSkeleton
=
===============
*/
void SpawnWallSkeleton (s16 tilex, s16 tiley) B
{
	static const s8 xofs[] = {0,0,-1,+1};
	static const s8 yofs[] = {-1,+1,0,0};

	objtype *ob;
	int wallx=tilex,wally=tiley,wallbase,wallmode,loop;
	u16 tile,current_delay;

	for (loop=0; loop<4; loop++)
	{
		tile = MAP0(tilex+xofs[loop],tiley+yofs[loop]);
		switch (tile)
		{
			case WALL_SKELETON_CODE:
			case WALL_SKELETON_CODE+1:
			case WALL_SKELETON_CODE+2:
				wallmode = ws_wall1+(tile-WALL_SKELETON_CODE);
				wallbase = WALL_SKELETON_CODE;
				goto foundtile;
			break;

			case 41:
			case 43:
				wallmode = ws_wall1+(tile-41);
				wallbase = 41;
				goto foundtile;
			break;

			case 42:
			case 44:
				wallmode = ws_wall1+(tile-42);
				wallbase = 42;
				goto foundtile;
			break;
		}
	}

	return;
foundtile:;

	wallx += xofs[loop];
	wally += yofs[loop];

	SpawnNewObj(tilex,tiley,&s_wallskel,PIXRADIUS*35);
	ob = new;
	new->obclass = wallskelobj;
	new->speed = 1900;
	new->flags &= ~of_shootable;
	new->hitpoints = EasyHitPoints(5);

//	new->tilex = wallx;
//	new->tiley = wally;
	wskel_wallx = wallx;
	wskel_wally = wally;
	wskel_base = wallbase;
	new->active = no;

	wskel_mode = wallmode;

	tile = MAP2FULL(wallx,wally);
	if (tile)
		wskel_delay = (tile>>8)*30;
	else
	{
		current_delay = (2*60)+rnd(4*60);
		wskel_delay = zombie_base_delay+current_delay;
		zombie_base_delay += current_delay;
		if (zombie_base_delay > 8*60)
			zombie_base_delay = 0;
	}
}

/*
===============
=
= T_WallSkeleton
=
===============
*/
void T_WallSkeleton (objtype *ob) B
{
	int x=wskel_wallx,y=wskel_wally;

	wskel_delay -= realtics;
	if (wskel_delay > 0)
		return;

	switch (wskel_mode)
	{
		case ws_wall2:
			if ((wskel_base == 41) || (wskel_base == 42))
				wskel_mode++;
		case ws_wall1:
		case ws_wall3:
			actorat[x][y]
				= tilemap[x][y]
				= MAP0(x,y)
				= wskel_base+(wskel_mode-ws_wall1);

			wskel_mode++;
			wskel_delay = (120);
			ob->active = always;
		break;

		case ws_exit:
//			wskel_delay = 0;
//			if (objectcount >= MAXACTORS-3)
//				break;

			actorat[x][y]
				= tilemap[x][y]
				= MAP0(x,y)
				= wskel_base;
			ob->tilex = ob->x >> TILESHIFT;
			ob->tiley = ob->y >> TILESHIFT;

			ob->obclass = skeletonobj;
			ob->speed = 2036;
			ob->flags |= of_shootable;
			ob->hitpoints = EasyHitPoints(12);
			ob->state = &s_skel_1;
			ob->ticcount = ob->state->tictime;

//			SpawnSkeleton(ob->tilex,ob->tiley);
//			ob->state = NULL;
		break;
	}
}


/*
=============================================================================

								SKELETONS

=============================================================================
*/

void T_Skeleton (objtype *ob) B;













/*
===============
=
= SpawnSkeleton
=
===============
*/
void SpawnSkeleton (s16 tilex, s16 tiley) B
{
	SpawnNewObj(tilex,tiley,&s_skel_1,PIXRADIUS*35);
	new->obclass = skeletonobj;
	new->speed = 2036;
	new->flags |= of_shootable;
	new->hitpoints = EasyHitPoints(12);
}


/*
===============
=
= T_Skeleton
=
===============
*/

void T_Skeleton (objtype *ob) B
{
	if (Chase (ob,true) || (rnd(1000)<RANDOM_ATTACK))
	{
		ob->state = &s_skel_attack1;
		ob->ticcount = ob->state->tictime;
		return;
	}
}

/*
=============================================================================

								EYE

=============================================================================
*/

void T_Eye (objtype *ob) B;
boolean T_EyeShoot (objtype *ob, boolean eyeshot) B;

enum eye_modes {em_other1,em_player1,em_other2,em_player2,em_other3,em_player3,em_other4,em_player4};

#define eye_mode	ob->temp1
#define eye_delay	ob->temp2

//-------------------------------------------------------------------------
// SpawnEye()
//-------------------------------------------------------------------------
void SpawnEye (s16 tilex, s16 tiley) B
{
	objtype *ob;

	SpawnNewObj(tilex,tiley,&s_eye_1,PIXRADIUS*35);
	ob = new;
	new->obclass = eyeobj;
	new->speed = 1200;
	new->flags |= of_shootable;
	new->hitpoints = EasyHitPoints(14);
	eye_mode = em_other1;
}


//-------------------------------------------------------------------------
// T_Eye()
//-------------------------------------------------------------------------
void T_Eye (objtype *ob) B
{
	fixed tempx,tempy;
	u16 temp_tilex,temp_tiley;

	eye_delay -= realtics;
	if (eye_delay < 0)
	{
		eye_mode = rnd(em_player4);
		eye_delay = (10*60);
	}

	tempx = player->x;
	tempy = player->y;
	temp_tilex = player->tilex;
	temp_tiley = player->tiley;


	switch (eye_mode)
	{
		case em_other1:
		case em_other2:
		case em_other3:
		case em_other4:
			player->x = ((s32)other_x[eye_mode]<<TILESHIFT)+TILEGLOBAL/2;
			player->y = ((s32)other_y[eye_mode]<<TILESHIFT)+TILEGLOBAL/2;
			player->tilex = other_x[eye_mode];
			player->tiley = other_y[eye_mode];
		break;
	}

	if (Chase(ob,true))
		eye_delay = 0;

	player->x = tempx;
	player->y = tempy;
	player->tilex = temp_tilex;
	player->tiley = temp_tiley;

	if (!rnd(2))
		ShootPlayer(ob,eshotobj,ESHOTSPEED,&s_eshot1);
}

/*
=============================================================================

							ORCS

=============================================================================
*/

void T_Orc (objtype *ob) B;








/*
===============
=
= SpawnOrc
=
===============
*/

void SpawnOrc (s16 tilex, s16 tiley) B
{
	SpawnNewObj(tilex,tiley,&s_orc1,PIXRADIUS*35);
	new->obclass = orcobj;
	new->speed = 1536;
	new->flags |= of_shootable;
	new->hitpoints = EasyHitPoints(6);
}


/*
===============
=
= T_Orc
=
===============
*/

void T_Orc (objtype *ob) B
{
	if (Chase (ob,true) || (rnd(1000)<RANDOM_ATTACK))
	{
		ob->state = &s_orcattack1;
		ob->ticcount = ob->state->tictime;
		return;
	}
}


/*
=============================================================================

							DEMON

=============================================================================
*/

void T_Demon (objtype *ob) B;








/*
===============
=
= SpawnDemon
=
===============
*/

void SpawnDemon (s16 tilex, s16 tiley) B
{
	SpawnNewObj(tilex,tiley,&s_demon1,PIXRADIUS*35);
	new->obclass = demonobj;
	new->speed = 2048;
	new->flags |= of_shootable;
	new->hitpoints = EasyHitPoints(50);
}


/*
===============
=
= T_Demon
=
===============
*/

void T_Demon (objtype *ob) B
{
	if (Chase (ob,true) || (rnd(1000)<RANDOM_ATTACK))
	{
		ob->state = &s_demonattack1;
		ob->ticcount = ob->state->tictime;
		return;
	}
}


/*
=============================================================================

							MAGE

=============================================================================
*/


void T_Mage (objtype *ob) B;
void T_MageShoot (objtype *ob) B;





//const statetype s_mageattack1 = {MAGEATTACKPIC,20,NULL,&s_mageattack2};
//const statetype s_mageattack2 = {MAGEATTACKPIC,-1,T_MageShoot,&s_mageattack3};







/*
===============
=
= SpawnMage
=
===============
*/

void SpawnMage (s16 tilex, s16 tiley) B
{
	SpawnNewObj(tilex,tiley,&s_mage1,PIXRADIUS*35);
	new->obclass = mageobj;
	new->speed = 3072;
	new->flags |= of_shootable;
	new->hitpoints = EasyHitPoints(5);
}


/*
===============
=
= T_Mage
=
===============
*/

void T_Mage (objtype *ob) B
{
	fixed tempx,tempy;
	u16 temp_tilex,temp_tiley;

	eye_delay -= realtics;
	if (eye_delay < 0)
	{
		eye_mode = rnd(em_player4);
		eye_delay = (10*60);
	}

	tempx = player->x;
	tempy = player->y;
	temp_tilex = player->tilex;
	temp_tiley = player->tiley;


	switch (eye_mode)
	{
		case em_other1:
		case em_other2:
		case em_other3:
		case em_other4:
			player->x = ((s32)other_x[eye_mode]<<TILESHIFT)+TILEGLOBAL/2;
			player->y = ((s32)other_y[eye_mode]<<TILESHIFT)+TILEGLOBAL/2;
			player->tilex = other_x[eye_mode];
			player->tiley = other_y[eye_mode];
		break;
	}

	if (Chase(ob,true))
		eye_delay = 0;

	player->x = tempx;
	player->y = tempy;
	player->tilex = temp_tilex;
	player->tiley = temp_tiley;

	if (!rnd(10))
		if (ShootPlayer(ob,mshotobj,MSHOTSPEED,&s_mshot1))
		{
			ob->state = &s_mageattack3;
			ob->ticcount = ob->state->tictime;
		}
}

/*
=============================================================================

						RED DEMON

=============================================================================
*/

void T_RedDemon (objtype *ob) B;








/*
===============
=
= SpawnRedDemon
=
===============
*/

void SpawnRedDemon (s16 tilex, s16 tiley) B
{
	SpawnNewObj(tilex,tiley,&s_red_demon1,PIXRADIUS*35);
	new->obclass = reddemonobj;
	new->speed = 2048;
	new->flags |= of_shootable;
	new->hitpoints = EasyHitPoints(50);
}


/*
===============
=
= T_RedDemon
=
===============
*/

void T_RedDemon (objtype *ob) B
{
	if (Chase (ob,true) || (rnd(1000)<RANDOM_ATTACK))
	{
		ob->state = &s_red_demonattack1;
		ob->ticcount = ob->state->tictime;
		return;
	}
}


/*
=============================================================================

							GRELMINAR

=============================================================================
*/


void T_Grelminar (objtype *ob) B;
void T_GrelminarShoot (objtype *ob) B;
void T_Grelm_DropKey (objtype *ob) B;










//const statetype s_grelattack1 = {GRELATTACKPIC,20,NULL,&s_grelattack2};
//const statetype s_grelattack2 = {GRELATTACKPIC,-1,T_GrelminarShoot,&s_grelattack3};

/*
===============
=
= SpawnGrelminar
=
===============
*/

void SpawnGrelminar (s16 tilex, s16 tiley) B
{
	SpawnNewObj(tilex,tiley,&s_grel1,PIXRADIUS*35);
	new->obclass = grelmobj;
	new->speed = 2048;
	new->flags |= of_shootable;
	new->hitpoints = EasyHitPoints(100);
}


/*
===============
=
= T_Grelminar
=
===============
*/

void T_Grelminar (objtype *ob) B
{
	Chase (ob,false);

	if (!rnd(10))
		if (ShootPlayer(ob,gshotobj,10000,&s_gshot1))
		{
			ob->state = &s_grelattack3;
		}
}


//=================================
//
// T_Grelm_DropKey
//
//=================================
void T_Grelm_DropKey (objtype *ob) B
{
	if (ob->temp1)
		return;

	SpawnBonus(ob->tilex,ob->tiley,B_RKEY);
	SD_PlaySound(GRELM_DEADSND);
	ob->temp1 = true;
}

/*
=============================================================================

							BAT

=============================================================================
*/

void T_Bat (objtype *ob) B;
void T_BatPast (objtype *ob) B;





/*
===============
=
= SpawnBat
=
===============
*/

void SpawnBat (s16 tilex, s16 tiley) B
{
	SpawnNewObj(tilex,tiley,&s_bat1,PIXRADIUS*35);
	new->obclass =batobj;
	new->flags |= of_shootable;

	new->hitpoints = 1;
	new->speed = 2000;
}


/*
==================================
=
= BatChaseThink
=
==================================
*/

void BatChaseThink (objtype *obj) B
{
	int deltax,deltay;

	deltax=player->tilex - obj->tilex;
	deltay=player->tiley - obj->tiley;

	if (deltax>0)
		deltax = 2;
	else if (deltax<0)
		deltax = 0;
	else deltax = 1;

	if (deltay>0)
		deltay = 2;
	else if (deltay<0)
		deltay = 0;
	else deltay = 1;

	obj->dir = dirtable[deltay*3+deltax];
	if (Walk(obj))
		return;

	obj->dir = dirtable[3+deltax];
	if (Walk(obj))
		return;

	obj->dir = dirtable[deltay*3+1];
	if (Walk(obj))
		return;

	obj->dir = nodir;
}


void BatRunThink (objtype *obj) B
{
	int deltax,deltay;

	deltax=player->tilex - obj->tilex;
	deltay=player->tiley - obj->tiley;

	if (deltax>=0)
		deltax = 0;
	else
		deltax = 2;

	if (deltay>=0)
		deltay = 0;
	else
		deltay = 2;

	obj->dir = dirtable[deltay*3+deltax];
	if (Walk(obj))
		return;

	obj->dir = dirtable[3+deltax];
	if (Walk(obj))
		return;

	obj->dir = dirtable[deltay*3+1];
	Walk(obj);
}



/*
===============
=
= T_Bat
=
===============
*/

void T_Bat (objtype *ob) B
{
	s32 move;
	s32 deltax,deltay,size;

	move = ob->speed*tics;
	size = (s32)ob->size + player->size + move;


	do
	{
		deltax = ob->x - player->x;
		deltay = ob->y - player->y;

		if (deltax <= size && deltax >= -size
		&& deltay <= size && deltay >= -size && !ob->temp1)
		{
			TakeDamage (4);
			ob->temp1 = 2;
		}

		if (move < ob->distance)
		{
			MoveObj (ob,move);
			break;
		}

		actorat[ob->tilex][ob->tiley] = 0;	// pick up marker from goal
		if (ob->dir == nodir)
			ob->dir = north;

		ob->x = ((s32)ob->tilex<<TILESHIFT)+TILEGLOBAL/2;
		ob->y = ((s32)ob->tiley<<TILESHIFT)+TILEGLOBAL/2;
		move -= ob->distance;

		if (ob->temp1)
		{
			Walk (ob);				// go straight
			if (!--ob->temp1)
			{
				ob->state = &s_batpast;
				ob->ticcount = ob->state->tictime;
			}
		}
		else
			BatChaseThink (ob);		// head towards player

		actorat[ob->tilex][ob->tiley] = ACTOROBJ(ob);	// set down a new goal marker
	} while (0);	// just once
	CalcBounds (ob);
}


/*
===============
=
= T_BatPast
=
===============
*/

void T_BatPast (objtype *ob) B
{
	s32 move;
	s32 deltax,deltay,size;

	move = ob->speed*tics;

	do
	{
		if (move < ob->distance)
		{
			MoveObj (ob,move);
			break;
		}
		actorat[ob->tilex][ob->tiley] = 0;	// pick up marker from goal

		ob->x = ((s32)ob->tilex<<TILESHIFT)+TILEGLOBAL/2;
		ob->y = ((s32)ob->tiley<<TILESHIFT)+TILEGLOBAL/2;
		move -= ob->distance;

		BatRunThink (ob);

		actorat[ob->tilex][ob->tiley] = ACTOROBJ(ob);	// set down a new goal marker
	} while (0);	//(move)
	CalcBounds (ob);
}


//--------------------------------------------------------------------------
// ShootPlayer()
//--------------------------------------------------------------------------
boolean ShootPlayer (objtype *ob, s16 obclass, s16 speed, const statetype *state) B
{
	int angle = AngleNearPlayer(ob);

	if (angle == -1)
		return(false);

	DSpawnNewObjFrac (ob->x,ob->y,state,PIXRADIUS*35);
	new->speed = speed;
	new->obclass = obclass;
	new->active = always;
	new->angle = angle;

	return(true);
}

//--------------------------------------------------------------------------
// T_ShootPlayer()
//--------------------------------------------------------------------------
void T_ShootPlayer (objtype *ob) B
{
	objtype *check;
	s32 xmove,ymove,speed;

	speed = ob->speed*tics;

	xmove = FixedByFrac(speed,fcos(ob->angle));
	ymove = -FixedByFrac(speed,fsin(ob->angle));

	if (ShotClipMove(ob,xmove,ymove))
	{
		ob->state = &s_pshot_exp1;
		ob->ticcount = ob->state->tictime;
		return;
	}

	ob->tilex = ob->x >> TILESHIFT;
	ob->tiley = ob->y >> TILESHIFT;

// check for collision with wall
//
	if (tilemap[ob->tilex][ob->tiley])
	{
		SD_PlaySound (SHOOTWALLSND);
		ob->state = &s_pshot_exp1;
		ob->ticcount = s_pshot_exp1.tictime;
		return;
	}

//
//
	if ( ob->xl <= player->xh
	&& ob->xh >= player->xl
	&& ob->yl <= player->yh
	&& ob->yh >= player->yl)
	{
		switch (ob->obclass)
		{
			case eshotobj:
				TakeDamage (ESHOTDAMAGE);
			break;

			case mshotobj:
				TakeDamage (MSHOTDAMAGE);
			break;

			case gshotobj:
				TakeDamage (25);
			break;
		}
		ob->state = NULL;
		return;
	}

// check for collision
//
	for (check = player->next; check; check=check->next)
		if ((ob->flags & of_shootable) && ob->obclass != mageobj
		&& ob->xl <= check->xh
		&& ob->xh >= check->xl
		&& ob->yl <= check->yh
		&& ob->yh >= check->yl)
		{
			switch (ob->obclass)
			{
				case eshotobj:
					ShootActor (check,ESHOTDAMAGE);
				break;

				case mshotobj:
					ShootActor (check,MSHOTDAMAGE);
				break;

				case gshotobj:
					ShootActor (check,25);
				break;

				case pshotobj:
					ShootActor (check,25);
				break;
			}
			ob->state = &s_pshot_exp1;
			ob->ticcount = s_pshot_exp1.tictime;
			return;
		}
}

//-------------------------------------------------------------------------
// AngleNearPlayer()
//-------------------------------------------------------------------------
s16 AngleNearPlayer (objtype *ob) B
{
	int angle=-1;
	int xdiff = ob->tilex-player->tilex;
	int ydiff = ob->tiley-player->tiley;

	if (ob->tiley == player->tiley)
	{
		if (ob->tilex < player->tilex)
			angle = 0;
		else
			angle = 180;
	}
	else
	if (ob->tilex == player->tilex)
	{
		if (ob->tiley < player->tiley)
			angle = 270;
		else
			angle = 90;
	}
	else
	if (xdiff == ydiff)
		if (ob->tilex < player->tilex)
		{
			if (ob->tiley < player->tiley)
				angle = 315;
			else
				angle = 45;
		}
		else
		{
			if (ob->tiley < player->tiley)
				angle = 225;
			else
				angle = 135;
		}

	return(angle);
}

/////////////////////////////////////////////////////////////////////////////
//
//	EasyHitPoints
//
//	Checks to see if the player has selected the easy mode for playing.
//	If so then the normal hit points are cut in half.
//	This is when the object is spawned.
//
//	Parms
//		NrmHitPts - the normal hit points
//
//	Returns
//		Half of NrmHitPts
//
/////////////////////////////////////////////////////////////////////////////

s16 EasyHitPoints (s16 NrmHitPts) B
{
	if (EASYMODEON)				// Wimpy, Wimpy, Wimpy!!!!!
	{
		return(NrmHitPts/4);
	}
	else
		return(NrmHitPts);
}

/////////////////////////////////////////////////////////////////////////////
//
//	EasyDoDamage
//
//	Checks to see if the player has selected the easy mode for playing.
//	If so then the normal amount of damage is cut in half.
//	This is called each time a monster does damage.
//
//	Parms
//		Damage - the normal damage taken
//
//	Returns
//		Half of Damage
//
/////////////////////////////////////////////////////////////////////////////

s16 EasyDoDamage (s16 Damage) B
{
	if (EASYMODEON)				// Wimpy, Wimpy, Wimpy!!!!!
		return(Damage/2);
	else
		return(Damage);
}

