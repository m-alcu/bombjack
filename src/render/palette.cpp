#include "render/palette.h"
#include "core/constants.h"
#include <algorithm>
#include <cstdint>
#include <iterator>

// The arcade steps the enemies' pen 1 red level through this table
// (main ROM 0x0705).
static constexpr Uint8 EYE_PULSE[] = {68, 136, 204, 255, 204, 136, 68, 0};

Uint8 eyePulse(float t) {
    return EYE_PULSE[(int)(t * ARCADE_HZ / EYE_PULSE_FRAMES) % (int)(sizeof(EYE_PULSE))];
}

// The hardware rotates palette 15 one entry every 3 frames through this table
// (measured from the arcade in MAME).
static constexpr Color PAL15[35] = {
    {255,255,102}, {255,255,136}, {255,255,170}, {255,255,204}, {255,255,255}, {204,204,255},
    {170,170,255}, {136,136,255}, {102,102,255}, { 68, 68,255}, { 34, 34,255}, {  0,  0,255},
    { 34,  0,255}, { 68,  0,255}, {102,  0,255}, {136,  0,255}, {170,  0,255}, {204,  0,255},
    {255,  0,255}, {255,  0,204}, {255,  0,170}, {255,  0,136}, {255,  0,102}, {255,  0, 68},
    {255,  0, 34}, {255,  0,  0}, {255, 34,  0}, {255, 68,  0}, {255,102,  0}, {255,136,  0},
    {255,170,  0}, {255,204,  0}, {255,255,  0}, {255,255, 34}, {255,255, 68},
};

Color pal15(float t) {
    return PAL15[(int)(t * ARCADE_HZ / PAL_ROT_FRAMES) % (int)std::size(PAL15)];
}

// Palette 14 tables, in the ROM's xBGR 4:4:4 words: the rainbow (0x0D48) and
// the Power ball's colour families 1-7 (0x0D9E): blue, red, magenta, green,
// cyan, yellow, each fading to white, and silver.
static constexpr uint16_t PAL14_RAINBOW[42] = {
    0x000F, 0x002F, 0x004F, 0x006F, 0x008F, 0x00AF, 0x00CF, 0x00FF,
    0x00FC, 0x00FA, 0x00F8, 0x00F6, 0x00F4, 0x00F2, 0x00F0, 0x02F0,
    0x04F0, 0x06F0, 0x08F0, 0x0AF0, 0x0CF0, 0x0FF0, 0x0FC0, 0x0FA0,
    0x0F80, 0x0F60, 0x0F40, 0x0F20, 0x0F00, 0x0F02, 0x0F04, 0x0F06,
    0x0F08, 0x0F0A, 0x0F0C, 0x0F0F, 0x0C0F, 0x0A0F, 0x080F, 0x060F,
    0x040F, 0x020F,
};
static constexpr uint16_t PAL14_FAMILY[7][8] = {
    {0x0F00, 0x0F22, 0x0F44, 0x0F66, 0x0F88, 0x0FAA, 0x0FCC, 0x0FEE},
    {0x000F, 0x022F, 0x044F, 0x066F, 0x088F, 0x0AAF, 0x0CCF, 0x0EEF},
    {0x0F0F, 0x0F2F, 0x0F4F, 0x0F6F, 0x0F8F, 0x0FAF, 0x0FCF, 0x0FEF},
    {0x00F0, 0x02F2, 0x04F4, 0x06F6, 0x08F8, 0x0AFA, 0x0CFC, 0x0EFE},
    {0x0FF0, 0x0FF2, 0x0FF4, 0x0FF6, 0x0FF8, 0x0FFA, 0x0FFC, 0x0FFE},
    {0x00FF, 0x02FF, 0x04FF, 0x06FF, 0x08FF, 0x0AFF, 0x0CFF, 0x0EFF},
    {0x0FFF, 0x0EEE, 0x0DDD, 0x0CCC, 0x0BBB, 0x0AAA, 0x0999, 0x0888},
};
static constexpr Color xbgr444(uint16_t v) {
    return {(Uint8)((v & 15) * 17), (Uint8)((v >> 4 & 15) * 17), (Uint8)((v >> 8 & 15) * 17)};
}

// The loaded table rotates one entry every PAL_ROT_FRAMES starting from its
// first, pen k showing entry step + k - 1.
Color pal14Pen(const Palette14& pal, float t, int pen) {
    const int step = std::max(0, (int)((t - pal.loadedAt) * ARCADE_HZ / PAL_ROT_FRAMES));
    if (pal.table >= 1 && pal.table <= 7)
        return xbgr444(PAL14_FAMILY[pal.table - 1][(step + pen - 1) % 8]);
    return xbgr444(PAL14_RAINBOW[(step + pen - 1) % (int)std::size(PAL14_RAINBOW)]);
}

Color colorCycle3(float t) {
    const float CYCLE = 0.4f;
    int stage = (int)(t / CYCLE);
    float f = t / CYCLE - (float)stage;
    Uint8 a = (Uint8)(255.0f * f), b = (Uint8)(255.0f * (1.0f - f));
    switch (((stage % 3) + 3) % 3) {
        case 0:  return {255, a, 0};
        case 1:  return {b, b, a};
        default: return {a, 0, b};
    }
}
