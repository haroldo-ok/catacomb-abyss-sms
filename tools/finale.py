"""FINALEPIC for the SMS: shown in the 3-D view area (224x80 is too small,
so the victory picture takes the 256x88 above the message line, tiles
0..279), with the banner text reset in the game font (3 lines)."""
import numpy as np
from common import pic
from fonts import PropFont
from fullscreen import scale_screen, quantize_tiles

LINES = ['You have emerged from the', 'Catacomb Abyss as a Conqueror', 'and Valiant Warrior!']


def image():
    p = np.array(pic(4), np.uint8)
    rows = [int((i + 0.5) * 120 / 88) for i in range(88)]
    s = scale_screen(p[rows, :], 256, 88)
    assert s.shape == (88, 256)
    f = PropFont()
    x0, x1, y1 = 6, 250, 30
    s[0:y1 + 1, x0:x1] = 7
    s[0, x0:x1] = 0
    s[1, x0:x1] = 8
    s[y1 - 1, x0:x1] = 8
    s[y1, x0:x1] = 0
    s[0:y1 + 1, x0] = 0
    s[1:y1, x0 + 1] = 8
    s[0:y1 + 1, x1 - 1] = 0
    s[1:y1, x1 - 2] = 8
    canvas = np.zeros((40, 256), np.uint8)
    for i, l in enumerate(LINES):
        w = f.width(l)
        f.draw(canvas, (256 - w) // 2, 2 + 9 * i - 1, l, 16)
    s[:40][canvas == 16] = 0
    return s


def build():
    """-> (tiles planar list, 32x11 nametable of tile|flags)"""
    s = image()
    tiles, cmap = quantize_tiles(s, 280, protect_rects=[(0, 0, 256, 32)])
    return tiles, cmap


if __name__ == '__main__':
    from common import save_png
    from fullscreen import render_tiles
    t, c = build()
    print(len(t))
    save_png(render_tiles(t, c).tolist(), '/tmp/fin3.png', 3)
