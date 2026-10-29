#pragma once
#include "constants.h"

// ---------------------------------------------------------------------------
// Atlas layout: where every sprite lives inside sprites_full_png.
//
// This header is *data only* — the pixel coordinates of each sprite in the
// shared atlas, kept separate from the slicing/processing code in sprites.cpp
// and renderer.cpp. Tweak a sprite's position here; the build code stays put.
//
//   AtlasRect  — a single source rectangle {x, y, w, h}.
//   AtlasStrip — N evenly-spaced frames; frame i is at {x0 + i*dx, y, w, h}.
// ---------------------------------------------------------------------------

struct AtlasRect  { int x, y, w, h; };
struct AtlasStrip { int x0, y, w, h, dx; };

namespace atlas {

// Jack living poses (JF_COUNT), all SIZE_16PX x JACK_FH. Order matches JackFrame.
inline constexpr AtlasRect JACK[] = {
    {  4, 4, SIZE_16PX, JACK_FH},
    { 24, 4, SIZE_16PX, JACK_FH}, { 44, 4, SIZE_16PX, JACK_FH},
    { 64, 4, SIZE_16PX, JACK_FH}, { 84, 4, SIZE_16PX, JACK_FH},
    {104, 4, SIZE_16PX, JACK_FH}, {124, 4, SIZE_16PX, JACK_FH},
    {144, 4, SIZE_16PX, JACK_FH}, {164, 4, SIZE_16PX, JACK_FH},
    {224, 4, SIZE_16PX, JACK_FH}, {264, 4, SIZE_16PX, JACK_FH},
    {304, 4, SIZE_16PX, JACK_FH},
    {204, 4, SIZE_16PX, JACK_FH}, {284, 4, SIZE_16PX, JACK_FH},
    {324, 4, SIZE_16PX, JACK_FH},
};

// Jack death / win variable-size frames.
inline constexpr AtlasRect JACK_DANCE[] = {
    {249, 31, 16, 17}, {269, 31, 16, 17}, {64, 34, 15, 14},
};
inline constexpr AtlasRect JACK_PLF[] = {
    {44, 32, 15, 15}, {64, 32, 15, 16}, {84, 32, 15, 16}, {124, 32, 15, 15},
};
inline constexpr AtlasRect JACK_DEAD[] = {
    {145, 27, 16, 18}, {166, 29, 13, 16}, {184, 24, 17, 24}, {205, 27, 23, 21},
};
inline constexpr AtlasRect JACK_WIN[] = {
    {4, 32, 16, 16}, {24, 32, 16, 16}, {286, 32, 16, 16}, {44, 32, 16, 16},
};

// HUD GAME OVER banner (white mask).
inline constexpr AtlasRect GAMEOVER = {2, 155, GAMEOVER_W, GAMEOVER_H};

// Hollow yellow picture-frame for the "YOU ARE LUCKY" screen.
inline constexpr AtlasRect LUCKY_FRAME = {200, 304, 64, 64};

// Rounded frame around the name being edited on the high-score screen.
inline constexpr AtlasRect NAME_FRAME = {317, 212, 35, 19};

// Boxed multiplier indicators x / 1..5 (16x16 cells).
inline constexpr AtlasStrip MULT = {4, 192, SIZE_16PX, SIZE_16PX, 20};

// Bonus B / E / S spin (4 frames each, shared y/h, irregular x and width).
inline constexpr AtlasRect BONUS_B[] = {
    {78, 114, 13, 13}, {98, 114, 12, 13}, {117, 114, 7, 13}, {130, 114, 12, 13},
};
inline constexpr AtlasRect BONUS_E[] = {
    {150, 114, 13, 13}, {170, 114, 12, 13}, {189, 114, 7, 13}, {202, 114, 12, 13},
};
inline constexpr AtlasRect BONUS_S[] = {
    {222, 114, 13, 13}, {242, 114, 12, 13}, {261, 114, 7, 13}, {274, 114, 12, 13},
};

// Bonus-collect flash (6 frames 32x32).
inline constexpr AtlasStrip BONUS_TAKEN = {266, 327, 32, 32, 33};

// Bomb frames: 0=resting, 1-6=lit fuse.
inline constexpr AtlasStrip BOMB = {46, 136, 12, 16, 20};

// Bomb-clear explosion (3 growing frames).
inline constexpr AtlasRect EXPL[] = {
    {24, 120, 8, 8}, {38, 114, 12, 12}, {56, 112, 16, 16},
};

// Enemy frames by arcade sprite code: the 16x16 cell holding that ROM sprite
// (matched pixel-for-pixel against the gfx ROMs, which store them rotated).
// Mummy 0x04 stand / 0x05 fall / 0x06-0x08 walk right / 0x09-0x0B walk left;
// bird 0x29-0x2B sideways / 0x2C-0x2E up-down / 0x2F turning (0x30 is 0x2F
// mirrored); chasers 0x50-0x53 horn, 0x34 + 0x67-0x6B sphere, 0x54-0x5A orb,
// 0x38-0x3B club, 0x60-0x65 UFO. Right-facing poses are these, flipped.
struct CodeCell { int code, x, y; };
inline constexpr CodeCell ENEMY_CELLS[] = {
    {0x04,   4, 52}, {0x05,  24, 52}, {0x06,  44, 52}, {0x07,  60, 52},
    {0x08,  76, 52}, {0x09,  88, 52}, {0x0A, 104, 52}, {0x0B, 120, 52},
    {0x29, 140, 52}, {0x2A, 160, 52}, {0x2B, 180, 52},
    {0x2C, 200, 52}, {0x2D, 220, 52}, {0x2E, 240, 52}, {0x2F, 260, 52},
    {0x50, 280, 52}, {0x51, 300, 52}, {0x52, 316, 52}, {0x53, 332, 52},
    {0x34, 124, 92},
    {0x67,   4, 72}, {0x68,  24, 72}, {0x69,  44, 72}, {0x6A,  64, 72}, {0x6B,  84, 72},
    {0x54, 184, 72}, {0x55, 204, 72}, {0x56, 224, 72}, {0x57, 244, 72},
    {0x58, 264, 72}, {0x59, 284, 72}, {0x5A, 304, 72},
    {0x38, 304, 92}, {0x39, 324, 92}, {0x3A, 344, 92}, {0x3B, 364, 92},
    {0x60,   4, 96}, {0x61,  24, 96}, {0x62,  44, 96},
    {0x63,  64, 96}, {0x64,  84, 96}, {0x65, 104, 96},
};

// Frozen enemy (code 0x3F): a coin. The rip caught pen 5 mid-glint (white,
// normally 0xDD yellow) and pen 6 at rest; the arcade flashes those two pens.
inline constexpr AtlasRect FROZEN = {306, 114, 16, 16};
constexpr unsigned FROZEN_PEN5 = 0xFFFFFF, FROZEN_PEN6 = 0xEFEF00;   // RGB in the rip
constexpr unsigned FROZEN_PEN5_REST = 0xDEDE00;


// Spawn flash (4 frames, white masks, centred into INIT_FW x INIT_FH).
inline constexpr AtlasRect INIT[] = {
    {40, 158, 18, 29}, {65, 157, 29, 31}, {101, 158, 29, 29}, {138, 159, 28, 28},
};

// "START!" halves (each 56x12, split into box + text layers).
inline constexpr AtlasRect START[] = {
    {240, 260, 56, 12}, {300, 260, 56, 12},
};

// Coin-pickup sparkle (4 frames, white masks).
inline constexpr AtlasRect PICK[] = {
    {175, 159, 26, 26}, {209, 159, 30, 28}, {244, 159, 32, 29}, {282, 156, 32, 32},
};

// Font glyphs: two rows of FONT_W x FONT_H cells, plus punctuation.
inline constexpr int FONT_LETTER_Y = 212;   // A-Z
inline constexpr int FONT_DIGIT_Y  = 224;    // 0-9 and punctuation
inline constexpr int FONT_X0       = 5;
inline constexpr int FONT_DX       = 12;

} // namespace atlas
