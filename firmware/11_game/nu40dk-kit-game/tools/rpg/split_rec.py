#!/usr/bin/env python3
"""한 번에 녹음한 바람 대사를 대사별로 자른다.

    python3 tools/rpg/split_rec.py <녹음 파일(m4a/wav/aiff)> [--dry]

baram_lines.txt 순서대로 읽고, 대사 사이에 1~2 초 쉬어 녹음한다.
무음 구간으로 나눈 뒤 대사 수에 맞춰 (짧은 쉼은 합쳐서) tools/rpg/voice_rec/<번호>.wav 로 저장한다.
그다음 gen_voice.py 를 다시 실행하면 녹음이 TTS 대신 쓰인다.
"""
import audioop
import os
import subprocess
import sys
import tempfile
import wave

HERE = os.path.dirname(os.path.abspath(__file__))
RATE = 16000
FRAME = RATE // 50          # 20 ms


def load(path):
    with tempfile.TemporaryDirectory() as td:
        raw = os.path.join(td, 'a.wav')
        subprocess.run(['afconvert', '-f', 'WAVE', '-d', 'LEI16@%d' % RATE, '-c', '1', path, raw], check=True)
        with wave.open(raw) as r:
            return r.readframes(r.getnframes())


def lines_list():
    out = []
    for l in open(os.path.join(HERE, 'baram_lines.txt'), encoding='utf-8'):
        l = l.strip()
        if l and not l.startswith('#'):
            crc, text = l.split(None, 1)
            out.append((crc, text))
    return out


def segments(data):
    n = len(data) // 2 // FRAME
    rms = [audioop.rms(data[i * FRAME * 2:(i + 1) * FRAME * 2], 2) for i in range(n)]
    loud_sorted = sorted(rms)
    noise = loud_sorted[len(rms) // 10] if rms else 0          # 조용한 10 % = 바닥 잡음
    peak = loud_sorted[int(len(rms) * 0.95)] if rms else 0
    th = noise + (peak - noise) * 0.06
    voiced = [v > th for v in rms]
    segs = []                                                  # (시작, 끝) 프레임
    i = 0
    while i < n:
        if voiced[i]:
            j = i
            while j < n and voiced[j]:
                j += 1
            segs.append([i, j])
            i = j
        else:
            i += 1
    # 0.4 초 이하 끊김(음절·구절 사이)은 먼저 이어 붙이고, 0.25 초보다 짧은 잡음은 버린다
    merged = []
    for s in segs:
        if merged and s[0] - merged[-1][1] <= 20:
            merged[-1][1] = s[1]
        else:
            merged.append(s)
    return [s for s in merged if s[1] - s[0] >= 12]


def syllables(text):
    n = 0
    for ch in text:
        if 0xAC00 <= ord(ch) <= 0xD7A3:
            n += 1
        elif ch.isalnum():
            n += 0.5
    return max(n, 1)


def fit(segs, texts):
    """말소리 구간들을 대사 수만큼 이어진 묶음으로 나눈다.
    대사 안에서도 구절 사이를 길게 쉬는 경우가 있어 쉼 길이만으로는 경계를 못 찾는다.
    묶음 길이가 글자 수에 비례하도록(동적 계획법), 긴 쉼은 경계일 가능성을 조금 높인다."""
    n, m = len(segs), len(texts)
    syl = [syllables(t) for t in texts]
    speech = sum(b - a for a, b in segs)
    rate = speech / sum(syl)                                   # 글자당 프레임
    INF = float('inf')
    # cost[i][j]: 앞 i 개 구간으로 앞 j 개 대사를 만든 최소 비용
    cost = [[INF] * (m + 1) for _ in range(n + 1)]
    back = [[0] * (m + 1) for _ in range(n + 1)]
    cost[0][0] = 0
    for i in range(1, n + 1):
        for j in range(1, min(i, m) + 1):
            for k in range(j - 1, i):                           # 구간 k..i-1 이 j 번째 대사
                if cost[k][j - 1] == INF:
                    continue
                dur = segs[i - 1][1] - segs[k][0]               # 안쪽 쉼 포함 길이
                inner = sum(segs[t + 1][0] - segs[t][1] for t in range(k, i - 1))
                exp = syl[j - 1] * rate
                c = ((dur - inner * 0.5) - exp) ** 2 / (exp + 10)
                if i < n:
                    c -= (segs[i][0] - segs[i - 1][1]) * 0.05   # 뒤 쉼이 길면 경계일 가능성
                c += cost[k][j - 1]
                if c < cost[i][j]:
                    cost[i][j] = c
                    back[i][j] = k
    out = []
    i, j = n, m
    while j > 0:
        k = back[i][j]
        out.append([segs[k][0], segs[i - 1][1], k, i - 1])
        i, j = k, j - 1
    return out[::-1]


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return
    src = sys.argv[1]
    dry = '--dry' in sys.argv
    lines = lines_list()
    data = load(src)
    segs = segments(data)
    print('녹음 %.1f 초, 말소리 구간 %d 개, 대사 %d 개' % (len(data) / 2 / RATE, len(segs), len(lines)))
    if len(segs) < len(lines):
        print('구간이 대사보다 적습니다. 대사 사이를 조금 더 길게(1~2 초) 쉬어 다시 녹음해 주세요.')
        for k, s in enumerate(segs):
            print('  %2d  %5.1f ~ %5.1f s' % (k + 1, s[0] * 0.02, s[1] * 0.02))
        sys.exit(1)
    segs = fit(segs, [t for c, t in lines])
    out = os.path.join(HERE, 'voice_rec')
    os.makedirs(out, exist_ok=True)
    for (crc, text), (a, b, k0, k1) in zip(lines, segs):
        a = max(0, a - 5)                                      # 앞 0.1 초, 뒤 0.2 초 여유
        b = min(len(data) // 2 // FRAME, b + 10)
        seg = data[a * FRAME * 2:b * FRAME * 2]
        print('  %s  %4.1f s  %5.1f ~ %5.1f  [구간 %d~%d]  %s' % (crc, len(seg) / 2 / RATE, a * 0.02, b * 0.02, k0, k1, text))
        if not dry:
            with wave.open(os.path.join(out, crc + '.wav'), 'wb') as w:
                w.setnchannels(1)
                w.setsampwidth(2)
                w.setframerate(RATE)
                w.writeframes(seg)
    if not dry:
        print('저장: %s  (이제 python3 tools/rpg/gen_voice.py)' % out)


if __name__ == '__main__':
    main()
