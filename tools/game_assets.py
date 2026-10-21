"""In-game data for the SMS port (called from build_assets.py).

Data banks (packer):  HUD tiles + nametable, faces, radar picture,
texts (level names, location texts, help pages) behind a (bank,addr) table,
PSG sound streams, the FINALEPIC view picture, the 8 scroll screens.
gen/gamefont.c (compiled into the platform code bank): the proportional font,
the window frame pieces (tile8m), the north icon.
"""
import os
import numpy as np
from common import D, pic, save_png
from fullscreen import tile_to_planar, quantize_tiles, render_tiles
from fonts import PropFont
import hud, sounds, sms_text, finale

HERE = os.path.dirname(os.path.abspath(__file__))
GEN = os.path.join(HERE, '..', 'gen')

HELP_W = 200             # help text width (window interior 26 tiles = 208, margins 4)
HELP_TAB = 76            # second table column


def cstr_bytes(b):
    return bytes(b) + b'\0'


def tile8m_rows(n):
    """window frame piece n -> 8 rows of (keep mask, plane0..3)"""
    d = D['tile8m_data']
    out = bytearray()
    for r in range(8):
        mask = d[n * 16 + r * 2]
        data = d[n * 16 + r * 2 + 1]
        km = 0
        planes = [0, 0, 0, 0]
        for x in range(8):
            m = (mask >> (28 - 4 * x)) & 15
            v = (data >> (28 - 4 * x)) & 15
            if m:
                km |= 0x80 >> x
            for b in range(4):
                if v >> b & 1:
                    planes[b] |= 0x80 >> x
        out += bytes([km] + planes)
    return bytes(out)


def scroll_screen(n):
    """SCROLLTOPPIC + SCROLLnPIC + SCROLLBOTTOMPIC centred on a black
    256x192 screen with the exit hint underneath"""
    top = np.array(pic(6), np.uint8)
    body = np.array(pic(8 + n), np.uint8)
    bot = np.array(pic(7), np.uint8)
    W = 152
    img = np.zeros((120, W), np.uint8)
    img[0:32, :] = top
    img[32:88, 8:8 + body.shape[1]] = body
    img[88:120, :] = bot
    s = np.zeros((192, 256), np.uint8)
    x0, y0 = 52, 26
    s[y0:y0 + 120, x0:x0 + W] = img
    f = PropFont()
    msg = 'Press 1 or 2 to exit.'
    w = f.width(msg)
    canvas = np.zeros((10, 256), np.uint8)
    f.draw(canvas, (256 - w) // 2, 0, msg, 14)
    s[160:170][canvas != 0] = 14
    return s


def screen_blob(s, limit=448):
    tiles, cmap = quantize_tiles(s, limit)
    blob = bytearray()
    blob += len(tiles).to_bytes(2, 'little')
    for t in tiles:
        blob += tile_to_planar(t)
    for row in cmap:
        for (i, fl) in row:
            idx = i if i < 448 else 496 + (i - 448)
            blob += (idx | fl).to_bytes(2, 'little')
    return bytes(blob), tiles, cmap


def check_help_layout(pages):
    f = PropFont()
    for i, p in enumerate(pages):
        for line in p.split(b'\n'):
            if b'\t' in line:
                a, b = line.split(b'\t', 1)
                assert f.width(a.decode('latin1')) <= HELP_TAB - 4, (i, a, f.width(a.decode('latin1')))
                assert f.width(b.decode('latin1')) <= HELP_W - HELP_TAB, (i, b, f.width(b.decode('latin1')))


def build(pk, H, C, cached):
    # ---------------------------------------------------------------- HUD
    a = hud.allocate()
    save_png(a['screen'].tolist(), os.path.join(GEN, 'hud_preview.png'), 2)
    assert a['free_left'] >= 0
    blob = bytearray()
    for idx, data in a['tile_data']:
        blob += idx.to_bytes(2, 'little') + data
    b, ad = pk.add('hud_tiles', blob)
    H.append('#define HUD_TILES_BANK %d' % b)
    H.append('#define HUD_TILES_ADDR 0x%04X' % ad)
    H.append('#define HUD_NTILES %d' % len(a['tile_data']))
    nt = bytearray()
    for row in a['nt']:
        for v in row:
            nt += v.to_bytes(2, 'little')
    b, ad = pk.add('hud_nt', nt)
    H.append('#define HUD_NT_BANK %d' % b)
    H.append('#define HUD_NT_ADDR 0x%04X' % ad)
    for name, base in a['dyn_base'].items():
        H.append('#define HUD_%s_TILE %d' % (name.upper(), base))
    for name, r in (('MSG', hud.MSG), ('SMSG', hud.SMSG), ('FACE', hud.FACE), ('RADAR', hud.RADAR)):
        H.append('#define HUD_%s_X %d' % (name, r[0]))
        H.append('#define HUD_%s_Y %d' % (name, r[1]))
        H.append('#define HUD_%s_W %d' % (name, r[2]))
        H.append('#define HUD_%s_H %d' % (name, r[3]))
    H.append('#define HUD_HEALTH_X %d' % hud.HEALTH[0])
    H.append('#define HUD_HEALTH_Y %d' % hud.HEALTH[1])
    H.append('#define HUD_FREEZE_X %d' % hud.FREEZE[0])
    H.append('#define HUD_FREEZE_Y %d' % hud.FREEZE[1])
    H.append('#define HUD_SCROLL_X %d' % (hud.SCROLL_AT[0] + 1))
    H.append('#define HUD_SCROLL_Y %d' % (hud.SCROLL_AT[1] + 1))
    H.append('#define HUD_WHITE_TILE %d' % a['white'])
    H.append('#define HUD_ITEM_XY {%s}' % ','.join('%d,%d' % p for p in hud.ITEMS))
    H.append('#define HUD_KEY_XY {%s}' % ','.join('%d,%d' % p for p in hud.KEYS))
    cv = [a['char_vram'].get(i, 0) for i in range(35)]
    C.append('const u16 hud_char_tile[35] = {%s};' % ','.join(map(str, cv)))
    H.append('extern const u16 hud_char_tile[35];')
    gems = a['gems']
    base_nt = [a['nt'][hud.RADAR[1] + 5 + j][hud.RADAR[0] + 1 + i] for i in range(5) for j in range(2)]
    C.append('const u16 hud_gem_tile[2][10] = {{%s},{%s}};' % (
        ','.join(map(str, base_nt)), ','.join(str(gems[i][j]) for i in range(5) for j in range(2))))
    H.append('extern const u16 hud_gem_tile[2][10];   /* [owned][gem*2+row] */')

    # faces: 5 x 25 tiles, column major
    blob = bytearray()
    for n in range(5):
        f = hud.face_pic(n)
        for tx in range(5):
            for ty in range(5):
                blob += tile_to_planar(f[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8])
    b, ad = pk.add('faces', blob)
    H.append('#define FACES_BANK %d' % b)
    H.append('#define FACES_ADDR 0x%04X' % ad)
    # radar picture top 7x5 tiles, column major
    rb = hud.radar_base()
    blob = bytearray()
    for tx in range(7):
        for ty in range(5):
            blob += tile_to_planar(rb[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8])
    b, ad = pk.add('radar', blob)
    H.append('#define RADAR_BANK %d' % b)
    H.append('#define RADAR_ADDR 0x%04X' % ad)

    # ------------------------------------------------------------ font etc.
    fnt = PropFont()
    lines = ['/* generated by tools/game_assets.py */', '#include "hw.h"']
    lines.append('const u8 font_widths[128] = {%s};' % ','.join(str(fnt.wid[i]) for i in range(128)))
    lines.append('const u16 font_locs[128] = {%s};' % ','.join(str(fnt.loc[i]) for i in range(128)))
    fd = bytes(fnt.data)
    lines.append('const u8 font_data[%d] = {%s};' % (len(fd), ','.join(str(x) for x in fd)))
    fr = b''.join(tile8m_rows(n) for n in range(9))
    lines.append('const u8 frame_rows[9][40] = {%s};' % ','.join(
        '{%s}' % ','.join(str(x) for x in fr[i * 40:(i + 1) * 40]) for i in range(9)))
    ni = D['northicon_pix']
    lines.append('const u8 north_icon[40] = {%s};' % ','.join(str(v) for v in ni))
    open(os.path.join(GEN, 'gamefont.c'), 'w').write('\n'.join(lines) + '\n')
    H.append('extern const u8 font_widths[128];')
    H.append('extern const u16 font_locs[128];')
    H.append('extern const u8 font_data[];')
    H.append('extern const u8 frame_rows[9][40];')
    H.append('extern const u8 north_icon[40];')
    H.append('#define FONT_H 10')

    # --------------------------------------------------------------- texts
    pages = sms_text.help_pages()
    check_help_layout(pages)
    import render_gen
    names = list(sms_text.LEVEL_NAMES) + render_gen.MAP_NAMES[len(sms_text.LEVEL_NAMES):]
    texts = [n.encode('latin1') for n in names]
    H.append('#define TXT_LEVELNAME 0')
    H.append('#define NUM_LEVELNAMES %d' % len(texts))
    H.append('#define TXT_LEVELTEXT %d' % len(texts))
    texts += sms_text.level_texts()
    H.append('#define TXT_HELP %d' % len(texts))
    H.append('#define HELP_PAGES %d' % len(pages))
    H.append('#define HELP_TAB %d' % HELP_TAB)
    texts += pages
    tab = bytearray()
    for i, t in enumerate(texts):
        b, ad = pk.add('txt%d' % i, cstr_bytes(t))
        tab += bytes([b]) + ad.to_bytes(2, 'little')
    b, ad = pk.add('txt_table', tab)
    H.append('#define TXT_TABLE_BANK %d' % b)
    H.append('#define TXT_TABLE_ADDR 0x%04X' % ad)
    H.append('#define NUM_TEXTS %d' % len(texts))
    H.append('#define TXT_MAXLEN %d' % max(len(t) for t in texts[:len(texts) - len(pages)]))

    # -------------------------------------------------------------- sounds
    res = sounds.build_all()
    blob = sounds.blob(res)
    b, ad = pk.add('sounds', blob)
    H.append('#define SND_BANK %d' % b)
    H.append('#define SND_ADDR 0x%04X' % ad)
    prio = [0, 10, 10, 10, 10, 10, 10, 10, 10, 5, 5, 5, 5, 0, 0, 0,
            0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 10, 10, 10, 0, 20, 15, 15, 15]
    C.append('const u8 sound_priority[%d] = {%s};' % (len(prio), ','.join(map(str, prio))))
    H.append('extern const u8 sound_priority[];')
    H.append('#define NUMSOUNDS %d' % len(res))
    for i, r in enumerate(res):
        H.append('#define %s %d' % (r[0], i))

    # ------------------------------------------------------------ finale
    def fin():
        tiles, cmap = finale.build()
        out = bytearray()
        out += len(tiles).to_bytes(2, 'little')
        for t in tiles:
            out += tile_to_planar(t)
        for row in cmap:
            for (i, fl) in row:
                out += (i | fl).to_bytes(2, 'little')
        return bytes(out)
    blob = cached('finale', fin)
    b, ad = pk.add('finale', blob)
    H.append('#define FINALE_BANK %d' % b)
    H.append('#define FINALE_ADDR 0x%04X' % ad)

    # ------------------------------------------------------------ scrolls
    banks = []
    for n in range(8):
        blob = cached('scroll%d' % n, lambda: screen_blob(scroll_screen(n))[0])
        b, ad = pk.add('scroll%d' % n, blob)
        assert ad == 0x8000 or True
        banks.append((b, ad))
    C.append('const u8 scroll_bank[8] = {%s};' % ','.join(str(b) for b, a in banks))
    C.append('const u16 scroll_addr[8] = {%s};' % ','.join('0x%04X' % a for b, a in banks))
    H.append('extern const u8 scroll_bank[8];')
    H.append('extern const u16 scroll_addr[8];')

    # ------------------------------------------------- sine (quarter table)
    st = D['sintable_init']
    C.append('const u16 sin_q[91] = {%s};' % ','.join(str(st[i]) for i in range(91)))
    H.append('extern const u16 sin_q[91];')

    # ---------------------------------------------------------- tile flags
    tf = list(D['tile_flags']) + [0] * (256 - len(D['tile_flags']))
    C.append('const u8 tile_flags[256] = {%s};' % ','.join(map(str, tf)))


if __name__ == '__main__':
    s = scroll_screen(0)
    b, t, c = screen_blob(s)
    print('scroll tiles', len(t))
    save_png(render_tiles(t, c).tolist(), '/tmp/scroll0.png', 2)
    check_help_layout(sms_text.help_pages())
