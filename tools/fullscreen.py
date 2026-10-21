"""320x200 EGA screens -> 256x192 SMS tile screens.

Scaling: the SMS pixel on a 4:3 TV is ~1.25x wider than an EGA 320-mode
pixel, so 320->256 horizontally and 200->192 vertically keeps the picture's
on-screen proportions.  One column in every five and one row in every 25
is dropped, chosen per group so the removed line carries the least unique
detail (thin strokes survive).

Tiles: duplicates are merged (including H/V flips); if the screen still
needs more than the available pattern slots, the most similar tiles are
merged (perceptual RGB error, weighted by use count)."""
import numpy as np
from common import EGA

KMEANS_IT = 0
STRUCT_W = 1.0
STRUCT_W2 = 1.0
EGA_NP = np.array(EGA, dtype=np.int32)


def _col_cost(a, b):
    return int(np.count_nonzero(a != b))


def _line_costs(p):
    """cost of removing each column of p (h x w)"""
    h, w = p.shape
    costs = np.zeros(w)
    for c in range(w):
        cs = []
        if c > 0: cs.append(np.count_nonzero(p[:, c] != p[:, c - 1]))
        if c < w - 1: cs.append(np.count_nonzero(p[:, c] != p[:, c + 1]))
        costs[c] = min(cs)
    return costs


def _choose(costs, k, gmin, gmax):
    """choose k indices to drop, consecutive gaps in [gmin,gmax], first < gmax,
    last >= n-gmax; minimise total cost (+ tiny regularity term)."""
    n = len(costs)
    INF = 1e18
    # dp[j][i]: min cost choosing j+1 drops with the last at i
    dp = np.full((k, n), INF)
    back = np.zeros((k, n), np.int32)
    ideal = n / k
    for i in range(min(gmax, n)):
        dp[0][i] = costs[i] * 16 + abs(i - ideal / 2) * 0.01
    for j in range(1, k):
        prev = dp[j - 1]
        for i in range(n):
            lo, hi = i - gmax, i - gmin
            if hi < 0: continue
            lo = max(lo, 0)
            seg = prev[lo:hi + 1]
            a = int(np.argmin(seg))
            if seg[a] >= INF: continue
            reg = abs(i - (j + 0.5) * ideal) * 0.01
            dp[j][i] = seg[a] + costs[i] * 16 + reg
            back[j][i] = lo + a
    last = dp[k - 1][max(0, n - gmax):]
    i = max(0, n - gmax) + int(np.argmin(last))
    out = [i]
    for j in range(k - 1, 0, -1):
        i = back[j][i]
        out.append(i)
    return sorted(out)


def _bands(p, min_h=1):
    """split rows into bands separated by uniform rows"""
    h = p.shape[0]
    uni = [len(np.unique(p[r])) == 1 for r in range(h)]
    bands = []
    s = None
    for r in range(h + 1):
        if r < h and not uni[r]:
            if s is None: s = r
        else:
            if s is not None:
                bands.append((s, r)); s = None
    return bands, uni


def scale_screen(pix, out_w=256, out_h=192, return_rows=False):
    p = np.array(pix, dtype=np.uint8)
    h, w = p.shape
    # rows first (global): drop rows that duplicate a neighbour
    k = h - out_h
    keep_rows = list(range(h))
    if k:
        rc = _line_costs(p.T)
        drop = set(_choose(rc, k, h // k - 8, h // k + 8))
        keep_rows = [r for r in range(h) if r not in drop]
        p = p[keep_rows, :]
    h = p.shape[0]
    k = w - out_w
    out = np.zeros((h, out_w), np.uint8)
    bands, uni = _bands(p)
    for r in range(h):
        if uni[r]:
            out[r, :] = p[r, 0]
    for (a, b) in bands:
        seg = p[a:b]
        cc = _line_costs(seg)
        drop = set(_choose(cc, k, 3, 7))
        keep = [c for c in range(w) if c not in drop]
        out[a:b] = seg[:, keep]
    if return_rows:
        return out, keep_rows
    return out


def tile_variants(t):
    """t: 8x8 array. yields (key, flags) for the 4 flip variants"""
    yield t.tobytes(), 0
    yield t[:, ::-1].tobytes(), 0x200      # H flip
    yield t[::-1, :].tobytes(), 0x400      # V flip
    yield t[::-1, ::-1].tobytes(), 0x600


def quantize_tiles(p, limit, allow_flip=True, protect=20.0, protect_rects=()):
    """p: HxW index image (multiple of 8).  Returns (tiles list of 8x8 arrays,
    map: list of rows of (tileindex|flags))."""
    H, W = p.shape
    th, tw = H // 8, W // 8
    cells = [[p[y*8:y*8+8, x*8:x*8+8].copy() for x in range(tw)] for y in range(th)]
    # exact dedupe with flips
    uniq = []          # arrays
    counts = []
    key2 = {}
    cmap = [[None]*tw for _ in range(th)]
    prot = set()
    for y in range(th):
        for x in range(tw):
            t = cells[y][x]
            hit = None
            if allow_flip:
                for k, fl in tile_variants(t):
                    if k in key2:
                        hit = (key2[k], fl); break
            else:
                k = t.tobytes()
                if k in key2: hit = (key2[k], 0)
            if hit is None:
                key2[t.tobytes()] = len(uniq)
                uniq.append(t)
                counts.append(0)
                hit = (len(uniq) - 1, 0)
            counts[hit[0]] += 1
            cmap[y][x] = hit
            for (rx0, ry0, rx1, ry1) in protect_rects:
                if rx0 <= x*8 < rx1 and ry0 <= y*8 < ry1:
                    prot.add(hit[0])
    n = len(uniq)
    if n > limit:
        rgb = np.stack([EGA_NP[u].reshape(-1) for u in uniq]).astype(np.int64)  # n x 192
        # pairwise distance (no flips for the lossy stage)
        sq = (rgb * rgb).sum(1)
        dist = (sq[:, None] + sq[None, :] - 2 * rgb @ rgb.T).astype(np.float64)
        # structure term: penalise merging tiles of different 'busyness'
        lum = (rgb.reshape(n, 64, 3) * np.array([3, 6, 1])).sum(2) / 10.0
        sd = lum.std(1)
        edge = np.abs(np.diff(lum.reshape(n, 8, 8), axis=1)).sum((1, 2)) + np.abs(np.diff(lum.reshape(n, 8, 8), axis=2)).sum((1, 2))
        dist = dist + STRUCT_W * 64 * (sd[:, None] - sd[None, :]) ** 2 + STRUCT_W2 * (edge[:, None] - edge[None, :]) ** 2 / 64.0
        alive = np.ones(n, bool)
        cnt = np.array(counts, np.float64)
        imp = np.array([(protect if len(np.unique(u)) <= 2 else 1.0) * (50.0 if k in prot else 1.0) for k, u in enumerate(uniq)])
        cnt = cnt * imp
        target = list(range(n))
        BIG = 1e30
        np.fill_diagonal(dist, BIG)
        while alive.sum() > limit:
            w = np.minimum(cnt[:, None], cnt[None, :])
            cost = dist * w
            cost[~alive, :] = BIG
            cost[:, ~alive] = BIG
            i, j = np.unravel_index(np.argmin(cost), cost.shape)
            # keep the more used one
            if cnt[i] < cnt[j]:
                i, j = j, i
            alive[j] = False
            cnt[i] += cnt[j]
            target[j] = i
            dist[j, :] = BIG
            dist[:, j] = BIG
        def resolve(k):
            while target[k] != k:
                k = target[k]
            return k
        centers = [uniq[k].copy() for k in range(n) if alive[k]]
        U = np.stack([EGA_NP[u] for u in uniq]).astype(np.float64)   # n x 8 x 8 x 3
        ucnt = np.array(counts, np.float64) * imp
        assign = np.array([0]*n)
        cid = {k: i for i, k in enumerate([k for k in range(n) if alive[k]])}
        for k in range(n):
            assign[k] = cid[resolve(k)]
        for it in range(KMEANS_IT):
            # recompute centres: per pixel the palette colour with least weighted error
            for c in range(len(centers)):
                mem = np.nonzero(assign == c)[0]
                if len(mem) == 0:
                    continue
                X = U[mem]                       # m x 8 x 8 x 3
                wts = ucnt[mem][:, None, None, None]
                # error for each palette colour: sum_m w * |X - pal|^2
                err = ((X[None] - EGA_NP[:, None, None, None, :]) ** 2 * wts[None]).sum(axis=(1, 4))  # 16 x 8 x 8
                centers[c] = err.argmin(0).astype(np.uint8)
            C = np.stack([EGA_NP[c] for c in centers]).reshape(len(centers), -1).astype(np.float64)
            Uf = U.reshape(n, -1)
            d = (Uf * Uf).sum(1)[:, None] + (C * C).sum(1)[None, :] - 2 * Uf @ C.T
            na = d.argmin(1)
            if (na == assign).all():
                break
            assign = na
        tiles = centers
        for y in range(th):
            for x in range(tw):
                k, fl = cmap[y][x]
                c = assign[k]
                best = None
                for kk, f in tile_variants(tiles[c]):
                    v = np.frombuffer(kk, np.uint8).reshape(8, 8)
                    e = int(((EGA_NP[v] - EGA_NP[cells[y][x]]) ** 2).sum())
                    if best is None or e < best[0]:
                        best = (e, f)
                    if not allow_flip:
                        break
                cmap[y][x] = (int(c), best[1])
        return tiles, cmap
    return uniq, cmap


def tile_to_planar(t):
    out = bytearray()
    for row in t:
        for b in range(4):
            v = 0
            for x in range(8):
                if row[x] >> b & 1:
                    v |= 0x80 >> x
            out.append(v)
    return bytes(out)


def render_tiles(tiles, cmap):
    th = len(cmap); tw = len(cmap[0])
    out = np.zeros((th*8, tw*8), np.uint8)
    for y in range(th):
        for x in range(tw):
            i, fl = cmap[y][x]
            t = tiles[i]
            if fl & 0x200: t = t[:, ::-1]
            if fl & 0x400: t = t[::-1, :]
            out[y*8:y*8+8, x*8:x*8+8] = t
    return out
