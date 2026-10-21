/* input.c - control pad handling (replaces ID_IN), banked.
 *
 *   D-pad            Up/Down advance/retreat, Left/Right turn
 *   1                Magick Missile (hold to charge the big shot)
 *   2 + Left/Right   Sidestep
 *   2 (tap)          Zapper
 *   2 (hold) + 1     Xterminator
 *   2 (hold) + Down  Cure potion
 *   Left/Right twice (hold the second)   Quick turn
 *   PAUSE            Options menu;  PAUSE with 1+2 held: debug keys
 */
#include "game/def.h"

GameButtons gbuttons;
u8 pad_now, pad_last;

static u8 twoheld, twocombo;
static u8 ack_last;
static u8 lr_last_dir, lr_time, quick;

void IN_ReadControl(void *vinfo) B
{
    ControlInfo *info = (ControlInfo *)vinfo;
    u8 p = pad_now;
    info->xaxis = (p & PAD_LEFT) ? -1 : (p & PAD_RIGHT) ? 1 : 0;
    info->yaxis = (p & PAD_UP) ? -1 : ((p & PAD_DOWN) && !(p & PAD_2)) ? 1 : 0;
    info->x = info->xaxis * 127;
    info->y = info->yaxis * 127;
    info->button0 = (p & PAD_1) && !(p & PAD_2);
    info->button1 = (p & PAD_2) ? 1 : 0;
    info->dir = 0;
}

void IN_PollGameButtons(void) B
{
    u8 p = pad_read();
    u8 pressed = p & ~pad_last;
    u8 lr;
    pad_last = p;
    pad_now = p & ~PAD_PAUSE;

    gbuttons.fire = (p & PAD_1) && !(p & PAD_2);
    gbuttons.strafe = (p & PAD_2) && (p & (PAD_LEFT | PAD_RIGHT));

    /* quick turn: second press of the same direction soon after the first */
    lr = pressed & (PAD_LEFT | PAD_RIGHT);
    if (lr && !(p & PAD_2))
    {
        if (lr == lr_last_dir && (u8)(frame_count - lr_time) < 18)
            quick = 1;
        lr_last_dir = lr;
        lr_time = frame_count;
    }
    if (!(p & (PAD_LEFT | PAD_RIGHT)) || (p & PAD_2))
        quick = 0;
    gbuttons.quickturn = quick;

    if (pressed & PAD_2)
    {
        twoheld = 1;
        twocombo = 0;
    }
    if (twoheld)
    {
        if (p & PAD_2)
        {
            if (pressed & PAD_1) { gbuttons.nuke = 1; twocombo = 1; }
            if (pressed & PAD_DOWN) { gbuttons.potion = 1; twocombo = 1; }
            if (p & (PAD_LEFT | PAD_RIGHT | PAD_UP)) twocombo = 1;
        }
        else
        {
            if (!twocombo) gbuttons.zapper = 1;
            twoheld = 0;
        }
    }
    if (p & PAD_PAUSE)
    {
        if ((p & (PAD_1 | PAD_2)) == (PAD_1 | PAD_2))
        {
            gbuttons.debug = 1;
            twoheld = 0;
        }
        else
            gbuttons.options = 1;
    }
}

void IN_ClearKeysDown(void) B
{
    pad_last = pad_now = pad_read() & ~PAD_PAUSE;
    ack_last = pad_now;
    twoheld = twocombo = 0;
    quick = 0;
    memset(&gbuttons, 0, sizeof(gbuttons));
}

u8 IN_PollPress(void) B
{
    u8 p = pad_read();
    u8 pressed = (p & ~ack_last) | (p & PAD_PAUSE);
    ack_last = p & ~PAD_PAUSE;
    return pressed;
}

u8 IN_WaitPress(void) B
{
    u8 p, pressed;
    ack_last = pad_read() & ~PAD_PAUSE;
    for (;;)
    {
        wait_vblank();
        p = pad_read();
        pressed = (p & ~ack_last) | (p & PAD_PAUSE);
        ack_last = p & ~PAD_PAUSE;
        if (pressed)
            break;
    }
    pad_last = pad_now = ack_last;
    return pressed;
}

void IN_Ack(void) B
{
    IN_WaitPress();
    IN_ClearKeysDown();
}
