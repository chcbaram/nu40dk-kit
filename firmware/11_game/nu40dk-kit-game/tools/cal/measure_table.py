# gb caltable 화면 사진 → 출발 색 x 목표 색 정규화 밝기와 목표별 차이. 사용: python3 measure_table.py photo.jpg [모서리 대략 위치]
import sys, numpy as np
from PIL import Image

img = np.asarray(Image.open(sys.argv[1]).convert('L'), dtype=np.float32)
H, W = img.shape

# 1) 모서리 표시 사각형 찾기: 대략 위치(첫 사진 기준) 주변에서 어두운 점의 무게중심
guess = {'tl': (519, 188), 'tr': (1400, 193), 'bl': (541, 855), 'br': (1378, 855)}
if len(sys.argv) > 2:
    v = [int(x) for x in sys.argv[2].split(',')]
    guess = {'tl': (v[0], v[1]), 'tr': (v[2], v[3]), 'bl': (v[4], v[5]), 'br': (v[6], v[7])}
cent = {}
for k, (gx, gy) in guess.items():
    r = 40
    win = img[gy - r:gy + r, gx - r:gx + r]
    th = win.min() + (np.median(win) - win.min()) * 0.5
    ys, xs = np.nonzero(win < th)
    cent[k] = (gx - r + xs.mean(), gy - r + ys.mean())
# 패널 좌표의 사각형 중심
src = {'tl': (5, 5), 'tr': (395, 5), 'bl': (5, 295), 'br': (395, 295)}

# 2) 호모그래피 (패널 → 사진)
A = []
for k in src:
    x, y = src[k]; u, v = cent[k]
    A.append([x, y, 1, 0, 0, 0, -u * x, -u * y, -u])
    A.append([0, 0, 0, x, y, 1, -v * x, -v * y, -v])
_, _, Vt = np.linalg.svd(np.array(A))
Hm = Vt[-1].reshape(3, 3)

def to_img(x, y):
    p = Hm @ np.array([x, y, 1.0])
    return p[0] / p[2], p[1] / p[2]

def region_mean(x0, y0, x1, y1, inset=0.25):
    # 패널 사각형 안쪽(가장자리 inset 비율 제외)을 촘촘히 샘플
    xs = np.linspace(x0 + (x1 - x0) * inset, x1 - (x1 - x0) * inset, 12)
    ys = np.linspace(y0 + (y1 - y0) * inset, y1 - (y1 - y0) * inset, 12)
    vals = []
    for yy in ys:
        for xx in xs:
            u, v = to_img(xx, yy)
            vals.append(img[int(round(v)), int(round(u))])
    return float(np.median(vals))

black  = region_mean(16, 20, 192, 70)
wblack = region_mean(16, 74, 192, 86, inset=0.1)
rb     = black / wblack
names = "WLDK"
res = []
print("from\\to   W     L     D     K")
for r in range(4):
    y = 90 + r * 38
    row = []
    for c in range(4):
        x  = 16 + c * 64
        v  = region_mean(x, y, x + 48, y + 24)
        wl = region_mean(x, y - 11, x + 48, y - 3, inset=0.1)
        row.append(((v / wl) - rb) / (1 - rb))
    res.append(row)
    print(f"  {names[r]}    " + ' '.join(f'{v:5.2f}' for v in row))
for c in range(4):
    col = [res[r][c] for r in range(4) if r != c]
    print(f"to {names[c]}: spread {max(col)-min(col):.2f}  mean {sum(col)/len(col):.2f}")
