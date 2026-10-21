/* hw.c - minimal Sega Master System hardware layer (bank 0) */
#include "hw.h"
#include "snd.h"
#include <string.h>

volatile u8  frame_count;
volatile u32 time_count;
volatile u8  pad_state;
volatile u8  pause_hit;
volatile u8  vdp_status_last;
static u8 tc_acc;
u8 slot2_cur;
u8 vdp_reg1;

/* VBlank / line interrupt.  Registers are saved by hand: the C body may
 * use IY, and the renderer keeps state in the alternate set (which the C
 * code never touches). */
void isr_body(void)
{
    u8 st = VDPCtrl;
    vdp_status_last = st;
    if (st & 0x80)
    {
        frame_count++;
        /* 70 Hz clock: add 70/60 per frame */
        tc_acc += 10;
        time_count++;
        if (tc_acc >= 60) { tc_acc -= 60; time_count++; }
        pad_state = (u8)~IOPortA & 0x3F;
    }
    snd_tick();
}

void isr_vblank(void) __naked
{
    __asm
        push af
        push bc
        push de
        push hl
        push ix
        push iy
        call _isr_body
        pop iy
        pop ix
        pop hl
        pop de
        pop bc
        pop af
        ei
        reti
    __endasm;
}

void nmi_handler(void) __naked
{
    __asm
        push af
        ld a,#1
        ld (_pause_hit),a
        pop af
        retn
    __endasm;
}

void vdp_reg(u8 reg, u8 val)
{
    __asm__("di");
    VDPCtrl = val;
    VDPCtrl = reg | 0x80;
    __asm__("ei");
}

void vdp_display(u8 on)
{
    if (on) vdp_reg1 |= 0x40; else vdp_reg1 &= ~0x40;
    vdp_reg(1, vdp_reg1);
}

void vdp_set_addr(u16 addr)
{
    __asm__("di");
    VDPCtrl = addr & 0xFF;
    VDPCtrl = (addr >> 8) | 0x40;
    __asm__("ei");
}

void vdp_set_read(u16 addr)
{
    __asm__("di");
    VDPCtrl = addr & 0xFF;
    VDPCtrl = (addr >> 8) & 0x3F;
    __asm__("ei");
}

void vdp_set_cram(u8 index)
{
    __asm__("di");
    VDPCtrl = index;
    VDPCtrl = 0xC0;
    __asm__("ei");
}

void vram_fill(u16 addr, u8 val, u16 len)
{
    vdp_set_addr(addr);
    while (len--)
    {
        VDPData = val;
        __asm__("nop\n nop\n nop");
    }
}

/* fast copy, only safe with the display off or in vblank */
void vram_write(u16 addr, const u8 *src, u16 len)
{
    vdp_set_addr(addr);
    while (len--)
        VDPData = *src++;
}

/* paced copy: >= 27 cycles between OUTs */
void vram_write_safe(u16 addr, const u8 *src, u16 len) __naked
{
    (void)addr; (void)src; (void)len;
    __asm
        ; hl = addr, de = src, len on stack
        di
        ld a,l
        out (#0xBF),a
        ld a,h
        or a,#0x40
        out (#0xBF),a
        ei
        ld hl,#2
        add hl,sp
        ld c,(hl)
        inc hl
        ld b,(hl)
        ex de,hl
        ld a,b
        or a,c
        jr z,00002$
00001$:
        ld a,(hl)       ; 7
        out (#0xBE),a   ; 11
        inc hl          ; 6
        dec bc          ; 6
        ld a,b          ; 4
        or a,c          ; 4
        jr nz,00001$    ; 12  -> 39 per byte
00002$:
        pop hl
        pop bc
        jp (hl)
    __endasm;
}

/* 32 bytes, paced (outi + jr: 28 cycles a byte) */
void vram_write32(u16 addr, const u8 *src) __naked
{
    (void)addr; (void)src;
    __asm
        di
        ld a,l
        out (#0xBF),a
        ld a,h
        or a,#0x40
        out (#0xBF),a
        ei
        ex de,hl
        ld bc,#0x20BE
00001$:
        outi
        jr nz,00001$
        ret
    __endasm;
}

void vram_read32(u16 addr, u8 *dst) __naked
{
    (void)addr; (void)dst;
    __asm
        di
        ld a,l
        out (#0xBF),a
        ld a,h
        and a,#0x3F
        out (#0xBF),a
        ei
        ex de,hl
        ld bc,#0x20BE
        push ix
        pop ix
00001$:
        ini
        jr nz,00001$
        ret
    __endasm;
}

/* paced read (any time) */
void vram_read(u16 addr, u8 *dst, u16 len)
{
    vdp_set_read(addr);
    while (len--)
    {
        *dst++ = VDPData;
    }
}

void vram_write_nt(u16 addr, const u16 *src, u8 count)
{
    vdp_set_addr(addr);
    while (count--)
    {
        u16 w = *src++;
        VDPData = w & 0xFF;
        __asm__("nop\n nop\n nop\n nop");
        VDPData = w >> 8;
    }
}

void vram_put_word(u16 addr, u16 w)
{
    vdp_set_addr(addr);
    VDPData = w & 0xFF;
    __asm__("nop\n nop\n nop\n nop\n nop");
    VDPData = w >> 8;
}

void cram_load(const u8 *pal, u8 first, u8 n)
{
    vdp_set_cram(first);
    while (n--)
    {
        VDPData = *pal++;
        __asm__("nop\n nop\n nop");
    }
}

void cram_set(u8 index, u8 val)
{
    vdp_set_cram(index);
    VDPData = val;
}

void wait_vblank(void)
{
    u8 f = frame_count;
    while (f == frame_count)
        __asm__("halt");
}

void wait_frames(u16 n)
{
    while (n--)
        wait_vblank();
}

u8 pad_read(void)
{
    u8 p = (u8)~IOPortA & 0x3F;
    if (pause_hit) { pause_hit = 0; p |= PAD_PAUSE; }
    return p;
}

void slot2_map(u8 bank)
{
    slot2_cur = bank;
    if (bank == SLOT2_CARTRAM)
        MapCtrl = MAP_CARTRAM;
    else if (bank == SLOT2_CARTRAM1)
        MapCtrl = MAP_CARTRAM1;
    else
    {
        MapCtrl = 0;
        MapSlot2 = bank;
    }
}

u8 rom_bank_slot1(u8 bank)
{
    u8 old = MapSlot1;
    MapSlot1 = bank;
    return old;
}

/* copy from a ROM bank (addressed as slot 2, 0x8000-0xBFFF) to RAM */
void rom_copy(u8 bank, const void *src, void *dst, u16 len)
{
    u8 old = slot2_cur;
    slot2_map(bank);
    memcpy(dst, src, len);
    slot2_map(old);
}

void rom_to_vram(u8 bank, const void *src, u16 vaddr, u16 len)
{
    u8 old = slot2_cur;
    slot2_map(bank);
    vram_write(vaddr, (const u8 *)src, len);
    slot2_map(old);
}

void rom_to_vram_safe(u8 bank, const void *src, u16 vaddr, u16 len)
{
    u8 old = slot2_cur;
    slot2_map(bank);
    vram_write_safe(vaddr, (const u8 *)src, len);
    slot2_map(old);
}

__sfr __at 0x04 DbgPort;
void dbg_byte(u8 v)
{
    DbgPort = v;
}

static const u8 vdp_init_regs[11] = {
    0x16,   /* mode 4, line irq enable */
    0xA0,   /* display off, frame irq on */
    0xFF,   /* name table 0x3800 */
    0xFF, 0xFF,
    0xFF,   /* SAT 0x3F00 */
    0xFB,   /* sprite tiles from 0x0000 */
    0x00,   /* backdrop colour 0 of sprite palette */
    0x00, 0x00,
    96      /* line interrupt at line 96 -> two sound ticks per frame */
};

void hw_init(void)
{
    u8 i;
    MapCtrl = 0;
    slot2_cur = 2;
    for (i = 0; i < 11; i++)
        vdp_reg(i, vdp_init_regs[i]);
    vdp_reg1 = 0xA0;
    /* clear VRAM and CRAM */
    vram_fill(0, 0, 0x4000);
    vdp_set_cram(0);
    for (i = 0; i < 32; i++) VDPData = 0;
    /* sprites off */
    vdp_set_addr(VRAM_SAT);
    VDPData = 0xD0;
    SD_Startup();
    __asm__("ei");
}
