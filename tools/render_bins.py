import os, sys
from PIL import Image
EGA = [(0,0,0),(0,0,170),(0,170,0),(0,170,170),(170,0,0),(170,0,170),(170,85,0),(170,170,170),(85,85,85),(85,85,255),(85,255,85),(85,255,255),(255,85,85),(255,85,85+170),(255,255,85),(255,255,255)]
EGA[13]=(255,85,255)
BIN = os.path.expanduser('~/catacomb-abbyss-md/res/bin')
def load(name):
    d = open(os.path.join(BIN, name + '.bin'), 'rb').read()
    pix = [[0]*320 for _ in range(200)]
    for bx in range(40):
        for y in range(200):
            o = (bx*200 + y)*4
            v = int.from_bytes(d[o:o+4], 'big')
            for x in range(8):
                pix[y][bx*8+x] = (v >> (28-4*x)) & 15
    return pix
def save(pix, fn):
    h=len(pix); w=len(pix[0])
    im=Image.new('RGB',(w,h)); im.putdata([EGA[p] for r in pix for p in r]); im.save(fn)
if __name__=='__main__':
    os.makedirs('/tmp/bins',exist_ok=True)
    for f in sorted(os.listdir(BIN)):
        n=f[:-4]; save(load(n), '/tmp/bins/%s.png'%n)
