#!/bin/zsh
# 보드에 캘리브레이션 화면을 띄우고 카메라로 찍어 측정한다.
#   사용: capture.sh curve|table [frames] [카메라 이름]
#   필요: baram-term(baram-ctl), imagesnap (brew install imagesnap), python3 + numpy + Pillow
D=$(cd "$(dirname "$0")" && pwd)
MODE=${1:-table}; FR=${2:-4}; CAM=${3:-$(imagesnap -l | sed -n 's/^=> //p' | tail -1)}
OUT=${CAL_OUT:-/tmp}/cal_${MODE}_$(date +%H%M%S).jpg
if [[ $MODE == curve ]]; then CMD="gb cal $FR 10"; PYS=measure_curve.py; else CMD="gb caltable $FR"; PYS=measure_table.py; fi
baram-ctl send "gb stop" --until 'cli# $' --timeout 5 >/dev/null
baram-ctl send "$CMD" --until 'done' --timeout 60 >/dev/null
sleep 2
imagesnap -d "$CAM" -w 2 "$OUT" >/dev/null 2>&1
echo "photo: $OUT"
python3 "$D/$PYS" "$OUT"
