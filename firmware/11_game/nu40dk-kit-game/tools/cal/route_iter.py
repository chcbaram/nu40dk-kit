# 경로별 보정 측정-보정 반복: Transition test 오차를 경로별 펄스 가감으로 되먹임
# 사용: python3 route_iter.py N [gain]   (CAL_OUT, CAM 환경 변수)
#   사각형 오차 (바탕 X → 사각형 Y): route[X][Y][X 에 도착한 방향]
#   지나간 자리 오차 (Y 갔다가 X 로 돌아옴): route[Y][X][X→Y 가 어두운 쪽이면 1]
#   흰색 쪽 전환이 너무 밝으면 줄이고, 검정 쪽 전환이 너무 밝으면 늘린다. 1 펄스 ≈ 0.18
import os, re, subprocess, sys, json
D = os.path.dirname(os.path.abspath(__file__))
N = int(sys.argv[1]) if len(sys.argv) > 1 else 3
GAIN = float(sys.argv[2]) if len(sys.argv) > 2 else 0.7
STEP = 0.18
S = "WLDK"
route = {}
def cli(cmd):
    subprocess.run(['baram-ctl', 'send', cmd, '--until', 'cli# $', '--timeout', '3'], capture_output=True)
def push():
    for (f, t, a), v in route.items():
        cli(f"gb route {f} {t} {a} {v}")
hist = []
for it in range(N + 1):
    push()
    out = subprocess.run([os.path.join(D, 'trans.sh'), f'ri{it}', '3', str(it % 4)], capture_output=True, text=True).stdout
    rows = re.findall(r'^\s+([WLDK])\s+([WLDK])\s+([+-][0-9.]+)\s+([+-][0-9.]+)', out, re.M)
    m = re.search(r'MEAN \|rect\| ([0-9.]+)\s+\|track\| ([0-9.]+)', out)
    if not m:
        print("measure fail", out[-300:]); break
    hist.append((float(m.group(1)), float(m.group(2))))
    print(f"iter {it}: |rect| {m.group(1)} |track| {m.group(2)}  route {json.dumps({f'{S[k[0]]}{S[k[1]]}{k[2]}': v for k, v in route.items() if v})}", flush=True)
    if it == N: break
    acc = {}
    for bgc, rc, rerr, terr in rows:
        X, Y = S.index(bgc), S.index(rc)
        # 사각형: X → Y. X 에 도착한 방향: 시작은 흰 바탕에서 X 로 (X 가 흰색이 아니면 어두운 쪽으로 도착)
        k1 = (X, Y, 1 if X != 0 else 0); toward_white = Y < X
        acc.setdefault(k1, []).append(-float(rerr) if toward_white else float(rerr))
        # 지나간 자리: Y → X 로 돌아옴, Y 에 도착한 방향 = X→Y 가 어두운 쪽이면 1
        k2 = (Y, X, 1 if Y > X else 0); toward_white = X < Y
        acc.setdefault(k2, []).append(-float(terr) if toward_white else float(terr))
    for k, errs in acc.items():
        e = sum(errs) / len(errs)
        d = int(round(GAIN * e / STEP))
        d = max(-3, min(3, d))
        route[k] = max(-8, min(8, route.get(k, 0) + d))
print("history", hist)
print("FINAL", json.dumps({f'{S[k[0]]}{S[k[1]]}{k[2]}': v for k, v in route.items()}))
