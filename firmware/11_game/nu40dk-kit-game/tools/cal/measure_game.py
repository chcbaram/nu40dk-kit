# 게임 화면 사진 vs GB 화면 버퍼(lcddump) 비교 → 잔상 점수
# 사용: python3 measure_game.py photo.jpg dump.lcd [tlx,tly,trx,try,blx,bly,brx,bry]
#   모서리 좌표 = GB 화면 네 모서리 4x4 의 사진 위치 (gb calmove 사진에서 measure_move.py 가 찾은 marks). 카메라 고정 가정.
#   결과: 색(W/L/D/K)별 밝기, 흰색이어야 하는 곳의 어두움(잔상), 잔상 지도 <photo>.ghost.png
import sys, numpy as np
from PIL import Image
img = np.asarray(Image.open(sys.argv[1]).convert('L'), dtype=np.float32)
rows = [l.split()[1] for l in open(sys.argv[2]) if l.startswith('L')]
assert len(rows) == 144, f"dump rows {len(rows)}"
shade = np.zeros((144, 160), dtype=np.uint8)
for y, r in enumerate(rows):
    b = bytes.fromhex(r)
    for x in range(160):
        shade[y, x] = (b[x >> 2] >> (6 - 2 * (x & 3))) & 3
c = [607, 202, 1312, 205, 624, 845, 1296, 844]
if len(sys.argv) > 3: c = [float(v) for v in sys.argv[3].split(',')]
src = [(2, 2), (158, 2), (2, 142), (158, 142)]
dst = [(c[0], c[1]), (c[2], c[3]), (c[4], c[5]), (c[6], c[7])]
A = []
for (x, y), (u, v) in zip(src, dst):
    A += [[x, y, 1, 0, 0, 0, -u * x, -u * y, -u], [0, 0, 0, x, y, 1, -v * x, -v * y, -v]]
Hm = np.linalg.svd(np.array(A))[2][-1].reshape(3, 3)
val = np.zeros((144, 160), dtype=np.float32)
for y in range(144):
    for x in range(160):
        p = Hm @ np.array([x + 0.5, y + 0.5, 1.0]); u, v = int(round(p[0] / p[2])), int(round(p[1] / p[2]))
        val[y, x] = img[v - 1:v + 2, u - 1:u + 2].mean()
# 경계 제외: 자신과 8 이웃이 같은 색인 픽셀만
same = np.ones((144, 160), dtype=bool)
for dy in (-1, 0, 1):
    for dx in (-1, 0, 1):
        sh = np.roll(np.roll(shade, dy, 0), dx, 1)
        same &= (sh == shade)
same[[0, -1], :] = False; same[:, [0, -1]] = False
names = "WLDK"
med = {}
for s in range(4):
    m = same & (shade == s)
    if m.sum() > 20:
        med[s] = float(np.median(val[m]))
        print(f"{names[s]}: {m.sum():5d} px  median {med[s]:6.1f}  spread(p10~p90) {np.percentile(val[m],10):6.1f}~{np.percentile(val[m],90):6.1f}")
# 잔상: 흰색이어야 하는 곳이 주변 흰 바탕보다 얼마나 어두운가.
#   기준 = 흰 픽셀만으로 만든 부드러운 바탕 (반경 R 상자 평균, 조명 차이 제거). 잔상 자체가 기준에 섞이지 않게 중앙값 근처만
W = same & (shade == 0)
R = 12
med_w = np.median(val[W])
use = W & (val > med_w * 0.94)                       # 뚜렷한 잔상은 바탕 계산에서 뺌
def boxsum(a):
    c = np.pad(np.cumsum(np.cumsum(a, 0), 1), ((1, 0), (1, 0)))
    y0 = np.clip(np.arange(144) - R, 0, 144); y1 = np.clip(np.arange(144) + R + 1, 0, 144)
    x0 = np.clip(np.arange(160) - R, 0, 160); x1 = np.clip(np.arange(160) + R + 1, 0, 160)
    return c[y1][:, x1] - c[y0][:, x1] - c[y1][:, x0] + c[y0][:, x0]
bg = boxsum(np.where(use, val, 0)) / np.maximum(boxsum(use.astype(np.float32)), 1)
bg = np.where(bg > 0, bg, med_w)
ghost = np.where(W, 1 - val / bg, 0)
g = ghost[W]
print(f"WHITE ABS {np.abs(g).mean():.4f}")
kmed = med.get(3, np.median(val[shade == 3]) if (shade == 3).any() else 60)
Lm = same & (shade == 1)
if Lm.sum() > 20:
    print(f"L SPREAD {(np.percentile(val[Lm], 90) - np.percentile(val[Lm], 10)) / max(med_w - kmed, 1):.4f}")
print(f"WHITE GHOST mean {np.clip(g,0,1).mean():.4f}  p95 {np.percentile(g,95):.3f}  >0.03 {100*(g>0.03).mean():.1f}%  >0.06 {100*(g>0.06).mean():.1f}%")
Image.fromarray((255 - np.clip(ghost, 0, 1) * 255 * 6).clip(0, 255).astype(np.uint8)).resize((640, 576), Image.NEAREST).save(sys.argv[1] + '.ghost.png')
