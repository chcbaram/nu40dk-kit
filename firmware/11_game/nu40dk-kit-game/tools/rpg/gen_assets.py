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


tile('hedge', False, '''
................
...####...####..
..#x.x.#.#x.x#..
.#.x.x.x#.x.x.#.
.#x.x.x.#x.x.x#.
#.x.x.x.x.x.x.x#
#x.x.x.x.x.x.x.#
#.x.x.x.x.x.x.x#
#x.x.x.x.x.x.x.#
.#.x.x.x.x.x.x#.
..#x.x.x.x.x.#..
.#.x.x.x.x.x.x#.
#.x.x.x.x.x.x.x#
#x.x.x.x.x.x.x.#
.##############.
................''')

tile('reed', False, '''
................
..#.....#.......
..#..#..#....#..
.###.#..#..#.#..
.###.#.###.#.#..
..#..#.###.#.#..
..#.###.#..#.#..
..#.###.#.###...
..#..#..#.###.#.
..#..#..#..#..#.
.#.#.#..#..#.###
.#.#.#.#.#.#.###
#...#..#.#.#..#.
-.-.-.-.-.-.-.-.
................
-.-.-.-.-.-.-.-.''')

tile('stonewall', False, '''
################
#....#.....#...#
#.x..#..x..#.x.#
#....#.....#...#
################
#..#......#....#
#.x#..x...#..x.#
#..#......#....#
################
#.....#.....#..#
#..x..#..x..#.x#
#.....#.....#..#
################
#..#.....#.....#
#.x#..x..#..x..#
################''')

tile('cliff', False, '''
################
#x.x.x.##.x.x.x#
#.x.x#..#x.x.x.#
#x.x#....#.x.x.#
##.#......##.x.#
#.#.........#.##
#x...........#.#
#.x.........x.x#
#x.x.......x.x.#
#.x.x.x.x.x.x.x#
##x.x.x.x.x.x.##
#.##x.x.x.x.##.#
#x.x###x.###x.x#
#.x.x.x#.x.x.x.#
#x.x.x.x.x.x.x.#
################''')

tile('mount', False, '''
................
.......##.......
......#..#......
.....#.x..#.....
....#.x.x..#....
...#.x.x.x..#...
..#.x.x.x.x..#..
..#x.x.x.x.x.#..
.#.x.x.x.x.x..#.
.#x.x.x.x.x.x.#.
#.x.x.x.x.x.x..#
#x.x.x.x.x.x.x.#
################
................
................
................''')

tile('cactus', False, '''
-.......-.......
.......##.......
......#..#......
......#..#.##...
..##..#..#.#.#..
.#..#.#..#.#.#..
.#..#.#..#.#.#..
.#..###..###.#..
..#..........#..
...####..####...
......#..#......
......#..#......
......#..#......
-....######...-.
................
....-.......-...''')

tile('sand', True, '''
-.......-.......
................
....-.......-...
................
-.......-.......
................
....-.......-...
................
-.......-.......
................
....-.......-...
................
-.......-.......
................
....-.......-...
................''')

TILE_IDX = {t[0]: i for i, t in enumerate(TILES)}

# 맵 문자 -> 타일
MAP_CHARS = {
    '.': 'grass', ',': 'flower', ':': 'path', 'T': 'tree', '~': 'water',
    '=': 'bridge', 'B': 'brick', 'R': 'roof', 'D': 'door', 'F': 'fence',
    'o': 'rock', 'S': 'sign', 'C': 'chest', 'G': 'gate', 'A': 'altar',
    'P': 'pillar', '_': 'floor', 'H': 'hedge', 'r': 'reed', 'W': 'stonewall',
    'X': 'cliff', 'M': 'mount', 'K': 'cactus', 's': 'sand',
}


# ---------------------------------------------------------------------------
# 스프라이트 (' ' 투명)
# ---------------------------------------------------------------------------
SPRITES = []


def sprite(name, art):
    rows = art.split('\n')
    if rows and rows[0] == '':
        rows = rows[1:]                          # ''' 바로 뒤 줄바꿈
    rows = [r.rstrip() for r in rows]
    assert len(rows) <= 16, (name, len(rows))
    rows = rows + [''] * (16 - len(rows))        # 아래쪽 빈 줄 채움
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


# 마을 사람
sprite('villager', '''
     ######
    #xxxxxx#
   #xxxxxxxx#
   #x......x#
   #.#....#.#
   #........#
    #..##..#
     ######
    #......#
   #.x.x.x.x#
   #x.x.x.x.#
   #.x.x.x.x#
    ########
     #.  .#
     #.  .#
     ##  ##''')

# 데이터시트 현자 (뾰족 모자, 책)
sprite('sage', '''
       ##
      #xx#
     #xxxx#
    ########
     #....#
     #.#.##
     #....#
    ##....##
   #.######.#
   #.#....#.#
   #.#.##.#.#
   #.#....#.#
   #.######.#
    #xxxxxx#
    #xxxxxx#
    ########''')

# DMA 장인 (고글, 렌치)
sprite('dma', '''
     ######
    #xxxxxx#
   ##########
   #.##..##.#
   #.##..##.#
   #........#
    #..##..#  #
     ######  ##
   ##xxxxxx##
  #.#xxxxxx#
  #.#xxxxxx#
   ##xxxxxx#
    #xxxxxx#
    #..##..#
    #.#  #.#
    ##   ##''')

# 스케줄러 수도승 (민머리, 승복)
sprite('monk', '''
     ######
    #......#
   #........#
   #.##..##.#
   #........#
   #...##...#
    #......#
     ######
   ##xxxxxx##
  #x#xxxxxx#x#
  #x#xxxxxx#x#
   ##xxxxxx##
    #xxxxxx#
    #xxxxxx#
    ########
     ##  ##''')

# 배터리 은둔자 (두건, 배터리 지팡이)
sprite('hermit', '''
  ########   ##
 #xxxxxxxx# #..#
#xx#....#xx##..#
#x#.#..#.#x##..#
#x#......#x# ##
 ##.####.##  #
  #......#   #
 #xxxxxxxx####
#xxxxxxxxxx# #
#xxxxxxxxxx# #
#xxxxxxxxxx# #
#xxxxxxxxxx# #
#xxxxxxxxxx# #
############ #
 ##     ##
''')

# 애벌레 (작은 버그)
sprite('larva', '''

     #  #
      ##
    ######
   #......#
   #.#..#.#
   #......#
    #x.x.x#####
   #x.x.x#x.x.x#
   #.x.x.#.x.x.#
    ####x.x.x.x#
       #########
        # # # #

''')

# 나방
sprite('moth', '''

  ##        ##
 #xx#  ##  #xx#
#x.xx#.##.#xx.x#
#.x.xx#..#xx.x.#
#x.x.x#..#x.x.x#
 #x.x.#..#.x.x#
  ####.##.####
   #x.x#..#.x#
   #.x.#..#x.#
    ###.##.###
       #..#
        ##


''')

# 레이스 컨디션 쌍둥이 (값 하나를 둘이 잡아당김)
sprite('twins', '''

  ####    ####
 #....#  #....#
#.#..#.##.#..#.#
#......##......#
#..##..##..##..#
 #....####....#
  #####..#####
   #.#.##.#.#
   ##########
   #.x.x.x.x#
   #x.x.x.x.#
   ##########


''')

# 데드락 골렘 (자물쇠 가슴)
sprite('golem', '''
    ########
    #......#
    #.#..#.#
    #......#
  ############
 #x#........#x#
 #x#..####..#x#
 #x#..#..#..#x#
 #x#.######.#x#
 ###.##..##.###
    #.######.#
    #........#
    ###....###
    #x#    #x#
    #x#    #x#
    ###    ###''')

# 누설전류 모래벌레
sprite('worm', '''
      ####
     #....#
    #.#..#.#
    #......#
    #.####.#
     #x.x.#
     #.x.x#
     #x.x.#
     #.x.x#
    #x.x.x.#
   #.x.x.x.x#
-.-#x.x.x.x#-.-.
.-.-######-.-.-.
-.-.-.-.-.-.-.-.

''')

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
# 전투 그림 32x32 (' ' 투명). 전투 화면에 3 배로 그린다
# ---------------------------------------------------------------------------
BATTLE = []


def battle(name, art):
    rows = [r.rstrip() for r in art.strip('\n').split('\n')]
    assert len(rows) <= 32, (name, len(rows))
    for r in rows:
        assert len(r) <= 32, (name, r, len(r))
    pad = (32 - len(rows)) // 2
    rows = [''] * (32 - len(rows) - pad) + rows + [''] * pad   # 아래쪽 정렬
    BATTLE.append((name, [r.ljust(32) for r in rows]))


battle('b_larva', '''
          #      #
           #    #
            #  #
          ########
         #........#
        #..##..##..#
        #..##..##..#
        #..........#
        #...####...#
         #........#######
          ###x.x.x#x.x.x.##
         #x.x.x.x#x.x.x.x.x#
        #.x.x.x.x#.x.x.x.x.x#
        #x.x.x.x.#x.x.x.x.x.x#
         #.x.x.x.#.x.x.x.x.x.x#
          ########x.x.x.x.x.x.#
             #.#  #.x.x.x.x.x.#
             # #   ############
                    # # # # #''')

battle('b_moth', '''
   ####                  ####
  #x.x.#                #.x.x#
 #x.x.x.#     #  #     #.x.x.x#
 #.x##x.x#     ##     #.x.##.x#
#x.####.x.#  ######  #.x.####.x#
#.x.##.x.x.##......##.x.x.##.x.#
#x.x.x.x.x.#..#..#..#.x.x.x.x.x#
#.x.x.x.x.x#........#x.x.x.x.x.#
 #x.x.x.x.x#..####..#.x.x.x.x.#
  ##x.x.x.##........##x.x.x.##
    ######.#......#.######
      #.x.x#......#x.x.#
     #x.##x.#....#.x##.x#
     #.x##.x#....#x.##x.#
      #x.x.#  ##  #.x.x#
       ####        ####''')

battle('b_ghost', '''
           ##########
         ##..........##
       ##..............##
      #..................#
     #....................#
    #......................#
    #.....###......###.....#
   #.....#####....#####.....#
   #.....##.##....##.##.....#
   #.....#####....#####.....#
   #......###......###......#
   #........................#
   #...........####.........#
   #..........#....#........#
   #...........####.........#
   #........................#
   #.x.x.x.x.x.x.x.x.x.x.x.x#
   #x.x.x.x.x.x.x.x.x.x.x.x.#
   #.x.x.x.x.x.x.x.x.x.x.x.x#
   #x.x.x.x.x.x.x.x.x.x.x.x.#
   #.x.x.##.x.x.##.x.x.##.x.#
   #x.x.#  #x.x#  #x.x#  #.x#
   #.x.#    #x#    #x#    #x#
   ###       #      #      ##''')

battle('b_key', '''
        #            #
         #          #
          ##########
        ##..........##
       #..###....###..#
       #..###....###..#
       #..............#
   ##  #....######....#  ##
  #..# #...#......#...# #..#
   ##   #...######...#   ##
     ##################
    #x.x.x.x.x.x.x.x.x.#
   #x.x.##########.x.x.x#
   #.x.#..........#x.x.x.#
   #x.x#.###..###.#.x.x.x#  ##
   #.x.#.#..##..#.#x.x.x.###..#
   #x.x#.###..###.#.x.x.x#  ##
   #.x.#..........#x.x.x.#
   #x.x.##########.x.x.x.#
    #x.x.x.x.x.x.x.x.x.x#
     ##################
      #  #    #  #    #
     #  #    #  #    #''')

battle('b_stack', '''
             ######
           ##......##
          #..#....#..#
          #..........#
           ##......##
    ######################
    #....................#
    #..##..........##....#
    #..##..........##....#
    ######################
    #x.x.x.x.x.x.x.x.x.x.#
    #.x.x.x.x.x.x.x.x.x.x#
    ######################
    #....................#
    #....##########......#
    #....................#
    ######################
    #x.x.x.x.x.x.x.x.x.x.#
    #.x.x.x.x.x.x.x.x.x.x#
  ##########################
 #..........................#
#..x.x.x.x.x.x.x.x.x.x.x.x...#
##############################''')

battle('b_twins', '''
     ######          ######
   ##......##      ##......##
  #..........#    #..........#
 #..##....##..#  #..##....##..#
 #..##....##..#  #..##....##..#
 #............#  #............#
 #...######...#  #...######...#
 #....#..#....#  #....#..#....#
  #..........#    #..........#
   ##......##      ##......##
     ###.###########.###
        #.#.......#.#
        ##############
        #x.x.x.x.x.x.#
        #.x.x.x.x.x.x#
        #x.x.x.x.x.x.#
        ##############
      #.#            #.#
     ##.##          ##.##''')

battle('b_golem', '''
          ############
          #..........#
          #.###..###.#
          #.###..###.#
          #..........#
          #...####...#
          ############
   ##########################
  #x.x.#.................#x.x#
  #.x.x#....########.....#.x.#
  #x.x.#....#......#.....#x.x#
  #.x.x#...##.####.##....#.x.#
  #x.x.#...#..#..#..#....#x.x#
  #.x.x#...#...##...#....#.x.#
  #x.x.#...#........#....#x.x#
  ######...##########....######
       #.................#
       #x.x.x.x.x.x.x.x.x#
       ######.....#######
       #x.x.#     #x.x.#
       #.x.x#     #.x.x#
       #x.x.#     #x.x.#
      ########   ########''')

battle('b_worm', '''
            ########
          ##........##
         #...##..##...#
         #...##..##...#
         #............#
         #..########..#
         #..#.#.#.#.#.#
          #..........#
          #x.x.x.x.x.#
          #.x.x.x.x.x#     ##
          #x.x.x.x.x.#    #..#
          #.x.x.x.x.x#   #.##.#
         #x.x.x.x.x.x.#   #..#
        #.x.x.x.x.x.x.x#   ##
       #x.x.x.x.x.x.x.x.#
-.-.-.#.x.x.x.x.x.x.x.x.x#.-.-.-
.-.-.-##################-.-.-.-.
-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.''')

battle('b_fault', '''
     #  #  #  #  #  #  #  #
     #  #  #  #  #  #  #  #
   ########################
   #x.x.x.x.x.#x.x.x.x.x.x#
####.##....#x.#.x#....##.####
   #.x.##.##.x#x.##.##.x.x#
####.x.#####.x.#x.#####.x####
   #.x.x.x.x.x#x.x.x.x.x.x#
####.x.x.x.x.x.#x.x.x.x.x####
   #.x.x.######.#######.x.#
####.x.x#.#.#.#.#.#.#.#x.####
   #.x.x.x.x.x.#x.x.x.x.x.#
   ########################
     #  #  #  #  #  #  #  #
     #  #  #  #  #  #  #  #''')


# ---------------------------------------------------------------------------
# 이야기 (story.py) 읽기, 검사, C 헤더 출력
# ---------------------------------------------------------------------------
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import story as S  # noqa: E402

W, H = S.W, S.H


def parse_rooms():
    rooms = []
    for i, (name, border, ckpt, art) in enumerate(S.ROOMS):
        rows = art.strip('\n').split('\n')
        assert len(rows) == H, ('room', name, len(rows))
        rows = [''.join(border if (c == 'T' and (y in (0, H - 1) or x in (0, W - 1))) else c
                        for x, c in enumerate(r)) for y, r in enumerate(rows)]
        for r in rows:
            assert len(r) == W, ('room', name, r)
            for c in r:
                assert c in MAP_CHARS, ('room', name, repr(c))
        rooms.append(rows)
    return rooms


def walkable(c):
    if c == 'G':
        return True                      # 철문은 열린 것으로 (도달 가능성·가장자리 검사)
    return TILES[TILE_IDX[MAP_CHARS[c]]][1]


def links(n):
    # 동서로 한 줄: (북, 남, 서, 동)
    return [(-1, -1, i - 1 if i > 0 else -1, i + 1 if i < n - 1 else -1) for i in range(n)]


def check(rooms):
    L = links(len(rooms))
    for i, (n, s_, w, e) in enumerate(L):
        for y in range(H):
            if e >= 0:
                assert walkable(rooms[i][y][W - 1]) == walkable(rooms[e][y][0]), ('edge', i, e, y)
            else:
                assert not walkable(rooms[i][y][W - 1]), ('open east', i, y)
            if w < 0:
                assert not walkable(rooms[i][y][0]), ('open west', i, y)
        for x in range(W):
            assert not walkable(rooms[i][0][x]) and not walkable(rooms[i][H - 1][x]), ('open n/s', i, x)
    for i, g in S.GATES.items():
        assert any('G' in r for r in rooms[i]), ('gate missing', i)
    blocked = {(r, x, y) for (r, x, y, spr, kind, val) in S.ENTITIES if kind != 'OBJ'}
    start = S.START
    seen = {start}
    q = deque([start])
    while q:
        r, x, y = q.popleft()
        for dx, dy, d in ((0, -1, 0), (0, 1, 1), (-1, 0, 2), (1, 0, 3)):
            nr, nx, ny = r, x + dx, y + dy
            if nx < 0 or nx >= W or ny < 0 or ny >= H:
                nr = L[r][d]
                if nr < 0:
                    continue
                nx %= W
                ny %= H
            if not walkable(rooms[nr][ny][nx]) or (nr, nx, ny) in blocked:
                continue
            if (nr, nx, ny) not in seen:
                seen.add((nr, nx, ny))
                q.append((nr, nx, ny))
    for (r, x, y, spr, kind, val) in S.ENTITIES:
        ok = any((r, x + dx, y + dy) in seen for dx, dy in ((0, 1), (0, -1), (1, 0), (-1, 0)))
        assert ok, ('entity unreachable', r, x, y, val)
        if kind == 'OBJ':
            assert not walkable(rooms[r][y][x]), ('object on walkable tile', r, x, y)
        else:
            assert walkable(rooms[r][y][x]), ('entity on solid tile', r, x, y, val)
    # 스크립트 주인·참조 검사
    owners = {val for (r, x, y, spr, kind, val) in S.ENTITIES if kind != 'BUG'}
    for sc in S.SCRIPTS:
        assert sc[0] in owners, ('script owner', sc[0])
        cond, quiz, act = sc[1], sc[3], sc[4]
        for k in ('need', 'nope'):
            if k in cond:
                assert cond[k] in S.FLAG_IDX, cond
        if quiz:
            assert quiz in S.QUIZZES, quiz
        for k in ('set', 'win_set'):
            if k in act:
                assert act[k] in S.FLAG_IDX, act
        if 'skill' in act:
            assert act['skill'] in S.SKILL_IDX, act
        if 'battle' in act:
            assert act['battle'] in S.ENEMY_IDX, act
    # 대사 폭 (한글 16px, 영문 8px), 대화창 안쪽 약 360px
    def width(t):
        return sum(16 if ord(ch) > 0x7F else 8 for ch in t)
    def lines_of(blocks):
        for who, ls in blocks:
            for l in ls:
                yield l
    allt = []
    for sc in S.SCRIPTS:
        allt += list(lines_of(sc[2])) + list(lines_of(sc[5]))
    for q in S.QUIZZES.values():
        allt += q[0] + q[3] + q[4]
    allt += list(lines_of(S.INTRO))
    for t in allt:
        assert width(t) <= 360, ('line too wide', t, width(t))


def bits(rows, on):
    out = []
    for y, r in enumerate(rows):
        v = 0
        for x, c in enumerate(r):
            if on(c, x, y):
                v |= (1 << (len(r) - 1 - x))
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


def cstr(t):
    return '"' + t.replace('\\', '\\\\').replace('"', '\\"') + '"'


def main():
    rooms = parse_rooms()
    check(rooms)

    o = []
    A = o.append
    A('// 자동 생성: tools/rpg/gen_assets.py + story.py (직접 고치지 말 것)')
    A('#ifndef RPG_ASSETS_H_')
    A('#define RPG_ASSETS_H_')
    A('')
    A('#define RPG_MAP_W      %d' % W)
    A('#define RPG_MAP_H      %d' % H)
    A('#define RPG_ROOM_CNT   %d' % len(rooms))
    A('#define RPG_TILE_CNT   %d' % len(TILES))
    A('#define RPG_SPR_CNT    %d' % len(SPRITES))
    A('#define RPG_BAT_CNT    %d' % len(BATTLE))
    A('#define RPG_ENT_CNT    %d' % len(S.ENTITIES))
    A('#define RPG_SKILL_CNT  %d' % len(S.SKILLS))
    A('#define RPG_ITEM_CNT   %d' % len(S.ITEMS))
    A('#define RPG_EQUIP_CNT  %d' % len(S.EQUIPS))
    A('#define RPG_ENEMY_CNT  %d' % len(S.ENEMIES))
    A('#define RPG_FLAG_CNT   %d' % len(S.FLAGS))
    A('')
    for i, t in enumerate(TILES):
        A('#define RPG_T_%-12s %d' % (t[0].upper(), i))
    for i, sp in enumerate(SPRITES):
        A('#define RPG_S_%-12s %d' % (sp[0].upper(), i))
    for i, b in enumerate(BATTLE):
        A('#define RPG_B_%-12s %d' % (b[0][2:].upper(), i))
    for i, f in enumerate(S.FLAGS):
        A('#define RPG_F_%-12s %d' % (f, i))
    A('')
    A('static const uint16_t rpg_tile_bits[RPG_TILE_CNT][16] = {')
    for name, walk, rows in TILES:
        A('  { ' + ', '.join('0x%04X' % v for v in bits(rows, ink)) + ' },  // ' + name)
    A('};')
    A('static const uint8_t rpg_tile_walk[RPG_TILE_CNT] = { ' + ', '.join('1' if t[1] else '0' for t in TILES) + ' };')
    A('')
    A('static const uint16_t rpg_spr_bits[RPG_SPR_CNT][16] = {')
    for name, rows in SPRITES:
        A('  { ' + ', '.join('0x%04X' % v for v in bits(rows, ink)) + ' },  // ' + name)
    A('};')
    A('static const uint16_t rpg_spr_mask[RPG_SPR_CNT][16] = {')
    for name, rows in SPRITES:
        A('  { ' + ', '.join('0x%04X' % v for v in bits(rows, lambda c, x, y: c != ' ')) + ' },')
    A('};')
    A('')
    A('// 전투 그림 32x32 (비트 1 = 검정, MSB 왼쪽)')
    A('static const uint32_t rpg_bat_bits[RPG_BAT_CNT][32] = {')
    for name, rows in BATTLE:
        A('  { ' + ', '.join('0x%08X' % v for v in bits(rows, ink)) + ' },  // ' + name)
    A('};')
    A('static const uint32_t rpg_bat_mask[RPG_BAT_CNT][32] = {')
    for name, rows in BATTLE:
        A('  { ' + ', '.join('0x%08X' % v for v in bits(rows, lambda c, x, y: c != ' ')) + ' },')
    A('};')
    A('')
    A('#define RPG_PORTRAIT_CNT %d' % len(PORTRAITS))
    for i, pr in enumerate(PORTRAITS):
        A('#define RPG_P_%-10s %d' % (pr[0].upper(), i))
    A('static const uint32_t rpg_portrait[RPG_PORTRAIT_CNT][32] = {')
    for name, rows in PORTRAITS:
        A('  { ' + ', '.join('0x%08X' % v for v in bits(rows, ink)) + ' },  // ' + name)
    A('};')
    A('')
    A('static const uint8_t rpg_map[RPG_ROOM_CNT][RPG_MAP_H][RPG_MAP_W] = {')
    for i, rows in enumerate(rooms):
        A('  {  // %d %s' % (i, S.ROOMS[i][0]))
        for r in rows:
            A('    { ' + ','.join('%2d' % TILE_IDX[MAP_CHARS[c]] for c in r) + ' },')
        A('  },')
    A('};')
    A('static const char *const rpg_room_name[RPG_ROOM_CNT] = { ' + ', '.join(cstr(r[0]) for r in S.ROOMS) + ' };')
    A('static const uint8_t rpg_room_ckpt[RPG_ROOM_CNT] = { ' + ', '.join('1' if r[2] else '0' for r in S.ROOMS) + ' };')
    A('static const int8_t rpg_room_gate[RPG_ROOM_CNT] = { ' +
      ', '.join(str(S.FLAG_IDX[S.GATES[i]]) if i in S.GATES else '-1' for i in range(len(rooms))) + ' };')
    A('static const int8_t rpg_room_link[RPG_ROOM_CNT][4] = {  // 북, 남, 서, 동')
    for l in links(len(rooms)):
        A('  { %d, %d, %d, %d },' % l)
    A('};')
    A('')
    A('typedef struct { const char *name; uint8_t lo, hi; const char *msg; } rpg_skill_t;')
    A('static const rpg_skill_t rpg_skill[RPG_SKILL_CNT] = {')
    for n, lo, hi, m in S.SKILLS:
        A('  { %s, %d, %d, %s },' % (cstr(n), lo, hi, cstr(m)))
    A('};')
    A('#define RPG_ITEM_HEAL 0')
    A('#define RPG_ITEM_DUCK 1')
    A('typedef struct { const char *name; uint8_t kind; uint8_t val; } rpg_item_t;')
    A('static const rpg_item_t rpg_item[RPG_ITEM_CNT] = {')
    for n, k, v in S.ITEMS:
        A('  { %s, %s, %d },' % (cstr(n), 'RPG_ITEM_HEAL' if k == 'heal' else 'RPG_ITEM_DUCK', v))
    A('};')
    A('typedef struct { const char *name; uint8_t atk; } rpg_equip_t;')
    A('static const rpg_equip_t rpg_equip[RPG_EQUIP_CNT] = {')
    for n, a in S.EQUIPS:
        A('  { %s, %d },' % (cstr(n), a))
    A('};')
    A('typedef struct { const char *name; uint8_t bat; uint8_t hp, atk; const char *atk_msg; int8_t weak; const char *desc; uint8_t tough; } rpg_enemy_t;')
    A('static const rpg_enemy_t rpg_enemy[RPG_ENEMY_CNT] = {')
    bidx = {b[0]: i for i, b in enumerate(BATTLE)}
    for n, b, hp, atk, msg, weak, desc, tough in S.ENEMIES:
        A('  { %s, %d, %d, %d, %s, %d, %s, %d },' % (cstr(n), bidx[b], hp, atk, cstr(msg),
          S.SKILL_IDX[weak] if weak else -1, cstr(desc), 1 if tough else 0))
    A('};')
    A('')
    # 엔티티와 주인
    owners = []
    for (r, x, y, spr, kind, val) in S.ENTITIES:
        if kind != 'BUG' and val not in owners:
            owners.append(val)
    oidx = {n: i for i, n in enumerate(owners)}
    # 상자: 처음 스크립트가 'nope' 로 보는 깃발 = 열림 상태
    state = {}
    for sc in S.SCRIPTS:
        if sc[0].startswith('chest') and 'nope' in sc[1] and sc[0] not in state:
            state[sc[0]] = S.FLAG_IDX[sc[1]['nope']]
    A('#define RPG_ENT_NPC  0')
    A('#define RPG_ENT_BUG  1')
    A('#define RPG_ENT_OBJ  2')
    A('typedef struct { uint8_t room, x, y; int8_t spr; uint8_t kind; uint8_t id; int8_t flag; } rpg_ent_t;')
    A('// id: NPC·OBJ = 스크립트 주인, BUG = 적. flag: BUG = 이기면 세울 깃발, OBJ = 열림 상태 깃발 (상자)')
    A('static const rpg_ent_t rpg_ent[RPG_ENT_CNT] = {')
    for (r, x, y, spr, kind, val) in S.ENTITIES:
        s_ = SPR_IDX[spr] if spr else -1
        if kind == 'BUG':
            en, wf = val
            A('  { %d, %d, %d, %d, RPG_ENT_BUG, %d, %d },  // %s' % (r, x, y, s_, S.ENEMY_IDX[en],
              S.FLAG_IDX[wf] if wf else -1, en))
        else:
            A('  { %d, %d, %d, %d, RPG_ENT_%s, %d, %d },  // %s' % (r, x, y, s_, kind, oidx[val],
              state.get(val, -1), val))
    A('};')
    A('')
    # 대사 블록
    A('typedef struct { const char *who; const char *const *lines; } rpg_block_t;   // who NULL = 안내, "바람" = 초상화')
    tcount = [0]
    def lines_arr(ls):
        name = 'rpg_t%d' % tcount[0]
        tcount[0] += 1
        A('static const char *const %s[] = { %s, NULL };' % (name, ', '.join(cstr(l) for l in ls)))
        return name
    def blocks_arr(blocks):
        if not blocks:
            return 'NULL', 0
        names = [(who, lines_arr(ls)) for who, ls in blocks]
        name = 'rpg_bk%d' % tcount[0]
        tcount[0] += 1
        A('static const rpg_block_t %s[] = { %s };' % (name, ', '.join(
            '{ %s, %s }' % (cstr(w) if w else 'NULL', n) for w, n in names)))
        return name, len(blocks)
    # 문답
    qnames = list(S.QUIZZES.keys())
    qidx = {n: i for i, n in enumerate(qnames)}
    qrows = []
    # 정답 위치를 문제마다 1·2·3 번에 고르게 (story.py 에서는 정답을 아무 데나 적어도 된다)
    ans_pos = [1, 2, 0, 2, 1, 0, 2, 1, 0]
    for qi, n in enumerate(qnames):
        qq, opts, ans, right, wrong = S.QUIZZES[n]
        correct = opts[ans]
        others = [o for k, o in enumerate(opts) if k != ans]
        pos = ans_pos[qi % len(ans_pos)]
        opts = others[:pos] + [correct] + others[pos:]
        qrows.append((lines_arr(qq), opts, pos, lines_arr(right), lines_arr(wrong)))
    A('typedef struct { const char *const *q; const char *opt[3]; uint8_t ans; const char *const *right; const char *const *wrong; } rpg_quiz_t;')
    A('#define RPG_QUIZ_CNT %d' % len(qrows))
    A('static const rpg_quiz_t rpg_quiz[RPG_QUIZ_CNT] = {')
    for qa, opts, ans, ra, wa in qrows:
        A('  { %s, { %s }, %d, %s, %s },' % (qa, ', '.join(cstr(x) for x in opts), ans, ra, wa))
    A('};')
    # 스크립트
    srows = []
    for owner, cond, pre, quiz, act, post in S.SCRIPTS:
        pa, pn = blocks_arr(pre)
        qa, qn = blocks_arr(post)
        it = act.get('item')
        srows.append('  { %d, %d, %d, %s, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %s, %d },' % (
            oidx[owner],
            S.FLAG_IDX[cond['need']] if 'need' in cond else -1,
            S.FLAG_IDX[cond['nope']] if 'nope' in cond else -1,
            pa, pn,
            qidx[quiz] if quiz else -1,
            S.FLAG_IDX[act['set']] if 'set' in act else -1,
            S.SKILL_IDX[act['skill']] if 'skill' in act else -1,
            S.ITEM_IDX[it[0]] if it else -1, it[1] if it else 0,
            S.EQUIP_IDX[act['equip']] if 'equip' in act else -1,
            S.ENEMY_IDX[act['battle']] if 'battle' in act else -1,
            S.FLAG_IDX[act['win_set']] if 'win_set' in act else -1,
            1 if act.get('chain') else 0, 1 if act.get('end') else 0,
            qa, qn))
    A('typedef struct {')
    A('  uint8_t owner; int8_t need, nope;')
    A('  const rpg_block_t *pre; uint8_t npre;')
    A('  int8_t quiz, set, skill, item; uint8_t item_n; int8_t equip, battle, win_set; uint8_t chain, end;')
    A('  const rpg_block_t *post; uint8_t npost;')
    A('} rpg_script_t;')
    A('#define RPG_SCRIPT_CNT %d' % len(srows))
    A('static const rpg_script_t rpg_script[RPG_SCRIPT_CNT] = {')
    for r in srows:
        A(r)
    A('};')
    ia, inn = blocks_arr(S.INTRO)
    ga, gn = blocks_arr(S.GATE_OPEN)
    A('static const rpg_block_t *const rpg_intro = %s;' % ia)
    A('#define RPG_INTRO_CNT %d' % inn)
    A('static const rpg_block_t *const rpg_gate_open = %s;' % ga)
    A('#define RPG_GATE_OPEN_CNT %d' % gn)
    A('')
    A('#define RPG_START_ROOM %d' % S.START[0])
    A('#define RPG_START_X    %d' % S.START[1])
    A('#define RPG_START_Y    %d' % S.START[2])
    A('')
    A('#endif')

    here = os.path.dirname(os.path.abspath(__file__))
    dst = os.path.join(here, '..', '..', 'src', 'ap', 'modules', 'game', 'rpg', 'rpg_assets.h')
    with open(dst, 'w') as f:
        f.write('\n'.join(o) + '\n')
    print('ok: %d tiles, %d sprites, %d battle, %d rooms, %d entities, %d scripts -> %s' % (
        len(TILES), len(SPRITES), len(BATTLE), len(rooms), len(S.ENTITIES), len(srows), os.path.normpath(dst)))

    if '--preview' in sys.argv:
        from PIL import Image
        out = sys.argv[sys.argv.index('--preview') + 1]
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
            for (r, ex, ey, spr, kind, val) in S.ENTITIES:
                if r != i or not spr:
                    continue
                srows_ = SPRITES[SPR_IDX[spr]][1]
                for y in range(16):
                    for x in range(16):
                        c = srows_[y][x]
                        if c != ' ':
                            px[ex * 16 + x, ey * 16 + y] = 0 if ink(c, x, y) else 1
            im.resize((W * 32, H * 32)).save('%s_room%02d.png' % (out, i))
        for name, rows in BATTLE:
            im = Image.new('L', (32, 32), 200)
            for y in range(32):
                for x in range(32):
                    c = rows[y][x]
                    if c != ' ':
                        im.putpixel((x, y), 0 if ink(c, x, y) else 255)
            im.resize((128, 128), Image.NEAREST).save('%s_%s.png' % (out, name))


if __name__ == '__main__':
    main()
