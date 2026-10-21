"""Fonts used by the asset tools."""
from common import D

# the 3x6 shadowed lettering of the title bar key hints (from the Genesis
# port's screens.py, extended with the glyphs the SMS pad names need)
HINT = {
    'A': ['.#.', '#.#', '#.#', '###', '#.#', '#.#'],
    'B': ['##.', '#.#', '##.', '#.#', '#.#', '##.'],
    'C': ['.##', '#..', '#..', '#..', '#..', '.##'],
    'D': ['##.', '#.#', '#.#', '#.#', '#.#', '##.'],
    'E': ['###', '#..', '##.', '#..', '#..', '###'],
    'F': ['###', '#..', '##.', '#..', '#..', '#..'],
    'G': ['.##', '#..', '#..', '#.#', '#.#', '.##'],
    'H': ['#.#', '#.#', '###', '#.#', '#.#', '#.#'],
    'I': ['#', '#', '#', '#', '#', '#'],
    'L': ['#..', '#..', '#..', '#..', '#..', '###'],
    'M': ['#...#', '##.##', '#.#.#', '#...#', '#...#', '#...#'],
    'N': ['#..#', '##.#', '#.##', '#..#', '#..#', '#..#'],
    'O': ['.#.', '#.#', '#.#', '#.#', '#.#', '.#.'],
    'P': ['##.', '#.#', '#.#', '##.', '#..', '#..'],
    'R': ['##.', '#.#', '#.#', '##.', '#.#', '#.#'],
    'S': ['.##', '#..', '.#.', '..#', '..#', '##.'],
    'T': ['###', '.#.', '.#.', '.#.', '.#.', '.#.'],
    'U': ['#.#', '#.#', '#.#', '#.#', '#.#', '###'],
    'W': ['#...#', '#...#', '#...#', '#.#.#', '##.##', '#...#'],
    '1': ['.#', '##', '.#', '.#', '.#', '.#'],
    '2': ['##.', '..#', '..#', '.#.', '#..', '###'],
    "'": ['#', '#'],
    '-': ['....', '....', '####', '....', '....', '....'],
    ' ': ['..', '..', '..', '..', '..', '..'],
}


def hint_width(parts):
    return sum(sum(len(HINT[ch][0]) + 1 for ch in t) for t, col in parts)


def draw_hint(pix, x, y, parts, shadow=(1, 1)):
    """pix: 2D (numpy or list of lists)"""
    for t, col in parts:
        for ch in t:
            g = HINT[ch]
            for gy, row in enumerate(g):
                for gx, v in enumerate(row):
                    if v == '#':
                        pix[y + gy + shadow[1]][x + gx + shadow[0]] = 0
            for gy, row in enumerate(g):
                for gx, v in enumerate(row):
                    if v == '#':
                        pix[y + gy][x + gx] = col
            x += len(g[0]) + 1
    return x


class PropFont:
    """The game's proportional font (STARTFONT)."""
    def __init__(self):
        self.h = 10
        self.loc = D['font_locs']
        self.wid = D['font_widths']
        self.data = D['font_data']

    def width(self, s):
        return sum(self.wid[ord(c)] for c in s)

    def draw(self, pix, x, y, s, col):
        for c in s:
            cw = self.wid[ord(c)]
            bpr = (cw + 7) >> 3
            off = self.loc[ord(c)]
            for r in range(self.h):
                for i in range(cw):
                    if self.data[off + r * bpr + (i >> 3)] & (0x80 >> (i & 7)):
                        pix[y + r][x + i] = col
            x += cw
        return x


def glyph4x8(ch):
    return D['font4x8'][ch * 8:ch * 8 + 8]
