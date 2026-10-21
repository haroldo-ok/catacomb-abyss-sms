#!/bin/bash
# three reference views -> /tmp/abc_new.png, compared with tests/bench/ref_views.png
export PATH=$HOME/.devkitsms/bin:$PATH
cd "$(dirname "$0")/.."
smstest catacomb_abyss.sms tests/bench/views.txt >/dev/null
python3 - <<'PY'
from PIL import Image, ImageChops
import os
ims=[Image.open('/tmp/v_%s.ppm'%n).convert('RGB') for n in 'abcd']
o=Image.new('RGB',(256,192*4))
for k,i in enumerate(ims): o.paste(i,(0,k*192))
o.save('/tmp/abc_new.png')
if os.path.exists('tests/bench/ref_views.png'):
    r=Image.open('tests/bench/ref_views.png').convert('RGB')
    print('diff bbox vs ref:', ImageChops.difference(r,o).getbbox())
else:
    o.save('tests/bench/ref_views.png'); print('reference saved')
PY
