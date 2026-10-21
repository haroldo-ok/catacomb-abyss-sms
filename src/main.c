/* main.c - Catacomb Abyss for the Sega Master System (bank 0) */
#include "game/def.h"

void main(void)
{
    hw_init();
    slot2_map(SLOT2_CARTRAM);
    memset((u8 *)0x8000, 0, 0x4000);        /* the game's cart RAM half */
    gfx_init();
    for (;;)
    {
        IntroProgram();         /* INTRO.EXE: title, difficulty choice */
        if (DemoLoop())         /* CATABYSS.EXE: the game */
            EndScreens();       /* LOADSCN */
    }
}
