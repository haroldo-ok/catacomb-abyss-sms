"""In-game screen layout for the SMS: stone frame around the 3-D view and the
status panel (rebuilt on the tile grid from STATUSPIC art and the tile8 chars).

VRAM tiles free in game mode: 283..447, 496..503, 505..511 (180 tiles).
Dynamic surfaces (column major, drawn at run time):
  message line 30x1, status message 14x1, face 5x5, radar 7x7.
"""
import numpy as np
from common import D, pic, save_png
from fullscreen import tile_to_planar
from fonts import PropFont

FREE = list(range(283, 448)) + list(range(496, 504)) + list(range(505, 512))
TILE_BLANK = 282

# screen layout (tile coordinates)
MSG = (0, 11, 32, 1)        # x, y, w, h
SMSG = (11, 21, 14, 1)
FACE = (1, 13, 5, 5)
RADAR = (24, 13, 7, 5)      # dynamic part; rows 5-6 of the radar picture are static (gems)
HEALTH = (2, 19)            # 3 digits + '%'
ITEMS = [(7, 13), (7, 16), (7, 19)]                  # bolt, nuke, potion: icon x, y
KEYS = [(11, 14), (15, 18), (15, 14), (11, 18)]      # red, yellow, green, blue
SCROLL_AT = (18, 14)        # 6x4 scroll picture, chars at (x+1..x+4, y+1..y+2)
FREEZE = (22, 21)           # 3 digits over the status message line

BLANKCHAR, BOLTCHAR, NUKECHAR, POTIONCHAR, KEYCHARS, SCROLLCHARS, NUMBERCHARS = 9, 10, 11, 12, 13, 17, 25
CHARS = [BLANKCHAR, BOLTCHAR, NUKECHAR, POTIONCHAR] + list(range(KEYCHARS, KEYCHARS + 4)) + \
        list(range(SCROLLCHARS, SCROLLCHARS + 8)) + list(range(NUMBERCHARS, NUMBERCHARS + 10))


def tile8(ch):
    t = D['tile8_data']
    return np.array([[(t[ch * 8 + r] >> (28 - 4 * x)) & 15 for x in range(8)] for r in range(8)], np.uint8)


def status_pic():
    return np.array(pic(5), np.uint8)


def stone_tiles():
    """stone blocks from the STATUSPIC border: 8x8 cells of its left edge"""
    p = status_pic()
    out = []
    for y in (16, 24, 32, 40, 48, 56):
        out.append(p[y:y + 8, 0:8].copy())
    return out


def wood_tile():
    t = np.full((8, 8), 6, np.uint8)
    t[6, :] = 8
    t[7, :] = 5
    return t


def label(text_parts, w_tiles, font):
    """grey label box with coloured prop-font text (like the Genesis
    START-MENU / B-FIRE plates)"""
    W = w_tiles * 8
    img = np.full((8, W), 8, np.uint8)
    img[0, :] = 7
    img[7, :] = 0
    tw = sum(font.width(t) for t, c in text_parts)
    x = (W - tw) // 2
    canvas = np.zeros((10, W), np.uint8)
    for t, c in text_parts:
        x = font.draw(canvas, x, 0, t, c)
    for r in range(1, 8):           # glyph rows 1..7 (caps)
        for xx in range(W):
            if canvas[r][xx]:
                img[r][xx] = canvas[r][xx] & 15
    return img


def build():
    font = PropFont()
    screen = np.zeros((24 * 8, 32 * 8), np.uint8)
    kind = [[None] * 32 for _ in range(24)]   # per cell: ('static',) / ('dyn', name, idx) / ('char', ch) / ('view',)
    stones = stone_tiles()
    wood = wood_tile()

    def put(tx, ty, t, k=('static',)):
        screen[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8] = t
        kind[ty][tx] = k

    # stone frame
    for ty in range(24):
        for tx in range(32):
            st = stones[(tx * 3 + ty * 5) % len(stones)]
            if (tx + ty) & 1:
                st = st[:, ::-1]
            if ty == 0 or ty == 22 or tx in (0, 31) or (ty <= 10 and tx in (1, 30)):
                put(tx, ty, st)
            elif ty == 23:
                kind[ty][tx] = ('blank',)
            elif 1 <= ty <= 10:
                kind[ty][tx] = ('view',)
            else:
                put(tx, ty, wood)
    # dynamic surfaces
    for name, (x0, y0, w, h) in (('msg', MSG), ('smsg', SMSG), ('face', FACE), ('radar', RADAR)):
        for i in range(w):
            for j in range(h):
                c = 8 if name in ('msg', 'smsg') else 0
                screen[(y0 + j) * 8:(y0 + j) * 8 + 8, (x0 + i) * 8:(x0 + i) * 8 + 8] = c
                kind[y0 + j][x0 + i] = ('dyn', name, i * h + j)
    # radar preview art
    rad = radar_base()
    x0, y0 = RADAR[0] * 8, RADAR[1] * 8
    screen[y0:y0 + 56, x0:x0 + 56] = rad
    for j in (5, 6):
        for i in range(7):
            kind[RADAR[1] + j][RADAR[0] + i] = ('static',)
    # face preview
    f = face_pic(0)
    screen[FACE[1] * 8:FACE[1] * 8 + 40, FACE[0] * 8:FACE[0] * 8 + 40] = f
    # chars
    x, y = HEALTH
    for i in range(3):
        put(x + i, y, tile8(NUMBERCHARS + (1 if i == 0 else 0)), ('char', NUMBERCHARS))
    put(x + 3, y, percent_tile(), ('static',))
    for i, (x, y) in enumerate(ITEMS):
        put(x, y, tile8(BOLTCHAR + i), ('char', BOLTCHAR + i))
        put(x + 1, y, tile8(BLANKCHAR), ('char', BLANKCHAR))
        put(x + 2, y, tile8(NUMBERCHARS), ('char', NUMBERCHARS))
    for i, (x, y) in enumerate(KEYS):
        put(x, y, tile8(KEYCHARS + i), ('char', KEYCHARS + i))
        put(x + 1, y, tile8(BLANKCHAR), ('char', BLANKCHAR))
        put(x + 2, y, tile8(NUMBERCHARS), ('char', NUMBERCHARS))
    # scroll picture (STATUSPIC x 200..247, y 22..53), chars at rows 1-2
    p = status_pic()
    sx, sy = SCROLL_AT
    white = np.full((8, 8), 15, np.uint8)
    for j in (1, 2):
        for i in range(1, 5):
            put(sx + i, sy + j, white, ('static',))
    # labels
    lab1 = label([('PAUSE', 12), ('-', 16), ('MENU', 9)], 10, font)
    for i in range(10):
        put(1 + i, 21, lab1[:, i * 8:i * 8 + 8])
    lab2 = label([('1', 12), ('-', 16), ('FIRE', 9)], 6, font)
    for i in range(6):
        put(25 + i, 21, lab2[:, i * 8:i * 8 + 8])
    return screen, kind


def percent_tile():
    """the '%' after the health number (STATUSPIC x 96, y 57)"""
    p = status_pic()
    return p[57:65, 96:104].copy()


def face_pic(n):
    """faces (pics 16, 18..21) cropped to 40x40"""
    pics = [18, 19, 20, 21, 16]     # FACE1..FACE4, FACE5 (dead)
    f = np.array(pic(pics[n]), np.uint8)
    return f[:, 4:44].copy()


def radar_base():
    top = np.array(pic(22), np.uint8)     # 56x40
    bot = np.array(pic(23), np.uint8)     # 56x16
    return np.vstack([top, bot])


def gem_tiles():
    """for each gem: the two radar tiles (column 1+i, rows 5,6) with the
    gem pic composited at y 42 (as RADAR_xGEMPIC at (32+i, 53))"""
    base = radar_base()
    out = []
    for i in range(5):
        g = np.array(pic(24 + i), np.uint8)   # 8x16
        b = base.copy()
        for r in range(16):
            if 42 + r < 56:
                b[42 + r, 8 + 8 * i:16 + 8 * i] = g[r]
        out.append([b[40:48, 8 + 8 * i:16 + 8 * i].copy(), b[48:56, 8 + 8 * i:16 + 8 * i].copy()])
    return out


def north_icon():
    pix = D['northicon_pix']
    return pix   # 8x5, 255 = transparent


def tile_key(t):
    return t.tobytes()


def allocate():
    screen, kind = build()
    # dynamic ranges first (contiguous)
    dyn_base = {}
    nxt = 0
    for name, (x0, y0, w, h) in (('msg', MSG), ('smsg', SMSG), ('face', FACE), ('radar', RADAR)):
        dyn_base[name] = FREE[nxt]
        assert FREE[nxt + w * h - 1] == FREE[nxt] + w * h - 1
        nxt += w * h
    pool = FREE[nxt:]
    tiles = {}          # key -> vram index
    tile_data = []      # (vram, 32 bytes)
    char_vram = {}

    def alloc(t):
        nonlocal pool
        variants = [(t, 0), (t[:, ::-1], 0x200), (t[::-1, :], 0x400), (t[::-1, ::-1], 0x600)]
        for v, fl in variants:
            k = tile_key(v)
            if k in tiles:
                return tiles[k] | fl
        assert pool, 'out of HUD tiles'
        idx = pool[0]
        pool = pool[1:]
        tiles[tile_key(t)] = idx
        tile_data.append((idx, tile_to_planar(t)))
        return idx

    for ch in CHARS:
        char_vram[ch] = alloc(tile8(ch))
    gems = [[alloc(t) for t in g] for g in gem_tiles()]
    nt = [[0] * 32 for _ in range(24)]
    for ty in range(24):
        for tx in range(32):
            k = kind[ty][tx]
            if k is None or k[0] == 'static':
                nt[ty][tx] = alloc(screen[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8])
            elif k[0] == 'char':
                nt[ty][tx] = char_vram[k[1]]
            elif k[0] == 'dyn':
                nt[ty][tx] = dyn_base[k[1]] + k[2]
            elif k[0] == 'blank':
                nt[ty][tx] = TILE_BLANK
            elif k[0] == 'view':
                tx2, ty2 = tx - 2, ty - 1
                nt[ty][tx] = tx2 * 10 + ty2
    return dict(screen=screen, nt=nt, tile_data=tile_data, char_vram=char_vram,
                dyn_base=dyn_base, gems=gems, free_left=len(pool), white=alloc(np.full((8, 8), 15, np.uint8)))


if __name__ == '__main__':
    a = allocate()
    print('static/char tiles', len(a['tile_data']), 'left', a['free_left'])
    save_png(a['screen'].tolist(), '/tmp/hud.png', 3)
