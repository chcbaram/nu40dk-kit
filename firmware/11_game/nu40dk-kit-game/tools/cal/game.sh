#!/bin/zsh
# 실제 게임 화면 잔상 측정: game.sh <label>
#   gb run → Start → 자동 키로 진행 → 정지 → 화면 처리 끝날 때까지 대기 → 촬영 + GB 화면 버퍼 덤프 → 비교
#   KEYS 환경 변수로 키 순서 지정 ("키:ms 키:ms ..."), 기본은 오른쪽으로 걷기와 점프 반복
D=$(cd "$(dirname "$0")" && pwd)
LABEL=${1:-game}
CAM=${CAM:-$(imagesnap -l | sed -n 's/^=> //p' | tail -1)}
OUT=${CAL_OUT:-/tmp}/game_${LABEL}
KEYS=${KEYS:-"right,b:1500 right,b,a:600 right,b:1500 right,b,a:600 right,b:1500 right,b,a:600 right,b:1500"}
C() { baram-ctl "$@" 2>/dev/null; }
C send "gb stop" --until 'cli# $' --timeout 5 >/dev/null
# 위치 맞춤: 모서리 표시만 있는 화면을 찍는다 (보드가 움직였을 수 있음)
C send "gb calmove 0 1 0 1 4" --until 'calmove :.*' --timeout 30 >/dev/null
sleep 5
imagesnap -d "$CAM" -w 1 "${OUT}_marks.jpg" >/dev/null 2>&1
MARKS=$(python3 "$D/find_marks.py" "${OUT}_marks.jpg")
echo "marks: $MARKS"
C send "gb stop" --until 'cli# $' --timeout 5 >/dev/null
C send "gb run" --until 'cli# $' --timeout 20 >/dev/null
sleep 7
C send "gb key start 300" --until 'cli# $' --timeout 3 >/dev/null
sleep 4
for k in ${=KEYS}; do
  key=${k%%:*}; ms=${k##*:}
  C send "gb key $key $ms" --until 'cli# $' --timeout 3 >/dev/null
  sleep $(( ms / 1000.0 ))
done
sleep 0.5
C send "gb pause 1" --until 'cli# $' --timeout 3 >/dev/null
sleep ${SETTLE:-4}
imagesnap -d "$CAM" -w 1 "$OUT.jpg" >/dev/null 2>&1
M=$(baram-ctl --json status | python3 -c 'import sys,json;print(json.load(sys.stdin)["mark"])')
C send "gb lcddump" --timeout 1 >/dev/null
baram-ctl wait --since $M --until 'lcddump end' --timeout 20 >/dev/null 2>&1
baram-ctl read --since $M 2>/dev/null | grep -E "^L[0-9]{3} " > "$OUT.lcd"
echo "photo: $OUT.jpg  lcd: $OUT.lcd ($(wc -l < $OUT.lcd) lines)"
python3 "$D/measure_game.py" "$OUT.jpg" "$OUT.lcd" "$MARKS"
