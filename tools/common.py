import os, pickle, re
from PIL import Image
HERE = os.path.dirname(os.path.abspath(__file__))
D = pickle.load(open(os.path.join(HERE, 'data.pkl'), 'rb'))
MD = os.path.expanduser('~/catacomb-abbyss-md')
EGA = [(0,0,0),(0,0,170),(0,170,0),(0,170,170),(170,0,0),(170,0,170),(170,85,0),(170,170,170),
       (85,85,85),(85,85,255),(85,255,85),(85,255,255),(255,85,85),(255,85,255),(255,255,85),(255,255,255)]
def sms_color(rgb):
    r,g,b = [ {0:0,85:1,170:2,255:3}[c] for c in rgb ]
    return r | (g<<2) | (b<<4)
EGA_SMS = [sms_color(c) for c in EGA]

def load_bin(name):
    d = open(os.path.join(MD, 'res/bin', name + '.bin'), 'rb').read()
    pix = [[0]*320 for _ in range(200)]
    for bx in range(40):
        for y in range(200):
            o = (bx*200 + y)*4
            v = int.from_bytes(d[o:o+4], 'big')
            row = pix[y]
            for x in range(8):
                row[bx*8+x] = (v >> (28-4*x)) & 15
    return pix

def pic(num):
    tab = re.findall(r'\{(\d+),(\d+),(\w+)\}', D['pic_table'])
    w, h, name = tab[num]
    w = int(w); h = int(h)
    longs = D[name]
    pix = [[0]*(w*8) for _ in range(h)]
    for bx in range(w):
        for y in range(h):
            v = longs[bx*h+y]
            for x in range(8):
                pix[y][bx*8+x] = (v >> (28-4*x)) & 15
    return pix

def save_png(pix, fn, scale=1, pal=EGA):
    h=len(pix); w=len(pix[0])
    im=Image.new('RGB',(w,h)); im.putdata([pal[p] for r in pix for p in r])
    if scale!=1: im=im.resize((w*scale,h*scale),Image.NEAREST)
    im.save(fn)

def glyph4x8(ch):
    g = D['font4x8'][ch*8:ch*8+8]
    return g  # 8 rows, bit3 = leftmost

def recover_text_screen(pix):
    """320x200 rendered 80x25 text screen -> list of (char, attr) per cell"""
    gl = [tuple(glyph4x8(c)) for c in range(256)]
    lookup = {}
    for c in list(range(32,127)) + list(range(127,256)) + list(range(0,32)):
        lookup.setdefault(gl[c], c)
    out = []
    for cy in range(25):
        for cx in range(80):
            cell = [pix[cy*8+r][cx*4:cx*4+4] for r in range(8)]
            cols = set(v for row in cell for v in row)
            best = None
            for bg in sorted(cols):
                for fg in sorted(cols):
                    if fg == bg and len(cols) > 1: continue
                    m = tuple(sum((8>>i) for i in range(4) if row[i]==fg and fg!=bg) for row in cell)
                    if all(all((row[i]==fg) if (m[r]&(8>>i)) else (row[i]==bg) for i in range(4)) for r,row in enumerate(cell)):
                        if m in lookup:
                            best = (lookup[m], fg | (bg<<4)); break
                if best: break
            if best is None:
                raise ValueError('cell %d,%d unmatched' % (cx, cy))
            out.append(best)
    return out
