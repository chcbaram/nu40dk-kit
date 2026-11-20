# gb calmove 1 40 120 3 3 (회색 띠 위로 상자) 사진 → 상자가 지나간 띠 vs 안 지나간 띠 비교
# 사용: python3 measure_band.py after.jpg base.jpg   (+ = 지나간 자리가 더 어두움, - = 더 하얌)
import sys, numpy as np
src = open(__file__.replace('measure_band.py', 'measure_move.py')).read().split('print(f"marks')[0]
exec(src)
gbv = gb   # 사진 값 (GB 픽셀)
ref = np.median(gbv[96:124, 4:56])                     # 안 지나간 띠 (상자 시작 x 60 왼쪽)
for name, x0, x1 in (("stop@40", 102, 122), ("stop@80", 142, 158), ("moving 62~100", 62, 100)):
    z = gbv[96:124, x0:x1]
    print(f"{name:14s} {1 - np.median(z) / ref:+.3f}")
print(f"band untouched {ref:.1f}, white {np.median(gbv[130:140, 6:150]):.1f}")
