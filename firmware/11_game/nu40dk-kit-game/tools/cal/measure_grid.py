# gb calrev / caldwell 화면 사진 → 줄 x 칸 정규화 밝기. 사용: python3 measure_grid.py photo.jpg [rows=6] [first_col=0]
import sys, os
sys.argv_save = list(sys.argv)
sys.argv = sys.argv[:2]           # measure_table 의 모서리 인자 처리와 겹치지 않게
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'measure_table.py')).read().split("black  = region_mean")[0])
black  = region_mean(16, 20, 192, 70)
wblack = region_mean(16, 74, 192, 86, inset=0.1)
rb     = black / wblack
ROWS = int(sys.argv_save[2]) if len(sys.argv_save) > 2 else 6
C0   = int(sys.argv_save[3]) if len(sys.argv_save) > 3 else 0
print("row\\col " + ' '.join(f'{m + C0:5d}' for m in range(9)))
for k in range(ROWS):
    y = 90 + k * 24
    row = []
    for m in range(9):
        x  = 16 + m * 40
        v  = region_mean(x, y, x + 32, y + 14)
        wl = region_mean(x, y - 9, x + 32, y - 2, inset=0.1)
        row.append(((v / wl) - rb) / (1 - rb))
    print(f"r{k}     " + ' '.join(f'{v:5.2f}' for v in row))
