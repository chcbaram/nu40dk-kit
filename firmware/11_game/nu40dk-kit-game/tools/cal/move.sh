#!/bin/zsh
# 움직이는 패턴으로 잔상 측정: move.sh [label] [calmove 인자...]
#   정리 갱신(장면 전환, 조용한 순간)을 끄고 패턴을 돌린 뒤 사진을 찍어 잔상 점수를 낸다.
D=$(cd "$(dirname "$0")" && pwd)
LABEL=${1:-run}; shift
CAM=${CAM:-$(imagesnap -l | sed -n 's/^=> //p' | tail -1)}
OUT=${CAL_OUT:-/tmp}/move_${LABEL}.jpg
C() { baram-ctl "$@" 2>/dev/null; }
C send "gb quiet 0" --until 'cli# $' --timeout 3 >/dev/null
C send "gb scene 0" --until 'cli# $' --timeout 3 >/dev/null
C send "gb calclean" --until 'calclean done' --timeout 30 >/dev/null
sleep 2
imagesnap -d "$CAM" -w 2 "${OUT%.jpg}_base.jpg" >/dev/null 2>&1
C send "gb calmove $*" --until 'calmove :.*' --timeout 30 >/dev/null
sleep ${WAIT:-12}
imagesnap -d "$CAM" -w 2 "$OUT" >/dev/null 2>&1
C send "gb stop" --until 'cli# $' --timeout 5 >/dev/null
echo "photo: $OUT"
BIG=${BIG:-0} python3 "$D/measure_move.py" "$OUT" "${OUT%.jpg}_base.jpg"
