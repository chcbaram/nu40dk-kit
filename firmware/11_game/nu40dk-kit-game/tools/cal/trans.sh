#!/bin/zsh
# Transition test 측정: trans.sh <label> [passes=3] [shift=0]
D=$(cd "$(dirname "$0")" && pwd)
LABEL=${1:-trans}; PASSES=${2:-3}; SHIFT=${3:-0}
CAM=${CAM:-$(imagesnap -l | sed -n 's/^=> //p' | tail -1)}
OUT=${CAL_OUT:-/tmp}/trans_${LABEL}
C() { baram-ctl "$@" 2>/dev/null; }
C send "gb stop" --until 'cli# $' --timeout 5 >/dev/null
C send "gb calmove 0 1 0 1 4" --until 'calmove :.*' --timeout 30 >/dev/null
sleep 5
imagesnap -d "$CAM" -w 1 "${OUT}_marks.jpg" >/dev/null 2>&1
MARKS=$(python3 "$D/find_marks.py" "${OUT}_marks.jpg")
C send "gb stop" --until 'cli# $' --timeout 5 >/dev/null
C send "gb caltrans $PASSES $SHIFT" --until 'caltrans :.*' --timeout 30 >/dev/null
sleep $(( PASSES * 6 + 3 ))
imagesnap -d "$CAM" -w 1 "$OUT.jpg" >/dev/null 2>&1
echo "photo: $OUT.jpg  marks: $MARKS"
python3 "$D/measure_trans.py" "$OUT.jpg" "$MARKS" $SHIFT
