#pragma once
// Attract mode: the title cycle and the demo, which replays the arcade's own
// recordings of Jack's joystick (demo_data.h) and of the enemies (demo_runs.h).
#include <cstddef>
#include <cstdint>
#include "core/constants.h"

struct Game;
struct Input;

// Attract cycle, as on the arcade (RAM 0x8079): 0 = high-score table,
// 1 = demo of round 1, 2 = banner with the title tune, 3 = demo of round 2.
// `cycle` counts completed cycles; its parity picks the demo recordings.
struct AttractState {
    int   step  = 0;
    int   cycle = 0;
    float timer = 0.0f;
    bool  showTable = false;   // TITLE: showing the table vs the banner
    bool  titleTune = false;   // TITLE: tune already sounded for this banner
};

// Attract-mode demo: Jack replays one of the arcade's recorded input streams
// (demo_data.h) one arcade frame at a time, while the rest of the game runs
// normally. The player's score, lives and round are kept aside meanwhile.
struct DemoState {
    bool    active = false;
    int     round  = 1;
    int     stream = 0;         // demodata::kStreams[round - 1][stream]
    int     step   = 0;         // next step to load
    int     hold   = 0;         // frames left on the current bits (arcade 0x807B)
    uint8_t bits   = 0;         // joystick byte being replayed (arcade 0x807A)
    uint8_t hist[2] = {};       // the bits of the two previous frames
    uint8_t held   = 0;         // bits the game sees after the input filter
    bool    pressed = false;    // jump button went down on the last replayed frame
    int     frame  = 0;         // arcade frames replayed so far
    int     trackX = 0, trackY = 0;  // arcade path at `frame`, 1/16 arcade px
    bool    onTrack = false;    // the recorded path still covers `frame`
    // Arcade enemy run being replayed (demo_runs.h) and its decoder state:
    // one arcade sprite per slot, as code / x / y in Jack's coordinates.
    int     run    = 0;
    int     runFrame = -1;      // last decoded frame of the run
    size_t  runPos = 0;         // read position in the run's data
    uint8_t slotCode[10] = {}, slotX[10] = {}, slotY[10] = {};
    uint8_t prevX[10] = {}, prevY[10] = {};
    int8_t  slotDir[10] = {};   // last horizontal direction seen per slot
    int     slotKind[10] = {};  // enemy kind last shown per slot
    float   frameAcc = 0.0f;    // arcade frames owed to the replay
    int     savedScore = 0, savedLives = 0, savedRound = 1;
    float   savedPower = 0.0f;
};

// Attract cycle timings, in arcade frames measured in MAME.
constexpr float ATTRACT_TABLE_TIME  = 305.0f / ARCADE_HZ;  // high-score table
constexpr float ATTRACT_BANNER_TIME = 603.0f / ARCADE_HZ;  // banner + title tune

// Go to the TITLE state at the given attract step (0 = table, 2 = banner).
void enterAttract(Game& g, int step);

// TITLE state: sound the title tune, step the cycle, start on the button.
void updateTitle(Game& g, const Input& in, float dt);

// Start a demo of round 1 or 2, run one step of it, or abandon it.
void startDemo(Game& g, int round);
void updateDemo(Game& g, float dt);
void stopDemo(Game& g);
