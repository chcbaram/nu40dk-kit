# gb calcycle 화면 사진 → 고정 기준(위)과 24번 오간 패치(아래) 비교.
# 사용: python3 measure_cycle.py photo.jpg [tlx,...]   (GB 모서리 4x4 검정 위치 대략, measure_move 와 같음)
# 값: 0 = 위 줄 K 기준, 1 = 흰 바탕. 차이 = 아래 - 위 (+ 면 더 밝음, - 면 더 어두움)
import sys, numpy as np
from PIL import Image
img = np.asarray(Image.open(sys.argv[1]).convert('L'), dtype=np.float32)
g = [608, 202, 1312, 205, 625, 843, 1296, 842]
if len(sys.argv) > 2: g = [int(v) for v in sys.argv[2].split(',')]
guess = {'tl': g[0:2], 'tr': g[2:4], 'bl': g[4:6], 'br': g[6:8]}
cent = {}
for k, (gx, gy) in guess.items():
    r = 30; win = img[gy - r:gy + r, gx - r:gx + r]
    th = win.min() + (np.median(win) - win.min()) * 0.5
    ys, xs = np.nonzero(win < th); cent[k] = (gx - r + xs.mean(), gy - r + ys.mean())
src = {'tl': (2, 2), 'tr': (158, 2), 'bl': (2, 142), 'br': (158, 142)}
A = []
for k in src:
    x, y = src[k]; u, v = cent[k]
    A += [[x, y, 1, 0, 0, 0, -u * x, -u * y, -u], [0, 0, 0, x, y, 1, -v * x, -v * y, -v]]
Hm = np.linalg.svd(np.array(A))[2][-1].reshape(3, 3)
def val(x0, y0, w, h):
    vs = []
    for yy in np.linspace(y0 + h * 0.25, y0 + h * 0.75, 10):
        for xx in np.linspace(x0 + w * 0.25, x0 + w * 0.75, 10):
            p = Hm @ np.array([xx, yy, 1.0]); vs.append(img[int(round(p[1] / p[2])), int(round(p[0] / p[2]))])
    return float(np.median(vs))
white = np.median([val(8 + 38 * i, 60, 32, 18) for i in range(4)])     # 두 줄 사이 흰 바탕
kref  = val(8, 24, 32, 32)
names = "KDLW"
print(f"marks ok, white {white:.1f}, K ref {kref:.1f}")
print("      fixed  cycled  diff")
d = []
for i in range(4):
    a = (val(8 + 38 * i, 24, 32, 32) - kref) / (white - kref)
    b = (val(8 + 38 * i, 84, 32, 32) - kref) / (white - kref)
    d.append(b - a)
    print(f"  {names[i]}   {a:5.2f}   {b:5.2f}  {b - a:+.2f}")
print(f"MEAN |diff| {np.mean(np.abs(d)):.3f}")
