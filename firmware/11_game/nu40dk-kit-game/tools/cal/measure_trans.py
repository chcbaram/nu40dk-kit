# gb caltrans 화면 사진 → 12 가지 전환: 사각형 색 오차, 지나간 자리(track) 오차
# 사용: python3 measure_trans.py photo.jpg marks(tlx,...) [shift=0]
#   값은 (흰 바탕 - 검정 바탕) 범위로 나눈 차이. 사각형: 같은 색 바탕 대비 (+ 밝음 / - 어두움)
#   track: 사각형이 지나간 줄 vs 같은 띠의 줄 사이 바탕 (+ 밝음 = 너무 돌아옴 / - 어두움 = 덜 돌아옴)
import sys, numpy as np
from PIL import Image
img = np.asarray(Image.open(sys.argv[1]).convert('L'), dtype=np.float32)
c = [float(v) for v in sys.argv[2].split(',')]
shift = int(sys.argv[3]) if len(sys.argv) > 3 else 0
src = [(2, 2), (158, 2), (2, 142), (158, 142)]
dst = [(c[0], c[1]), (c[2], c[3]), (c[4], c[5]), (c[6], c[7])]
A = []
for (x, y), (u, v) in zip(src, dst):
    A += [[x, y, 1, 0, 0, 0, -u * x, -u * y, -u], [0, 0, 0, x, y, 1, -v * x, -v * y, -v]]
Hm = np.linalg.svd(np.array(A))[2][-1].reshape(3, 3)
def region(x0, x1, y0, y1):
    vs = []
    for yy in np.arange(y0 + 0.5, y1, 1.0):
        for xx in np.arange(x0 + 0.5, x1, 1.0):
            p = Hm @ np.array([xx, yy, 1.0]); vs.append(img[int(round(p[1] / p[2])), int(round(p[0] / p[2]))])
    return float(np.median(vs))
names = "WLDK"
bgval, lanes = {}, []
for b in range(4):
    bg = 3 - ((b + shift) & 3); y0 = 6 + b * 33
    # 줄 사이 바탕: 띠 위 3 줄(y0..y0+2), 줄 사이 1 줄 두 개, 띠 아래
    gap = np.median([region(10, 135, y0, y0 + 2), region(10, 135, y0 + 12, y0 + 13), region(10, 135, y0 + 22, y0 + 23), region(10, 135, y0 + 32, y0 + 33)])
    bgval[bg] = gap
    lane = 0
    for col in range(4):
        if col == bg: continue
        ly = y0 + 3 + lane * 10
        rect  = region(150, 159, ly + 2, ly + 7)
        track = region(12, 132, ly + 2, ly + 7)
        lanes.append((bg, col, rect, track, gap))
        lane += 1
rng = bgval[0] - bgval[3]
print(f"bg W {bgval[0]:.0f} L {bgval[1]:.0f} D {bgval[2]:.0f} K {bgval[3]:.0f}")
print("band  rect  rect_err  track_err")
te, re_ = [], []
for bg, col, rect, track, gap in lanes:
    r_err = (rect - bgval[col]) / rng
    t_err = (track - gap) / rng
    te.append(abs(t_err)); re_.append(abs(r_err))
    print(f"  {names[bg]}    {names[col]}    {r_err:+.3f}   {t_err:+.3f}")
print(f"MEAN |rect| {np.mean(re_):.4f}  |track| {np.mean(te):.4f}")
