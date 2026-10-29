#pragma once
// Colour cycles. Most are the arcade's own palette rotations, measured in
// MAME or read from the main ROM; the renderer recolours white masks with
// them (SDL colour-mod) instead of rotating a hardware palette.
#include <SDL3/SDL.h>
#include "core/color.h"
#include "game/powerball.h"

// Arcade frames per step of each cycle (main ROM 0x0705 / 0x5BF1).
constexpr int EYE_PULSE_FRAMES    = 4;   // pen 1 (the enemies' red) level step
constexpr int PAL_ROT_FRAMES      = 3;   // palettes 14 and 15 rotation step
constexpr int FROZEN_GLINT_CYCLE  = 21;  // frozen coin: pens 5 then 6 turn white
constexpr int FROZEN_GLINT_FRAMES = 3;   //   for this long, once per cycle
constexpr int FROZEN_BLINK_FRAMES = 56;  // last frames of a freeze: 4 on / 4 off

// Red level of the enemies' pen 1 (palette 4).
Uint8 eyePulse(float t);

// Palette 15: the "SIDE-ONE" HUD text and the enemies' flash-in.
Color pal15(float t);

// Palette 14 pen `pen` (1-7) at time t: the multiplier box, power bar, Power
// ball and Jack while the freeze lasts.
Color pal14Pen(const Palette14& pal, float t, int pen);

// 3-stage colour cycle (red→yellow→blue→…) of the clone's own screens:
// GAME OVER, special bonus, high-score table.
Color colorCycle3(float t);
