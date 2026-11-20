// 자동 생성: tools/rpg/gen_assets.py + story.py (직접 고치지 말 것)
#ifndef RPG_ASSETS_H_
#define RPG_ASSETS_H_

#define RPG_MAP_W      12
#define RPG_MAP_H      8
#define RPG_ROOM_CNT   13
#define RPG_TILE_CNT   26
#define RPG_SPR_CNT    23
#define RPG_BAT_CNT    9
#define RPG_ENT_CNT    27
#define RPG_SKILL_CNT  7
#define RPG_ITEM_CNT   3
#define RPG_EQUIP_CNT  3
#define RPG_ENEMY_CNT  14
#define RPG_FLAG_CNT   19

#define RPG_T_GRASS        0
#define RPG_T_GRASS2       1
#define RPG_T_FLOWER       2
#define RPG_T_PATH         3
#define RPG_T_TREE         4
#define RPG_T_WATER        5
#define RPG_T_BRIDGE       6
#define RPG_T_BRICK        7
#define RPG_T_ROOF         8
#define RPG_T_DOOR         9
#define RPG_T_FENCE        10
#define RPG_T_ROCK         11
#define RPG_T_SIGN         12
#define RPG_T_CHEST        13
#define RPG_T_CHEST_OPEN   14
#define RPG_T_GATE         15
#define RPG_T_ALTAR        16
#define RPG_T_PILLAR       17
#define RPG_T_FLOOR        18
#define RPG_T_HEDGE        19
#define RPG_T_REED         20
#define RPG_T_STONEWALL    21
#define RPG_T_CLIFF        22
#define RPG_T_MOUNT        23
#define RPG_T_CACTUS       24
#define RPG_T_SAND         25
#define RPG_S_HERO_DOWN0   0
#define RPG_S_HERO_DOWN1   1
#define RPG_S_HERO_UP0     2
#define RPG_S_HERO_UP1     3
#define RPG_S_HERO_SIDE0   4
#define RPG_S_HERO_SIDE1   5
#define RPG_S_SENIOR       6
#define RPG_S_MASTER_IRQ   7
#define RPG_S_MASTER_FW    8
#define RPG_S_BUG_GHOST    9
#define RPG_S_BUG_KEY      10
#define RPG_S_BUG_STACK    11
#define RPG_S_BUG_FAULT    12
#define RPG_S_VILLAGER     13
#define RPG_S_SAGE         14
#define RPG_S_DMA          15
#define RPG_S_MONK         16
#define RPG_S_HERMIT       17
#define RPG_S_LARVA        18
#define RPG_S_MOTH         19
#define RPG_S_TWINS        20
#define RPG_S_GOLEM        21
#define RPG_S_WORM         22
#define RPG_B_LARVA        0
#define RPG_B_MOTH         1
#define RPG_B_GHOST        2
#define RPG_B_KEY          3
#define RPG_B_STACK        4
#define RPG_B_TWINS        5
#define RPG_B_GOLEM        6
#define RPG_B_WORM         7
#define RPG_B_FAULT        8
#define RPG_F_SENIOR       0
#define RPG_F_GATE1        1
#define RPG_F_LEARN1       2
#define RPG_F_GATE2        3
#define RPG_F_LEARN2       4
#define RPG_F_GATE3        5
#define RPG_F_CHEST1       6
#define RPG_F_CHEST2       7
#define RPG_F_CHEST3       8
#define RPG_F_LEARN3       9
#define RPG_F_GATE4        10
#define RPG_F_LEARN4       11
#define RPG_F_GATE5        12
#define RPG_F_LEARN5       13
#define RPG_F_GATE6        14
#define RPG_F_T1           15
#define RPG_F_T2           16
#define RPG_F_T3           17
#define RPG_F_WIN          18

static const uint16_t rpg_tile_bits[RPG_TILE_CNT][16] = {
  { 0x0000, 0x0000, 0x2000, 0x0000, 0x0040, 0x0000, 0x0000, 0x0808, 0x0000, 0x0000, 0x0020, 0x4000, 0x0000, 0x0200, 0x0000, 0x0000 },  // grass
  { 0x0000, 0x0100, 0x0000, 0x0000, 0x2008, 0x0000, 0x0000, 0x0000, 0x0040, 0x0000, 0x1000, 0x0000, 0x0000, 0x0008, 0x0400, 0x0000 },  // grass2
  { 0x0000, 0x0000, 0x0A00, 0x0400, 0x0A00, 0x0414, 0x0208, 0x0014, 0x0408, 0x0000, 0x0000, 0x1400, 0x0800, 0x1400, 0x0800, 0x0000 },  // flower
  { 0xAAAA, 0x0000, 0xAAAA, 0x0000, 0xAEAA, 0x0400, 0xAAAA, 0x0000, 0xAAAA, 0x0018, 0xAAAA, 0x0000, 0xBAAA, 0x0000, 0xAAAA, 0x0000 },  // path
  { 0x07E0, 0x1958, 0x2AA4, 0x5552, 0x6AAA, 0x8001, 0x8001, 0x8001, 0x4002, 0x355C, 0x0E70, 0x0340, 0x02C0, 0x0340, 0x06E0, 0x0A50 },  // tree
  { 0xAAAA, 0x0000, 0xAAAA, 0x1C00, 0xA2AA, 0x0000, 0xAABE, 0x0022, 0xAAAA, 0x0000, 0xAEAA, 0x1100, 0xAAAA, 0x0000, 0xAAAA, 0x0000 },  // water
  { 0xFFFF, 0x8001, 0xFFFF, 0x8411, 0xFFFF, 0x9021, 0xFFFF, 0x8081, 0xFFFF, 0x8811, 0xFFFF, 0x8101, 0xFFFF, 0xA021, 0xFFFF, 0x8001 },  // bridge
  { 0xFFFF, 0x8101, 0x8101, 0xFFFF, 0x1010, 0x1010, 0xFFFF, 0x8101, 0x8101, 0xFFFF, 0x1010, 0x1010, 0xFFFF, 0x8101, 0x8101, 0xFFFF },  // brick
  { 0xFFFF, 0xAAAA, 0xFFFF, 0x5555, 0xFFFF, 0xAAAA, 0xFFFF, 0x5555, 0xFFFF, 0xAAAA, 0xFFFF, 0x5555, 0xFFFF, 0xAAAA, 0xFFFF, 0xFFFF },  // roof
  { 0xFFFF, 0x8001, 0xBFFD, 0xB555, 0xA005, 0xB555, 0xA005, 0xB555, 0xA035, 0xB575, 0xA005, 0xB555, 0xA005, 0xB555, 0xBFFD, 0xFFFF },  // door
  { 0x0000, 0x0000, 0x300C, 0x300C, 0xFFFF, 0x8001, 0xFFFF, 0x300C, 0x300C, 0xFFFF, 0x8001, 0xFFFF, 0x300C, 0x300C, 0x300C, 0x0000 },  // fence
  { 0x0000, 0x0000, 0x07E0, 0x1818, 0x2004, 0x4602, 0x4802, 0x8001, 0x8001, 0x8001, 0x8003, 0x4056, 0x32AC, 0x0FFC, 0x0000, 0x0000 },  // rock
  { 0x0000, 0x0000, 0x7FFE, 0x8001, 0xBDDD, 0x8001, 0xBBD9, 0x8001, 0x7FFE, 0x0180, 0x0180, 0x0180, 0x0180, 0x03C0, 0x0000, 0x0000 },  // sign
  { 0x0000, 0x0000, 0x0000, 0x3FFC, 0x4002, 0x9551, 0xAAA9, 0xFFFF, 0x8181, 0x8241, 0x8181, 0x9551, 0xAAA9, 0xFFFF, 0x0000, 0x0000 },  // chest
  { 0x0000, 0x3FFC, 0x6AAA, 0xD555, 0xFFFF, 0x8001, 0x8001, 0xFFFF, 0x8001, 0x8001, 0x8001, 0x9551, 0xAAA9, 0xFFFF, 0x0000, 0x0000 },  // chest_open
  { 0xFFFF, 0x8001, 0xFFFF, 0x4924, 0x4924, 0x4924, 0x4BD4, 0x4A54, 0x4BD4, 0x4994, 0x4994, 0x4924, 0x4924, 0xFFFF, 0x8001, 0xFFFF },  // gate
  { 0x0000, 0x0180, 0x0240, 0x05A0, 0x0240, 0x0180, 0x2008, 0x0820, 0x3FFC, 0x3554, 0x3FFC, 0x1008, 0x1008, 0x1008, 0x3FFC, 0x0000 },  // altar
  { 0x3FFC, 0x2004, 0x3FFC, 0x1428, 0x1428, 0x1428, 0x1428, 0x1428, 0x1428, 0x1428, 0x1428, 0x1428, 0x1428, 0x3FFC, 0x2AAC, 0x3FFC },  // pillar
  { 0xFFFF, 0x8001, 0x8001, 0x8001, 0x8001, 0x8001, 0x8001, 0x8001, 0x8001, 0x8001, 0x8001, 0x8001, 0x8001, 0x8001, 0x8001, 0xFFFF },  // floor
  { 0x0000, 0x1E3C, 0x216C, 0x5582, 0x6A82, 0x8001, 0x8001, 0x8001, 0x8001, 0x5556, 0x2004, 0x5556, 0xAAAB, 0xD555, 0x7FFE, 0x0000 },  // hedge
  { 0x0000, 0x2080, 0x2484, 0x7494, 0x75D4, 0x25D4, 0x2E94, 0x2EB8, 0x24BA, 0x2492, 0x5497, 0x5557, 0x8952, 0x0000, 0x0000, 0x0000 },  // reed
  { 0xFFFF, 0x8411, 0xA491, 0x8411, 0xFFFF, 0x9021, 0xB221, 0x9021, 0xFFFF, 0x8209, 0x820B, 0x8209, 0xFFFF, 0x9041, 0xB249, 0xFFFF },  // stonewall
  { 0xFFFF, 0xD581, 0xAC81, 0xD855, 0xD031, 0xA00B, 0x8005, 0x8001, 0x8001, 0x8001, 0xEAAB, 0xB00D, 0x8E7B, 0x8155, 0x8001, 0xFFFF },  // cliff
  { 0x0000, 0x0180, 0x0240, 0x0520, 0x0A90, 0x1548, 0x2AA4, 0x3554, 0x4002, 0x4002, 0xAAA9, 0xD555, 0xFFFF, 0x0000, 0x0000, 0x0000 },  // mount
  { 0x8080, 0x0180, 0x0240, 0x0258, 0x3254, 0x4A54, 0x4A54, 0x4E74, 0x2004, 0x1E78, 0x0240, 0x0240, 0x0240, 0x07E0, 0x0000, 0x0000 },  // cactus
  { 0x8080, 0x0000, 0x0808, 0x0000, 0x8080, 0x0000, 0x0808, 0x0000, 0x8080, 0x0000, 0x0808, 0x0000, 0x8080, 0x0000, 0x0808, 0x0000 },  // sand
};
static const uint8_t rpg_tile_walk[RPG_TILE_CNT] = { 1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1 };

static const uint16_t rpg_spr_bits[RPG_SPR_CNT][16] = {
  { 0x07E0, 0x0D50, 0x1AA8, 0x17F8, 0x1428, 0x1248, 0x1008, 0x0990, 0x07E0, 0x1D58, 0x2AB4, 0x2D54, 0x1AB8, 0x0990, 0x0A50, 0x0C60 },  // hero_down0
  { 0x07E0, 0x0D50, 0x1AA8, 0x17F8, 0x1428, 0x1248, 0x1008, 0x0990, 0x07E0, 0x1D58, 0x2AB4, 0x2D54, 0x1AB8, 0x0990, 0x0A50, 0x0630 },  // hero_down1
  { 0x07E0, 0x0D50, 0x1AA8, 0x1558, 0x1AA8, 0x1558, 0x1808, 0x0810, 0x07E0, 0x1D58, 0x2AB4, 0x2D54, 0x1AB8, 0x0990, 0x0A50, 0x0C60 },  // hero_up0
  { 0x07E0, 0x0D50, 0x1AA8, 0x1558, 0x1AA8, 0x1558, 0x1808, 0x0810, 0x07E0, 0x1D58, 0x2AB4, 0x2D54, 0x1AB8, 0x0990, 0x0A50, 0x0630 },  // hero_up1
  { 0x07E0, 0x0D50, 0x1AA8, 0x15F4, 0x1B08, 0x1628, 0x1A08, 0x0E10, 0x07E0, 0x0D50, 0x0AB0, 0x0F50, 0x0AB0, 0x0990, 0x0A50, 0x0C60 },  // hero_side0
  { 0x07E0, 0x0D50, 0x1AA8, 0x15F4, 0x1B08, 0x1628, 0x1A08, 0x0E10, 0x07E0, 0x0D50, 0x0AB0, 0x0DD0, 0x0AB0, 0x05A0, 0x05A0, 0x0660 },  // hero_side1
  { 0x07E0, 0x0FF0, 0x1FF8, 0x1008, 0x1668, 0x1998, 0x1008, 0x0990, 0x07E0, 0x1818, 0x2004, 0x2814, 0x281E, 0x1816, 0x099E, 0x0C30 },  // senior
  { 0x0FF1, 0x3FFD, 0x0422, 0x0524, 0x0428, 0x0250, 0x07E0, 0x1D58, 0x2AB4, 0x2D54, 0x1AB8, 0x0D50, 0x0AB0, 0x0990, 0x0A50, 0x0C60 },  // master_irq
  { 0x03C1, 0x0567, 0x0FF1, 0x0421, 0x0561, 0x0421, 0x0423, 0x0245, 0x0439, 0x0D61, 0x16A1, 0x1561, 0x0EA1, 0x0D51, 0x0AB1, 0x0FF1 },  // master_fw
  { 0x07E0, 0x1818, 0x2004, 0x2664, 0x2664, 0x2004, 0x2184, 0x2004, 0x2AAC, 0x3554, 0x2AAC, 0x3554, 0x2AAC, 0x34D4, 0x2AAA, 0x0000 },  // bug_ghost
  { 0x2004, 0x1008, 0x0990, 0x07E0, 0x0A50, 0x0810, 0x3FF0, 0x5568, 0x9A24, 0x5568, 0x9A24, 0x5568, 0x9A24, 0x3FF0, 0x1240, 0x2220 },  // bug_key
  { 0x03C0, 0x0420, 0x0BD0, 0x0810, 0x17E8, 0x1008, 0x2FF4, 0x2004, 0x4C32, 0x4C32, 0x4002, 0x43C2, 0x4002, 0xD555, 0xFFFF, 0x0000 },  // bug_stack
  { 0x4002, 0x63C6, 0x7EBA, 0x3556, 0x2EBA, 0x3346, 0x2ADA, 0x3556, 0x2D5A, 0x32A6, 0x2AAA, 0x7556, 0xAAAD, 0x9FF9, 0x6426, 0x0660 },  // bug_fault
  { 0x07E0, 0x0D50, 0x1AA8, 0x1018, 0x1428, 0x1008, 0x0990, 0x07E0, 0x0810, 0x1558, 0x1AA8, 0x1558, 0x0FF0, 0x0420, 0x0420, 0x0660 },  // villager
  { 0x0180, 0x0340, 0x06A0, 0x0FF0, 0x0420, 0x0560, 0x0420, 0x0C30, 0x17E8, 0x1428, 0x15A8, 0x1428, 0x17E8, 0x0D50, 0x0AB0, 0x0FF0 },  // sage
  { 0x07E0, 0x0D50, 0x1FF8, 0x1668, 0x1668, 0x1008, 0x0992, 0x07E6, 0x1AB8, 0x2D50, 0x2AB0, 0x1D50, 0x0AB0, 0x0990, 0x0A50, 0x0C60 },  // dma
  { 0x07E0, 0x0810, 0x1008, 0x1668, 0x1008, 0x1188, 0x0810, 0x07E0, 0x1AB8, 0x3D54, 0x2ABC, 0x1D58, 0x0AB0, 0x0D50, 0x0FF0, 0x0660 },  // monk
  { 0x3FC6, 0x5569, 0xB0B9, 0xE959, 0xA076, 0x6F64, 0x2044, 0x557C, 0xAAB4, 0xD554, 0xAAB4, 0xD554, 0xAAB4, 0xFFF4, 0x60C0, 0x0000 },  // hermit
  { 0x0000, 0x0480, 0x0300, 0x0FC0, 0x1020, 0x14A0, 0x1020, 0x0D7E, 0x1AEB, 0x1555, 0x0FAB, 0x01FF, 0x00AA, 0x0000, 0x0000, 0x0000 },  // larva
  { 0x0000, 0x300C, 0x699A, 0xD5B1, 0xAA61, 0xD641, 0x6A42, 0x3DBC, 0x1B2C, 0x1534, 0x0EDC, 0x0120, 0x00C0, 0x0000, 0x0000, 0x0000 },  // moth
  { 0x0000, 0x3C3C, 0x4242, 0xA5A5, 0x8181, 0x9999, 0x43C2, 0x3E7C, 0x15A8, 0x1FF8, 0x1008, 0x1008, 0x1FF8, 0x0000, 0x0000, 0x0000 },  // twins
  { 0x0FF0, 0x0810, 0x0A50, 0x0810, 0x3FFC, 0x500E, 0x73CA, 0x524E, 0x77EA, 0x766E, 0x0BF4, 0x0804, 0x0E1C, 0x0E14, 0x0A1C, 0x0E1C },  // golem
  { 0x03C0, 0x0420, 0x0A50, 0x0810, 0x0BD0, 0x0420, 0x0420, 0x0420, 0x0420, 0x0D50, 0x1008, 0x1010, 0x0FEA, 0x0000, 0x0000, 0x0000 },  // worm
};
static const uint16_t rpg_spr_mask[RPG_SPR_CNT][16] = {
  { 0x07E0, 0x0FF0, 0x1FF8, 0x1FF8, 0x1FF8, 0x1FF8, 0x1FF8, 0x0FF0, 0x07E0, 0x1FF8, 0x3FFC, 0x3FFC, 0x1FF8, 0x0FF0, 0x0E70, 0x0C60 },
  { 0x07E0, 0x0FF0, 0x1FF8, 0x1FF8, 0x1FF8, 0x1FF8, 0x1FF8, 0x0FF0, 0x07E0, 0x1FF8, 0x3FFC, 0x3FFC, 0x1FF8, 0x0FF0, 0x0E70, 0x0630 },
  { 0x07E0, 0x0FF0, 0x1FF8, 0x1FF8, 0x1FF8, 0x1FF8, 0x1FF8, 0x0FF0, 0x07E0, 0x1FF8, 0x3FFC, 0x3FFC, 0x1FF8, 0x0FF0, 0x0E70, 0x0C60 },
  { 0x07E0, 0x0FF0, 0x1FF8, 0x1FF8, 0x1FF8, 0x1FF8, 0x1FF8, 0x0FF0, 0x07E0, 0x1FF8, 0x3FFC, 0x3FFC, 0x1FF8, 0x0FF0, 0x0E70, 0x0630 },
  { 0x07E0, 0x0FF0, 0x1FF8, 0x1FFC, 0x1FF8, 0x1FF8, 0x1FF8, 0x0FF0, 0x07E0, 0x0FF0, 0x0FF0, 0x0FF0, 0x0FF0, 0x0FF0, 0x0E70, 0x0C60 },
  { 0x07E0, 0x0FF0, 0x1FF8, 0x1FFC, 0x1FF8, 0x1FF8, 0x1FF8, 0x0FF0, 0x07E0, 0x0FF0, 0x0FF0, 0x0FF0, 0x0FF0, 0x07E0, 0x07E0, 0x0660 },
  { 0x07E0, 0x0FF0, 0x1FF8, 0x1FF8, 0x1FF8, 0x1FF8, 0x1FF8, 0x0FF0, 0x07E0, 0x1FF8, 0x3FFC, 0x3FFC, 0x3FFE, 0x1FFE, 0x0FFE, 0x0C30 },
  { 0x0FF1, 0x3FFD, 0x07E2, 0x07E4, 0x07E8, 0x03D0, 0x07E0, 0x1FF8, 0x3FFC, 0x3FFC, 0x1FF8, 0x0FF0, 0x0FF0, 0x0FF0, 0x0E70, 0x0C60 },
  { 0x03C1, 0x07E7, 0x0FF1, 0x07E1, 0x07E1, 0x07E1, 0x07E3, 0x03C5, 0x07F9, 0x0FE1, 0x1FE1, 0x1FE1, 0x0FE1, 0x0FF1, 0x0FF1, 0x0FF1 },
  { 0x07E0, 0x1FF8, 0x3FFC, 0x3FFC, 0x3FFC, 0x3FFC, 0x3FFC, 0x3FFC, 0x3FFC, 0x3FFC, 0x3FFC, 0x3FFC, 0x3FFC, 0x3FFC, 0x2AAA, 0x0000 },
  { 0x2004, 0x1008, 0x0990, 0x07E0, 0x0FF0, 0x0FF0, 0x3FF0, 0x7FF8, 0xFFFC, 0x7FF8, 0xFFFC, 0x7FF8, 0xFFFC, 0x3FF0, 0x1240, 0x2220 },
  { 0x03C0, 0x07E0, 0x0FF0, 0x0FF0, 0x1FF8, 0x1FF8, 0x3FFC, 0x3FFC, 0x7FFE, 0x7FFE, 0x7FFE, 0x7FFE, 0x7FFE, 0xFFFF, 0xFFFF, 0x0000 },
  { 0x4002, 0x63C6, 0x7FFE, 0x3FFE, 0x3FFE, 0x3FFE, 0x3FFE, 0x3FFE, 0x3FFE, 0x3FFE, 0x3FFE, 0x7FFE, 0xFFFF, 0xFFFF, 0x6426, 0x0660 },
  { 0x07E0, 0x0FF0, 0x1FF8, 0x1FF8, 0x1FF8, 0x1FF8, 0x0FF0, 0x07E0, 0x0FF0, 0x1FF8, 0x1FF8, 0x1FF8, 0x0FF0, 0x0660, 0x0660, 0x0660 },
  { 0x0180, 0x03C0, 0x07E0, 0x0FF0, 0x07E0, 0x07E0, 0x07E0, 0x0FF0, 0x1FF8, 0x1FF8, 0x1FF8, 0x1FF8, 0x1FF8, 0x0FF0, 0x0FF0, 0x0FF0 },
  { 0x07E0, 0x0FF0, 0x1FF8, 0x1FF8, 0x1FF8, 0x1FF8, 0x0FF2, 0x07E6, 0x1FF8, 0x3FF0, 0x3FF0, 0x1FF0, 0x0FF0, 0x0FF0, 0x0E70, 0x0C60 },
  { 0x07E0, 0x0FF0, 0x1FF8, 0x1FF8, 0x1FF8, 0x1FF8, 0x0FF0, 0x07E0, 0x1FF8, 0x3FFC, 0x3FFC, 0x1FF8, 0x0FF0, 0x0FF0, 0x0FF0, 0x0660 },
  { 0x3FC6, 0x7FEF, 0xFFFF, 0xFFFF, 0xFFF6, 0x7FE4, 0x3FC4, 0x7FFC, 0xFFF4, 0xFFF4, 0xFFF4, 0xFFF4, 0xFFF4, 0xFFF4, 0x60C0, 0x0000 },
  { 0x0000, 0x0480, 0x0300, 0x0FC0, 0x1FE0, 0x1FE0, 0x1FE0, 0x0FFE, 0x1FFF, 0x1FFF, 0x0FFF, 0x01FF, 0x00AA, 0x0000, 0x0000, 0x0000 },
  { 0x0000, 0x300C, 0x799E, 0xFFFF, 0xFFFF, 0xFFFF, 0x7FFE, 0x3FFC, 0x1FFC, 0x1FFC, 0x0FFC, 0x01E0, 0x00C0, 0x0000, 0x0000, 0x0000 },
  { 0x0000, 0x3C3C, 0x7E7E, 0xFFFF, 0xFFFF, 0xFFFF, 0x7FFE, 0x3FFC, 0x1FF8, 0x1FF8, 0x1FF8, 0x1FF8, 0x1FF8, 0x0000, 0x0000, 0x0000 },
  { 0x0FF0, 0x0FF0, 0x0FF0, 0x0FF0, 0x3FFC, 0x7FFE, 0x7FFE, 0x7FFE, 0x7FFE, 0x7FFE, 0x0FFC, 0x0FFC, 0x0FFC, 0x0E1C, 0x0E1C, 0x0E1C },
  { 0x03C0, 0x07E0, 0x0FF0, 0x0FF0, 0x0FF0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x0FF0, 0x1FF8, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0x0000 },
};

// 전투 그림 32x32 (비트 1 = 검정, MSB 왼쪽)
static const uint32_t rpg_bat_bits[RPG_BAT_CNT][32] = {
  { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00204000, 0x00108000, 0x00090000, 0x003FC000, 0x00402000, 0x00999000, 0x00999000, 0x00801000, 0x008F1000, 0x00403F80, 0x003D7560, 0x006AEAB0, 0x00805558, 0x00806AAC, 0x00555556, 0x003FEAAA, 0x00052002, 0x00051FFE, 0x00000AA8, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },  // b_larva
  { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x1E000078, 0x35000084, 0x6A824102, 0x5C418236, 0x9EA7E47B, 0x8D581835, 0x80124AAB, 0x80100D55, 0x6AB3CAAA, 0x30300C0C, 0x0FD02FC0, 0x02103500, 0x06E84C80, 0x05D84C80, 0x02132B00, 0x01E01E00, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },  // b_moth
  { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x001FF800, 0x00600600, 0x01800180, 0x02000040, 0x04000020, 0x08000010, 0x08381C10, 0x107C3E08, 0x106C3608, 0x107C3E08, 0x10381C08, 0x10000008, 0x1001E008, 0x10021008, 0x1001E008, 0x10000008, 0x10000008, 0x10000008, 0x10000008, 0x10000008, 0x106AC1A8, 0x10913658, 0x110A1C28, 0x1C040818, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },  // b_ghost
  { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00800400, 0x00400800, 0x003FF000, 0x00C00C00, 0x01387200, 0x01387200, 0x01000200, 0x190FC260, 0x25102290, 0x188FC460, 0x07FFFE00, 0x08000100, 0x10FFD580, 0x11002040, 0x1173A04C, 0x114CA072, 0x1173A04C, 0x11002040, 0x10FFD540, 0x08000080, 0x07FFFE00, 0x02424200, 0x04848400, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },  // b_key
  { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0007E000, 0x00181800, 0x00242400, 0x00200400, 0x00181800, 0x0FFFFFC0, 0x08000040, 0x09801840, 0x09801840, 0x0FFFFFC0, 0x0D555540, 0x0AAAAAC0, 0x0FFFFFC0, 0x08000040, 0x087FE040, 0x08000040, 0x0FFFFFC0, 0x08000040, 0x08000040, 0x3FFFFFF0, 0x40000008, 0x80000004, 0xFFFFFFFC, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },  // b_stack
  { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x07E007E0, 0x18181818, 0x20042004, 0x4C324C32, 0x4C324C32, 0x40024002, 0x47E247E2, 0x42424242, 0x20042004, 0x18181818, 0x077FF700, 0x00A02800, 0x00FFFC00, 0x00800400, 0x00800400, 0x00800400, 0x00FFFC00, 0x02800500, 0x06C00D80, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },  // b_twins
  { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x003FFC00, 0x00200400, 0x002E7400, 0x002E7400, 0x00200400, 0x0023C400, 0x003FFC00, 0x1FFFFFF8, 0x35000044, 0x2B0FF044, 0x35081044, 0x2B1BD844, 0x35124844, 0x2B118844, 0x35100844, 0x3F1FF87E, 0x01000040, 0x01AAAAC0, 0x01F83F80, 0x01A82100, 0x01582100, 0x01A82100, 0x03FC7F80, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },  // b_golem
  { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x000FF000, 0x00300C00, 0x00466200, 0x00466200, 0x00400200, 0x004FF200, 0x004AAA00, 0x00200400, 0x00355400, 0x002AAC18, 0x00355424, 0x002AAC5A, 0x00400224, 0x00AAAB18, 0x01000080, 0xAAAAAAC0, 0x03FFFF00, 0xAAAAAAAA, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },  // b_worm
  { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x04924920, 0x04924920, 0x1FFFFFE0, 0x1AAA0020, 0xF6124378, 0x11B26C20, 0xF5F53E78, 0x10020020, 0xF5550078, 0x107EFEA0, 0xF5AAAB78, 0x1001AAA0, 0x1FFFFFE0, 0x04924920, 0x04924920, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },  // b_fault
};
static const uint32_t rpg_bat_mask[RPG_BAT_CNT][32] = {
  { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00204000, 0x00108000, 0x00090000, 0x003FC000, 0x007FE000, 0x00FFF000, 0x00FFF000, 0x00FFF000, 0x00FFF000, 0x007FFF80, 0x003FFFE0, 0x007FFFF0, 0x00FFFFF8, 0x00FFFFFC, 0x007FFFFE, 0x003FFFFE, 0x00073FFE, 0x00051FFE, 0x00000AA8, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
  { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x1E000078, 0x3F0000FC, 0x7F8241FE, 0x7FC183FE, 0xFFE7E7FF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x7FFFFFFE, 0x3FFFFFFC, 0x0FFFFFC0, 0x03FFFF00, 0x07FFFF80, 0x07FFFF80, 0x03F33F00, 0x01E01E00, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
  { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x001FF800, 0x007FFE00, 0x01FFFF80, 0x03FFFFC0, 0x07FFFFE0, 0x0FFFFFF0, 0x0FFFFFF0, 0x1FFFFFF8, 0x1FFFFFF8, 0x1FFFFFF8, 0x1FFFFFF8, 0x1FFFFFF8, 0x1FFFFFF8, 0x1FFFFFF8, 0x1FFFFFF8, 0x1FFFFFF8, 0x1FFFFFF8, 0x1FFFFFF8, 0x1FFFFFF8, 0x1FFFFFF8, 0x1FFFFFF8, 0x1F9F3E78, 0x1F0E1C38, 0x1C040818, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
  { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00800400, 0x00400800, 0x003FF000, 0x00FFFC00, 0x01FFFE00, 0x01FFFE00, 0x01FFFE00, 0x19FFFE60, 0x3DFFFEF0, 0x18FFFC60, 0x07FFFE00, 0x0FFFFF00, 0x1FFFFF80, 0x1FFFFFC0, 0x1FFFFFCC, 0x1FFFFFFE, 0x1FFFFFCC, 0x1FFFFFC0, 0x1FFFFFC0, 0x0FFFFF80, 0x07FFFE00, 0x02424200, 0x04848400, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
  { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0007E000, 0x001FF800, 0x003FFC00, 0x003FFC00, 0x001FF800, 0x0FFFFFC0, 0x0FFFFFC0, 0x0FFFFFC0, 0x0FFFFFC0, 0x0FFFFFC0, 0x0FFFFFC0, 0x0FFFFFC0, 0x0FFFFFC0, 0x0FFFFFC0, 0x0FFFFFC0, 0x0FFFFFC0, 0x0FFFFFC0, 0x0FFFFFC0, 0x0FFFFFC0, 0x3FFFFFF0, 0x7FFFFFF8, 0xFFFFFFFC, 0xFFFFFFFC, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
  { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x07E007E0, 0x1FF81FF8, 0x3FFC3FFC, 0x7FFE7FFE, 0x7FFE7FFE, 0x7FFE7FFE, 0x7FFE7FFE, 0x7FFE7FFE, 0x3FFC3FFC, 0x1FF81FF8, 0x07FFFF00, 0x00FFF800, 0x00FFFC00, 0x00FFFC00, 0x00FFFC00, 0x00FFFC00, 0x00FFFC00, 0x03800700, 0x07C00F80, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
  { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x003FFC00, 0x003FFC00, 0x003FFC00, 0x003FFC00, 0x003FFC00, 0x003FFC00, 0x003FFC00, 0x1FFFFFF8, 0x3FFFFFFC, 0x3FFFFFFC, 0x3FFFFFFC, 0x3FFFFFFC, 0x3FFFFFFC, 0x3FFFFFFC, 0x3FFFFFFC, 0x3FFFFFFE, 0x01FFFFC0, 0x01FFFFC0, 0x01FFFF80, 0x01F83F00, 0x01F83F00, 0x01F83F00, 0x03FC7F80, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
  { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x000FF000, 0x003FFC00, 0x007FFE00, 0x007FFE00, 0x007FFE00, 0x007FFE00, 0x007FFE00, 0x003FFC00, 0x003FFC00, 0x003FFC18, 0x003FFC3C, 0x003FFC7E, 0x007FFE3C, 0x00FFFF18, 0x01FFFF80, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
  { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x04924920, 0x04924920, 0x1FFFFFE0, 0x1FFFFFE0, 0xFFFFFFF8, 0x1FFFFFE0, 0xFFFFFFF8, 0x1FFFFFE0, 0xFFFFFFF8, 0x1FFFFFE0, 0xFFFFFFF8, 0x1FFFFFE0, 0x1FFFFFE0, 0x04924920, 0x04924920, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
};

#define RPG_PORTRAIT_CNT 1
#define RPG_P_BARAM      0
static const uint32_t rpg_portrait[RPG_PORTRAIT_CNT][32] = {
  { 0x00000000, 0x001FF000, 0x00FFFD00, 0x03FFFF80, 0x07FFFFC0, 0x0FFFFFE0, 0x0FF7BFE0, 0x1FE31FF0, 0x1FC20FF0, 0x1F800BF0, 0x1F0005F0, 0x1E0001F0, 0x1E783CF0, 0x0E0000E0, 0x18E03860, 0x14B068A0, 0x140000A0, 0x0C0100C0, 0x04010080, 0x04070080, 0x04183080, 0x0227C900, 0x02000100, 0x0107C200, 0x00838400, 0x00639800, 0x001FC000, 0x00E03800, 0x0FBFEF80, 0x3FF07FE0, 0x7FF8FFF0, 0xFFFFFFFF },  // baram
};

static const uint8_t rpg_map[RPG_ROOM_CNT][RPG_MAP_H][RPG_MAP_W] = {
  {  // 0 부트로더 마을
    { 19,19,19,19,19,19,19,19,19,19,19,19 },
    { 19, 0, 8, 8, 8, 8, 0, 0, 2, 0, 0,19 },
    { 19, 0, 7, 7, 9, 7, 0, 0,12, 0, 0,19 },
    { 19, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0 },
    { 19, 0, 0, 0, 3, 3, 3, 3, 3, 3, 3, 3 },
    { 19, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 19, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0,19 },
    { 19,19,19,19,19,19,19,19,19,19,19,19 },
  },
  {  // 1 마을 광장
    { 10,10,10,10,10,10,10,10,10,10,10,10 },
    { 10, 0, 0, 2, 0, 0, 0,11, 0, 0, 0,10 },
    { 10, 0, 4, 4, 0, 0, 0, 0, 2, 0, 0,10 },
    {  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,15, 0 },
    {  3, 3, 3, 3, 3, 3, 3, 3, 3, 3,15, 3 },
    {  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,15, 0 },
    { 10, 0, 0,11, 0, 0, 0, 0, 2, 0, 0,10 },
    { 10,10,10,10,10,10,10,10,10,10,10,10 },
  },
  {  // 2 호숫가 길
    { 20,20,20,20,20,20,20,20,20,20,20,20 },
    { 20, 0, 0, 0, 5, 5, 5, 5, 0, 0,13,20 },
    { 20, 0, 0, 5, 5, 5, 5, 5, 5, 0, 0,20 },
    {  0, 0, 0, 0, 5, 5, 5, 5, 0, 0, 0, 0 },
    {  3, 3, 3, 3, 6, 6, 6, 6, 3, 3, 3, 3 },
    {  0, 0, 0, 0, 5, 5, 5, 5, 0, 0, 0, 0 },
    { 20, 0,12, 0, 0, 5, 5, 0, 0, 0, 2,20 },
    { 20,20,20,20,20,20,20,20,20,20,20,20 },
  },
  {  // 3 인터럽트 호수
    { 20,20,20,20,20,20,20,20,20,20,20,20 },
    { 20, 0, 0, 5, 5, 5, 5, 5, 5, 0, 0,20 },
    { 20, 0, 5, 5, 5, 5, 5, 5, 5, 5, 0,20 },
    {  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,15, 0 },
    {  3, 3, 3, 3, 3, 3, 3, 3, 3, 3,15, 3 },
    {  0, 0, 0, 2, 0, 0, 0, 0, 0, 0,15, 0 },
    { 20, 0, 0, 5, 5, 5, 5, 5, 0, 0, 2,20 },
    { 20,20,20,20,20,20,20,20,20,20,20,20 },
  },
  {  // 4 레지스터 숲 입구
    {  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4 },
    {  4, 0, 4, 0, 0, 0, 0, 4, 0, 0, 4, 4 },
    {  4, 0, 0, 0, 4, 0, 0, 0, 0, 0, 4, 4 },
    {  0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0 },
    {  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 },
    {  0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0 },
    {  4, 0, 4, 0, 0, 0, 0, 0, 0, 4, 0, 4 },
    {  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4 },
  },
  {  // 5 레지스터 숲
    {  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4 },
    {  4, 0, 0, 4, 0, 0, 4, 0, 0,13, 0, 4 },
    {  4, 0, 4, 0, 0, 0, 0, 4, 0, 0, 0, 4 },
    {  0, 0, 0, 0, 4, 0, 0, 0, 0, 0,15, 0 },
    {  3, 3, 3, 3, 3, 3, 3, 3, 3, 3,15, 3 },
    {  0, 0, 0, 4, 0, 0, 4, 0, 0, 0,15, 0 },
    {  4, 0, 0, 0, 0, 4, 0, 0, 0, 4, 0, 4 },
    {  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4 },
  },
  {  // 6 DMA 협곡 입구
    { 22,22,22,22,22,22,22,22,22,22,22,22 },
    { 22,25,25,25,22,22,25,25,25,25,25,22 },
    { 22,25,25,25,25,25,25,25,25,22,22,22 },
    { 25,25,25,25,25,25,25,25,25,25,25,25 },
    {  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 },
    { 25,25,25,25,22,25,25,25,25,25,25,25 },
    { 22,25,25,25,22,22,25,25,25,25,25,22 },
    { 22,22,22,22,22,22,22,22,22,22,22,22 },
  },
  {  // 7 DMA 협곡
    { 22,22,22,22,22,22,22,22,22,22,22,22 },
    { 22,25,25,22,22,22,25,25,25,25,25,22 },
    { 22,25,25,25,25,25,25,25,22,25,25,22 },
    { 25,25,25,25,25,25,25,25,25,25,15,25 },
    {  3, 3, 3, 3, 3, 3, 3, 3, 3, 3,15, 3 },
    { 25,25,25,25,22,22,25,25,25,25,15,25 },
    { 22,25,25,25,25,25,25,25,22,25,25,22 },
    { 22,22,22,22,22,22,22,22,22,22,22,22 },
  },
  {  // 8 RTOS 산기슭
    { 23,23,23,23,23,23,23,23,23,23,23,23 },
    { 23, 0, 0,23, 0, 0, 0, 0, 0,23, 0,23 },
    { 23, 0, 0, 0, 0, 0, 0,23, 0, 0, 0,23 },
    {  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    {  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 },
    {  0, 0, 0,23, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 23, 0, 2, 0, 0, 0, 0,23, 0, 0, 0,23 },
    { 23,23,23,23,23,23,23,23,23,23,23,23 },
  },
  {  // 9 RTOS 산맥
    { 23,23,23,23,23,23,23,23,23,23,23,23 },
    { 23, 0,23,23, 0, 0,23, 0, 0,23,13,23 },
    { 23, 0, 0, 0, 0, 0,23,23, 0, 0, 0,23 },
    {  0, 0, 0,23, 0, 0, 0, 0, 0, 0,15, 0 },
    {  3, 3, 3, 3, 3, 3, 3, 3, 3, 3,15, 3 },
    {  0, 0, 0, 0,23, 0, 0, 0, 0, 0,15, 0 },
    { 23, 0, 0,23,23, 0, 0, 0,23, 0, 0,23 },
    { 23,23,23,23,23,23,23,23,23,23,23,23 },
  },
  {  // 10 저전력 오아시스
    { 24,24,24,24,24,24,24,24,24,24,24,24 },
    { 24,25,25, 5, 5, 5,25,25,25,25,25,24 },
    { 24,25,25, 5, 5, 5,25,25,24,25,25,24 },
    { 25,25,25,25,25,25,25,25,25,25,25,25 },
    {  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 },
    { 25,25,25,24,25,25,25,25,25,25,25,25 },
    { 24,25,25,25,25,25,24,25,25,25,24,24 },
    { 24,24,24,24,24,24,24,24,24,24,24,24 },
  },
  {  // 11 저전력 사막
    { 24,24,24,24,24,24,24,24,24,24,24,24 },
    { 24,25,25,25,24,25,25,25,25,24,25,24 },
    { 24,25,24,25,25,25,25,25,25,25,24,24 },
    { 25,25,25,25,25,25,24,25,25,25,15,25 },
    {  3, 3, 3, 3, 3, 3, 3, 3, 3, 3,15, 3 },
    { 25,25,25,24,25,25,25,25,25,25,15,25 },
    { 24,25,25,25,25,24,25,25,25,25,25,24 },
    { 24,24,24,24,24,24,24,24,24,24,24,24 },
  },
  {  // 12 커널 사당
    { 21,21,21,21,21,21,21,21,21,21,21,21 },
    { 21, 0,17, 0, 0,16, 0, 0,17, 0, 0,21 },
    { 21, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,21 },
    {  0, 0, 0, 0,18,18,18,18, 0, 0, 0,21 },
    {  3, 3, 3, 3,18,18,18,18, 0, 0, 0,21 },
    {  0, 0, 0, 0,18,18,18,18, 0, 0, 0,21 },
    { 21, 0,17, 0, 0, 0, 0, 0, 0,17, 0,21 },
    { 21,21,21,21,21,21,21,21,21,21,21,21 },
  },
};
static const char *const rpg_room_name[RPG_ROOM_CNT] = { "부트로더 마을", "마을 광장", "호숫가 길", "인터럽트 호수", "레지스터 숲 입구", "레지스터 숲", "DMA 협곡 입구", "DMA 협곡", "RTOS 산기슭", "RTOS 산맥", "저전력 오아시스", "저전력 사막", "커널 사당" };
static const uint8_t rpg_room_ckpt[RPG_ROOM_CNT] = { 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1 };
static const int8_t rpg_room_gate[RPG_ROOM_CNT] = { -1, 1, -1, 3, -1, 5, -1, 10, -1, 12, -1, 14, -1 };
static const int8_t rpg_room_link[RPG_ROOM_CNT][4] = {  // 북, 남, 서, 동
  { -1, -1, -1, 1 },
  { -1, -1, 0, 2 },
  { -1, -1, 1, 3 },
  { -1, -1, 2, 4 },
  { -1, -1, 3, 5 },
  { -1, -1, 4, 6 },
  { -1, -1, 5, 7 },
  { -1, -1, 6, 8 },
  { -1, -1, 7, 9 },
  { -1, -1, 8, 10 },
  { -1, -1, 9, 11 },
  { -1, -1, 10, 12 },
  { -1, -1, 11, -1 },
};

typedef struct { const char *name; uint8_t lo, hi; const char *msg; } rpg_skill_t;
static const rpg_skill_t rpg_skill[RPG_SKILL_CNT] = {
  { "printf", 2, 4, "printf 로 로그를 찍었다!" },
  { "중단점", 4, 7, "브레이크포인트를 걸었다!" },
  { "레지스터", 4, 7, "레지스터를 덤프했다!" },
  { "분석기", 5, 8, "로직 분석기로 파형을 봤다!" },
  { "뮤텍스", 5, 8, "뮤텍스로 순서를 정했다!" },
  { "슬립모드", 5, 8, "슬립 모드로 재웠다!" },
  { "근본원인", 8, 12, "근본 원인을 분석했다!" },
};
#define RPG_ITEM_HEAL 0
#define RPG_ITEM_DUCK 1
typedef struct { const char *name; uint8_t kind; uint8_t val; } rpg_item_t;
static const rpg_item_t rpg_item[RPG_ITEM_CNT] = {
  { "커피", RPG_ITEM_HEAL, 10 },
  { "에너지드링크", RPG_ITEM_HEAL, 0 },
  { "고무오리", RPG_ITEM_DUCK, 0 },
};
typedef struct { const char *name; uint8_t atk; } rpg_equip_t;
static const rpg_equip_t rpg_equip[RPG_EQUIP_CNT] = {
  { "멀티미터", 1 },
  { "오실로스코프", 2 },
  { "JTAG", 3 },
};
typedef struct { const char *name; uint8_t bat; uint8_t hp, atk; const char *atk_msg; int8_t weak; const char *desc; uint8_t tough; } rpg_enemy_t;
static const rpg_enemy_t rpg_enemy[RPG_ENEMY_CNT] = {
  { "버그 애벌레", 0, 7, 2, "코드를 갉아먹었다!", -1, "어디에나 있는 작은 버그", 0 },
  { "잔상 유령", 2, 12, 3, "잔상이 화면에 번졌다!", 0, "지난 화면이 남아 있어요", 0 },
  { "경고 나방", 1, 9, 2, "경고를 잔뜩 뿌렸다!", -1, "무시한 컴파일 경고예요", 0 },
  { "키 씹힘 벌레", 3, 16, 3, "입력을 씹어 먹었다!", 1, "눌렀는데 안 먹혀요", 0 },
  { "매직넘버 애벌레", 0, 11, 3, "알 수 없는 숫자를 던졌다!", -1, "0x1F 가 왜 0x1F 인지 몰라요", 0 },
  { "스택 오버플로", 4, 20, 4, "스택을 덮어썼다!", 2, "상자가 넘쳐 옆 짐을 덮었어요", 0 },
  { "오프바이원 나방", 1, 13, 3, "배열을 한 칸 넘어 썼다!", -1, "딱 한 칸씩 넘어가요", 0 },
  { "레이스 컨디션 쌍둥이", 5, 24, 4, "값을 동시에 바꿔 버렸다!", 3, "둘이 동시에 같은 걸 고쳐요", 0 },
  { "메모리 누수 애벌레", 0, 15, 4, "메모리를 삼켜 버렸다!", -1, "빌려 간 메모리를 안 돌려줘요", 0 },
  { "데드락 골렘", 6, 28, 5, "모든 것을 멈춰 세웠다!", 4, "서로 비켜 주길 기다려요", 0 },
  { "워치독 나방", 1, 16, 4, "시스템을 리셋하려 했다!", -1, "안 쓰다듬으면 리셋해요", 0 },
  { "누설전류 모래벌레", 7, 30, 5, "배터리를 빨아먹었다!", 5, "꺼 놨는데 배터리가 닳아요", 0 },
  { "수련용 버그", 0, 10, 2, "가볍게 덤볐다!", 1, "고수가 불러낸 수련 상대", 1 },
  { "하드폴트", 8, 40, 6, "CPU 를 멈춰 세웠다!", 6, "프로그램이 길을 잃었어요", 1 },
};

#define RPG_ENT_NPC  0
#define RPG_ENT_BUG  1
#define RPG_ENT_OBJ  2
typedef struct { uint8_t room, x, y; int8_t spr; uint8_t kind; uint8_t id; int8_t flag; } rpg_ent_t;
// id: NPC·OBJ = 스크립트 주인, BUG = 적. flag: BUG = 이기면 세울 깃발, OBJ = 열림 상태 깃발 (상자)
static const rpg_ent_t rpg_ent[RPG_ENT_CNT] = {
  { 0, 8, 5, 6, RPG_ENT_NPC, 0, -1 },  // senior
  { 0, 8, 2, -1, RPG_ENT_OBJ, 1, -1 },  // sign0
  { 0, 4, 2, -1, RPG_ENT_OBJ, 2, -1 },  // door0
  { 1, 3, 5, 13, RPG_ENT_NPC, 3, -1 },  // villager
  { 1, 5, 2, 18, RPG_ENT_BUG, 0, -1 },  // 버그 애벌레
  { 1, 9, 4, 9, RPG_ENT_BUG, 1, 1 },  // 잔상 유령
  { 2, 9, 5, 19, RPG_ENT_BUG, 2, -1 },  // 경고 나방
  { 2, 2, 6, -1, RPG_ENT_OBJ, 4, -1 },  // sign2
  { 2, 10, 1, -1, RPG_ENT_OBJ, 5, 7 },  // chest2
  { 3, 5, 5, 7, RPG_ENT_NPC, 6, -1 },  // irq
  { 3, 9, 4, 10, RPG_ENT_BUG, 3, 3 },  // 키 씹힘 벌레
  { 4, 5, 2, 14, RPG_ENT_NPC, 7, -1 },  // sage
  { 4, 8, 5, 18, RPG_ENT_BUG, 4, -1 },  // 매직넘버 애벌레
  { 5, 9, 4, 11, RPG_ENT_BUG, 5, 5 },  // 스택 오버플로
  { 5, 9, 1, -1, RPG_ENT_OBJ, 8, 6 },  // chest5
  { 6, 6, 2, 15, RPG_ENT_NPC, 9, -1 },  // dma
  { 6, 3, 5, 19, RPG_ENT_BUG, 6, -1 },  // 오프바이원 나방
  { 7, 9, 4, 20, RPG_ENT_BUG, 7, 10 },  // 레이스 컨디션 쌍둥이
  { 8, 5, 2, 16, RPG_ENT_NPC, 10, -1 },  // monk
  { 8, 9, 5, 18, RPG_ENT_BUG, 8, -1 },  // 메모리 누수 애벌레
  { 9, 9, 4, 21, RPG_ENT_BUG, 9, 12 },  // 데드락 골렘
  { 9, 10, 1, -1, RPG_ENT_OBJ, 11, 8 },  // chest9
  { 10, 7, 2, 17, RPG_ENT_NPC, 12, -1 },  // hermit
  { 10, 5, 5, 19, RPG_ENT_BUG, 10, -1 },  // 워치독 나방
  { 11, 9, 4, 22, RPG_ENT_BUG, 11, 14 },  // 누설전류 모래벌레
  { 12, 5, 2, 8, RPG_ENT_NPC, 13, -1 },  // master
  { 12, 5, 1, -1, RPG_ENT_OBJ, 14, -1 },  // altar
};

typedef struct { const char *who; const char *const *lines; } rpg_block_t;   // who NULL = 안내, "바람" = 초상화
static const char *const rpg_t0[] = { "printf 를 했는데 외계어가", "나온다. 무엇부터 볼까?", NULL };
static const char *const rpg_t1[] = { "그렇지! 양쪽 속도가 다르면", "글자가 깨지지.", NULL };
static const char *const rpg_t2[] = { "음... 보레이트부터 맞춰 봐.", NULL };
static const char *const rpg_t3[] = { "인터럽트 안에서", "오래 기다려도 될까?", NULL };
static const char *const rpg_t4[] = { "옳거니. 인터럽트는 짧게!", NULL };
static const char *const rpg_t5[] = { "허허, 그러다 다른", "인터럽트를 놓치네.", NULL };
static const char *const rpg_t6[] = { "LED 가 안 켜진다.", "레지스터에서 무엇을 볼까?", NULL };
static const char *const rpg_t7[] = { "그렇지. 클럭이 없으면", "핀도 잠들어 있느니라.", NULL };
static const char *const rpg_t8[] = { "다시 생각해 보거라.", NULL };
static const char *const rpg_t9[] = { "DMA 로 받은 값이 옛날 값이다.", "무엇을 의심할까?", NULL };
static const char *const rpg_t10[] = { "맞아! CPU 는 캐시에 남은", "옛날 값을 보고 있었지.", NULL };
static const char *const rpg_t11[] = { "흠, 그건 아닐 걸.", NULL };
static const char *const rpg_t12[] = { "두 태스크가 서로를 기다리며", "멈췄습니다. 이것은?", NULL };
static const char *const rpg_t13[] = { "그렇습니다. 데드락이지요.", NULL };
static const char *const rpg_t14[] = { "다시 생각해 보세요.", NULL };
static const char *const rpg_t15[] = { "꺼 놨는데 배터리가 닳는다.", "무엇을 재야 할까?", NULL };
static const char *const rpg_t16[] = { "그래. 잠든 동안 새는", "전류를 재야지.", NULL };
static const char *const rpg_t17[] = { "아니야, 다시.", NULL };
static const char *const rpg_t18[] = { "버그를 만나면", "가장 먼저 할 일은?", NULL };
static const char *const rpg_t19[] = { "그렇다. 재현해야 고칠 수 있다.", NULL };
static const char *const rpg_t20[] = { "허허, 다시 생각해 보아라.", NULL };
static const char *const rpg_t21[] = { "증상이 사라졌다.", "버그를 잡은 것일까?", NULL };
static const char *const rpg_t22[] = { "그렇다. 증상이 사라졌다고", "원인을 잡은 것은 아니다.", NULL };
static const char *const rpg_t23[] = { "그러다 다시 만난다.", NULL };
typedef struct { const char *const *q; const char *opt[3]; uint8_t ans; const char *const *right; const char *const *wrong; } rpg_quiz_t;
#define RPG_QUIZ_CNT 8
static const rpg_quiz_t rpg_quiz[RPG_QUIZ_CNT] = {
  { rpg_t0, { "보레이트", "컴파일러 버전", "기도하기" }, 0, rpg_t1, rpg_t2 },
  { rpg_t3, { "그래도 된다", "짧게 끝낸다", "상관없다" }, 1, rpg_t4, rpg_t5 },
  { rpg_t6, { "핀 클럭이 켜졌나", "화면 밝기", "배터리 색깔" }, 0, rpg_t7, rpg_t8 },
  { rpg_t9, { "캐시", "키보드", "날씨" }, 0, rpg_t10, rpg_t11 },
  { rpg_t12, { "데드락", "오버클럭", "점심시간" }, 0, rpg_t13, rpg_t14 },
  { rpg_t15, { "대기 전류", "화면 크기", "코드 줄 수" }, 0, rpg_t16, rpg_t17 },
  { rpg_t18, { "재현하기", "코드 다 지우기", "퇴근하기" }, 0, rpg_t19, rpg_t20 },
  { rpg_t21, { "원인을 찾아야 안다", "잡았다", "모른 척한다" }, 0, rpg_t22, rpg_t23 },
};
static const char *const rpg_t24[] = { "바람, 고수가 되고 싶다고?", "그럼 수련을 떠나야지.", "", "가는 길마다 버그가 길을 막아.", "지역마다 스승을 찾아가", "기술을 배우도록 해.", "", "그 전에 하나만 물어보자.", NULL };
static const rpg_block_t rpg_bk25[] = { { "선배 개발자", rpg_t24 } };
static const char *const rpg_t26[] = { "printf 는 이미 쓸 줄 알지?", "커피 두 잔 챙겨 가.", NULL };
static const char *const rpg_t27[] = { "(커피 2잔을 얻었다)", NULL };
static const rpg_block_t rpg_bk28[] = { { "선배 개발자", rpg_t26 }, { NULL, rpg_t27 } };
static const char *const rpg_t29[] = { "동쪽 광장에 잔상 유령이", "나타났대. printf 로 상태를", "찍어 보면 약점이 보일 거야.", NULL };
static const rpg_block_t rpg_bk30[] = { { "선배 개발자", rpg_t29 } };
static const char *const rpg_t31[] = { "잘하고 있구나!", "", "예전에 키보드 버그를 잡는데,", "범인이 물려 둔 전류계였지.", "", "계측 도구도 대상을", "바꿀 수 있다는 걸 잊지 마.", NULL };
static const rpg_block_t rpg_bk32[] = { { "선배 개발자", rpg_t31 } };
static const char *const rpg_t33[] = { "[부트로더 마을]", "동: 마을 광장, 호숫가 길", NULL };
static const rpg_block_t rpg_bk34[] = { { NULL, rpg_t33 } };
static const char *const rpg_t35[] = { "바람의 집이다.", "책상 위에 굽다 만 보드가 있다.", NULL };
static const rpg_block_t rpg_bk36[] = { { NULL, rpg_t35 } };
static const char *const rpg_t37[] = { "광장에 유령이 나와서", "화면이 이상해!", "지난 화면이 계속 남아 있어...", NULL };
static const rpg_block_t rpg_bk38[] = { { "마을 사람", rpg_t37 } };
static const char *const rpg_t39[] = { "유령이 사라지니", "화면이 깨끗해졌어!", "고마워, 바람!", NULL };
static const rpg_block_t rpg_bk40[] = { { "마을 사람", rpg_t39 } };
static const char *const rpg_t41[] = { "상자를 열었다!", "멀티미터와 고무오리를 얻었다.", "", NULL };
static const char *const rpg_t42[] = { "고무 오리에게 설명하다 보면", "버그가 보인다고 했지!", NULL };
static const rpg_block_t rpg_bk43[] = { { NULL, rpg_t41 }, { "바람", rpg_t42 } };
static const char *const rpg_t44[] = { "빈 상자다.", NULL };
static const rpg_block_t rpg_bk45[] = { { NULL, rpg_t44 } };
static const char *const rpg_t46[] = { "[호숫가 길]", "동쪽: 인터럽트 호수", NULL };
static const rpg_block_t rpg_bk47[] = { { NULL, rpg_t46 } };
static const char *const rpg_t48[] = { "허허, 물고기는 기다림으로,", "버그는 관찰로 잡는 법이지.", "", "한 가지 묻겠네.", NULL };
static const rpg_block_t rpg_bk49[] = { { "인터럽트 장인", rpg_t48 } };
static const char *const rpg_t50[] = { "브레이크포인트를 가르쳐 주마.", "멈춰 세우고 들여다보는 기술이지.", NULL };
static const char *const rpg_t51[] = { "(기술 '중단점'을 배웠다!)", "(커피 1잔을 얻었다)", NULL };
static const rpg_block_t rpg_bk52[] = { { "인터럽트 장인", rpg_t50 }, { NULL, rpg_t51 } };
static const char *const rpg_t53[] = { "동쪽에 키 씹힘 벌레가 있네.", "중단점이 잘 들을 걸세.", NULL };
static const rpg_block_t rpg_bk54[] = { { "인터럽트 장인", rpg_t53 } };
static const char *const rpg_t55[] = { "잘했네. 이제 레지스터 숲으로.", "데이터시트 현자가 기다리네.", NULL };
static const rpg_block_t rpg_bk56[] = { { "인터럽트 장인", rpg_t55 } };
static const char *const rpg_t57[] = { "답은 늘 데이터시트에 있느니라.", "", "물어보마.", NULL };
static const rpg_block_t rpg_bk58[] = { { "데이터시트 현자", rpg_t57 } };
static const char *const rpg_t59[] = { "레지스터를 직접 읽는 법을", "알려 주마.", NULL };
static const char *const rpg_t60[] = { "(기술 '레지스터'를 배웠다!)", "(커피 1잔을 얻었다)", NULL };
static const rpg_block_t rpg_bk61[] = { { "데이터시트 현자", rpg_t59 }, { NULL, rpg_t60 } };
static const char *const rpg_t62[] = { "숲 깊은 곳에", "스택 오버플로가 산다.", "레지스터로 스택을 보거라.", NULL };
static const rpg_block_t rpg_bk63[] = { { "데이터시트 현자", rpg_t62 } };
static const char *const rpg_t64[] = { "DMA 협곡의 장인을", "찾아가거라.", NULL };
static const rpg_block_t rpg_bk65[] = { { "데이터시트 현자", rpg_t64 } };
static const char *const rpg_t66[] = { "상자를 열었다!", "오실로스코프를 얻었다.", "(공격력 +2)", NULL };
static const rpg_block_t rpg_bk67[] = { { NULL, rpg_t66 } };
static const char *const rpg_t68[] = { "빈 상자다.", NULL };
static const rpg_block_t rpg_bk69[] = { { NULL, rpg_t68 } };
static const char *const rpg_t70[] = { "DMA 는 CPU 몰래", "일하는 일꾼이지.", "", "하나 물어보지.", NULL };
static const rpg_block_t rpg_bk71[] = { { "DMA 장인", rpg_t70 } };
static const char *const rpg_t72[] = { "로직 분석기로 신호를", "보는 법을 알려 주지.", NULL };
static const char *const rpg_t73[] = { "(기술 '분석기'를 배웠다!)", "(커피 1잔을 얻었다)", NULL };
static const rpg_block_t rpg_bk74[] = { { "DMA 장인", rpg_t72 }, { NULL, rpg_t73 } };
static const char *const rpg_t75[] = { "협곡 끝에 쌍둥이가 살아.", "둘이 동시에 움직이는 걸", "파형으로 보면 잡혀.", NULL };
static const rpg_block_t rpg_bk76[] = { { "DMA 장인", rpg_t75 } };
static const char *const rpg_t77[] = { "RTOS 산맥의 수도승을", "만나 보게.", NULL };
static const rpg_block_t rpg_bk78[] = { { "DMA 장인", rpg_t77 } };
static const char *const rpg_t79[] = { "모든 태스크에는", "차례가 있지요.", "", "문제를 내겠습니다.", NULL };
static const rpg_block_t rpg_bk80[] = { { "스케줄러 수도승", rpg_t79 } };
static const char *const rpg_t81[] = { "뮤텍스로 순서를 지키는", "법을 알려 드리지요.", NULL };
static const char *const rpg_t82[] = { "(기술 '뮤텍스'를 배웠다!)", "(커피 1잔을 얻었다)", NULL };
static const rpg_block_t rpg_bk83[] = { { "스케줄러 수도승", rpg_t81 }, { NULL, rpg_t82 } };
static const char *const rpg_t84[] = { "산 위의 골렘은 서로를", "기다리다 굳어 버린", "녀석입니다.", NULL };
static const rpg_block_t rpg_bk85[] = { { "스케줄러 수도승", rpg_t84 } };
static const char *const rpg_t86[] = { "사막의 은둔자를", "찾아가 보세요.", NULL };
static const rpg_block_t rpg_bk87[] = { { "스케줄러 수도승", rpg_t86 } };
static const char *const rpg_t88[] = { "상자를 열었다!", "JTAG 디버거를 얻었다.", "(공격력 +3)", NULL };
static const rpg_block_t rpg_bk89[] = { { NULL, rpg_t88 } };
static const char *const rpg_t90[] = { "빈 상자다.", NULL };
static const rpg_block_t rpg_bk91[] = { { NULL, rpg_t90 } };
static const char *const rpg_t92[] = { "켜 있는 것만 일하는 게", "아니야. 잠드는 법을 알아야", "오래 가지.", "", "물어보마.", NULL };
static const rpg_block_t rpg_bk93[] = { { "배터리 은둔자", rpg_t92 } };
static const char *const rpg_t94[] = { "제대로 재우는 법을", "알려 주지.", NULL };
static const char *const rpg_t95[] = { "(기술 '슬립모드'를 배웠다!)", "(에너지드링크 1개를 얻었다)", NULL };
static const rpg_block_t rpg_bk96[] = { { "배터리 은둔자", rpg_t94 }, { NULL, rpg_t95 } };
static const char *const rpg_t97[] = { "사막의 모래벌레가", "전류를 빨아먹어.", "제대로 재워야 이긴다.", NULL };
static const rpg_block_t rpg_bk98[] = { { "배터리 은둔자", rpg_t97 } };
static const char *const rpg_t99[] = { "이제 커널 사당으로 가거라.", "펌웨어 고수가 기다린다.", NULL };
static const rpg_block_t rpg_bk100[] = { { "배터리 은둔자", rpg_t99 } };
static const char *const rpg_t101[] = { "제단에 리셋 벡터가", "새겨져 있다.", NULL };
static const rpg_block_t rpg_bk102[] = { { NULL, rpg_t101 } };
static const char *const rpg_t103[] = { "여기까지 왔구나, 바람.", "고수가 되려면", "세 가지 수련이 남았다.", "", "첫째, 마음가짐이다.", NULL };
static const rpg_block_t rpg_bk104[] = { { "펌웨어 고수", rpg_t103 } };
static const char *const rpg_t105[] = { "둘째, 실전이다.", "이 수련용 버그의", "약점을 찾아라.", "", "(힌트: 멈춰 세우고", " 들여다보아라)", NULL };
static const rpg_block_t rpg_bk106[] = { { "펌웨어 고수", rpg_t105 } };
static const char *const rpg_t107[] = { "셋째, 끝을 보는 눈이다.", NULL };
static const rpg_block_t rpg_bk108[] = { { "펌웨어 고수", rpg_t107 } };
static const char *const rpg_t109[] = { "좋다. 내 비기를 전하마.", NULL };
static const char *const rpg_t110[] = { "(기술 '근본원인'을 배웠다!)", "(에너지드링크 2개를 얻었다)", NULL };
static const rpg_block_t rpg_bk111[] = { { "펌웨어 고수", rpg_t109 }, { NULL, rpg_t110 } };
static const char *const rpg_t112[] = { "마지막 시험이다.", "부팅마다 멈추는", "저 하드폴트를 잡아 보거라!", NULL };
static const rpg_block_t rpg_bk113[] = { { "펌웨어 고수", rpg_t112 } };
static const char *const rpg_t114[] = { "스택 프레임에서 PC 를", "찾았구나. 널 포인터 하나가", "범인이었지.", "", "이제 너는 펌웨어 고수다,", "바람!", NULL };
static const char *const rpg_t115[] = { "버그는 무섭지 않아.", "재현하고, 관찰하고,", "고치면 돼!", NULL };
static const char *const rpg_t116[] = { "- 끝 -", "플레이 고마워요!", NULL };
static const rpg_block_t rpg_bk117[] = { { "펌웨어 고수", rpg_t114 }, { "바람", rpg_t115 }, { NULL, rpg_t116 } };
static const char *const rpg_t118[] = { "버그는 늘 다시 온다.", "그때마다 침착하게", "재현하고 관찰하거라.", NULL };
static const rpg_block_t rpg_bk119[] = { { "펌웨어 고수", rpg_t118 } };
typedef struct {
  uint8_t owner; int8_t need, nope;
  const rpg_block_t *pre; uint8_t npre;
  int8_t quiz, set, skill, item; uint8_t item_n; int8_t equip, battle, win_set; uint8_t chain, end;
  const rpg_block_t *post; uint8_t npost;
} rpg_script_t;
#define RPG_SCRIPT_CNT 35
static const rpg_script_t rpg_script[RPG_SCRIPT_CNT] = {
  { 0, -1, 0, rpg_bk25, 1, 0, 0, -1, 0, 2, -1, -1, -1, 0, 0, rpg_bk28, 2 },
  { 0, 0, 1, rpg_bk30, 1, -1, -1, -1, -1, 0, -1, -1, -1, 0, 0, NULL, 0 },
  { 0, 1, -1, rpg_bk32, 1, -1, -1, -1, -1, 0, -1, -1, -1, 0, 0, NULL, 0 },
  { 1, -1, -1, rpg_bk34, 1, -1, -1, -1, -1, 0, -1, -1, -1, 0, 0, NULL, 0 },
  { 2, -1, -1, rpg_bk36, 1, -1, -1, -1, -1, 0, -1, -1, -1, 0, 0, NULL, 0 },
  { 3, -1, 1, rpg_bk38, 1, -1, -1, -1, -1, 0, -1, -1, -1, 0, 0, NULL, 0 },
  { 3, 1, -1, rpg_bk40, 1, -1, -1, -1, -1, 0, -1, -1, -1, 0, 0, NULL, 0 },
  { 5, -1, 7, rpg_bk43, 2, -1, 7, -1, 2, 1, 0, -1, -1, 0, 0, NULL, 0 },
  { 5, 7, -1, rpg_bk45, 1, -1, -1, -1, -1, 0, -1, -1, -1, 0, 0, NULL, 0 },
  { 4, -1, -1, rpg_bk47, 1, -1, -1, -1, -1, 0, -1, -1, -1, 0, 0, NULL, 0 },
  { 6, -1, 2, rpg_bk49, 1, 1, 2, 1, 0, 1, -1, -1, -1, 0, 0, rpg_bk52, 2 },
  { 6, 2, 3, rpg_bk54, 1, -1, -1, -1, -1, 0, -1, -1, -1, 0, 0, NULL, 0 },
  { 6, 3, -1, rpg_bk56, 1, -1, -1, -1, -1, 0, -1, -1, -1, 0, 0, NULL, 0 },
  { 7, -1, 4, rpg_bk58, 1, 2, 4, 2, 0, 1, -1, -1, -1, 0, 0, rpg_bk61, 2 },
  { 7, 4, 5, rpg_bk63, 1, -1, -1, -1, -1, 0, -1, -1, -1, 0, 0, NULL, 0 },
  { 7, 5, -1, rpg_bk65, 1, -1, -1, -1, -1, 0, -1, -1, -1, 0, 0, NULL, 0 },
  { 8, -1, 6, rpg_bk67, 1, -1, 6, -1, -1, 0, 1, -1, -1, 0, 0, NULL, 0 },
  { 8, 6, -1, rpg_bk69, 1, -1, -1, -1, -1, 0, -1, -1, -1, 0, 0, NULL, 0 },
  { 9, -1, 9, rpg_bk71, 1, 3, 9, 3, 0, 1, -1, -1, -1, 0, 0, rpg_bk74, 2 },
  { 9, 9, 10, rpg_bk76, 1, -1, -1, -1, -1, 0, -1, -1, -1, 0, 0, NULL, 0 },
  { 9, 10, -1, rpg_bk78, 1, -1, -1, -1, -1, 0, -1, -1, -1, 0, 0, NULL, 0 },
  { 10, -1, 11, rpg_bk80, 1, 4, 11, 4, 0, 1, -1, -1, -1, 0, 0, rpg_bk83, 2 },
  { 10, 11, 12, rpg_bk85, 1, -1, -1, -1, -1, 0, -1, -1, -1, 0, 0, NULL, 0 },
  { 10, 12, -1, rpg_bk87, 1, -1, -1, -1, -1, 0, -1, -1, -1, 0, 0, NULL, 0 },
  { 11, -1, 8, rpg_bk89, 1, -1, 8, -1, 2, 1, 2, -1, -1, 0, 0, NULL, 0 },
  { 11, 8, -1, rpg_bk91, 1, -1, -1, -1, -1, 0, -1, -1, -1, 0, 0, NULL, 0 },
  { 12, -1, 13, rpg_bk93, 1, 5, 13, 5, 1, 1, -1, -1, -1, 0, 0, rpg_bk96, 2 },
  { 12, 13, 14, rpg_bk98, 1, -1, -1, -1, -1, 0, -1, -1, -1, 0, 0, NULL, 0 },
  { 12, 14, -1, rpg_bk100, 1, -1, -1, -1, -1, 0, -1, -1, -1, 0, 0, NULL, 0 },
  { 14, -1, -1, rpg_bk102, 1, -1, -1, -1, -1, 0, -1, -1, -1, 0, 0, NULL, 0 },
  { 13, -1, 15, rpg_bk104, 1, 6, 15, -1, -1, 0, -1, -1, -1, 1, 0, NULL, 0 },
  { 13, 15, 16, rpg_bk106, 1, -1, -1, -1, -1, 0, -1, 12, 16, 1, 0, NULL, 0 },
  { 13, 16, 17, rpg_bk108, 1, 7, 17, 6, 1, 2, -1, -1, -1, 1, 0, rpg_bk111, 2 },
  { 13, 17, 18, rpg_bk113, 1, -1, -1, -1, -1, 0, -1, 13, 18, 0, 1, rpg_bk117, 3 },
  { 13, 18, -1, rpg_bk119, 1, -1, -1, -1, -1, 0, -1, -1, -1, 0, 0, NULL, 0 },
};
static const char *const rpg_t120[] = { "개발자 바람은 오늘도", "보드 앞에 앉았다.", "이유 모를 리셋과 화면의 잔상...", NULL };
static const char *const rpg_t121[] = { "펌웨어 고수가 되고 싶다!", "수련을 떠나자.", NULL };
static const char *const rpg_t122[] = { "방향키: 이동", "A: 조사/대화  B: 메뉴", "먼저 선배에게 말을 걸어 보자.", NULL };
static const rpg_block_t rpg_bk123[] = { { NULL, rpg_t120 }, { "바람", rpg_t121 }, { NULL, rpg_t122 } };
static const char *const rpg_t124[] = { "동쪽 철문이 열렸다!", NULL };
static const rpg_block_t rpg_bk125[] = { { NULL, rpg_t124 } };
static const rpg_block_t *const rpg_intro = rpg_bk123;
#define RPG_INTRO_CNT 3
static const rpg_block_t *const rpg_gate_open = rpg_bk125;
#define RPG_GATE_OPEN_CNT 1

#define RPG_START_ROOM 0
#define RPG_START_X    6
#define RPG_START_Y    5

#endif
