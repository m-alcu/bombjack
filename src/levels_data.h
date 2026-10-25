// Generated from the original Bomb Jack ROM set (MAME "bombjack", Tehkan 1984)
// and video-RAM dumps of each round taken in MAME. World space is 512x448 for the
// 224x224 play area: world = play_px * (512/224, 2), play_px = arcade_px - 16.
//
//   rounds        ROM 0xDE38, 6 bytes/round: platform layout, -, background
//                 picture, bomb layout, scene, music. 64 rounds; after round 64
//                 the arcade continues at round 32 (ROM 0x4131).
//   bombs         ROM 0xC828 (24 packed nibbles per layout, LUT 0xC9C8), stored as
//                 centres. List order is the lit-bomb sequence.
//   platforms     solid tiles of each platform layout (8x8 cells) read from video RAM.
//   mummy spawns  ROM 0x3DCF, 4 candidate centres per platform layout.
//
// The enemy data (birds, bouncing, mummy transform lists) still comes from the
// LOVE/Lua reference (bombjack-resources/bombjack-main/levels.json) for rounds
// 1-30; it has not been located in the ROM. Rounds 31-64 reuse round ((n-1)%30)+1.
#pragma once
namespace leveldata {
struct Plat { float x, y, w, h; };
struct Pt   { float x, y; };
static const Plat plats_0[] = {{274.3,96,128,16}, {109.7,144,73.1,16}, {219.4,288,109.7,16}, {54.9,336,73.1,16}, {292.6,384,164.6,16}};
static const Plat plats_1[] = {{182.9,96,146.3,16}, {182.9,336,146.3,16}, {109.7,160,18.3,128}, {384,160,18.3,128}};
static const Plat plats_2[] = {{237.7,96,91.4,16}, {18.3,144,146.3,16}, {329.1,192,109.7,16}, {109.7,288,237.7,16}, {329.1,208,18.3,80}};
static const Plat plats_3[] = {{73.1,144,109.7,16}, {329.1,144,109.7,16}, {73.1,288,109.7,16}, {329.1,288,109.7,16}, {164.6,64,18.3,80}, {164.6,304,18.3,80}, {329.1,64,18.3,80}, {329.1,304,18.3,80}};
static const Plat plats_4[] = {{0,0,0,0}};
static const Plat plats_5[] = {{54.9,96,73.1,16}, {219.4,96,73.1,16}, {384,96,73.1,16}, {128,192,91.4,16}, {292.6,192,91.4,16}, {54.9,288,73.1,16}, {219.4,288,73.1,16}, {384,288,73.1,16}, {128,384,91.4,16}, {292.6,384,91.4,16}};
static const Plat plats_6[] = {{219.4,96,73.1,16}, {109.7,192,109.7,16}, {292.6,192,109.7,16}, {109.7,240,109.7,16}, {292.6,240,109.7,16}, {219.4,336,73.1,16}};
static const Plat plats_7[] = {{73.1,96,91.4,16}, {347.4,96,91.4,16}, {73.1,336,91.4,16}, {347.4,336,91.4,16}};
static const Plat plats_8[] = {{128,96,91.4,16}, {292.6,96,91.4,16}, {219.4,288,73.1,16}, {54.9,336,73.1,16}, {384,336,73.1,16}};
static const Plat plats_9[] = {{109.7,96,292.6,16}, {219.4,288,73.1,16}, {109.7,112,18.3,176}, {384,112,18.3,176}};
static const Plat plats_10[] = {{219.4,48,54.9,16}, {219.4,96,73.1,16}, {164.6,144,128,16}, {219.4,192,73.1,16}, {219.4,240,182.9,16}, {219.4,288,73.1,16}, {54.9,336,237.7,16}, {219.4,384,73.1,16}, {219.4,64,18.3,32}, {219.4,160,18.3,32}, {219.4,256,18.3,32}, {219.4,352,18.3,32}};
static const Plat plats_11[] = {{18.3,144,91.4,16}, {402.3,144,91.4,16}, {73.1,240,91.4,16}, {347.4,240,91.4,16}, {128,336,91.4,16}, {292.6,336,91.4,16}};
static const Plat plats_12[] = {{109.7,96,164.6,16}, {329.1,96,73.1,16}, {109.7,336,73.1,16}, {237.7,336,164.6,16}, {109.7,112,18.3,224}, {384,112,18.3,224}};
static const Plat plats_13[] = {{109.7,48,182.9,16}, {54.9,192,128,16}, {329.1,240,128,16}, {219.4,384,182.9,16}, {54.9,208,18.3,144}, {219.4,288,18.3,96}, {274.3,64,18.3,96}, {438.9,96,18.3,144}};
static const Plat plats_14[] = {{402.3,96,91.4,16}, {18.3,144,91.4,16}, {402.3,192,91.4,16}, {18.3,240,91.4,16}, {402.3,288,91.4,16}, {18.3,336,91.4,16}};
static const Plat plats_15[] = {{128,48,91.4,16}, {292.6,48,91.4,16}, {219.4,144,73.1,16}, {219.4,288,73.1,16}, {128,384,91.4,16}, {292.6,384,91.4,16}, {54.9,112,18.3,80}, {54.9,256,18.3,80}, {164.6,192,18.3,64}, {329.1,192,18.3,64}, {438.9,112,18.3,80}, {438.9,256,18.3,80}};
static const Pt bombs_0[] = {{256,128}, {310.9,128}, {365.7,128}, {420.6,128}, {475.4,176}, {475.4,224}, {475.4,272}, {475.4,320}, {36.6,176}, {36.6,224}, {36.6,272}, {36.6,320}, {91.4,32}, {146.3,32}, {201.1,32}, {420.6,368}, {365.7,368}, {310.9,368}, {201.1,416}, {146.3,416}, {91.4,416}, {475.4,32}, {420.6,32}, {365.7,32}};
static const Pt bombs_1[] = {{201.1,128}, {256,128}, {310.9,128}, {420.6,176}, {420.6,224}, {420.6,272}, {420.6,320}, {420.6,368}, {146.3,272}, {146.3,224}, {146.3,176}, {91.4,176}, {91.4,224}, {91.4,272}, {91.4,320}, {91.4,368}, {365.7,176}, {365.7,224}, {365.7,272}, {420.6,80}, {310.9,80}, {256,80}, {201.1,80}, {91.4,80}};
static const Pt bombs_2[] = {{146.3,176}, {91.4,176}, {36.6,176}, {36.6,224}, {36.6,272}, {36.6,320}, {36.6,368}, {475.4,368}, {475.4,320}, {475.4,272}, {475.4,224}, {475.4,176}, {420.6,176}, {365.7,176}, {365.7,320}, {310.9,320}, {201.1,320}, {146.3,320}, {310.9,80}, {256,80}, {91.4,32}, {36.6,32}, {420.6,32}, {475.4,32}};
static const Pt bombs_3[] = {{310.9,80}, {310.9,128}, {310.9,176}, {365.7,416}, {365.7,368}, {365.7,320}, {475.4,272}, {420.6,272}, {365.7,272}, {146.3,272}, {91.4,272}, {36.6,272}, {146.3,416}, {146.3,368}, {146.3,320}, {201.1,80}, {201.1,128}, {201.1,176}, {91.4,32}, {91.4,80}, {91.4,128}, {420.6,32}, {420.6,80}, {420.6,128}};
static const Pt bombs_4[] = {{420.6,80}, {420.6,128}, {420.6,176}, {420.6,272}, {420.6,320}, {420.6,368}, {310.9,80}, {310.9,128}, {310.9,176}, {201.1,272}, {201.1,320}, {201.1,368}, {91.4,80}, {91.4,128}, {91.4,176}, {91.4,272}, {91.4,320}, {91.4,368}, {310.9,272}, {310.9,320}, {310.9,368}, {201.1,80}, {201.1,128}, {201.1,176}};
static const Pt bombs_5[] = {{475.4,80}, {420.6,80}, {365.7,80}, {146.3,80}, {91.4,80}, {36.6,80}, {146.3,176}, {201.1,176}, {310.9,176}, {365.7,176}, {475.4,272}, {420.6,272}, {365.7,272}, {146.3,272}, {91.4,272}, {36.6,272}, {146.3,368}, {201.1,368}, {310.9,368}, {365.7,368}, {420.6,416}, {475.4,416}, {91.4,416}, {36.6,416}};
static const Pt bombs_6[] = {{310.9,320}, {310.9,368}, {310.9,416}, {310.9,224}, {365.7,224}, {420.6,224}, {201.1,320}, {201.1,368}, {201.1,416}, {201.1,224}, {146.3,224}, {91.4,224}, {91.4,80}, {146.3,80}, {201.1,80}, {310.9,80}, {365.7,80}, {420.6,80}, {36.6,320}, {36.6,368}, {36.6,416}, {475.4,320}, {475.4,368}, {475.4,416}};
static const Pt bombs_7[] = {{310.9,320}, {365.7,320}, {420.6,320}, {201.1,320}, {146.3,320}, {91.4,320}, {365.7,224}, {420.6,224}, {475.4,224}, {146.3,224}, {91.4,224}, {36.6,224}, {365.7,80}, {420.6,80}, {475.4,80}, {146.3,80}, {91.4,80}, {36.6,80}, {36.6,416}, {91.4,416}, {146.3,416}, {365.7,416}, {420.6,416}, {475.4,416}};
static const Pt bombs_8[] = {{36.6,320}, {91.4,320}, {146.3,320}, {201.1,272}, {201.1,224}, {201.1,176}, {310.9,176}, {310.9,224}, {310.9,272}, {365.7,320}, {420.6,320}, {475.4,320}, {310.9,80}, {365.7,80}, {420.6,32}, {475.4,32}, {201.1,80}, {146.3,80}, {91.4,32}, {36.6,32}, {146.3,416}, {201.1,416}, {310.9,416}, {365.7,416}};
static const Pt bombs_9[] = {{310.9,80}, {365.7,80}, {420.6,80}, {36.6,128}, {36.6,176}, {36.6,224}, {91.4,272}, {91.4,320}, {91.4,368}, {201.1,80}, {146.3,80}, {91.4,80}, {475.4,128}, {475.4,176}, {475.4,224}, {420.6,272}, {420.6,320}, {420.6,368}, {310.9,272}, {310.9,224}, {310.9,176}, {201.1,176}, {201.1,224}, {201.1,272}};
static const Pt bombs_10[] = {{146.3,176}, {146.3,128}, {146.3,80}, {91.4,80}, {91.4,128}, {91.4,176}, {91.4,320}, {146.3,320}, {201.1,320}, {310.9,224}, {365.7,224}, {420.6,224}, {365.7,320}, {365.7,368}, {365.7,416}, {475.4,128}, {420.6,128}, {365.7,128}, {475.4,320}, {475.4,368}, {475.4,416}, {420.6,32}, {365.7,32}, {310.9,32}};
static const Pt bombs_11[] = {{36.6,80}, {91.4,80}, {146.3,80}, {91.4,224}, {146.3,224}, {201.1,224}, {91.4,320}, {146.3,320}, {201.1,320}, {310.9,320}, {365.7,320}, {420.6,320}, {310.9,224}, {365.7,224}, {420.6,224}, {365.7,80}, {420.6,80}, {475.4,80}, {310.9,128}, {256,128}, {201.1,128}, {310.9,32}, {256,32}, {201.1,32}};
static const Pt bombs_12[] = {{310.9,80}, {310.9,128}, {310.9,176}, {365.7,224}, {365.7,272}, {365.7,320}, {146.3,128}, {146.3,176}, {146.3,224}, {201.1,272}, {201.1,320}, {201.1,368}, {420.6,32}, {420.6,80}, {420.6,128}, {475.4,320}, {475.4,368}, {475.4,416}, {91.4,32}, {91.4,80}, {91.4,128}, {36.6,320}, {36.6,368}, {36.6,416}};
static const Pt bombs_13[] = {{420.6,128}, {420.6,176}, {420.6,224}, {91.4,224}, {91.4,272}, {91.4,320}, {146.3,320}, {146.3,368}, {146.3,416}, {256,176}, {256,128}, {256,80}, {146.3,128}, {91.4,128}, {36.6,128}, {310.9,368}, {365.7,368}, {420.6,368}, {475.4,224}, {475.4,176}, {475.4,128}, {365.7,32}, {365.7,80}, {365.7,128}};
static const Pt bombs_14[] = {{36.6,32}, {36.6,80}, {36.6,128}, {91.4,176}, {91.4,224}, {91.4,272}, {420.6,224}, {420.6,272}, {420.6,320}, {475.4,320}, {475.4,368}, {475.4,416}, {146.3,416}, {91.4,416}, {36.6,416}, {365.7,32}, {420.6,32}, {475.4,32}, {420.6,176}, {475.4,176}, {475.4,128}, {310.9,128}, {256,128}, {201.1,128}};
static const Pt bombs_15[] = {{310.9,272}, {310.9,320}, {310.9,368}, {146.3,224}, {146.3,176}, {146.3,128}, {475.4,272}, {475.4,224}, {475.4,176}, {420.6,32}, {365.7,32}, {310.9,32}, {201.1,32}, {146.3,32}, {91.4,32}, {36.6,176}, {36.6,224}, {36.6,272}, {365.7,128}, {365.7,176}, {365.7,224}, {201.1,368}, {201.1,320}, {201.1,272}};
static const Pt mummyspawn_0[4] = {{128,112}, {347.4,64}, {128,112}, {347.4,64}};
static const Pt mummyspawn_1[4] = {{182.9,64}, {329.1,64}, {182.9,64}, {329.1,64}};
static const Pt mummyspawn_2[4] = {{73.1,112}, {347.4,160}, {73.1,112}, {347.4,160}};
static const Pt mummyspawn_3[4] = {{128,112}, {402.3,112}, {128,112}, {402.3,112}};
static const Pt mummyspawn_4[4] = {{128,112}, {347.4,112}, {128,112}, {347.4,112}};
static const Pt mummyspawn_5[4] = {{73.1,64}, {402.3,64}, {73.1,64}, {402.3,64}};
static const Pt mummyspawn_6[4] = {{128,160}, {347.4,160}, {128,160}, {347.4,160}};
static const Pt mummyspawn_7[4] = {{73.1,64}, {402.3,64}, {73.1,64}, {402.3,64}};
static const Pt mummyspawn_8[4] = {{128,64}, {292.6,64}, {128,64}, {292.6,64}};
static const Pt mummyspawn_9[4] = {{128,64}, {347.4,64}, {128,64}, {347.4,64}};
static const Pt mummyspawn_10[4] = {{182.9,112}, {347.4,112}, {182.9,112}, {347.4,112}};
static const Pt mummyspawn_11[4] = {{73.1,112}, {402.3,112}, {73.1,112}, {402.3,112}};
static const Pt mummyspawn_12[4] = {{182.9,64}, {347.4,64}, {182.9,64}, {347.4,64}};
static const Pt mummyspawn_13[4] = {{182.9,16}, {347.4,208}, {182.9,16}, {347.4,208}};
static const Pt mummyspawn_14[4] = {{73.1,112}, {402.3,64}, {73.1,112}, {402.3,64}};
static const Pt mummyspawn_15[4] = {{182.9,16}, {347.4,16}, {182.9,16}, {347.4,16}};
// Arcade rule: a spawn point counts as "left" when its X is below 0x78.
static const float MUMMY_SPAWN_SPLIT_X = 237.7;
struct PlatLayout { const Plat* plats; int nplats; const Pt* mummySpawn; };
static const PlatLayout platLayouts[16] = {
  {plats_0, 5, mummyspawn_0},
  {plats_1, 4, mummyspawn_1},
  {plats_2, 5, mummyspawn_2},
  {plats_3, 8, mummyspawn_3},
  {plats_4, 0, mummyspawn_4},
  {plats_5, 10, mummyspawn_5},
  {plats_6, 6, mummyspawn_6},
  {plats_7, 4, mummyspawn_7},
  {plats_8, 5, mummyspawn_8},
  {plats_9, 4, mummyspawn_9},
  {plats_10, 12, mummyspawn_10},
  {plats_11, 6, mummyspawn_11},
  {plats_12, 6, mummyspawn_12},
  {plats_13, 8, mummyspawn_13},
  {plats_14, 6, mummyspawn_14},
  {plats_15, 12, mummyspawn_15},
};
struct BombLayout { const Pt* bombs; int nbombs; };
static const BombLayout bombLayouts[16] = {
  {bombs_0, 24},
  {bombs_1, 24},
  {bombs_2, 24},
  {bombs_3, 24},
  {bombs_4, 24},
  {bombs_5, 24},
  {bombs_6, 24},
  {bombs_7, 24},
  {bombs_8, 24},
  {bombs_9, 24},
  {bombs_10, 24},
  {bombs_11, 24},
  {bombs_12, 24},
  {bombs_13, 24},
  {bombs_14, 24},
  {bombs_15, 24},
};
static const int mummies_0[] = {2, 2, 2, 6};
static const int mummies_1[] = {4, 4, 4, 6, 4};
static const int mummies_2[] = {3, 3, 2, 2, 3, 3};
static const int mummies_3[] = {4, 4, 2, 2, 4, 4, 4};
static const int mummies_4[] = {3, 3, 3, 3, 3, 3, 3};
static const int mummies_5[] = {2, 3, 2, 3};
static const int mummies_6[] = {6, 6, 6, 2, 6};
static const int mummies_7[] = {2, 2, 2, 2, 2, 2};
static const int mummies_8[] = {5, 5, 5, 5, 5, 5, 5};
static const int mummies_9[] = {4, 4, 6, 6, 3, 3, 6};
static const int mummies_10[] = {2, 2, 2, 2};
static const int mummies_11[] = {5, 5, 5, 6};
static const int mummies_12[] = {6, 6, 4, 4, 4, 4};
static const int mummies_13[] = {3, 3, 3, 3, 6, 6, 6};
static const int mummies_14[] = {6, 5, 6, 3, 6, 6, 2};
static const int mummies_15[] = {5, 6, 4, 2};
static const int mummies_16[] = {4, 4, 6, 6, 3};
static const int mummies_17[] = {2, 2, 2, 2, 3, 3};
static const int mummies_18[] = {5, 5, 5, 6, 6, 6};
static const int mummies_19[] = {6, 6, 6, 6, 4, 4, 4};
static const int mummies_20[] = {3, 3, 3, 3};
static const int mummies_21[] = {5, 6, 6, 3, 6};
static const int mummies_22[] = {5, 6, 4, 2, 3, 5};
static const int mummies_23[] = {4, 4, 6, 6, 3, 3, 6};
static const int mummies_24[] = {2, 2, 2, 2, 3, 3, 3};
static const int mummies_25[] = {5, 5, 5, 5};
static const int mummies_26[] = {6, 6, 6, 6, 4};
static const int mummies_27[] = {3, 3, 3, 3, 6, 6};
static const int mummies_28[] = {3, 5, 6, 3, 6, 2, 6};
static const int mummies_29[] = {5, 6, 4, 2, 3, 5, 6};
struct EnemyDef { int birds; int bouncing; const int* mummies; int nmummies; };
static const EnemyDef enemyDefs[30] = {
  {1, 3, mummies_0, (int)(sizeof(mummies_0)/sizeof(int))},  // round 1
  {1, 3, mummies_1, (int)(sizeof(mummies_1)/sizeof(int))},  // round 2
  {2, 1, mummies_2, (int)(sizeof(mummies_2)/sizeof(int))},  // round 3
  {2, 1, mummies_3, (int)(sizeof(mummies_3)/sizeof(int))},  // round 4
  {2, 0, mummies_4, (int)(sizeof(mummies_4)/sizeof(int))},  // round 5
  {2, 0, mummies_5, (int)(sizeof(mummies_5)/sizeof(int))},  // round 6
  {2, 1, mummies_6, (int)(sizeof(mummies_6)/sizeof(int))},  // round 7
  {2, 0, mummies_7, (int)(sizeof(mummies_7)/sizeof(int))},  // round 8
  {2, 0, mummies_8, (int)(sizeof(mummies_8)/sizeof(int))},  // round 9
  {2, 0, mummies_9, (int)(sizeof(mummies_9)/sizeof(int))},  // round 10
  {2, 0, mummies_10, (int)(sizeof(mummies_10)/sizeof(int))},  // round 11
  {2, 0, mummies_11, (int)(sizeof(mummies_11)/sizeof(int))},  // round 12
  {2, 0, mummies_12, (int)(sizeof(mummies_12)/sizeof(int))},  // round 13
  {2, 0, mummies_13, (int)(sizeof(mummies_13)/sizeof(int))},  // round 14
  {2, 0, mummies_14, (int)(sizeof(mummies_14)/sizeof(int))},  // round 15
  {2, 0, mummies_15, (int)(sizeof(mummies_15)/sizeof(int))},  // round 16
  {2, 0, mummies_16, (int)(sizeof(mummies_16)/sizeof(int))},  // round 17
  {2, 0, mummies_17, (int)(sizeof(mummies_17)/sizeof(int))},  // round 18
  {2, 0, mummies_18, (int)(sizeof(mummies_18)/sizeof(int))},  // round 19
  {2, 0, mummies_19, (int)(sizeof(mummies_19)/sizeof(int))},  // round 20
  {2, 0, mummies_20, (int)(sizeof(mummies_20)/sizeof(int))},  // round 21
  {2, 0, mummies_21, (int)(sizeof(mummies_21)/sizeof(int))},  // round 22
  {2, 0, mummies_22, (int)(sizeof(mummies_22)/sizeof(int))},  // round 23
  {2, 0, mummies_23, (int)(sizeof(mummies_23)/sizeof(int))},  // round 24
  {2, 0, mummies_24, (int)(sizeof(mummies_24)/sizeof(int))},  // round 25
  {2, 0, mummies_25, (int)(sizeof(mummies_25)/sizeof(int))},  // round 26
  {2, 0, mummies_26, (int)(sizeof(mummies_26)/sizeof(int))},  // round 27
  {2, 0, mummies_27, (int)(sizeof(mummies_27)/sizeof(int))},  // round 28
  {2, 0, mummies_28, (int)(sizeof(mummies_28)/sizeof(int))},  // round 29
  {2, 0, mummies_29, (int)(sizeof(mummies_29)/sizeof(int))},  // round 30
};
static const int ENEMY_DEF_COUNT = 30;
struct Round { int platLayout; int bombLayout; int screen; int music; };
static const Round rounds[] = {
  { 0,  0, 0, 0},  // round 1
  {11, 11, 1, 1},  // round 2
  { 8,  8, 2, 2},  // round 3
  { 7,  7, 3, 0},  // round 4
  { 4,  4, 4, 1},  // round 5
  { 2,  2, 0, 2},  // round 6
  { 6,  6, 1, 0},  // round 7
  { 3,  3, 2, 1},  // round 8
  {10, 10, 3, 2},  // round 9
  { 4,  0, 4, 0},  // round 10
  {12, 12, 0, 1},  // round 11
  {14, 14, 1, 2},  // round 12
  { 9,  9, 2, 0},  // round 13
  {15, 15, 3, 1},  // round 14
  { 4,  4, 4, 2},  // round 15
  {13, 13, 0, 0},  // round 16
  { 1,  1, 1, 1},  // round 17
  { 5,  5, 2, 2},  // round 18
  {11, 11, 3, 0},  // round 19
  { 4, 13, 4, 1},  // round 20
  {14, 14, 0, 2},  // round 21
  { 6,  6, 1, 0},  // round 22
  { 7,  7, 2, 1},  // round 23
  { 9,  9, 3, 2},  // round 24
  { 4, 15, 4, 0},  // round 25
  { 5,  5, 0, 1},  // round 26
  { 3,  3, 1, 2},  // round 27
  { 1,  1, 2, 0},  // round 28
  {12, 12, 3, 1},  // round 29
  { 4,  0, 4, 2},  // round 30
  { 2,  2, 0, 0},  // round 31
  { 8,  8, 1, 1},  // round 32
  {15, 15, 2, 2},  // round 33
  {13, 13, 3, 0},  // round 34
  { 4,  6, 4, 1},  // round 35
  {10, 10, 0, 2},  // round 36
  { 9,  9, 1, 0},  // round 37
  { 1,  1, 2, 1},  // round 38
  { 2,  2, 3, 2},  // round 39
  { 4,  2, 4, 0},  // round 40
  { 3,  3, 0, 1},  // round 41
  { 5,  5, 1, 2},  // round 42
  { 6,  6, 2, 0},  // round 43
  { 7,  7, 3, 1},  // round 44
  { 4, 14, 4, 2},  // round 45
  { 8,  8, 0, 0},  // round 46
  { 9,  9, 1, 1},  // round 47
  {10, 10, 2, 2},  // round 48
  {11, 11, 3, 0},  // round 49
  { 4, 10, 4, 1},  // round 50
  {12, 12, 0, 2},  // round 51
  {13, 13, 1, 0},  // round 52
  {14, 14, 2, 1},  // round 53
  {15, 15, 3, 2},  // round 54
  { 4,  6, 4, 0},  // round 55
  { 0,  0, 0, 1},  // round 56
  { 1,  1, 1, 2},  // round 57
  { 2,  2, 2, 0},  // round 58
  { 3,  3, 3, 1},  // round 59
  { 4,  8, 4, 2},  // round 60
  { 5,  5, 0, 0},  // round 61
  { 6,  6, 1, 1},  // round 62
  { 7,  7, 2, 2},  // round 63
  { 8,  8, 3, 0},  // round 64
};
static const int ROUND_COUNT = (int)(sizeof(rounds)/sizeof(Round));
static const int ROUND_LOOP_TO = 32;   // after the last round the arcade resumes here

// Index into rounds[] for a 1-based level number, following the arcade wrap.
inline int roundIndex(int level) {
    if (level <= ROUND_COUNT) return level - 1;
    const int span = ROUND_COUNT - ROUND_LOOP_TO + 1;
    return ROUND_LOOP_TO - 1 + (level - ROUND_COUNT - 1) % span;
}
}  // namespace leveldata
