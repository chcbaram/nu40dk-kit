#!/bin/zsh
# 보드 화면 버퍼를 PNG 로: tools/rpg/shot.sh out.png
OUT=${1:-shot.png}
M=$(baram-ctl --json status | python3 -c 'import sys,json;print(json.load(sys.stdin)["mark"])')
baram-ctl send "ui shot" --timeout 1 >/dev/null 2>&1
baram-ctl wait --since $M --until 'shot end' --timeout 30 >/dev/null 2>&1
baram-ctl read --since $M 2>/dev/null | python3 -c "
import sys
from PIL import Image
im = Image.new('1', (400, 300), 1)
px = im.load()
for l in sys.stdin:
    l = l.strip()
    if not (l.startswith('S') and len(l) == 105): continue
    y = int(l[1:4]); d = bytes.fromhex(l[5:])
    for i, b in enumerate(d):
        for k in range(8):
            px[i * 8 + k, y] = (b >> (7 - k)) & 1
im.resize((800, 600)).save('$OUT')
print('saved $OUT')
"
