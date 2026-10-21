"""Full screen pictures for the SMS: scale, redraw pad hints, tile-quantise."""
import numpy as np
from common import load_bin, save_png
from fullscreen import scale_screen, quantize_tiles, render_tiles, tile_to_planar
from fonts import HINT, hint_width, draw_hint, PropFont

PIC_SCREENS = [
    'scr_prepare', 'scr_softdisk', 'scr_title', 'scr_credits', 'scr_story',
    'scr_challenge', 'scr_novice', 'scr_warrior', 'scr_backformore',
    'scr_trilogy', 'scr_madgamers', 'scr_armapc',
    'cat_open', 'cat_trilogy', 'cat_quest', 'cat_invited', 'cat_groupic',
    'cat_legend', 'cat_follow', 'cat_cemetry', 'cat_orcmine', 'cat_grate',
    'cat_inferno', 'cat_cookie', 'cat_dragons', 'cat_flame', 'cat_crystal',
    'cat_altar', 'cat_redhall', 'cat_egypt', 'cat_apc', 'cat_vault',
    'cat_lounge', 'cat_homebbs', 'cat_victory',
]

MAX_TILES = 456


def pre_trilogy(p):
    """'Press C now' -> 'Press UP now' (letters cut from the picture: P from
    'Press', U from the 'n' of 'now' turned over and lengthened)."""
    y0, y1 = 172, 181
    rows = range(y0, y1)
    P = [p[y][125:132] for y in rows]
    # crop P to its ink columns
    def crop(g):
        cols = [x for x in range(len(g[0])) if any(r[x] for r in g)]
        a, b = cols[0], cols[-1] + 1
        return [r[a:b] for r in g]
    P = crop([list(r) for r in P])
    P = [[12 if v else 0 for v in r] for r in P]
    U = ["####.###",
         ".##...#.", ".##...#.", ".##...#.", ".##...#.",
         ".##...#.", ".##...#.", ".##..#..", "..###..."]
    U = [[1 if ch == '#' else 0 for ch in r] for r in U]
    U = [[12 if v else 0 for v in r] for r in U]
    # words 'Press' and 'now'
    def word(a, b):
        return [list(p[y][a:b]) for y in rows]
    press = crop(word(120, 158))
    now = crop(word(170, 200))
    space = 5
    line = [press, None, U, [[0]] * 9, P, None, now]
    widths = [len(g[0]) if g else space for g in line]
    total = sum(widths)
    for y in range(y0, y1 + 1):
        for x in range(320):
            p[y][x] = 0
    x = (320 - total) // 2
    for g in line:
        if g is None:
            x += space
            continue
        for r, row in enumerate(g):
            for c, v in enumerate(row):
                if v:
                    p[y0 + r][x + c] = v
        x += len(g[0])


def post_title(s):
    """title bar: SMS pad hints"""
    y = 180
    x0 = 5
    x1 = x0
    while s[179][x1 + 1] == 0:
        x1 += 1
    s[180:187, x0 + 1:x1] = 8
    items = [[('1-', 12), ('NEW GAME', 9)],
             [('2-', 12), ('RESTORE GAME', 9)],
             [('UP-', 12), ('DEMO', 9)],
             [('DOWN-', 12), ("USER'S GUIDE", 9)]]
    widths = [hint_width(i) for i in items]
    gap = (x1 - x0 - 6 - sum(widths)) // (len(items) - 1)
    x = x0 + 4
    for it, wd in zip(items, widths):
        draw_hint(s, x, y, it, shadow=(-1, 1))
        x += wd + gap


def post_challenge(s, keep_rows):
    font = PropFont()
    y0 = keep_rows.index(188) if 188 in keep_rows else 180
    s[y0:, :] = 0
    parts = [('(', 9), ('1', 12), (' = Novice      ', 9), ('2', 12), (' = Warrior)', 9)]
    tw = sum(font.width(t) for t, c in parts)
    x = (256 - tw) // 2
    y = min(y0, 192 - 10)
    for t, c in parts:
        x = font.draw(s, x, y, t, c)


# regions (SMS pixels x0,y0,x1,y1) whose tiles are protected in quantisation
PROTECT = {
    'scr_title': [(0, 176, 256, 190)],
    'scr_credits': [(96, 80, 252, 176)],
    'cat_orcmine': [(0, 112, 256, 192)],
}


def convert(name, debug_png=None):
    p = load_bin(name)
    if name == 'scr_trilogy':
        pre_trilogy(p)
    s, keep_rows = scale_screen(p, return_rows=True)
    if name == 'scr_title':
        post_title(s)
    elif name == 'scr_challenge':
        post_challenge(s, keep_rows)
    tiles, cmap = quantize_tiles(s, MAX_TILES, protect_rects=PROTECT.get(name, ()))
    if debug_png:
        save_png(render_tiles(tiles, cmap).tolist(), debug_png, 2)
    blob = bytearray()
    blob += len(tiles).to_bytes(2, 'little')
    for t in tiles:
        blob += tile_to_planar(t)
    for row in cmap:
        for (i, fl) in row:
            idx = i if i < 448 else 496 + (i - 448)
            blob += (idx | fl).to_bytes(2, 'little')
    return bytes(blob)


if __name__ == '__main__':
    import sys, os
    os.makedirs('/tmp/scr', exist_ok=True)
    for n in (sys.argv[1:] or PIC_SCREENS):
        b = convert(n, '/tmp/scr/%s.png' % n)
        print(n, len(b))
