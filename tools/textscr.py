from common import *
H_LINES = set([0xC4, 0xCD])  # horizontal box lines
FILL = set([32, 0, 255, 0xC4, 0xCD, 0xDB, 0xDF, 0xDC, 0xB0, 0xB1, 0xB2])
def same(a, b):
    if a[0] in (32, 0, 255) and b[0] in (32, 0, 255):
        return (a[1] >> 4) == (b[1] >> 4)
    return a == b
def deletable_col(t, x, W=80, H=25):
    for y in range(H):
        c = t[y*W+x]
        if c[0] not in FILL:
            return False
        l = t[y*W+x-1] if x > 0 else None
        r = t[y*W+x+1] if x < W-1 else None
        if not ((l and same(c, l)) or (r and same(c, r))):
            return False
    return True
def squeeze(t, W, H, target_w, keep=lambda x: False):
    cols = list(range(W))
    grid = [[t[y*W+x] for x in range(W)] for y in range(H)]
    while len(cols) > target_w:
        Wc = len(cols)
        flat = [grid[y][x] for y in range(H) for x in range(Wc)]
        cand = [x for x in range(Wc) if deletable_col(flat, x, Wc, H)]
        if not cand:
            raise ValueError('cannot squeeze')
        # prefer the middle of the widest run of deletable columns
        runs = []
        s = None
        for x in range(Wc+1):
            if x < Wc and x in cand:
                if s is None: s = x
            elif s is not None:
                runs.append((x-s, s)); s = None
        runs.sort(key=lambda r: (-r[0], r[1]))
        ln, s = runs[0]
        x = s + ln//2
        for y in range(H): del grid[y][x]
        del cols[x]
    return grid
if __name__ == '__main__':
    for n in ['scr_last','scr_lastcat','scr_shellbg']:
        t = recover_text_screen(load_bin(n))
        g = squeeze(t, 80, 25, 64)
        g = g[:24]
        pairs = set()
        for row in g:
            for x in range(0, 64, 2): pairs.add((row[x], row[x+1]))
        print(n, 'unique pair tiles', len(pairs))
        for row in g: print(''.join(chr(c) if 32<=c<127 else '#' for c,a in row))
