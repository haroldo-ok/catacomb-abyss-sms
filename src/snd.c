/* snd.c - sound effects: the original PC speaker effects as SN76489 tone
 * streams (tools/sounds.py), one period per frame, played from the vblank
 * interrupt with the priority rules of ID_SD.C.  Bank 0. */
#include "hw.h"
#include "assets.h"
#include "plat.h"

SDMode SoundMode;
static volatile u16 snd_ptr;        /* slot 1 address of the next period */
static volatile u16 snd_left;       /* frames left */
static volatile u8  snd_prio;
static volatile u8  snd_beep_left;

#define PSG_VOL 0x91                /* channel 0, attenuation 2 dB */

static void psg_off(void)
{
    PSGPort = 0x9F;
}

void SD_Startup(void)
{
    PSGPort = 0x9F; PSGPort = 0xBF; PSGPort = 0xDF; PSGPort = 0xFF;
    snd_left = 0;
    snd_prio = 0;
    SoundMode = sdm_PC;
}

void SD_SetSoundMode(SDMode mode)
{
    SD_StopSound();
    SoundMode = mode == sdm_Off ? sdm_Off : sdm_PC;
}

void SD_StopSound(void)
{
    __asm__("di");
    snd_left = 0;
    snd_prio = 0;
    snd_beep_left = 0;
    psg_off();
    __asm__("ei");
}

void SD_PlaySound(u16 sound)
{
    u8 old;
    u16 ofs, n;
    if (SoundMode == sdm_Off || sound >= NUMSOUNDS)
        return;
    if (snd_left && sound_priority[sound] < snd_prio)
        return;
    old = MapSlot1;
    MapSlot1 = SND_BANK;
    ofs = *(const u16 *)(SND_ADDR - 0x4000 + sound * 2);
    n = *(const u16 *)(SND_ADDR - 0x4000 + ofs);
    MapSlot1 = old;
    __asm__("di");
    snd_ptr = SND_ADDR - 0x4000 + ofs + 2;
    snd_left = n;
    snd_prio = sound_priority[sound];
    if (!n)
        psg_off();
    __asm__("ei");
}

u16 SD_SoundPlaying(void)
{
    return snd_left != 0;
}

void SD_WaitSoundDone(void)
{
    while (snd_left || snd_beep_left)
        __asm__("halt");
}

/* the intro beep: 800 Hz for 170 ms */
void snd_beep(void)
{
    if (SoundMode == sdm_Off)
        return;
    __asm__("di");
    snd_left = 0;
    snd_beep_left = 10;
    PSGPort = 0x80 | (140 & 0x0F);
    PSGPort = 140 >> 4;
    PSGPort = PSG_VOL;
    __asm__("ei");
}

/* vblank interrupt */
void snd_tick(void)
{
    u16 v;
    u8 old;
    if (snd_beep_left)
    {
        if (!--snd_beep_left)
            psg_off();
        return;
    }
    if (!snd_left)
        return;
    old = MapSlot1;
    MapSlot1 = SND_BANK;
    v = *(const u16 *)snd_ptr;
    MapSlot1 = old;
    snd_ptr += 2;
    if (v)
    {
        PSGPort = 0x80 | (v & 0x0F);
        PSGPort = (v >> 4) & 0x3F;
        PSGPort = PSG_VOL;
    }
    else
        psg_off();
    if (!--snd_left)
    {
        snd_prio = 0;
        psg_off();
    }
}
