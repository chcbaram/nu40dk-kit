# 맛보기 자동 탐색: 무작위 조합 N 번, 게임 잔상 점수(game.sh) 비교
# 사용: python3 tune_quick.py N   (CAL_OUT, CAM 환경 변수는 game.sh 와 같음)
import os, random, re, subprocess, sys, json
D = os.path.dirname(os.path.abspath(__file__))
N = int(sys.argv[1]) if len(sys.argv) > 1 else 8
random.seed(1)
space = {'add': [1, 2, 3], 'dwell': [50, 100, 150], 'prewhite': [2, 3, 4], 'kw': [5, 6, 7]}
base = {'add': 1, 'dwell': 100, 'prewhite': 3, 'kw': 6}
def cli(cmd):
    subprocess.run(['baram-ctl', 'send', cmd, '--until', 'cli# $', '--timeout', '3'], capture_output=True)
def trial(i, p):
    cli(f"gb hist on 100 {p['add']}"); cli(f"gb dwell {p['dwell']}"); cli(f"gb prewhite {p['prewhite']}"); cli(f"gb dose 3 0 {p['kw']}")
    out = subprocess.run([os.path.join(D, 'game.sh'), f'tq{i}'], capture_output=True, text=True).stdout
    m = re.search(r'WHITE GHOST mean ([0-9.]+)\s+p95 ([0-9.]+)\s+>0.03 ([0-9.]+)%', out)
    k = re.search(r'K:\s+\d+ px\s+median\s+([0-9.]+)', out)
    return (float(m.group(1)), float(m.group(2)), float(m.group(3)), float(k.group(1)) if k else -1) if m else None
res = []
for i in range(N):
    p = dict(base) if i == 0 else {k: random.choice(v) for k, v in space.items()}
    r = trial(i, p)
    res.append((p, r))
    print(f"{i}: {json.dumps(p)} -> " + (f"ghost mean {r[0]:.4f} p95 {r[1]:.3f} >3% {r[2]:.1f}%  K {r[3]:.0f}" if r else "fail"), flush=True)
best = min((x for x in res if x[1]), key=lambda x: x[1][0])
print("BEST", json.dumps(best[0]), best[1])
