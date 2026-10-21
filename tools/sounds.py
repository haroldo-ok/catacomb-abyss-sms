"""PC speaker sound effects -> SN76489 tone streams.

The Genesis port ships the PC speaker effects rendered to PCM by
tools/convert.py (pc_render): one byte per 140 Hz tick, value s ->
square wave of 1193181/(60*s) Hz with a continuous phase, 0 = silence,
13300 Hz, +-34 around 128.  Decoding re-runs that renderer: for every tick
the divisor whose output matches the recorded samples exactly is kept.

On the SMS the stream is replayed from the 60 Hz interrupt, so the 140 Hz
ticks are resampled (for each frame the tick covering most of it, tones
win over silence) and converted to PSG periods N = 3579545/(32 f).
"""
import os, wave
import numpy as np
from common import MD

PCM_RATE = 13300
SOUND_NAMES = ['HITWALLSND', 'WARPUPSND', 'WARPDOWNSND', 'GETBOLTSND', 'GETNUKESND',
               'GETPOTIONSND', 'GETKEYSND', 'GETSCROLLSND', 'GETPOINTSSND', 'USEBOLTSND',
               'USENUKESND', 'USEPOTIONSND', 'USEKEYSND', 'NOITEMSND', 'WALK1SND', 'WALK2SND',
               'TAKEDAMAGESND', 'MONSTERMISSSND', 'GAMEOVERSND', 'SHOOTSND', 'BIGSHOOTSND',
               'SHOOTWALLSND', 'SHOOTMONSTERSND', 'TAKEDMGHURTSND', 'BALLBOUNCESND', 'NOWAYSND',
               'WARPSND', 'HIT_GATESND', 'GETGEMSND', 'BOOMSND', 'GRELM_DEADSND',
               'FREEZETIMESND', 'TIMERETURNSND', 'TICKSND']


def load(name):
    w = wave.open(os.path.join(MD, 'res/sfx', name))
    d = np.frombuffer(w.readframes(w.getnframes()), np.uint8).astype(np.int32)
    return d


def decode(pcm):
    """-> list of divisors (one per 140 Hz tick)"""
    per = PCM_RATE / 140.0
    acc = 0.0
    pos = 0
    phase = 0.0
    out = []
    cands = np.arange(1, 256)
    freqs = 1193181.0 / (cands * 60)
    while pos < len(pcm):
        acc += per
        n = int(acc)
        acc -= n
        seg = pcm[pos:pos + n]
        if len(seg) == 0:
            break
        if np.all(seg == 128):
            out.append(0)
            pos += n
            continue
        k = np.arange(1, len(seg) + 1)
        ph = phase + np.outer(freqs / PCM_RATE, k)          # 255 x n
        gen = np.where((ph % 1.0) < 0.5, 128 + 34, 128 - 34)
        ok = np.all(gen == seg[None, :], axis=1)
        idx = np.nonzero(ok)[0]
        if len(idx) == 0:
            # tail of the file may be truncated; take the closest
            err = np.sum(gen != seg[None, :], axis=1)
            idx = [int(np.argmin(err))]
        # several divisors can match a short segment: prefer the one that
        # also explains the next segment
        best = int(idx[0])
        if len(idx) > 1:
            nxt = pcm[pos + n:pos + n + int(per)]
            if len(nxt):
                scores = []
                for i in idx:
                    p2 = ph[i, -1] + np.arange(1, len(nxt) + 1) * freqs[i] / PCM_RATE
                    g2 = np.where((p2 % 1.0) < 0.5, 162, 94)
                    scores.append(np.sum(g2 == nxt))
                best = int(idx[int(np.argmax(scores))])
        s = int(cands[best])
        phase = ph[best, -1]
        out.append(s)
        pos += n
    return out


def to_frames(ticks):
    """140 Hz ticks -> 60 Hz frames of divisors"""
    nframes = int(np.ceil(len(ticks) * 60 / 140.0))
    out = []
    for f in range(nframes):
        t0 = f * 140.0 / 60
        t1 = (f + 1) * 140.0 / 60
        cover = {}
        t = t0
        while t < t1 - 1e-9:
            i = int(t)
            e = min(t1, i + 1)
            if i < len(ticks):
                cover[ticks[i]] = cover.get(ticks[i], 0) + (e - t)
            t = e
        tones = {k: v for k, v in cover.items() if k}
        if tones:
            out.append(max(tones.items(), key=lambda kv: kv[1])[0])
        else:
            out.append(0)
    while out and out[-1] == 0:
        out.pop()
    return out


def psg_period(s):
    if not s:
        return 0
    f = 1193181.0 / (60 * s)
    n = int(round(3579545.0 / (32 * f)))
    return max(1, min(1023, n))


def build_all():
    res = []
    for nm in SOUND_NAMES:
        pcm = load('pc_%s.wav' % nm.lower())
        ticks = decode(pcm)
        frames = to_frames(ticks)
        res.append((nm, ticks, [psg_period(s) for s in frames]))
    return res


def blob(res):
    """[u16 offset table][per sound: u8 nframes_lo, nframes_hi, u16 periods...]"""
    data = bytearray()
    offs = []
    base = 2 * len(res)
    for nm, ticks, per in res:
        offs.append(base + len(data))
        data += len(per).to_bytes(2, 'little')
        for p in per:
            data += p.to_bytes(2, 'little')
    out = bytearray()
    for o in offs:
        out += o.to_bytes(2, 'little')
    return bytes(out + data)


if __name__ == '__main__':
    import sys
    for nm, ticks, per in build_all():
        mx = max(ticks) if ticks else 0
        print('%-16s ticks %4d frames %4d max s %3d %s' % (nm, len(ticks), len(per), mx, ticks[:24]))
