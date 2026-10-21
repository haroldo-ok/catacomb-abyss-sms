import pickle, re
from PIL import Image
D = pickle.load(open('data.pkl','rb'))
pal = D['ega_palette']
def genc(v):
    r=(v>>1)&7; g=(v>>5)&7; b=(v>>9)&7
    return (r*255//7, g*255//7, b*255//7)
PAL = [genc(v) for v in pal]
EGA = [(0,0,0),(0,0,170),(0,170,0),(0,170,170),(170,0,0),(170,0,170),(170,85,0),(170,170,170),(85,85,85),(85,85,255),(85,255,85),(85,255,255),(255,85,85),(255,85,255),(255,255,85),(255,255,255)]
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
def save(pix, fn, scale=2):
    h=len(pix); w=len(pix[0])
    im = Image.new('RGB',(w,h))
    im.putdata([EGA[p] for row in pix for p in row])
    im = im.resize((w*scale,h*scale), Image.NEAREST)
    im.save(fn)
if __name__ == '__main__':
    import os
    os.makedirs('/tmp/pics', exist_ok=True)
    tab = re.findall(r'\{(\d+),(\d+),(\w+)\}', D['pic_table'])
    print(len(tab), tab[:6])
    for i,(w,h,n) in enumerate(tab):
        if n!='NULL':
            save(pic(i), '/tmp/pics/pic%d.png'%i)
            print(i, int(w)*8, h)
