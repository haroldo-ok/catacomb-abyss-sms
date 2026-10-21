"""Renderer data for the SMS 3-D view.

View: 224x80 pixels = 28x10 tiles, 56 rays of 4 pixels.  Projection keeps
the original field of view (scale 145 over 320 pixels -> 101.5 over 224)
and wall heights scaled by 80/120 (452/dist EGA pixels -> 301.3/dist).

Outputs (raw bank images, generated machine code where needed):
  RD bank (mapped at slot 1, 0x4000): plane tables, trig tables, column
     tables, sprite start tables, hand overlay, wall picture tables
  scaler banks (slot 1): compiled wall scalers, position independent
  texture banks (slot 2): 64x64 walls, byte per texel (c * 0x11),
     column major, 4 per bank
  shape banks (slot 2): scaled shape column lists
"""
import math, re, os
import numpy as np
from common import D, MD

VIEW_W, VIEW_H = 224, 80
NRAYS = 56
CY = 40
MAXS = 110
KV_HALF = 452.0 * VIEW_H / 120.0 / 2.0   # half height at distance 1 tile
# horizontal focal length in pixels.  The original projects x and y with the
# same factor (TransformActor: viewx = ny/ratio, viewheight = 1/ratio, focal
# 65536/145 = 452 px for 120 view lines), so pixels are 'square' in projection
# space: a wall tile is as many pixels wide as it is high at a given distance
# and sprites (2s wide) match walls.  Horizontal FOV = 2 atan(112/301) = 41
# degrees (original 39).
KH = 2.0 * KV_HALF                        # 301.3
FINE = 3600

FIRSTWALLPIC = 192
NUMFLOORS = 71

# ------------------------------------------------------------------ enums
def gfx_enum():
    txt = open(os.path.join(MD, 'src/gfxe_abs.h')).read()
    body = txt[txt.index('{') + 1:]
    names = {}
    v = 0
    for line in body.split('\n'):
        line = re.sub(r'//.*', '', line).strip().rstrip(',')
        if not line or line.startswith('}'):
            if line.startswith('}'): break
            continue
        for part in line.split(','):
            part = part.strip()
            if not part: continue
            if '=' in part:
                n, val = part.split('=')
                v = int(val.strip(), 0)
                names[n.strip()] = v
            else:
                names[part] = v
            v += 1
    return names


def wall_tables():
    names = gfx_enum()
    src = open(os.path.join(MD, 'src/render.c')).read()
    out = {}
    for t in ('walllight1', 'walldark1', 'walllight2', 'walldark2'):
        m = re.search(r'%s\[NUMFLOORS\]\s*=\s*\{(.*?)\};' % t, src, re.S)
        items = [x.strip() for x in m.group(1).replace('\n', ' ').split(',') if x.strip()]
        vals = [int(x) if x.isdigit() else names[x] for x in items]
        vals += [0] * (NUMFLOORS - len(vals))
        out[t] = vals
    return out

# --------------------------------------------------------------- tables
def texv(c):
    """texel byte: colour c (0..15, or c*0x11) -> 0x80 + 4c.  As the high
    byte it selects the plane table page of a left pixel, as the low byte the
    entry of a right pixel (see plane_bank)."""
    return 0x80 + 4 * (c & 15)


def plane_bank():
    """slot 2 table: [(0x80+4L+b) << 8 | (0x80+4R)] = bitplane b of LLLLRRRR"""
    out = bytearray(0x4000)
    for L in range(16):
        for b in range(4):
            for R in range(16):
                v = (0xF0 if (L >> b) & 1 else 0) | (0x0F if (R >> b) & 1 else 0)
                out[(4 * L + b) * 256 + 0x80 + 4 * R] = v
    return bytes(out)


def plane_tables():
    out = bytearray()
    for b in range(4):
        for idx in range(256):
            L, R = idx >> 4, idx & 15
            v = (0xF0 if (L >> b) & 1 else 0) | (0x0F if (R >> b) & 1 else 0)
            out.append(v)
    return bytes(out)


def column_tables():
    fine, colk, colmin = [], [], []
    for c in range(NRAYS):
        t = ((4 * c + 2) - VIEW_W / 2.0) / KH
        a = -math.atan(t)
        fine.append(int(round(a * FINE / (2 * math.pi))))
        cs = math.cos(a)
        colk.append(min(65535, int(round(KV_HALF * 256 / cs))))
        colmin.append(int(math.ceil(256 / cs)))
    return fine, colk, colmin


def trig_quarter():
    """for fine angle 0..900: cos (0.16, 65535 max), 256/cos (8.8, saturated)"""
    cosq, invq = [], []
    for a in range(901):
        c = math.cos(a * 2 * math.pi / FINE)
        cosq.append(min(65535, int(round(c * 65535))))
        invq.append(min(65535, int(256.0 / c)) if c > 1e-9 else 65535)
    return cosq, invq


def row_range(s):
    return max(0, CY - s), min(VIEW_H, CY + s)


def texel_of_row(s, y):
    return ((y - (CY - s)) * 64) // (2 * s)


def sprite_starts():
    out = bytearray()
    for s in range(MAXS + 1):
        ss = max(s, 1)
        for src in range(65):
            r = (CY - ss) + (src * 2 * ss) // 64
            out.append(max(0, min(VIEW_H, r)))
    return bytes(out)


def sprite_stepinv():
    return [0] + [int(32 * 256 / s) for s in range(1, MAXS + 1)]

# --------------------------------------------------------------- scalers
def advance_l(k):
    """add k to L (texture column pointer, never crosses its 64 byte column)"""
    if k <= 0:
        return b''
    if k <= 3:
        return b'\x2C' * k                      # inc l
    return bytes([0x7D, 0xC6, k, 0x6F])          # ld a,l ; add a,k ; ld l,a


def scaler_code(s):
    """HL = texture column (64 byte aligned), DE = colbuf page + y0"""
    y0, y1 = row_range(s)
    code = bytearray()
    last, cur = None, 0
    for y in range(y0, y1):
        r = texel_of_row(s, y)
        if r != last:
            code += advance_l(r - cur)
            cur = r
            code += b'\x7E'                     # ld a,(hl)
            last = r
        code += b'\x12\x1C'                     # ld (de),a ; inc e
    code.append(0xC9)
    return bytes(code)


def sprite_starts_s(s):
    ss = max(s, 1)
    return [max(0, min(VIEW_H, (CY - ss) + (src * 2 * ss) // 64)) for src in range(65)]


def sprite_scaler(s, base):
    """256 aligned block at `base`: [0..63] first row of texel r,
    [64..127] entry lo, [128..191] entry hi, code from 192.
    Entry for texel r: HL = pixels, DE = colbuf page + starts[r], B = count.
    Each texel block: ld a,(hl) ; inc hl ; (ld (de),a ; inc e) * rows ; dec b ; ret z"""
    st = sprite_starts_s(s)
    code = bytearray()
    entries = []
    for r in range(64):
        entries.append(base + 192 + len(code))
        k = st[r + 1] - st[r]
        if k:
            code += b'\x7E\x23' + b'\x12\x1C' * k
        else:
            code += b'\x23'
        code += b'\x05\xC8'
    code.append(0xC9)
    blk = bytearray(st[:64]) + bytes(e & 0xFF for e in entries) + bytes(e >> 8 for e in entries)
    return bytes(blk) + bytes(code)


def build_sprite_scalers():
    banks = [bytearray()]
    table = [(0, 0x4000)]
    banks[-1] += sprite_scaler(0, 0x4000)
    for s in range(1, MAXS + 1):
        b = banks[-1]
        b += bytes((-len(b)) % 256)
        c = sprite_scaler(s, 0x4000 + len(b))
        if len(b) + len(c) > 0x4000:
            banks.append(bytearray())
            b = banks[-1]
            c = sprite_scaler(s, 0x4000)
        table.append((len(banks) - 1, 0x4000 + len(b)))
        b += c
    return banks, table


def build_scalers():
    """-> list of bank images, table of (bank_index, addr) per s"""
    banks = [bytearray()]
    table = [(0, 0x4000)]
    banks[-1] += b'\xC9'       # s = 0: just ret
    for s in range(1, MAXS + 1):
        c = scaler_code(s)
        if len(banks[-1]) + len(c) > 0x4000:
            banks.append(bytearray())
        table.append((len(banks) - 1, 0x4000 + len(banks[-1])))
        banks[-1] += c
    return banks, table


# --------------------------------------------------------------- textures
def wall_textures():
    wt = D['wall_tex']
    n = len(wt) // 8192
    walls = []
    for w in range(n):
        base = w * 8192
        walls.append(bytes(texv(wt[base + 2 * i]) for i in range(4096)))
    return walls

# --------------------------------------------------------------- shapes
def shapes():
    blob = D['shape_blob']
    offs = D['shape_offsets']
    out = []
    for i, o in enumerate(offs):
        if o == 0xFFFFFFFF:
            out.append(None)
            continue
        width = (blob[o] << 8) | blob[o + 1]
        # parse to find the end
        cols = []
        for x in range(width):
            co = (blob[o + 2 + 2 * x] << 8) | blob[o + 3 + 2 * x]
            p = o + co
            segs = []
            while blob[p] != 0xFF:
                top, cnt = blob[p], blob[p + 1]
                segs.append((top, cnt, bytes(blob[p + 2:p + 2 + cnt])))
                p += 2 + cnt
            cols.append(segs)
        # SMS format: u8 width, u16 col offsets (LE, relative), segments:
        # top, count, pixels (c*0x11) ; 0xFF end
        data = bytearray([width])
        data += bytes(2 * width)
        for x, segs in enumerate(cols):
            off = len(data)
            data[1 + 2 * x] = off & 0xFF
            data[2 + 2 * x] = off >> 8
            for top, cnt, px in segs:
                data += bytes([top, cnt]) + bytes(texv(v) for v in px)
            data.append(0xFF)
        out.append(bytes(data))
    return out

# --------------------------------------------------------------- hand
HAND_X = 84              # view pixel of the hand's left edge
HAND_TC0 = HAND_X // 8   # first tile column
HAND_W, HAND_H = 62, 48
HAND_TCOLS = (HAND_X + HAND_W + 7) // 8 - HAND_TC0


def hand_pixels():
    """88x72 masked pic -> rows of indices (16 = transparent)"""
    longs = D['hand_data']
    W, H = 11, 72
    pix = [[16] * 88 for _ in range(H)]
    for c in range(W):
        for r in range(H):
            m = longs[(c * H + r) * 2]
            d = longs[(c * H + r) * 2 + 1]
            for x in range(8):
                sh = 28 - 4 * x
                if ((m >> sh) & 15) == 0:
                    pix[r][c * 8 + x] = (d >> sh) & 15
    return pix


def hand_overlay():
    from fullscreen import _line_costs, _choose
    p = np.array(hand_pixels(), dtype=np.uint8)
    h, w = p.shape
    drop = set(_choose(_line_costs(p.T), h - HAND_H, 2, 4))
    p = p[[r for r in range(h) if r not in drop], :]
    drop = set(_choose(_line_costs(p), w - HAND_W, 2, 6))
    p = p[:, [c for c in range(w) if c not in drop]]
    assert p.shape == (HAND_H, HAND_W)
    out = bytearray()
    for tc in range(HAND_TCOLS):
        for r in range(HAND_H):
            mask = 0
            planes = [0, 0, 0, 0]
            for bit in range(8):
                x = (HAND_TC0 + tc) * 8 + bit - HAND_X
                v = p[r][x] if 0 <= x < HAND_W else 16
                if v == 16:
                    mask |= 0x80 >> bit
                else:
                    for b in range(4):
                        if (v >> b) & 1:
                            planes[b] |= 0x80 >> bit
            out += bytes([mask] + planes)
    return bytes(out), p



class OutEmitter:
    """machine code with VRAM pacing: an OUT starts >= 27 cycles after the
    previous OUT started (active display safe)"""
    GAP = 27

    def __init__(self):
        self.code = bytearray()
        self.since = 100

    def op(self, b, cyc):
        self.code += b
        self.since += cyc

    def out(self):
        while self.since < self.GAP:
            self.op(b'\x00', 4)                 # nop
        self.code += b'\xED\x79'               # out (c),a
        self.since = 12


def hand_code(ov):
    """compiled hand overlay rows.  Bank image for slot 1: two entry tables
    (HAND_TCOLS * HAND_H words each) then code.  Entry (tc, r): HL = colbuf L
    page + screen row, C = 0xBE, B = row count, plane bank in slot 2.
    Rows: transparent -> plane lookups, opaque -> immediates,
    mixed -> (lookup & mask) | hand.
    Table 2 (hand fully raised, VRAM already holding it): tiles the hand
    covers completely are skipped (VRAM address set to the next tile)."""
    ntab = HAND_TCOLS * HAND_H * 2
    em = OutEmitter()
    entries = []
    base = 0x4000 + ntab

    def row(tc, r):
        o = (tc * HAND_H + r) * 5
        mask, pl = ov[o], ov[o + 1:o + 5]
        if mask == 0xFF:
            em.op(b'\x56', 7); em.op(b'\x24', 4); em.op(b'\x5E', 7)   # ld d,(hl); inc h; ld e,(hl)
            em.op(b'\x25', 4)                                       # dec h
            for b in range(4):
                if b:
                    em.op(b'\x14', 4)                               # inc d
                if b == 1:
                    em.op(b'\x2C', 4)                               # inc l
                em.op(b'\x1A', 7)                                   # ld a,(de)
                em.out()
        elif mask == 0:
            em.op(b'\x2C', 4)                                       # inc l
            for b in range(4):
                em.op(bytes([0x3E, pl[b]]), 7)                      # ld a,#p
                em.out()
        else:
            em.op(b'\x56', 7); em.op(b'\x24', 4); em.op(b'\x5E', 7)
            em.op(b'\x25', 4); em.op(b'\x2C', 4)
            for b in range(4):
                if b:
                    em.op(b'\x14', 4)
                em.op(b'\x1A', 7)
                em.op(bytes([0xE6, mask]), 7)                       # and #m
                em.op(bytes([0xF6, pl[b]]), 7)                      # or #p
                em.out()
        em.op(b'\x05', 4); em.op(b'\xC8', 5)                      # dec b ; ret z

    hy0 = VIEW_H - HAND_H
    assert hy0 % 8 == 0
    for tc in range(HAND_TCOLS):
        em.since = 100
        fix = []                     # (operand offset, row) for jp nz,skip
        rowaddr = {}
        for r in range(HAND_H):
            entries.append(base + len(em.code))
            if r % 8 == 0 and all(ov[(tc * HAND_H + r + j) * 5] == 0 for j in range(8)):
                # B' = 1: hand fully raised and already in VRAM -> skip tile
                em.op(b'\xD9\x78\xD9\xB7', 16)             # exx; ld a,b; exx; or a
                em.op(b'\xC2\x00\x00', 10)                 # jp nz,skip
                fix.append((len(em.code) - 2, r))
            row(tc, r)
            rowaddr[r + 1] = base + len(em.code)
        em.op(b'\xC9', 10)
        for off, r in fix:
            sk = base + len(em.code)
            em.code[off] = sk & 0xFF
            em.code[off + 1] = sk >> 8
            tile = (HAND_TC0 + tc) * 10 + (hy0 + r) // 8 + 1
            addr = tile * 32
            em.op(b'\x7D\xC6\x08\x6F', 15)               # ld a,l; add a,#8; ld l,a
            em.op(bytes([0x3E, addr & 0xFF]), 7)           # ld a,#lo
            em.op(b'\xF3', 4)                              # di
            em.op(b'\xD3\xBF', 11)                         # out (0xBF),a
            em.op(bytes([0x3E, (addr >> 8) | 0x40]), 7)    # ld a,#hi
            em.op(b'\xD3\xBF', 11)                         # out (0xBF),a
            em.op(b'\xFB', 4)                              # ei
            em.op(b'\x78\xD6\x08\x47', 15)               # ld a,b; sub #8; ld b,a
            em.op(b'\xC8', 5)                              # ret z
            nxt = rowaddr[r + 8]
            em.op(bytes([0xC3, nxt & 0xFF, nxt >> 8]), 10)  # jp row r + 8
    tab = bytearray()
    for e in entries:
        tab += bytes([e & 0xFF, e >> 8])
    img = bytes(tab) + bytes(em.code)
    assert len(img) <= 0x4000, len(img)
    return img


def u16le(vals):
    out = bytearray()
    for v in vals:
        v &= 0xFFFF
        out += bytes([v & 0xFF, v >> 8])
    return bytes(out)


def build(pk, H, C):
    """pack all renderer data; append header (H) and bank-0 table (C) lines"""
    # textures: 4 per bank, consecutive banks
    walls = wall_textures()
    tex = b''.join(walls)
    tbanks = pk.add_split('walltex', tex, 0x4000)
    assert tbanks == list(range(tbanks[0], tbanks[0] + len(tbanks)))
    H.append('#define TEX_BANK0 %d' % tbanks[0])
    H.append('#define NUM_WALLTEX %d' % len(walls))
    # scalers
    sbanks, stab = build_scalers()
    sb = [pk.add_raw_bank('scalers%d' % i, b) for i, b in enumerate(sbanks)]
    ssbanks, sstab = build_sprite_scalers()
    ssb = [pk.add_raw_bank('sprscalers%d' % i, b) for i, b in enumerate(ssbanks)]
    plb = pk.add_raw_bank('planes', plane_bank())
    H.append('#define PLANE_BANK %d' % plb)
    ov, _ = hand_overlay()
    hb = pk.add_raw_bank('handcode', hand_code(ov))
    H.append('#define HAND_BANK %d' % hb)
    # casting tables (ROM for speed): s = min(MAXS, COLK[c] / dist) for
    # dist 256..16383, one bank per ray column; side distances
    # mulfrac8(INVQ[a], f) for a = 0..900, f = 0..255 (32 angles per bank)
    fine_, colk_, colmin_ = column_tables()
    divb = []
    for c in range(NRAYS):
        k = colk_[c]
        assert (k >> 8) < 256
        divb.append(pk.add_raw_bank('divtab%d' % c,
                    bytes(min(MAXS, k // d) for d in range(256, 16384))))
    assert divb == list(range(divb[0], divb[0] + NRAYS))
    H.append('#define DIV_BANK0 %d' % divb[0])
    _, invq_ = trig_quarter()
    sideb = []
    for a0 in range(0, 901, 32):
        buf = bytearray()
        for a in range(a0, min(a0 + 32, 901)):
            dl = invq_[a]
            for f in range(256):
                if dl == 0xFFFF:
                    v = 0xFFFF if f else 0
                else:
                    v = ((dl >> 8) * f + (((dl & 0xFF) * f) >> 8)) & 0xFFFF
                buf += bytes((v & 0xFF, v >> 8))
        sideb.append(pk.add_raw_bank('sidetab%d' % (a0 // 32), buf))
    assert sideb == list(range(sideb[0], sideb[0] + len(sideb)))
    H.append('#define SIDE_BANK0 %d' % sideb[0])
    # shapes
    shp = shapes()
    sh_bank, sh_addr = [], []
    for i, d in enumerate(shp):
        if d is None:
            sh_bank.append(0); sh_addr.append(0)
        else:
            b, a = pk.add('shape%d' % i, d)
            sh_bank.append(b); sh_addr.append(a)
    C.append('const u8 shape_width[%d] = {%s};' % (len(shp), ','.join(str(d[0] if d else 0) for d in shp)))
    H.append('extern const u8 shape_width[%d];   /* source columns, 0 = no shape */' % len(shp))
    # RD bank: tables used by casting and column drawing
    rd = bytearray()
    syms = []
    def put(buf, name, data, align=1):
        while len(buf) % align:
            buf.append(0)
        syms.append((name, 0x4000 + len(buf)))
        buf.extend(data)
    sq = [i * i // 4 for i in range(512)]
    put(rd, 'RD_QSQ_LO', bytes(v & 0xFF for v in sq), 256)
    put(rd, 'RD_QSQ_HI', bytes(v >> 8 for v in sq), 256)
    cosq, invq = trig_quarter()
    put(rd, 'RD_COSQ', u16le(cosq))
    put(rd, 'RD_INVQ', u16le(invq))
    fine, colk, colmin = column_tables()
    put(rd, 'RD_COLFINE', u16le(fine))
    put(rd, 'RD_COLK', u16le(colk))
    put(rd, 'RD_COLMIN', u16le(colmin))
    put(rd, 'RD_SCALER_BANK', bytes(sb[b] for b, a in stab))
    put(rd, 'RD_SCALER_ADDR', u16le([a for b, a in stab]))
    put(rd, 'RD_SSCALER_BANK', bytes(ssb[b] for b, a in sstab))
    put(rd, 'RD_SSCALER_HI', bytes(a >> 8 for b, a in sstab))
    wt = wall_tables()
    for t in ('walllight1', 'walldark1', 'walllight2', 'walldark2'):
        put(rd, 'RD_' + t.upper(), bytes((v - FIRSTWALLPIC) if v >= FIRSTWALLPIC else 0xFF
                                         for v in wt[t]))
    # RD2 bank: sprite tables
    rd2 = bytearray()
    put(rd2, 'RD2_STEPINV', u16le(sprite_stepinv()))
    put(rd2, 'RD2_SHAPE_BANK', bytes(sh_bank))
    put(rd2, 'RD2_SHAPE_ADDR', u16le(sh_addr))
    rdb = pk.add_raw_bank('renderdata', rd)
    rdb2 = pk.add_raw_bank('renderdata2', rd2)
    H.append('#define RD_BANK %d' % rdb)
    H.append('#define RD2_BANK %d' % rdb2)
    for n, a in syms:
        H.append('#define %s 0x%04X' % (n, a))
    H.append('#define MAXS %d' % MAXS)
    H.append('#define NRAYS %d' % NRAYS)
    H.append('#define VIEW_H %d' % VIEW_H)
    H.append('#define HAND_TC0 %d' % HAND_TC0)
    H.append('#define HAND_TCOLS %d' % HAND_TCOLS)
    H.append('#define HAND_ROWS %d' % HAND_H)
    H.append('#define NUMSCALEPICS %d' % len(shp))
    print('render data: RD %d RD2 %d bytes, scalers %s, sprite scalers %s, tex banks %d..%d' %
          (len(rd), len(rd2), [len(b) for b in sbanks], [len(b) for b in ssbanks], tbanks[0], tbanks[-1]))


MAP_NAMES = []

def build_maps(pk, H, C):
    defs = re.findall(r'\{(\d+),(\d+),(\w+),(\w+),"([^"]*)"\}', D['map_defs'])
    banks, addrs, names = [], [], []
    for w, h, p0, p2, name in defs:
        w, h = int(w), int(h)
        a0 = D[p0]; a2 = D[p2]
        assert len(a0) == w * h
        blob = (bytes([w, h]) + bytes(a0) + bytes(v & 0xFF for v in a2)
                + bytes((v >> 8) & 0xFF for v in a2))
        b, a = pk.add(p0, blob)
        banks.append(b); addrs.append(a); names.append(name)
    H.append('#define NUMMAPDEFS %d' % len(defs))
    H.append('extern const u8 map_bank[];')
    H.append('extern const u16 map_addr[];')
    C.append('const u8 map_bank[] = {%s};' % ','.join(map(str, banks)))
    C.append('const u16 map_addr[] = {%s};' % ','.join('0x%04X' % a for a in addrs))
    MAP_NAMES[:] = names

if __name__ == '__main__':
    banks, tables = build_scalers()
    print('scaler banks', len(banks), [len(b) for b in banks])
    print('textures', len(wall_textures()))
    sh = shapes()
    print('shapes', len(sh), sum(len(s) for s in sh if s))
    ov, p = hand_overlay()
    print('hand overlay', len(ov), HAND_TC0, HAND_TCOLS)
    from common import save_png
    save_png([[(v if v < 16 else 5) for v in r] for r in p.tolist()], '/tmp/hand.png', 4)
    wt = wall_tables()
    print(wt['walllight1'][:10])
