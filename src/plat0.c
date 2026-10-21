/* plat0.c - bank 0 part of the platform layer: string entry points, ROM data
 * accessors, fixed point math, timer.
 *
 * Banked code can't map another bank into slot 1 (it runs there), and its
 * constant strings are only readable while its own bank is mapped.  These
 * functions live in bank 0: they copy string arguments to RAM before calling
 * the banked implementation, and read ROM data banks through slot 1.
 */
#include "game/def.h"

char plat_str[320];
char plat_str2[128];
static char lvl_buf[48];
static char txt_buf[TXT_MAXLEN + 1];

/* ------------------------------------------------------------ timer */

u32 get_timecount(void)
{
    u32 t;
    __asm__("di");
    t = time_count;
    __asm__("ei");
    return t;
}

void set_timecount(u32 t)
{
    __asm__("di");
    time_count = t;
    __asm__("ei");
}

/* ------------------------------------------------------------ ROM data */

/* copy from a data bank (addresses as packed for slot 2, 0x8000..) through
 * slot 1; dst may be anywhere but slot 1 */
void rom_read(u8 bank, u16 addr, void *dst, u16 len)
{
    u8 old = MapSlot1;
    MapSlot1 = bank;
    memcpy(dst, (const u8 *)(addr - 0x4000), len);
    MapSlot1 = old;
}

u16 text_copy(u16 id, char *dst, u16 max)
{
    u8 e[3];
    u8 old;
    const char *s;
    u16 n = 0;
    rom_read(TXT_TABLE_BANK, TXT_TABLE_ADDR + id * 3, e, 3);
    old = MapSlot1;
    MapSlot1 = e[0];
    s = (const char *)((e[1] | (e[2] << 8)) - 0x4000);
    while (n + 1 < max && s[n])
    {
        dst[n] = s[n];
        n++;
    }
    dst[n] = 0;
    MapSlot1 = old;
    return n;
}

/* up to len bytes of help page `page` from position pos; returns the
 * count copied before the terminating zero (dst is zero terminated) */
u16 help_read(u16 page, u16 pos, char *dst, u16 len)
{
    u8 e[3];
    u8 old;
    const char *s;
    u16 n = 0;
    rom_read(TXT_TABLE_BANK, TXT_TABLE_ADDR + (TXT_HELP + page) * 3, e, 3);
    old = MapSlot1;
    MapSlot1 = e[0];
    s = (const char *)((e[1] | (e[2] << 8)) - 0x4000 + pos);
    while (n < len && s[n])
    {
        dst[n] = s[n];
        n++;
    }
    dst[n] = 0;
    MapSlot1 = old;
    return n;
}

const char *LevelName(s16 n)
{
    lvl_buf[0] = 0;
    if (n >= 0 && n < NUMMAPDEFS)
        text_copy(TXT_LEVELNAME + n, lvl_buf, sizeof(lvl_buf));
    return lvl_buf;
}

const char *LevelText(s16 map, s16 n)
{
    text_copy(TXT_LEVELTEXT + map * 27 + n, txt_buf, sizeof(txt_buf));
    return txt_buf;
}

/* 16 bit plane 2 value straight from the map in ROM */
u16 map2full(u16 x, u16 y)
{
    u8 m = gamestate.mapon;
    u16 a = map_addr[m] - 0x4000;
    u8 old = MapSlot1;
    u16 w, h, ofs, v;
    MapSlot1 = map_bank[m];
    w = ((const u8 *)a)[0];
    h = ((const u8 *)a)[1];
    ofs = 2 + w * h + y * w + x;
    v = ((const u8 *)a)[ofs] | (((const u8 *)a)[ofs + w * h] << 8);
    MapSlot1 = old;
    return v;
}

/* map blob of the current level: n bytes from offset ofs */
void map_read(u8 m, u16 ofs, u8 *dst, u16 n)
{
    rom_read(map_bank[m], map_addr[m] + ofs, dst, n);
}

/* ------------------------------------------------------- string entry */

static void cpy(char *d, const char *s, u16 max)
{
    u16 n = 0;
    if (s)
        while (n + 1 < max && s[n]) { d[n] = s[n]; n++; }
    d[n] = 0;
}

void Quit(const char *error)
{
    if (error && *error)
    {
        cpy(plat_str, error, sizeof(plat_str));
        DisplayMsgR(plat_str, NULL);
        for (;;)
            __asm__("halt");
    }
    quitgame = TRUE;
}

void US_Print(const char *s)        { cpy(plat_str, s, sizeof(plat_str)); US_PrintR(plat_str); }
void US_CPrint(const char *s)       { cpy(plat_str, s, sizeof(plat_str)); US_CPrintR(plat_str); }
void US_CPrintLine(const char *s)   { cpy(plat_str, s, sizeof(plat_str)); US_CPrintLineR(plat_str); }
void US_PrintCentered(const char *s){ cpy(plat_str, s, sizeof(plat_str)); US_PrintCenteredR(plat_str); }

char DisplayMsg(const char *text, const char *choices)
{
    cpy(plat_str, text, sizeof(plat_str));
    cpy(plat_str2, choices, sizeof(plat_str2));
    return DisplayMsgR(text ? plat_str : NULL, choices ? plat_str2 : NULL);
}

char DisplaySMsg(const char *text, const char *choices)
{
    cpy(plat_str, text, sizeof(plat_str));
    cpy(plat_str2, choices, sizeof(plat_str2));
    return DisplaySMsgR(text ? plat_str : NULL, choices ? plat_str2 : NULL);
}

char MenuChoice(const char *prompt, const char *choices)
{
    cpy(plat_str, prompt, sizeof(plat_str));
    cpy(plat_str2, choices, sizeof(plat_str2));
    return MenuChoiceR(prompt ? plat_str : NULL, plat_str2);
}

s16 MenuWindow(const char *title, u16 count, const char * const *items, s16 sel)
{
    static const char *ip[10];
    u16 i, p = 0;
    const char *t = NULL;
    if (title)
    {
        cpy(plat_str, title, sizeof(plat_str));
        t = plat_str;
        p = strlen(plat_str) + 1;
    }
    if (count > 10) count = 10;
    for (i = 0; i < count; i++)
    {
        cpy(plat_str + p, items[i], sizeof(plat_str) - p);
        ip[i] = plat_str + p;
        p += strlen(plat_str + p) + 1;
        if (p >= sizeof(plat_str)) p = sizeof(plat_str) - 1;
    }
    return MenuWindowR(t, count, ip, sel);
}

/* ------------------------------------------------------------- math */

/* FixedByFrac: a 16.16 number times a fraction; like the original only the
 * fraction's low 16 bits of magnitude count (sin 90 = 65535).
 * Two 16x16->32 multiplies (the library's shift-add routine). */
u32 __muluint2ulong(u16 a, u16 b);     /* SDCC library, 16x16 -> 32 */
fixed FixedByFrac(fixed a, fixed b)
{
    u8 neg = 0;
    u32 ua, r;
    u16 ub, hi, lo;
    if (a < 0) { ua = -a; neg = 1; } else ua = a;
    if (b < 0) { ub = (u16)(-b); neg ^= 1; } else ub = (u16)b;
    hi = ((u16 *)&ua)[1];
    lo = ((u16 *)&ua)[0];
    if (lo)
    {
        r = __muluint2ulong(lo, ub);
        lo = ((u16 *)&r)[1];
    }
    r = hi ? __muluint2ulong(hi, ub) + lo : lo;
    return neg ? -(fixed)r : (fixed)r;
}

/* n / d for n < d * 65536 (16 bit quotient), restoring division */
static u32 dv_n;
static u16 dv_d;
static u16 udiv_nd(void) __naked
{
    __asm
        ld hl,(_dv_n+2)
        ld de,(_dv_n)
        ld bc,(_dv_d)
        ld a,#16
00001$:
        ex de,hl
        add hl,hl
        ex de,hl
        adc hl,hl
        jr c,00003$
        or a,a
        sbc hl,bc
        jr nc,00002$
        add hl,bc
        jr 00004$
00003$:
        or a,a
        sbc hl,bc
00002$:
        inc e
00004$:
        dec a
        jr nz,00001$
        ret
    __endasm;
}

u16 udiv32_16(u32 n, u16 d)
{
    dv_n = n;
    dv_d = d;
    return udiv_nd();
}

/* sintable[angle], angle 0..449 (costable = sintable + 90) */
fixed fsin(s16 a)
{
    u16 v;
    while (a >= 360) a -= 360;
    while (a < 0) a += 360;
    if (a <= 90) v = sin_q[a];
    else if (a <= 180) v = sin_q[180 - a];
    else if (a <= 270) return -(fixed)sin_q[a - 180];
    else return -(fixed)sin_q[360 - a];
    return v;
}

fixed fcos(s16 a)
{
    return fsin(a + 90);
}

s16 IntSqrt(s32 va)
{
    u32 v = va, r, b;
    if (!v) return 0;
    r = 0;
    b = 1ul << 30;
    while (b > v) b >>= 2;
    while (b)
    {
        if (v >= r + b)
        {
            v -= r + b;
            r = (r >> 1) + b;
        }
        else
            r >>= 1;
        b >>= 2;
    }
    return (s16)r;
}

/* xorshift PRNG (SGDK's random() stand in) */
static u16 rnd_state = 0xACE1;
u16 random16(void)
{
    u16 x = rnd_state;
    x ^= x << 7;
    x ^= x >> 9;
    x ^= x << 8;
    rnd_state = x;
    return x;
}

void random_seed(u16 s)
{
    rnd_state = s ? s : 0xACE1;
}

/* decimal, at least minlen digits (zero padded) */
void intToStr(s16 v, char *s, u8 minlen)
{
    char t[7];
    u8 n = 0;
    u16 u;
    if (v < 0) { *s++ = '-'; u = -v; } else u = v;
    do { t[n++] = '0' + u % 10; u /= 10; } while (u);
    while (n < minlen) t[n++] = '0';
    while (n) *s++ = t[--n];
    *s = 0;
}

#include <stddef.h>
typedef char cb_chk1[(offsetof(objtype, x) == 14 && offsetof(objtype, y) == 18) ? 1 : -1];
typedef char cb_chk2[(offsetof(objtype, size) == 40 && offsetof(objtype, xl) == 44) ? 1 : -1];
typedef char cb_chk3[(offsetof(objtype, xh) == 48 && offsetof(objtype, yl) == 52 && offsetof(objtype, yh) == 56) ? 1 : -1];

/* ob->xl/xh/yl/yh = x/y -/+ size (objtype: x 14, y 18, size 40, xl 44,
 * xh 48, yl 52, yh 56; ob in HL) */
void CalcBounds(objtype *ob) __naked
{
    (void)ob;
    __asm
        push ix
        push hl
        pop ix
        ld c,40 (ix)
        ld b,41 (ix)
        ld e,42 (ix)
        ld d,43 (ix)
        ld a,14 (ix)
        sub a,c
        ld 44 (ix),a
        ld a,15 (ix)
        sbc a,b
        ld 45 (ix),a
        ld a,16 (ix)
        sbc a,e
        ld 46 (ix),a
        ld a,17 (ix)
        sbc a,d
        ld 47 (ix),a
        ld a,14 (ix)
        add a,c
        ld 48 (ix),a
        ld a,15 (ix)
        adc a,b
        ld 49 (ix),a
        ld a,16 (ix)
        adc a,e
        ld 50 (ix),a
        ld a,17 (ix)
        adc a,d
        ld 51 (ix),a
        ld a,18 (ix)
        sub a,c
        ld 52 (ix),a
        ld a,19 (ix)
        sbc a,b
        ld 53 (ix),a
        ld a,20 (ix)
        sbc a,e
        ld 54 (ix),a
        ld a,21 (ix)
        sbc a,d
        ld 55 (ix),a
        ld a,18 (ix)
        add a,c
        ld 56 (ix),a
        ld a,19 (ix)
        adc a,b
        ld 57 (ix),a
        ld a,20 (ix)
        adc a,e
        ld 58 (ix),a
        ld a,21 (ix)
        adc a,d
        ld 59 (ix),a
        pop ix
        ret
    __endasm;
}

/* signed 16x16 -> 32 (HL * DE -> HLDE).  SDCC 4.4.0's __mulsint2slong
 * tests the sign in the LOW bytes (bit 7,l / bit 7,e) and gets every
 * operand with bit 7 of the low byte set wrong; this is it fixed. */
long smul16(int a, int b) __naked
{
    (void)a; (void)b;
    __asm
        ld bc,#0
        bit 7,h
        jr z,00001$
        ld a,b
        sub a,l
        ld l,a
        ld a,b
        sbc a,h
        ld h,a
        inc c
00001$:
        bit 7,d
        jr z,00002$
        ld a,b
        sub a,e
        ld e,a
        ld a,b
        sbc a,d
        ld d,a
        inc c
00002$:
        push bc
        call ___muluint2ulong
        pop bc
        bit 0,c
        ret z
        ld a,b
        sub a,e
        ld e,a
        ld a,b
        sbc a,d
        ld d,a
        ld a,b
        sbc a,l
        ld l,a
        ld a,b
        sbc a,h
        ld h,a
        ret
    __endasm;
}
