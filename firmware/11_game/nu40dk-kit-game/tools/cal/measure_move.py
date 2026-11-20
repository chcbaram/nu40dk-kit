# gb calmove 화면 사진 → 물체가 지나간 자리의 잔상 (흰 기준 대비 어두워진 정도, 0 = 깨끗, 1 = 검정)
# 사용: BIG=1 python3 measure_move.py after.jpg base.jpg [tlx,...]  (BIG=1: gb calmove ... 1 큰 도형)  base = 패턴 전 흰 화면 사진 (gb calclean 후)
#   GB 픽셀마다 (base - after) / base 로 이번 움직임이 남긴 잔상만 계산 (원래 얼룩·조명 차이 상쇄)
#       결과 그림: photo.jpg.residue.png (잔상 지도, 진할수록 잔상)
import sys, numpy as np
from PIL import Image

img  = np.asarray(Image.open(sys.argv[1]).convert('L'), dtype=np.float32)
base = np.asarray(Image.open(sys.argv[2]).convert('L'), dtype=np.float32)
g = [608, 202, 1312, 205, 625, 843, 1296, 842]
if len(sys.argv) > 3: g = [int(v) for v in sys.argv[3].split(',')]
guess = {'tl': g[0:2], 'tr': g[2:4], 'bl': g[4:6], 'br': g[6:8]}
cent = {}
for k, (gx, gy) in guess.items():
    r = 30
    win = img[gy - r:gy + r, gx - r:gx + r]
    th = win.min() + (np.median(win) - win.min()) * 0.5
    ys, xs = np.nonzero(win < th)
    cent[k] = (gx - r + xs.mean(), gy - r + ys.mean())
# GB 좌표의 모서리 사각형 중심 (4x4 의 가운데)
src = {'tl': (2, 2), 'tr': (158, 2), 'bl': (2, 142), 'br': (158, 142)}
A = []
for k in src:
    x, y = src[k]; u, v = cent[k]
    A += [[x, y, 1, 0, 0, 0, -u * x, -u * y, -u], [0, 0, 0, x, y, 1, -v * x, -v * y, -v]]
Hm = np.linalg.svd(np.array(A))[2][-1].reshape(3, 3)

def sample(a, x, y):
    p = Hm @ np.array([x, y, 1.0]); u, v = int(round(p[0] / p[2])), int(round(p[1] / p[2]))
    return a[v - 1:v + 2, u - 1:u + 2].mean()      # 3x3 평균 (잡음 감소)

# GB 픽셀마다 가운데 값 (2x2 패널 픽셀의 중앙)
gb  = np.array([[sample(img,  x + 0.5, y + 0.5) for x in range(160)] for y in range(144)])
gb0 = np.array([[sample(base, x + 0.5, y + 0.5) for x in range(160)] for y in range(144)])
ratio = gb / gb0
ratio /= np.median(ratio[124:138, 6:150])          # 노출 차이 보정 (안 지나간 띠 = 1)
res = np.clip(1 - ratio, 0, 1)
white = np.median(gb0)

def zone(name, x0, x1, y0, y1):
    z = res[y0:y1, x0:x1]
    print(f"{name:22s} mean {z.mean():.3f}  p95 {np.percentile(z, 95):.3f}  max {z.max():.3f}")
    return z.mean()

print(f"marks {', '.join(f'{k}=({c[0]:.0f},{c[1]:.0f})' for k, c in cent.items())}  white {white:.1f}")
zone("control (untouched)", 6, 150, 124, 138)
import os
BIG = os.environ.get('BIG', '0') == '1'
stops = []
if not BIG:
    # 기본 패턴(speed 1, move 40 x 3 = 120px 이동): 물체가 지나갔고 지금은 없는 영역
    m = zone("mountain swept", 2, 116, 58, 102)
    c = zone("cloud swept", 8, 126, 28, 42)
    b = zone("box swept", 48, 156, 106, 122)
    for stop in (40, 80):
        stops.append(zone(f"stop@{stop} box", 50 + stop, 62 + stop, 108, 120))
        stops.append(zone(f"stop@{stop} cloud", 10 + stop, 34 + stop, 30, 40))
        line = [res[100 - t, stop + t] for t in range(40) if stop + t < 160] + \
               [res[60 + t, stop + 40 + t] for t in range(40) if stop + 40 + t < 160]
        print(f"{f'stop@{stop} line':22s} mean {np.mean(line):.3f}")
        stops.append(float(np.mean(line)))
else:
    # 큰 도형: 2px 산(x ox~ox+64, y 60~92), 구름 40x16 (ox+4, y 24), 상자 24x24 (ox+60, y 98)
    m = zone("mountain swept", 2, 118, 58, 94)
    c = zone("cloud swept", 6, 122, 26, 38)
    b = zone("box swept", 62, 158, 100, 120)
    for stop in (40, 80):
        stops.append(zone(f"stop@{stop} box", 62 + stop, min(82 + stop, 158), 100, 120))
        stops.append(zone(f"stop@{stop} cloud", 7 + stop, 41 + stop, 27, 37))
        line = [res[91 - t + dy, stop + t + dx] for t in range(32) for dx in (0, 1) for dy in (0, 1) if stop + t + dx < 160] + \
               [res[60 + t + dy, stop + 32 + t + dx] for t in range(32) for dx in (0, 1) for dy in (0, 1) if stop + 33 + t < 160]
        print(f"{f'stop@{stop} line':22s} mean {np.mean(line):.3f}")
        stops.append(float(np.mean(line)))
print(f"STOP GHOST {np.mean(stops):.3f}")
if not BIG:
    lines = [np.mean([res[100 - t, st + t] for t in range(40) if st + t < 160] + [res[60 + t, st + 40 + t] for t in range(40) if st + 40 + t < 160]) for st in (40, 80)]
    over  = np.clip(ratio - 1, 0, 1)
    lover = [np.mean([over[100 - t, st + t] for t in range(40) if st + t < 160] + [over[60 + t, st + 40 + t] for t in range(40) if st + 40 + t < 160]) for st in (40, 80)]
    print(f"LINE {np.mean(lines):.3f}  LINE OVER-WHITE {np.mean(lover):.3f}  (control over {over[124:138, 6:150].mean():.3f})")
if BIG:
    edges, inners = [], []
    for stop in (40, 80):
        x0, y0 = 60 + stop, 98
        box = res[y0:y0 + 24, x0:min(x0 + 24, 158)]
        edges.append(np.concatenate([box[:2].ravel(), box[-2:].ravel(), box[2:-2, :2].ravel()]).mean()); inners.append(box[4:-4, 4:-4].mean())
        cl = res[24:40, 4 + stop:44 + stop]
        edges.append(np.concatenate([cl[:1].ravel(), cl[-1:].ravel(), cl[:, :1].ravel(), cl[:, -1:].ravel()]).mean()); inners.append(cl[3:-3, 3:-3].mean())
    print(f"EDGE {np.mean(edges):.3f}  INNER {np.mean(inners):.3f}")
    over = np.clip(ratio - 1, 0, 1)
    ov = [over[98:122, 60 + st:min(84 + st, 158)].mean() for st in (40, 80)] + [over[24:40, 4 + st:44 + st].mean() for st in (40, 80)]
    print(f"OVER-WHITE {np.mean(ov):.3f}  (control {over[124:138, 6:150].mean():.3f})")
print(f"GHOST SCORE {(m + c + b) / 3:.3f}")
Image.fromarray((255 - res * 255 * 4).clip(0, 255).astype(np.uint8)).resize((640, 576), Image.NEAREST).save(sys.argv[1] + '.residue.png')
