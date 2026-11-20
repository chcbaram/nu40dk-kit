#!/usr/bin/env python3
"""RPG 대사 음성 생성 (macOS say, 한국어 TTS)

rpg.c 의 대사 배열과 그 배열을 쓰는 호출(dialog / say)에서 화자를 찾아,
페이지("" 로 나뉜 묶음)마다 WAV 하나를 만든다.

    python3 tools/rpg/gen_voice.py [출력폴더]      (기본 tools/rpg/out/rpg/v)

파일 이름은 페이지 글자(줄을 '\\n' 으로 이은 UTF-8)의 CRC32 (펌웨어 rpg.c voicePath 와 같다).
SD 카드의 /rpg/v/ 에 복사한다. 형식: WAV PCM 16bit 모노 16000Hz.
"""
import os
import re
import subprocess
import sys
import tempfile
import wave
import zlib
import audioop

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, '..', '..', 'src', 'ap', 'modules', 'game', 'rpg', 'rpg.c')

# 화자 -> Microsoft 신경망 음성 (edge-tts): (음성, 빠르기, 음높이)
#   한국어 음성이 셋뿐이라 빠르기·음높이로 캐릭터를 나눈다
EDGE = {
    None:            ('ko-KR-SunHiNeural',  '+0%',  '+0Hz'),
    '마을 사람':      ('ko-KR-SunHiNeural',  '+5%',  '+15Hz'),
    '배터리 은둔자':  ('ko-KR-SunHiNeural',  '-10%', '-20Hz'),
    '바람':          ('ko-KR-InJoonNeural', '+5%',  '+5Hz'),
    '선배 개발자':    ('ko-KR-InJoonNeural', '+5%',  '+0Hz'),
    '데이터시트 현자': ('ko-KR-InJoonNeural', '-10%', '-20Hz'),
    '스케줄러 수도승': ('ko-KR-InJoonNeural', '-15%', '-5Hz'),
    'DMA 장인':      ('ko-KR-HyunsuMultilingualNeural', '+5%',  '+0Hz'),
    '인터럽트 장인':  ('ko-KR-HyunsuMultilingualNeural', '-10%', '-15Hz'),
    '펌웨어 고수':    ('ko-KR-HyunsuMultilingualNeural', '-15%', '-25Hz'),
}
# 맥 say 음성 (--say 로 쓸 때)
VOICES = {
    None: 'Yuna',
    '바람': 'Eddy (한국어(한국))',
    '선배 개발자': 'Reed (한국어(한국))',
    '인터럽트 장인': 'Grandpa (한국어(한국))',
    '펌웨어 고수': 'Rocko (한국어(한국))',
}
RATE = 16000

# 읽기용 바꾸기 (화면 글자는 그대로)
SPEAK = [
    ('printf', '프린트에프'), ('ST-LINK', '에스티링크'), ('ISR', '아이에스알'),
    ('RTOS', '알토스'), ('SWD', '에스더블유디'), ('PC', '피씨'), ('CPU', '씨피유'),
    ('delay()', '딜레이'), ('0x0000_0004', '영 엑스 영영영영 영영영사'),
    ('A:', '에이,'), ('B:', '비,'), ('...', '.'), ('(', ' '), (')', ' '),
]


def c_unescape(s):
    return s.encode('utf-8').decode('unicode_escape').encode('latin-1').decode('utf-8')


def split_pages(lines):
    """대화창과 같은 규칙으로 쪽 나누기: "" 로 나누고, 한 쪽 최대 3 줄 (rpg.c dialogP)"""
    pages, page = [], []
    for ln in lines + ['']:
        if ln == '':
            if page:
                pages.append(page)
            page = []
        else:
            page.append(ln)
            if len(page) == 3:
                pages.append(page)
                page = []
    return pages


def parse_pages(src=None):
    """story.py 의 대사 블록·문답 대답을 (화자, 쪽) 목록으로"""
    sys.path.insert(0, HERE)
    import story as S
    out = []
    def add_blocks(blocks):
        for who, lines in blocks:
            for pg in split_pages(lines):
                out.append((who, pg))
    add_blocks(S.INTRO)
    add_blocks(S.GATE_OPEN)
    for owner, cond, pre, quiz, act, post in S.SCRIPTS:
        add_blocks(pre)
        if quiz:
            who = pre[-1][0] if pre else None
            q = S.QUIZZES[quiz]
            add_blocks([(who, q[3]), (who, q[4])])
        add_blocks(post)
    return out


def speak_text(page):
    s = ' '.join(page)
    for a, b in SPEAK:
        s = s.replace(a, b)
    return s


def trim_write(data, dst):
    step = 2 * RATE // 100
    def loud(i):
        return audioop.rms(data[i:i + step], 2) > 200
    a = 0
    while a < len(data) - step and not loud(a):
        a += step
    b = len(data) - step
    while b > a and not loud(b):
        b -= step
    data = data[max(0, a - step):min(len(data), b + 2 * step)]
    with wave.open(dst, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(data)
    return len(data) / 2 / RATE


def convert_rec(src, dst):
    """직접 녹음한 파일 → 16kHz 모노 16bit, 앞뒤 무음 정리, 소리 크기 맞춤"""
    with tempfile.TemporaryDirectory() as td:
        raw = os.path.join(td, 'a.wav')
        subprocess.run(['afconvert', '-f', 'WAVE', '-d', 'LEI16@%d' % RATE, '-c', '1', src, raw], check=True)
        with wave.open(raw) as r:
            data = r.readframes(r.getnframes())
    peak = audioop.max(data, 2) or 1
    data = audioop.mul(data, 2, min(4.0, 26000 / peak))
    return trim_write(data, dst)


def make_wav_edge(who, text, dst):
    voice, rate, pitch = EDGE.get(who, EDGE[None])
    with tempfile.TemporaryDirectory() as td:
        mp3 = os.path.join(td, 'a.mp3')
        for attempt in range(3):
            r = subprocess.run([sys.executable, '-m', 'edge_tts', '--voice', voice, '--rate=' + rate,
                                '--pitch=' + pitch, '--text', text, '--write-media', mp3],
                               capture_output=True)
            if r.returncode == 0 and os.path.exists(mp3) and os.path.getsize(mp3) > 0:
                break
        else:
            raise RuntimeError('edge-tts failed: ' + r.stderr.decode(errors='ignore')[-200:])
        raw = os.path.join(td, 'a.wav')
        subprocess.run(['afconvert', '-f', 'WAVE', '-d', 'LEI16@%d' % RATE, '-c', '1', mp3, raw], check=True)
        with wave.open(raw) as rd:
            data = rd.readframes(rd.getnframes())
    return trim_write(data, dst)


def make_wav(voice, text, dst):
    with tempfile.TemporaryDirectory() as td:
        aiff = os.path.join(td, 'a.aiff')
        txt = os.path.join(td, 'a.txt')
        open(txt, 'w', encoding='utf-8').write(text)
        subprocess.run(['say', '-v', voice, '-o', aiff, '-f', txt], check=True)
        raw = os.path.join(td, 'a.wav')
        subprocess.run(['afconvert', '-f', 'WAVE', '-d', 'LEI16@%d' % RATE, '-c', '1', aiff, raw], check=True)
        with wave.open(raw) as r:
            data = r.readframes(r.getnframes())
            assert r.getframerate() == RATE and r.getnchannels() == 1 and r.getsampwidth() == 2
    # 앞뒤 무음 줄이기
    step = 2 * RATE // 100
    def loud(i):
        return audioop.rms(data[i:i + step], 2) > 200
    a = 0
    while a < len(data) - step and not loud(a):
        a += step
    b = len(data) - step
    while b > a and not loud(b):
        b -= step
    data = data[max(0, a - step):min(len(data), b + 2 * step)]
    with wave.open(dst, 'wb') as w:            # 깔끔한 44 바이트 헤더
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(data)
    return len(data) / 2 / RATE


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    out = args[0] if args else os.path.join(HERE, 'out', 'rpg', 'v')
    os.makedirs(out, exist_ok=True)
    pages = parse_pages()
    rec_dir = os.path.join(HERE, 'voice_rec')       # 직접 녹음한 파일: voice_rec/<CRC>.(wav|m4a|aiff)
    script = []
    total_s, total_b, n = 0.0, 0, 0
    seen = set()
    for who, page in pages:
        key = '\n'.join(page).encode('utf-8')
        crc = zlib.crc32(key) & 0xFFFFFFFF
        if crc in seen:
            continue
        seen.add(crc)
        dst = os.path.join(out, '%08X.WAV' % crc)
        if who == '바람':
            script.append('%08X  %s' % (crc, ' / '.join(page)))
        rec = None
        for ext in ('wav', 'm4a', 'aiff', 'mp3'):
            cand = os.path.join(rec_dir, '%08X.%s' % (crc, ext))
            if os.path.exists(cand):
                rec = cand
        if rec:
            sec = convert_rec(rec, dst)
        else:
            if '--say' in sys.argv:
                sec = make_wav(VOICES.get(who, 'Yuna'), speak_text(page), dst)
            else:
                sec = make_wav_edge(who, speak_text(page), dst)
        total_s += sec
        total_b += os.path.getsize(dst)
        n += 1
        print('%08X %-8s %4.1fs  %s' % (crc, who or '-', sec, ' / '.join(page)))
    print('%d files, %.0f s, %.1f MB -> %s' % (n, total_s, total_b / 1e6, out))
    with open(os.path.join(HERE, 'baram_lines.txt'), 'w', encoding='utf-8') as f:
        f.write('# 바람 대사 녹음 목록: 각 줄을 녹음해 tools/rpg/voice_rec/<번호>.wav (또는 m4a) 로 저장\n')
        f.write('#   "/" 는 줄바꿈 (이어서 자연스럽게 읽으면 됨). 녹음이 있으면 TTS 대신 녹음을 쓴다\n')
        f.write('\n'.join(script) + '\n')
    print('바람 대사 %d 개 -> tools/rpg/baram_lines.txt' % len(script))


if __name__ == '__main__':
    main()
