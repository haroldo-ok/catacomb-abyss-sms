/* full screen pictures (bank 0) */
#include "hw.h"
#include "assets.h"
#include "screen.h"

const u8 ega_palette_sms[16] = {
    0x00, 0x20, 0x08, 0x28, 0x02, 0x22, 0x06, 0x2A,
    0x15, 0x35, 0x1D, 0x3D, 0x17, 0x37, 0x1F, 0x3F
};

void screen_load(u8 num)
{
    u8 old = slot2_cur;
    const u8 *p = (const u8 *)0x8000;
    u16 n, first;
    slot2_map(screen_bank[num]);
    n = p[0] | (p[1] << 8);
    p += 2;
    first = n > 448 ? 448 : n;
    vram_write(0, p, first * 32);
    p += first * 32;
    if (n > 448)
    {
        vram_write(0x3E00, p, (n - 448) * 32);
        p += (n - 448) * 32;
    }
    vram_write(VRAM_NAMETABLE, p, 768 * 2);
    slot2_map(old);
}

/* show a picture with a black palette fade */
void screen_show(u8 num)
{
    cram_load(ega_palette_sms + 0, 0, 0);
    vdp_display(0);
    screen_load(num);
    cram_load(ega_palette_sms, 0, 16);
    vdp_display(1);
}
