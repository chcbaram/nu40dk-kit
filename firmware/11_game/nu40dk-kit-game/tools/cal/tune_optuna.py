# 게임 잔상 자동 탐색 (Optuna TPE). 사용: python3 tune_optuna.py N [study.db]
#   점수 = 흰색이어야 하는 곳의 |바탕 대비 차이| 평균 + 0.05 x 연회색 밝기 퍼짐 (회색 일관성)
#   CAL_OUT, CAM 환경 변수는 game.sh 와 같음. 결과는 study.db (sqlite) 에 남아 이어서 돌릴 수 있음
import os, re, subprocess, sys, optuna
D = os.path.dirname(os.path.abspath(__file__))
N = int(sys.argv[1]) if len(sys.argv) > 1 else 40
DB = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.environ.get('CAL_OUT', '/tmp'), 'tune.db')
def cli(cmd):
    subprocess.run(['baram-ctl', 'send', cmd, '--until', 'cli# $', '--timeout', '3'], capture_output=True)
def objective(t):
    p = dict(lw=t.suggest_int('lw', 1, 4), dw=t.suggest_int('dw', 2, 6), kw=t.suggest_int('kw', 4, 8),
             kl=t.suggest_int('kl', 2, 5), kd=t.suggest_int('kd', 1, 3), dl=t.suggest_int('dl', 1, 3),
             add=t.suggest_int('add', 0, 3), gray=t.suggest_int('gray', 40, 100, step=20),
             dwell=t.suggest_int('dwell', 0, 150, step=25), pre=t.suggest_int('pre', 0, 5))
    cli(f"gb dose 1 0 {p['lw']}"); cli(f"gb dose 2 0 {p['dw']}"); cli(f"gb dose 3 0 {p['kw']}")
    cli(f"gb dose 3 1 {p['kl']}"); cli(f"gb dose 3 2 {p['kd']}"); cli(f"gb dose 2 1 {p['dl']}")
    cli(f"gb hist on 100 {p['add']}"); cli(f"gb histgray {p['gray']}"); cli(f"gb dwell {p['dwell']}"); cli(f"gb prewhite {p['pre']}")
    out = subprocess.run([os.path.join(D, 'game.sh'), f'opt{t.number}'], capture_output=True, text=True).stdout
    a = re.search(r'WHITE ABS ([0-9.]+)', out); l = re.search(r'L SPREAD ([0-9.]+)', out); g = re.search(r'WHITE GHOST mean ([0-9.]+)', out)
    if not a: raise optuna.TrialPruned()
    ls = float(l.group(1)) if l else 0.3
    t.set_user_attr('ghost', float(g.group(1))); t.set_user_attr('abs', float(a.group(1))); t.set_user_attr('lspread', ls)
    score = float(a.group(1)) + 0.05 * ls
    print(f"#{t.number} {p} abs {float(a.group(1)):.4f} ghost {float(g.group(1)):.4f} Lspread {ls:.3f} -> {score:.4f}", flush=True)
    return score
st = optuna.create_study(study_name='ink', storage=f'sqlite:///{DB}', load_if_exists=True, direction='minimize',
                         sampler=optuna.samplers.TPESampler(seed=1, n_startup_trials=10))
if len(st.trials) == 0:
    # 지금 기본값을 첫 시험으로
    st.enqueue_trial(dict(lw=2, dw=4, kw=6, kl=3, kd=2, dl=2, add=1, gray=60, dwell=100, pre=3))
optuna.logging.set_verbosity(optuna.logging.WARNING)
st.optimize(objective, n_trials=N)
b = st.best_trial
print("BEST", b.number, b.params, f"score {b.value:.4f}", b.user_attrs)
