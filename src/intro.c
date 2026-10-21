/* intro.c - the Gamer's Edge front end of THE CATACOMB ABYSS (banked):
 *
 *   INTRO.EXE    title sequence, "Novice or Warrior?" challenge, story
 *   DEMOCAT.EXE  the electronic catalog of the Catacomb 3-D series
 *   LOADSCN.EXE  the closing screen
 *
 * Title screen buttons (as drawn on the title bar):
 *   1  new game        2  restore a game
 *   UP catalog demo    DOWN user's guide
 */
#include "game/def.h"

#define TICKS_50MS(n)   ((n) * 7 / 2)       /* 70 Hz TimeCount ticks */

static void show_screen(u8 n)
{
    if (!screenfaded)
        VW_FadeOut();
    vid_show_screen_rom(screen_bank[n], 0x8000);
    VW_FadeIn();
}

/* waits up to 'ticks' (70 Hz) for a button; returns the buttons pressed */
static u8 wait_buttons(u16 ticks)
{
    u32 start = get_timecount();
    IN_PollPress();
    for (;;)
    {
        u8 p;
        wait_vblank();
        p = IN_PollPress();
        if (p)
            return p;
        if (ticks && get_timecount() - start >= ticks)
            return 0;
    }
}

/* -------------------------------------------------------- the catalog */

static void Catalog(void)
{
    s16 page = 0;
    for (;;)
    {
        u8 p;
        show_screen(CAT_OPEN + page);
        p = wait_buttons(0);
        if (p & (PAD_PAUSE | PAD_2))
            break;
        if (p & (PAD_LEFT | PAD_UP))
        {
            if (page > 0) page--;
        }
        else if (++page >= CAT_VICTORY - CAT_OPEN + 1)
            break;
    }
}

/* ------------------------------------------------------------ the intro */

/* "Dare you challenge the great Nemesis as Novice or Warrior?" */
static boolean NewGameFlow(void)
{
    u8 p;
    for (;;)
    {
        show_screen(SCR_CHALLENGE);
        p = wait_buttons(0);
        random_seed((u16)get_timecount() ^ ((u16)frame_count << 8));
        if (p & PAD_1)
        {
            EASYMODEON = TRUE;
            show_screen(SCR_NOVICE);
        }
        else if (p & PAD_2)
        {
            EASYMODEON = FALSE;
            show_screen(SCR_WARRIOR);
        }
        else if (p & PAD_PAUSE)
            return FALSE;           /* [ESC] */
        else
            continue;
        SD_PlaySound(GETBOLTSND);
        if (wait_buttons(0) & PAD_PAUSE)
            return FALSE;
        show_screen(SCR_STORY);
        wait_buttons(0);
        return TRUE;
    }
}

void IntroProgram(void) B
{
    static const u8 attract[5] = {
        SCR_MADGAMERS, SCR_SOFTDISK, SCR_TITLE, SCR_CREDITS, SCR_TRILOGY };
    u16 i = 0;

    restore_requested = FALSE;
    starting_level = 0;
#ifdef START_LEVEL
    starting_level = START_LEVEL;
#endif
    ingame = false;
    screenfaded = 1;
    IN_ClearKeysDown();

    /* "Prepare to enter the CATACOMB..." while INTRO.EXE loads */
    show_screen(SCR_PREPARE);
    wait_buttons(TICKS_50MS(30));

    for (;;)
    {
        u8 p;
        show_screen(attract[i]);
        p = wait_buttons(TICKS_50MS(i == 2 ? 150 : 100));
        if (p & PAD_1)
        {
            if (NewGameFlow())
                break;
        }
        else if (p & PAD_2)
        {
            /* "Good!  Back for more I see!" */
            show_screen(SCR_BACKFORMORE);
            wait_buttons(TICKS_50MS(30));
            restore_requested = TRUE;
            break;
        }
        else if (p & PAD_UP)
            Catalog();
        else if (p & PAD_DOWN)
        {
            VW_FadeOut();
            HelpScreens();
            VW_FadeOut();
        }
        else
        {
            i = (i + 1) % 5;
            continue;
        }
        i = 2;                      /* back to the title */
    }
    IN_ClearKeysDown();
    VW_FadeOut();
}

/* LOADSCN: the closing screen after quitting the game */
void EndScreens(void) B
{
    show_screen(SCR_ARMAPC);
    wait_buttons(TICKS_50MS(200));
    VW_FadeOut();
}
