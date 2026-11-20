#!/usr/bin/env python3
"""바람의 펌웨어 수련기 (RPG 프로토타입) 에셋 생성기

타일·스프라이트·맵을 문자 그림으로 정의하고 검사한 뒤 C 헤더로 만든다.
    python3 tools/rpg/gen_assets.py   ->  src/ap/modules/rpg/rpg_assets.h

문자 그림 (16x16)
  타일   : '#' 검정, '.' 흰색, 'x' 50% 체크, '-' 25% 점
  스프라이트: 위와 같고 ' ' 는 투명 (뒤 타일이 보임)
"""
import os
import sys
from collections import deque

# ---------------------------------------------------------------------------
# 타일 (이름, 통과 가능, 그림)
# ---------------------------------------------------------------------------
TILES = []


def tile(name, walk, art):
    rows = [r for r in art.strip('\n').split('\n')]
    assert len(rows) == 16, (name, len(rows))
    for r in rows:
        assert len(r) == 16, (name, r)
    TILES.append((name, walk, rows))


tile('grass', True, '''
................
................
..#.............
................
.........#......
................
................
....#.......#...
................
................
..........#.....
.#..............
................
......#.........
................
................''')

tile('grass2', True, '''
................
.......#........
................
................
..#.........#...
................
................
................
.........#......
................
...#............
................
................
............#...
.....#..........
................''')

tile('flower', True, '''
................
................
....#.#.........
.....#..........
....#.#.........
.....#.....#.#..
......#.....#...
...........#.#..
.....#......#...
................
................
...#.#..........
....#...........
...#.#..........
....#...........
................''')

tile('path', True, '''
-.-.-.-.-.-.-.-.
................
-.-.-.-.-.-.-.-.
................
-.-.-##.-.-.-.-.
.....#..........
-.-.-.-.-.-.-.-.
................
-.-.-.-.-.-.#.-.
...........##...
-.-.-.-.-.-.-.-.
................
-.-##.-.-.-.-.-.
................
-.-.-.-.-.-.-.-.
................''')

tile('tree', False, '''
.....######.....
...##..x.x.##...
..#.x.x.x.x..#..
.#.x.x.x.x.x..#.
.#x.x.x.x.x.x.#.
#.x.x.x.x.x.x.x#
#x.x.x.x.x.x.x.#
#.x.x.x.x.x.x.x#
.#.x.x.x.x.x.x#.
..##.x.x.x.x##..
....###..###....
......#xx#......
......#xx#......
......#x.#......
.....##xx##.....
....#.#..#.#....''')

tile('water', False, '''
-.-.-.-.-.-.-.-.
................
-.-.-.-.-.-.-.-.
...###..........
-.#-.-#.-.-.-.-.
................
-.-.-.-.-.-###-.
..........#...#.
-.-.-.-.-.-.-.-.
................
-.-.###.-.-.-.-.
...#...#........
-.-.-.-.-.-.-.-.
................
-.-.-.-.-.-.-.-.
................''')

tile('bridge', True, '''
################
#..............#
################
#....#.....#...#
################
#..#......#....#
################
#.......#......#
################
#...#......#...#
################
#......#.......#
################
#.#.......#....#
################
#..............#''')

tile('brick', False, '''
################
#......#.......#
#......#.......#
################
...#.......#....
...#.......#....
################
#......#.......#
#......#.......#
################
...#.......#....
...#.......#....
################
#......#.......#
#......#.......#
################''')

tile('roof', False, '''
################
#.#.#.#.#.#.#.#.
################
.#.#.#.#.#.#.#.#
################
#.#.#.#.#.#.#.#.
################
.#.#.#.#.#.#.#.#
################
#.#.#.#.#.#.#.#.
################
.#.#.#.#.#.#.#.#
################
#.#.#.#.#.#.#.#.
################
################''')

tile('door', False, '''
################
#..............#
#.############.#
#.#xxxxxxxxxx#.#
#.#x.x.x.x.x.#.#
#.#xxxxxxxxxx#.#
#.#x.x.x.x.x.#.#
#.#xxxxxxxxxx#.#
#.#x.x.x.x##.#.#
#.#xxxxxxx##x#.#
#.#x.x.x.x.x.#.#
#.#xxxxxxxxxx#.#
#.#x.x.x.x.x.#.#
#.#xxxxxxxxxx#.#
#.############.#
################''')

tile('fence', False, '''
................
................
..##........##..
..##........##..
################
#..............#
################
..##........##..
..##........##..
################
#..............#
################
..##........##..
..##........##..
..##........##..
................''')

tile('rock', False, '''
................
................
.....######.....
...##......##...
..#..........#..
.#...##.......#.
.#..#.........#.
#.............x#
#............x.#
#...........x.x#
#..........x.xx#
.#.......x.x.x#.
..##..x.x.xxx#..
....##########..
................
................''')

tile('sign', False, '''
................
................
.##############.
#..............#
#.####.###.###.#
#..............#
#.###.####.##..#
#..............#
.##############.
.......##.......
.......##.......
.......##.......
.......##.......
......####......
................
................''')

tile('chest', False, '''
................
................
................
..############..
.#............#.
#..x.x.x.x.x...#
#.x.x.x.x.x.x..#
################
#......##......#
#.....#..#.....#
#......##......#
#..x.x.x.x.x...#
#.x.x.x.x.x.x..#
################
................
................''')

tile('chest_open', False, '''
................
..############..
.#xxxxxxxxxxxx#.
#xxxxxxxxxxxxxx#
################
#..............#
#..............#
################
#..............#
#..............#
#..............#
#..x.x.x.x.x...#
#.x.x.x.x.x.x..#
################
................
................''')

tile('gate', False, '''
################
#..............#
################
.#..#..#..#..#..
.#..#..#..#..#..
.#..#..#..#..#..
.#..#.####.#.#..
.#..#.#..#.#.#..
.#..#.####.#.#..
.#..#..##..#.#..
.#..#..##..#.#..
.#..#..#..#..#..
.#..#..#..#..#..
################
#..............#
################''')

tile('altar', False, '''
................
.......##.......
......#..#......
.....#.##.#.....
......#..#......
.......##.......
..#.........#...
....#.....#.....
..############..
..#xxxxxxxxxx#..
..############..
...#........#...
...#.x.x.x.x#...
...#x.x.x.x.#...
..############..
................''')

tile('pillar', False, '''
..############..
..#..........#..
..############..
...#.#....#.#...
...#.#....#.#...
...#.#....#.#...
...#.#....#.#...
...#.#....#.#...
...#.#....#.#...
...#.#....#.#...
...#.#....#.#...
...#.#....#.#...
...#.#....#.#...
..############..
..#xxxxxxxxxx#..
..############..''')

tile('floor', True, '''
################
#..............#
#..............#
#..............#
#..............#
#..............#
#..............#
#..............#
#..............#
#..............#
#..............#
#..............#
#..............#
#..............#
#..............#
################''')

TILE_IDX = {t[0]: i for i, t in enumerate(TILES)}

# 맵 문자 -> 타일
MAP_CHARS = {
    '.': 'grass', ',': 'flower', ':': 'path', 'T': 'tree', '~': 'water',
    '=': 'bridge', 'B': 'brick', 'R': 'roof', 'D': 'door', 'F': 'fence',
    'o': 'rock', 'S': 'sign', 'C': 'chest', 'G': 'gate', 'A': 'altar',
    'P': 'pillar', '_': 'floor',
}

# ---------------------------------------------------------------------------
# 스프라이트 (' ' 투명)
# ---------------------------------------------------------------------------
SPRITES = []


def sprite(name, art):
    rows = [r.rstrip() for r in art.strip('\n').split('\n')]
    assert len(rows) == 16, (name, len(rows))
    rows = [r.ljust(16) for r in rows]
    for r in rows:
        assert len(r) == 16, (name, r)
    SPRITES.append((name, rows))


# 바람 (주인공): 아래 / 위 / 옆(오른쪽, 왼쪽은 좌우 반전) x 걸음 2
sprite('hero_down0', '''
     ######
    #xxxxxx#
   #xxxxxxxx#
   #x######x#
   #.#....#.#
   #..#..#..#
   #........#
    #..##..#
     ######
   ##xxxxxx##
  #.#xxxxxx#.#
  #.#xxxxxx#.#
   ##xxxxxx##
    #..##..#
    #.#  #.#
    ##   ##   ''')

sprite('hero_down1', '''
     ######
    #xxxxxx#
   #xxxxxxxx#
   #x######x#
   #.#....#.#
   #..#..#..#
   #........#
    #..##..#
     ######
   ##xxxxxx##
  #.#xxxxxx#.#
  #.#xxxxxx#.#
   ##xxxxxx##
    #..##..#
    #.#  #.#
     ##   ##  ''')

sprite('hero_up0', '''
     ######
    #xxxxxx#
   #xxxxxxxx#
   #xxxxxxxx#
   #xxxxxxxx#
   #xxxxxxxx#
   #x......x#
    #......#
     ######
   ##xxxxxx##
  #.#xxxxxx#.#
  #.#xxxxxx#.#
   ##xxxxxx##
    #..##..#
    #.#  #.#
    ##   ##   ''')

sprite('hero_up1', '''
     ######
    #xxxxxx#
   #xxxxxxxx#
   #xxxxxxxx#
   #xxxxxxxx#
   #xxxxxxxx#
   #x......x#
    #......#
     ######
   ##xxxxxx##
  #.#xxxxxx#.#
  #.#xxxxxx#.#
   ##xxxxxx##
    #..##..#
    #.#  #.#
     ##   ##  ''')

sprite('hero_side0', '''
     ######
    #xxxxxx#
   #xxxxxxxx#
   #xxxx####.#
   #xxx#....#
   #xx#...#.#
   #xx#.....#
    #x#....#
     ######
    #xxxxxx#
    #x#xxxx#
    #x#xxxx#
    #xxxxxx#
    #..##..#
    #.#  #.#
    ##   ##   ''')

sprite('hero_side1', '''
     ######
    #xxxxxx#
   #xxxxxxxx#
   #xxxx####.#
   #xxx#....#
   #xx#...#.#
   #xx#.....#
    #x#....#
     ######
    #xxxxxx#
    #xxx#xx#
    #xxx#xx#
    #xxxxxx#
     #.##.#
     #.##.#
     ##  ##   ''')

# 선배 개발자 (안경, 머그컵)
sprite('senior', '''
     ######
    ########
   ##########
   #........#
   #.##..##.#
   ##..##..##
   #........#
    #..##..#
     ######
   ##......##
  #..........#
  #.#......#.#
  #.#......####
   ##......#xx#
    #..##..####
    ##    ##  ''')

# 인터럽트 장인 (밀짚모자, 낚싯대)
sprite('master_irq', '''
    ########   #
  ############ #
     #....#   #
     #.#..#  #
     #....# #
      #..# #
     ######
   ##xxxxxx##
  #.#xxxxxx#.#
  #.#xxxxxx#.#
   ##xxxxxx##
    #xxxxxx#
    #xxxxxx#
    #..##..#
    #.#  #.#
    ##   ##   ''')

# 펌웨어 고수 (긴 수염, 지팡이)
sprite('master_fw', '''
      ####     #
     #xxxx#  ###
    ########   #
     #....#    #
     #.#.##    #
     #....#    #
     #....#   ##
      #..#   # #
     #....###  #
    ##xxxx#    #
   #.#xxxx#    #
   #.#xxxx#    #
    ##xxxx#    #
    #xxxxxx#   #
    #xxxxxx#   #
    ########   #''')

# 잔상 유령
sprite('bug_ghost', '''
     ######
   ##......##
  #..........#
  #..##..##..#
  #..##..##..#
  #..........#
  #....##....#
  #..........#
  #.x.x.x.x.x#
  #x.x.x.x.x.#
  #.x.x.x.x.x#
  #x.x.x.x.x.#
  #.x.x.x.x.x#
  #x.#x.#x.#x#
  # # # # # # #
              ''')

# 키 씹힘 벌레
sprite('bug_key', '''
  #          #
   #        #
    #  ##  #
     ######
    #.#..#.#
    #......#
  ##########
 #.#xxxxxx#.#
#..#x.xx.x#..#
 #.#xxxxxx#.#
#..#x.xx.x#..#
 #.#xxxxxx#.#
#..#x.xx.x#..#
  ##########
   #  #  #
  #   #   #   ''')

# 스택 오버플로 슬라임
sprite('bug_stack', '''
      ####
     #....#
    #.####.#
    #......#
   #.######.#
   #........#
  #.########.#
  #..........#
 #..##....##..#
 #..##....##..#
 #............#
 #....####....#
 #............#
#xxxxxxxxxxxxxx#
################
              ''')

# 하드폴트 (보스)
sprite('bug_fault', '''
 #            #
 ##   ####   ##
 #x###xxxx###x#
  #xxxxxxxxxxx#
  #x##xxxx##xx#
  #x..#xx#..xx#
  #x#.#xx#.#xx#
  #xxxxxxxxxxx#
  #xx#.#.#.#xx#
  #xx.#.#.#.xx#
  #xxxxxxxxxxx#
 ##xxxxxxxxxx##
#.#xxxxxxxxxx#.#
#..##########..#
 ##  #    #  ##
     ##  ##       ''')

SPR_IDX = {s[0]: i for i, s in enumerate(SPRITES)}

# ---------------------------------------------------------------------------
# 초상화 32x32 (대화창에 2배로)
# ---------------------------------------------------------------------------
PORTRAITS = []


def portrait(name, art):
    rows = art.strip('\n').split('\n')
    assert len(rows) == 32, (name, len(rows))
    for r in rows:
        assert len(r) == 32, (name, r, len(r))
    PORTRAITS.append((name, rows))


# 바람 (가운데 가르마 덥수룩한 머리, 콧수염·턱수염, 검은 티셔츠)
portrait('baram', '''
................................
...........#########............
........##############.#........
......###################.......
.....#####################......
....#######################.....
....########.####.#########.....
...########...##...#########....
...#######....#.....########....
...######...........#.######....
...#####.............#.#####....
...####................#####....
...####..####.....####..####....
....###.................###.....
...##...###.......###....##.....
...#.#..#.##.....##.#...#.#.....
...#.#..................#.#.....
....##.........#........##......
.....#.........#........#.......
.....#.......###........#.......
.....#.....##.....##....#.......
......#...#..#####..#..#........
......#................#........
.......#.....#####....#.........
........#.....###....#..........
.........##...###..##...........
...........#######..............
........###.......###...........
....#####.#########.#####.......
..##########.....##########.....
.############...############....
################################''')


# ---------------------------------------------------------------------------
# 맵: 3x2 방, 방 하나 = 한 화면 12 x 8 칸 (화면에는 2배, 32x32 로 그린다)
#   0 부트로더 마을  1 마을 동쪽  2 레지스터 숲
#   3 인터럽트 호수  4 커널 사당  5 숲 속
#   출구: 동서는 3~5 행, 남북은 5~6 열
# ---------------------------------------------------------------------------
W, H = 12, 8

ROOMS = [
    # 0 부트로더 마을
    '''
TTTTTTTTTTTT
T.RRRR..,..T
T.BBDB.....T
T...:.......
T...::::::::
T.,.........
T....:.....T
TTTTT..TTTTT''',
    # 1 마을 동쪽
    '''
TTTTTTTTTTTT
T..,...o...T
T.FFFF..S..T
............
::::::::::::
............
T..o....,..T
TTTTTTTTTTTT''',
    # 2 레지스터 숲
    '''
TTTTTTTTTTTT
T.T...T..T.T
T...T....T.T
...T...T...T
::::T......T
.......T.T.T
T.T...:....T
TTTTT..TTTTT''',
    # 3 인터럽트 호수
    '''
TTTTT..TTTTT
T....:.....T
T~~~~=~~~~.T
T~~~~=~~~~.T
T....:...o.T
T.S..:.....T
T..,.:..,..T
TTTTTTTTTTTT''',
    # 4 커널 사당
    '''
TTTTTTTTTTTT
TFFFFFFFFF.T
TF_P_A_P_F.T
TF_______F..
TF_______G..
TF_P___P_F..
TFFFFFFFFF.T
TTTTTTTTTTTT''',
    # 5 숲 속
    '''
TTTTT..TTTTT
T.T...:..T.T
T...T.:..C.T
......:.T..T
.......T...T
...T.....T.T
T.T..T...T.T
TTTTTTTTTTTT''',
]

ROOM_NAMES = ['부트로더 마을', '마을 동쪽', '레지스터 숲', '인터럽트 호수', '커널 사당', '숲 속']

# 이웃 방: (북, 남, 서, 동), -1 = 없음
ROOM_LINK = [
    (-1, 3, -1, 1),
    (-1, -1, 0, 2),
    (-1, 5, 1, -1),
    (0, -1, -1, -1),
    (-1, -1, -1, 5),
    (2, -1, 4, -1),
]

# 엔티티: (방, x, y, 스프라이트, 종류, 값)
#   종류 NPC: 값 = 대화 번호 / BUG: 값 = 적 번호
ENTITIES = [
    (0, 8, 5, 'senior', 'NPC', 0),
    (3, 3, 4, 'master_irq', 'NPC', 1),
    (4, 5, 4, 'master_fw', 'NPC', 2),
    (2, 8, 2, 'bug_ghost', 'BUG', 0),
    (5, 4, 5, 'bug_key', 'BUG', 1),
    (3, 9, 6, 'bug_stack', 'BUG', 2),
]

START = (0, 6, 5)   # 방, x, y


def parse_room(i, art):
    rows = art.strip('\n').split('\n')
    assert len(rows) == H, ('room', i, len(rows))
    for r in rows:
        assert len(r) == W, ('room', i, r, len(r))
        for c in r:
            assert c in MAP_CHARS, ('room', i, c)
    return rows


def walkable(c):
    return TILES[TILE_IDX[MAP_CHARS[c]]][1]


def check(rooms):
    # 출구 맞춤
    n, s, w, e = 0, 1, 2, 3
    for i, (ln, ls, lw, le) in enumerate(ROOM_LINK):
        if le >= 0:
            for y in range(H):
                a, b = walkable(rooms[i][y][W - 1]), walkable(rooms[le][y][0])
                assert a == b, ('east/west edge mismatch', i, le, y)
        if ls >= 0:
            for x in range(W):
                a, b = walkable(rooms[i][H - 1][x]), walkable(rooms[ls][0][x])
                assert a == b, ('south/north edge mismatch', i, ls, x)
        # 링크가 없는 가장자리는 막혀 있어야 한다
        for y in range(H):
            if lw < 0: assert not walkable(rooms[i][y][0]), ('open west edge', i, y)
            if le < 0: assert not walkable(rooms[i][y][W - 1]), ('open east edge', i, y)
        for x in range(W):
            if ln < 0: assert not walkable(rooms[i][0][x]), ('open north edge', i, x)
            if ls < 0: assert not walkable(rooms[i][H - 1][x]), ('open south edge', i, x)

    # 도달 가능성 (문은 열린 것으로, 엔티티는 막힘이지만 옆에서 말 걸기)
    def solid(r, x, y):
        c = rooms[r][y][x]
        if c == 'G':
            return False
        return not walkable(c)

    blocked = {(r, x, y) for (r, x, y, *_rest) in ENTITIES}
    seen = set()
    q = deque([START])
    seen.add(START)
    while q:
        r, x, y = q.popleft()
        for dx, dy, d in ((0, -1, 0), (0, 1, 1), (-1, 0, 2), (1, 0, 3)):
            nr, nx, ny = r, x + dx, y + dy
            if nx < 0 or nx >= W or ny < 0 or ny >= H:
                nr = ROOM_LINK[r][d]
                if nr < 0:
                    continue
                nx %= W
                ny %= H
            if solid(nr, nx, ny) or (nr, nx, ny) in blocked:
                continue
            if (nr, nx, ny) not in seen:
                seen.add((nr, nx, ny))
                q.append((nr, nx, ny))

    def adjacent_reachable(r, x, y):
        return any((r, x + dx, y + dy) in seen for dx, dy in ((0, 1), (0, -1), (1, 0), (-1, 0)))

    for (r, x, y, spr, kind, val) in ENTITIES:
        assert adjacent_reachable(r, x, y), ('entity unreachable', spr)
    for r in range(len(rooms)):
        for y in range(H):
            for x in range(W):
                if rooms[r][y][x] in 'CGAS':
                    assert adjacent_reachable(r, x, y), ('object unreachable', r, x, y, rooms[r][y][x])


def bits(rows, on):
    out = []
    for y, r in enumerate(rows):
        v = 0
        for x, c in enumerate(r):
            if on(c, x, y):
                v |= 0x8000 >> x
        out.append(v)
    return out


def ink(c, x, y):
    if c == '#':
        return True
    if c == 'x':
        return (x + y) % 2 == 0
    if c == '-':
        return x % 2 == 0 and y % 2 == 0
    return False


def main():
    rooms = [parse_room(i, a) for i, a in enumerate(ROOMS)]
    check(rooms)

    o = []
    o.append('// 자동 생성: tools/rpg/gen_assets.py (직접 고치지 말 것)')
    o.append('#ifndef RPG_ASSETS_H_')
    o.append('#define RPG_ASSETS_H_')
    o.append('')
    o.append('#define RPG_MAP_W      %d' % W)
    o.append('#define RPG_MAP_H      %d' % H)
    o.append('#define RPG_ROOM_CNT   %d' % len(rooms))
    o.append('#define RPG_TILE_CNT   %d' % len(TILES))
    o.append('#define RPG_SPR_CNT    %d' % len(SPRITES))
    o.append('#define RPG_ENT_CNT    %d' % len(ENTITIES))
    o.append('')
    for i, t in enumerate(TILES):
        o.append('#define RPG_T_%-12s %d' % (t[0].upper(), i))
    o.append('')
    for i, s in enumerate(SPRITES):
        o.append('#define RPG_S_%-12s %d' % (s[0].upper(), i))
    o.append('')
    o.append('// 타일: 비트 1 = 검정, MSB 가 왼쪽')
    o.append('static const uint16_t rpg_tile_bits[RPG_TILE_CNT][16] = {')
    for name, walk, rows in TILES:
        o.append('  { ' + ', '.join('0x%04X' % v for v in bits(rows, ink)) + ' },  // ' + name)
    o.append('};')
    o.append('')
    o.append('static const uint8_t rpg_tile_walk[RPG_TILE_CNT] = { ' +
             ', '.join('1' if t[1] else '0' for t in TILES) + ' };')
    o.append('')
    o.append('// 스프라이트: bits 1 = 검정, mask 1 = 그림 (0 = 투명)')
    o.append('static const uint16_t rpg_spr_bits[RPG_SPR_CNT][16] = {')
    for name, rows in SPRITES:
        o.append('  { ' + ', '.join('0x%04X' % v for v in bits(rows, ink)) + ' },  // ' + name)
    o.append('};')
    o.append('static const uint16_t rpg_spr_mask[RPG_SPR_CNT][16] = {')
    for name, rows in SPRITES:
        o.append('  { ' + ', '.join('0x%04X' % v for v in bits(rows, lambda c, x, y: c != ' ')) + ' },  // ' + name)
    o.append('};')
    o.append('')
    o.append('#define RPG_PORTRAIT_CNT %d' % len(PORTRAITS))
    for i, pr in enumerate(PORTRAITS):
        o.append('#define RPG_P_%-10s %d' % (pr[0].upper(), i))
    o.append('static const uint32_t rpg_portrait[RPG_PORTRAIT_CNT][32] = {')
    for name, rows in PORTRAITS:
        vals = []
        for y, r in enumerate(rows):
            v = 0
            for x, c in enumerate(r):
                if ink(c, x, y):
                    v |= 0x80000000 >> x
            vals.append('0x%08X' % v)
        o.append('  {  // ' + name)
        for k in range(0, 32, 8):
            o.append('    ' + ', '.join(vals[k:k + 8]) + ',')
        o.append('  },')
    o.append('};')
    o.append('')
    o.append('static const uint8_t rpg_map[RPG_ROOM_CNT][RPG_MAP_H][RPG_MAP_W] = {')
    for i, rows in enumerate(rooms):
        o.append('  {  // %d %s' % (i, ROOM_NAMES[i]))
        for r in rows:
            o.append('    { ' + ','.join('%2d' % TILE_IDX[MAP_CHARS[c]] for c in r) + ' },')
        o.append('  },')
    o.append('};')
    o.append('')
    o.append('static const char *const rpg_room_name[RPG_ROOM_CNT] = { ' +
             ', '.join('"%s"' % n for n in ROOM_NAMES) + ' };')
    o.append('static const int8_t rpg_room_link[RPG_ROOM_CNT][4] = {  // 북, 남, 서, 동')
    for l in ROOM_LINK:
        o.append('  { %d, %d, %d, %d },' % l)
    o.append('};')
    o.append('')
    o.append('typedef struct { uint8_t room, x, y, spr, kind, val; } rpg_ent_def_t;')
    o.append('#define RPG_ENT_NPC  0')
    o.append('#define RPG_ENT_BUG  1')
    o.append('static const rpg_ent_def_t rpg_ent_def[RPG_ENT_CNT] = {')
    for (r, x, y, spr, kind, val) in ENTITIES:
        o.append('  { %d, %d, %d, RPG_S_%s, RPG_ENT_%s, %d },' % (r, x, y, spr.upper(), kind, val))
    o.append('};')
    o.append('')
    o.append('#define RPG_START_ROOM %d' % START[0])
    o.append('#define RPG_START_X    %d' % START[1])
    o.append('#define RPG_START_Y    %d' % START[2])
    o.append('')
    o.append('#endif')

    here = os.path.dirname(os.path.abspath(__file__))
    dst = os.path.join(here, '..', '..', 'src', 'ap', 'modules', 'game', 'rpg', 'rpg_assets.h')
    with open(dst, 'w') as f:
        f.write('\n'.join(o) + '\n')
    print('ok: %d tiles, %d sprites, %d rooms -> %s' % (len(TILES), len(SPRITES), len(rooms), os.path.normpath(dst)))

    if '--preview' in sys.argv:
        # 방 미리보기 PNG (PIL)
        from PIL import Image
        for i, rows in enumerate(rooms):
            im = Image.new('1', (W * 16, H * 16), 1)
            px = im.load()
            for ty, r in enumerate(rows):
                for tx, c in enumerate(r):
                    t = TILES[TILE_IDX[MAP_CHARS[c]]][2]
                    for y in range(16):
                        for x in range(16):
                            if ink(t[y][x], x, y):
                                px[tx * 16 + x, ty * 16 + y] = 0
            for (r, ex, ey, spr, kind, val) in ENTITIES:
                if r != i:
                    continue
                srows = SPRITES[SPR_IDX[spr]][1]
                for y in range(16):
                    for x in range(16):
                        c = srows[y][x]
                        if c != ' ':
                            px[ex * 16 + x, ey * 16 + y] = 0 if ink(c, x, y) else 1
            out = sys.argv[sys.argv.index('--preview') + 1]
            im.resize((W * 32, H * 32)).save('%s_room%d.png' % (out, i))
        for name, rows in PORTRAITS:
            im = Image.new('1', (32, 32), 1)
            for y, r in enumerate(rows):
                for x, c in enumerate(r):
                    if ink(c, x, y):
                        im.putpixel((x, y), 0)
            im.resize((256, 256)).save('%s_portrait_%s.png' % (out, name))


if __name__ == '__main__':
    main()
