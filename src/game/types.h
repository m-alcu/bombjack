#pragma once
#include <SDL3/SDL.h>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>
#include "core/constants.h"
#include "core/color.h"
#include "render/animsprite.h"

enum BonusKind  { BK_B, BK_E, BK_S };
enum State      { TITLE, OPTIONS, PLAYING, ROUNDCLEAR, SPECIALBONUS, LUCKY, GAMEOVER, NAMEENTRY };
enum DeathPhase { DP_NONE, DP_DANCING, DP_FALLING, DP_DEAD, DP_WAIT };

struct Input  { bool left, right, up, down, jumpHeld, jumpPressed; };

struct Player {
    float x, y, vx, vy;
    bool  onGround;
    float invuln;
    int   face = 1;
    float crouch = 0.0f;   // >0 while crouching before a jump take-off
    float landing = 0.0f;  // >0 while held in place just after landing
};

struct Bomb { float x, y; bool collected; };

enum EKind  { EK_BIRD, EK_MUMMY, EK_SPHERE, EK_ORB, EK_HORN, EK_CLUB, EK_UFO };
enum EPhase { EP_INIT, EP_FLY, EP_APPEAR, EP_WALK, EP_FALL, EP_DISAPPEAR };

struct Enemy {
    float x, y, vx, vy, r;
    int   kind    = EK_BIRD;
    int   phase   = EP_FLY;
    float timer   = 0.0f;
    int   bounces = 0;
    int   becomes = EK_SPHERE;
    int   dirX = 0, dirY = 0;
    int   tick = 0;
    float tgtX = 0.0f, tgtY = 0.0f;
    // Arcade sprite shown (ROM sprite code + horizontal flip) and the state
    // the arcade keeps per object to step it (see animateEnemy).
    int   code = 0;
    bool  flip = false;
    int   animTimer = 0, animIdx = 0;
    int   animDir = -1;                 // mummy: 0 right / 1 left; bird: heading
    const uint8_t* turn = nullptr;      // bird heading-change poses still to show
    float animAcc = 0.0f;               // arcade frames owed to the animation
};

struct Popup { float x, y, age = 0.0f; int value = 0; };

// Shared layout for explosion flash, coin-pickup sparkle, bonus-collect flash.
// The vector name in Game says which kind it holds.
struct TimedEffect { float x, y, age = 0.0f; };

// ---------------------------------------------------------------------------
// Game-system sub-states (kept out of the flat Game struct for clarity)
// ---------------------------------------------------------------------------

struct DeathState {
    bool  active = false;
    int   phase  = DP_NONE;
    int   frame  = 0, loops = 0;
    float anim   = 0.0f, timer = 0.0f;
};

struct OrbState {
    bool  active = false;
    float x = 0, y = 0, vx = 0, vy = 0;
    int   family = 0;   // arcade 0x845F: 1-7 while the ball is up (POWER_POINTS index + 1)
};

struct BonusState {
    bool  active = false;
    int   kind   = BK_B;
    float x = 0, y = 0, vx = 0;
    float platX = 0, platW = 0;
    float anim  = 0;
};

struct SBState {
    int   phase     = 0;   // 0=begin hold, 1=count-up, 2=end hold
    int   remaining = 0, catched = 0;
    float timer     = 0;
};

struct GameOverState {
    float x = 0, y = 0, timer = 0;
    int   stage = 0;
};

// "YOU ARE LUCKY / ONE MORE PLAY" screen shown on collecting an S bonus.
struct LuckyState {
    int   phase = 0;   // 0 = text sliding in, 1 = hold before next round
    float timer = 0;   // slide progress (phase 0) then hold countdown (phase 1)
};

// One persistent high-score row: score, player name, level reached.
struct HiScoreEntry {
    int  score = 0;
    char name[NAME_MAX + 1] = {};
    int  level = 1;
};

// High-score UI state: the name being typed on the NAMEENTRY screen, plus
// whether the TITLE screen shows the table or the banner.
struct HiScoreState {
    char  entry[NAME_MAX + 1] = {};  // name currently being typed
    int   len = 0;
    int   rank = 0;                  // NAMEENTRY: table row the new score lands on
    bool  showTable = false;         // TITLE: showing the table vs the banner
    bool  titleTune = false;         // TITLE: tune already sounded for this banner
};

// Attract cycle, as on the arcade (RAM 0x8079): 0 = high-score table,
// 1 = demo of round 1, 2 = banner with the title tune, 3 = demo of round 2.
// `cycle` counts completed cycles; its parity picks the demo recordings.
struct AttractState {
    int   step  = 0;
    int   cycle = 0;
    float timer = 0.0f;
};

// Attract-mode demo: Jack replays one of the arcade's recorded input streams
// (demo_data.h) one arcade frame at a time, while the rest of the game runs
// normally. The player's score, lives and level are kept aside meanwhile.
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
    int     savedScore = 0, savedLives = 0, savedLevel = 1;
    float   savedPower = 0.0f;
};

// ---------------------------------------------------------------------------
// God struct: top-level game state
// ---------------------------------------------------------------------------
struct Game {
    int   state = TITLE;
    int   score = 0, lives = 3, level = 1, bombsLeft = 0;
    int   startLevel = 1, startLives = 3;
    int   imgStyle = 0, optSel = 0;
    bool  alwaysS = false;   // OPTIONS toggle: spawn an S bonus every level
    bool  start100k = false; // OPTIONS toggle: start a game with 100000 points
    int   litBomb  = -1, catched = 0;
    int   multiplier = 1;
    int   nextBonusScore = BONUS_LIMIT;
    int   bCoins = 0, nextEAt = BONUS_E_EVERY, livesLost = 0;
    int   killCount = 0;
    int   mummiesSpawned = 0, transformIdx = 0;
    float time = 0, clearTimer = 0, startAnim = 0;
    float powerMeter = 0, freezeTimer = 0, mummyTimer = 0;
    // Palette 14 (multiplier box, power bar, Power ball, powered-up Jack): the
    // table it rotates through (0 rainbow, 1-7 the ball's colour family) and
    // when that table was loaded, which restarts the rotation.
    int   pal14 = 0;
    float pal14Time = 0;

    DeathState    death;
    OrbState      orb;
    BonusState    bonus;
    SBState       sb;
    LuckyState    lucky;
    GameOverState go;
    HiScoreState  hs;
    AttractState  attract;
    DemoState     demo;
    HiScoreEntry  hi[HISCORE_COUNT];

    Player p{};
    AnimSprite jackSprite;   // owned animated sprite for the living player
    std::vector<SDL_FRect>   platforms;
    std::vector<Bomb>        bombs;
    std::vector<Enemy>       enemies;
    std::vector<Popup>       popups;
    std::vector<TimedEffect> explosions;
    std::vector<TimedEffect> coinPickups;
    std::vector<TimedEffect> bonusTakens;
    std::mt19937 rng{std::random_device{}()};
};
