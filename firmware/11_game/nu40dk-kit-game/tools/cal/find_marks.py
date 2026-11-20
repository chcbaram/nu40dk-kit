# GB 화면 네 모서리 4x4 검정 표시 찾기 (gb calmove 0 1 0 1 4 화면). 출력: tlx,tly,trx,try,blx,bly,brx,bry
# 사용: python3 find_marks.py photo.jpg   (패널이 사진 가운데쯤에 있다고 가정, 넓게 찾음)
import sys, numpy as np
from PIL import Image
img = np.asarray(Image.open(sys.argv[1]).convert('L'), dtype=np.float32)
H, W = img.shape
# 1) 대략 위치: 밝은 패널 영역의 경계에서 안쪽
bright = img > np.percentile(img, 70)
ys, xs = np.nonzero(bright[H // 8:H * 7 // 8, W // 6:W * 5 // 6])
# 2) 모서리 근처에서 작은 어두운 덩어리 찾기: 사분면마다 가장 어두운 12x12 창
def find(qx0, qx1, qy0, qy1):
    sub = img[qy0:qy1, qx0:qx1]
    k = 12
    c = np.cumsum(np.cumsum(sub, 0), 1)
    s = c[k:, k:] - c[:-k, k:] - c[k:, :-k] + c[:-k, :-k]
    y, x = np.unravel_index(np.argmin(s), s.shape)
    win = sub[y:y + 2 * k, x:x + 2 * k]
    th = win.min() + (np.median(win) - win.min()) * 0.5
    yy, xx = np.nonzero(win < th)
    return qx0 + x + xx.mean(), qy0 + y + yy.mean()
# 패널(밝은 영역) 범위 안의 네 모서리 근처를 찾는다
x0, x1 = W // 6 + np.percentile(xs, 2), W // 6 + np.percentile(xs, 98)
y0, y1 = H // 8 + np.percentile(ys, 2), H // 8 + np.percentile(ys, 98)
cw, ch = (x1 - x0) * 0.22, (y1 - y0) * 0.22
pts = [find(int(x0), int(x0 + cw), int(y0), int(y0 + ch)),
       find(int(x1 - cw), int(x1), int(y0), int(y0 + ch)),
       find(int(x0), int(x0 + cw), int(y1 - ch), int(y1)),
       find(int(x1 - cw), int(x1), int(y1 - ch), int(y1))]
print(','.join(f'{v:.1f}' for p in pts for v in p))
