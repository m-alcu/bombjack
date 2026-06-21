// Bomb Jack (minimal) — a small SDL3 / C++ arcade platformer.
//
// Goal: collect every bomb in the round while dodging the chasers. Grab the
// lit bomb (glowing fuse) to build a bonus multiplier. Clear all bombs to
// advance to the next, harder round. Touching a chaser costs a life.
//
// Controls:
//   Left / A      move left
//   Right / D     move right
//   Space/Up/W    jump on the ground; tap in the air to flutter, hold to glide
//   R             restart after game over
//   Esc / Q       quit
//
// Sprites + font are drawn from primitives and the round backdrops are an
// embedded PNG, so there are no runtime asset files to ship. Build with CMake
// (see CMakeLists.txt) or build.sh.

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

// Background art: the five 224x224 arcade backdrops are embedded as a single
// 1120x224 PNG (screens_png.h) and decoded at startup with the vendored,
// public-domain stb_image — so there are still no runtime asset files to ship.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wtype-limits"
#include "stb_image.h"
#pragma GCC diagnostic pop
#include "screens_png.h"
// banner/bombs/bonusE/bonusS/bonustaken/explosion now cropped from the atlas
#include "sprites_full_png.h" // full original sprites atlas (for Jack death frames)
#include "levels_data.h"    // hand-authored level layouts (from levels.json)

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
namespace {

constexpr int LOGW = 512;   // world coordinate space (game logic + drawing)
constexpr int LOGH = 448;

// The arcade screen is a 224-wide stack: a 16px top HUD strip, a 224x224 play
// area, and a 16px bottom HUD strip (224x256 total). The 512x448 world is scaled
// into the central play band; the HUD strips are drawn in raw screen pixels. The
// whole thing is presented pixel-quadrupled (x4) to an 896x1024 window.
constexpr int GAME_W   = 224;                          // play area
constexpr int GAME_H   = 224;
constexpr int HUD_H    = 16;                           // each HUD strip
constexpr int SCREEN_W = GAME_W;                       // 224
constexpr int SCREEN_H = HUD_H + GAME_H + HUD_H;       // 256
constexpr int WIN_SCALE = 4;

// The original Bomb Jack cabinet used a 4:3 monitor mounted vertically (TATE),
// so the 224x256 image was displayed at a 3:4 physical aspect — its width
// squeezed to ~6/7 (non-square pixels, slightly taller than wide). We keep the
// internal render at 224x256 and let SDL stretch it into a 3:4 window.
constexpr int WIN_H = SCREEN_H * WIN_SCALE;            // 1024
constexpr int WIN_W = WIN_H * 3 / 4;                   // 768  -> 3:4

// That 3:4 stretch squeezes screen pixels horizontally: the world maps to
// 1.5 px/unit across but 2.0 px/unit down, so a square-in-world sprite shows up
// at 3/4 width (stretched tall). Multiply character sprite widths by this to draw
// them with 1:1 (square) on-screen pixels.
constexpr float SPRITE_AR = 4.0f / 3.0f;

// Player physics — tuned for the floaty Bomb Jack feel.
constexpr float MOVE        = 170.0f;   // horizontal speed (px/s)
constexpr float JUMP_VEL    = -630.0f;  // initial jump velocity (stronger jump)
constexpr float FLUTTER     = 165.0f;   // upward kick from an in-air tap
constexpr float FLUTTER_MIN = -200.0f;  // cap on upward speed from fluttering
constexpr float GRAVITY     = 980.0f;
constexpr float GLIDE       = 150.0f;   // reduced gravity while holding jump
constexpr float MAXFALL     = 520.0f;

constexpr float PW = 18.0f;             // player size
constexpr float PH = 24.0f;
constexpr float BORDER_SOLID_X = 8.0f * (float)LOGW / (float)GAME_W;
constexpr float BORDER_SOLID_Y = 8.0f * (float)LOGH / (float)GAME_H;
// Bombs are 12x16 in the original 224x224 playfield.
constexpr float BOMB_HALF_W = 6.0f * (float)LOGW / (float)GAME_W;
constexpr float BOMB_HALF_H = 8.0f * (float)LOGH / (float)GAME_H;

constexpr float INVULN_TIME = 2.0f;     // post-hit invulnerability (s)
constexpr float CLEAR_TIME  = 3.2f;     // round-clear banner duration (s); also paces the victory dance
// End-of-level Special Bonus: catching bombs while their fuse is lit ("fire
// bombs") builds a per-level count; 20..23 of them awards a big between-level
// bonus that counts up on screen. Mirrors the reference score.lua /
// specialbonus.lua (catched 20->10000, 21->20000, 22->30000, 23->50000 pts).
constexpr float SB_BEGIN_TIME = 2.0f;   // box hold before the count-up starts
constexpr float SB_END_TIME   = 1.5f;   // hold after the count-up finishes
constexpr float SB_SCORE_TIME = 0.1f;   // seconds per 1000-pt count-up tick
constexpr float FREEZE_TIME = 5.0f;     // enemy freeze after grabbing the P orb
constexpr float POWER_NEEDED = 8.0f;    // lit-bomb "charge" needed to spawn a P orb
constexpr float ORB_SPEED = 60.0f;      // moving Power orb speed (world px/s)
// Bonus "B" coin: every BONUS_LIMIT points of score one appears; catching it
// bumps the points multiplier x1 -> x5 (capped). Mirrors the reference game's
// score.lua (BONUS_LIMIT=5000, multiplier resets each level). The coin patrols
// a platform until grabbed.
constexpr int   BONUS_LIMIT  = 5000;    // score per bonus opportunity
constexpr int   BONUS_POINTS = 1000;    // base value of a caught B coin
constexpr int   MAX_MULT     = 5;       // multiplier cap
constexpr float BONUS_W = 26.0f, BONUS_H = 26.0f;   // drawn 2x the 13px frame
constexpr float BONUS_SPEED = 45.0f;    // horizontal patrol speed (world px/s)
// Besides the B coin, the spawn slot can carry an E (extra life) or a rare S
// (special: free life + 5000 pts + skip to the next stage). E appears after
// enough B coins are collected (sooner if lives were lost); S is a rare roll.
enum BonusKind { BK_B, BK_E, BK_S };
constexpr int   BONUS_S_CHANCE = 6;     // % chance a bonus opportunity rolls an S
constexpr int   BONUS_E_EVERY  = 8;     // B coins between E offers (minus lives lost)
constexpr int   BONUS_S_POINTS = 5000;  // flat score for catching an S
constexpr float BONUSTAKEN_FRAME = 0.05f;  // per-frame time of the collect flash
constexpr float BONUSTAKEN_SCALE = 1.8f;   // enlarge flash so it wreaths the coin
// "START!" intro: two interlaced halves slide in from the screen edges, meet in
// the centre, and hold briefly. Plays at every phase start and on a new life.
// Bump START_SPEED to slow the whole intro down (1.0 = base timing); it scales
// every phase below uniformly.
constexpr float START_SPEED = 2.0f;     // intro duration multiplier (higher = slower)
constexpr float START_SLIDE = 0.55f * START_SPEED;   // slide-in duration (s)
constexpr float START_HOLD  = 0.65f * START_SPEED;   // centre hold duration (s)
constexpr float START_BLACK = 1.0f  * START_SPEED;   // background reaches black
constexpr float START_TOTAL = START_SLIDE + START_HOLD;   // then Jack drops
constexpr float ORB_R = 10.0f;          // Power orb collision / bounce radius
constexpr float DEATH_DANCE_TOTAL = 0.5f; // bj_dancing duration (3 frames)
constexpr int   DEATH_DANCE_LOOPS = 2;    // bj_dancing repeats
constexpr float DEATH_PLF_TOTAL = 1.0f;   // bj_PLF loop duration (4 frames)
constexpr float DEATH_DEAD_TOTAL = 1.0f;  // bj_dead one-shot duration (4 frames)
constexpr float DEATH_WAIT_TIME = 3.0f;   // wait after bj_dead before losing life

// Escalating points for each enemy killed during a single freeze (Bomb Jack).
constexpr int KILL_POINTS[] = {100, 200, 300, 500, 800, 1200, 2000};
// Power orb score tiers, indexed by colour (arcade table): blue 100, red 200,
// purple 300, green 500, turquoise 800, yellow 1000, silver 2000.
constexpr int POWER_POINTS[] = {100, 200, 300, 500, 800, 1000, 2000};

// Mummies drop in over time and transform into flying chasers on the ground.
constexpr float MUMMY_HALF_H = 14.0f;       // mummy collision half-height
constexpr float MUMMY_SPAWN_Y = 90.0f;      // default drop-in centre height
constexpr float MUMMY_DROP_GAP = 8.0f;      // clearance kept above a high target platform
constexpr float INIT_ENEMY_TIME = 0.6f;     // 4-frame spawn flash before a mummy appears
constexpr float MUMMY_APPEAR_TIME = 1.1f;   // pop-in pause before it moves
constexpr float MUMMY_DISAPPEAR_TIME = 0.5f; // bottom-floor vanish before transform
constexpr float MUMMY_SPAWN_DELAY = 3.6f;   // seconds between mummy spawns
constexpr float MUMMY_WALK_SPEED  = 70.0f;  // platform walk speed (world px/s)
constexpr float FLY_SPEED         = 120.0f; // base speed of transformed chasers

enum State { TITLE, PLAYING, ROUNDCLEAR, SPECIALBONUS, GAMEOVER };
enum DeathPhase { DP_NONE, DP_DANCING, DP_FALLING, DP_DEAD, DP_WAIT };

struct Color { Uint8 r, g, b; };

// Basic colours the spawn flash (EP_INIT) picks from at random.
constexpr Color BASIC_COLORS[] = {
    {255,  60,  60}, {60, 255,  60}, {80, 120, 255},  // red, green, blue
    {255, 235,  60}, {60, 235, 255}, {255, 110, 255},  // yellow, cyan, magenta
    {255, 160,  50}, {255, 255, 255},                  // orange, white
};

// The 7 Power-orb colours, by family index (matching POWER_POINTS): 0 blue=100,
// 1 red=200, 2 purple=300, 3 green=500, 4 turquoise=800, 5 yellow=1000,
// 6 silver=2000. The colour steps through this fixed cycle each time Jack jumps
// or hits a wall/barrier — collect it on silver (6) for the highest value.
constexpr Color POWER_COLORS[7] = {
    {45, 80, 240}, {235, 30, 45}, {165, 45, 225}, {45, 205, 65},
    {40, 215, 200}, {240, 220, 50}, {200, 205, 220}
};

struct Input { bool left, right, up, down, jumpHeld, jumpPressed; };

struct Player {
    float x, y, vx, vy;
    bool  onGround;
    float invuln;
    int   face = 1;   // 1 = facing right, -1 = facing left
};

struct Bomb { float x, y; bool collected; };  // x,y = center

// Enemy kinds match the level data's transform sequence (see levels_data.h).
enum EKind  { EK_BIRD, EK_MUMMY, EK_SPHERE, EK_ORB, EK_HORN, EK_CLUB, EK_UFO };
// EP_INIT plays a 4-frame spawn flash; the rest are the mummy lifecycle (others fly).
enum EPhase { EP_INIT, EP_FLY, EP_APPEAR, EP_WALK, EP_FALL, EP_DISAPPEAR };

struct Enemy {
    float x, y, vx, vy, r;
    int   kind   = EK_BIRD;
    int   phase  = EP_FLY;
    float timer  = 0.0f;     // appear countdown
    int   bounces = 0;       // platform direction-changes left before falling
    int   becomes = EK_SPHERE;  // what this mummy transforms into
    int   dirX = 0, dirY = 0;   // bird: committed compass step toward Jack
    int   tick = 0;             // bird: frame counter for the re-aim cadence
    float tgtX = 0.0f, tgtY = 0.0f;  // bird: Jack's position sampled at the last re-aim
    Color spawnTint{255, 255, 255};  // random basic colour of the EP_INIT flash
};

struct Popup { float x, y, age = 0.0f; int value = 0; };  // floating score text
struct Explosion { float x, y, age = 0.0f; };            // bomb-clear burst (3 frames)
constexpr float EXPL_FRAME = 0.06f;                       // per-frame time (quick)
struct CoinPickup { float x, y, age = 0.0f; };           // sparkle when a coin is collected
constexpr float PICKCOIN_FRAME = 0.06f;                   // per-frame time (4 frames)
struct BonusTaken { float x, y, age = 0.0f; };           // flash when a bonus is collected

struct Game {
    int   state = TITLE;
    int   score = 0, lives = 3, level = 1, bombsLeft = 0;
    // Fire-bomb chain + end-of-level Special Bonus.
    int   litBomb = -1;                // index of the bomb whose fuse is lit (-1 = none)
    int   catched = 0;                 // "fire bombs" caught this level (lit, in sequence)
    int   sbState = 0;                 // SPECIALBONUS phase: 0 begin, 1 count-up, 2 end
    float sbTimer = 0.0f;              // phase timer for the SPECIALBONUS screen
    int   sbRemaining = 0;             // bonus points still to count up onto the score
    int   sbCatched = 0;               // fire-bomb count shown on the bonus screen
    int   phaseStart = 0;              // total score when the current phase began
    float clearTimer = 0.0f;
    float time = 0.0f;                 // animation clock
    // GAME OVER sprite slide: stage 0 = move to mid-height, 1 = move to centre,
    // 2 = hold then return to the title. (goX, goY) are screen pixels.
    float goX = 0.0f, goY = 0.0f, goTimer = 0.0f;
    int   goStage = 0;
    // Power orb / enemy-freeze state.
    float powerMeter = 0.0f;           // charge from caught bombs; spawns orb at POWER_NEEDED
    bool  orbActive = false;           // a P orb is on the field
    float orbX = 0.0f, orbY = 0.0f;
    float orbVx = 0.0f, orbVy = 0.0f;
    int   orbFamily = 0;               // 0..6 PowerToColors family index
    float freezeTimer = 0.0f;          // >0 while enemies are frozen & killable
    Color freezeColor{200, 205, 220};  // colour of the grabbed orb (tints Jack)
    int   killCount = 0;               // enemies killed in the current freeze
    bool  playerDying = false;
    float deathTimer = 0.0f;
    int   deathPhase = DP_NONE;
    int   deathFrame = 0;
    int   deathLoops = 0;
    float deathAnim = 0.0f;
    // Mummy spawning.
    float mummyTimer = 0.0f;           // counts up to MUMMY_SPAWN_DELAY
    int   mummiesSpawned = 0;          // mummies dropped this round
    int   transformIdx = 0;            // index into the level's mummy sequence
    // Bonus coins (B/E/S) + points multiplier.
    int   multiplier = 1;              // x1..x5, multiplies all point gains
    int   nextBonusScore = BONUS_LIMIT;// next score threshold that spawns a bonus
    bool  bonusActive = false;         // a bonus coin is on the field
    int   bonusKind = BK_B;            // which letter the active coin shows
    int   bCoins = 0;                  // B coins collected this game (gates E)
    int   nextEAt = BONUS_E_EVERY;     // B coins needed before the next E offer
    int   livesLost = 0;               // deaths so far (brings E forward)
    float bonusX = 0.0f, bonusY = 0.0f, bonusVx = 0.0f;
    float bonusPlatX = 0.0f, bonusPlatW = 0.0f;  // platform the coin patrols
    float bonusAnim = 0.0f;            // spin-animation clock
    float startAnim = 0.0f;            // >0 while the "START!" intro plays
    bool  birdSpawnPending = false;    // place the bird from the held dir at intro end
    int   spawnHoldX = 0, spawnHoldY = 0;  // direction latched during the START intro
    Player p{};
    std::vector<SDL_FRect> platforms;
    std::vector<Bomb>      bombs;
    std::vector<Enemy>     enemies;
    std::vector<Popup>     popups;
    std::vector<Explosion> explosions;
    std::vector<CoinPickup> coinPickups;
    std::vector<BonusTaken> bonusTakens;
    std::mt19937           rng{std::random_device{}()};
};

// ---------------------------------------------------------------------------
// Bitmap font ripped from the sprite sheet
// ---------------------------------------------------------------------------
// The original Bomb Jack atlas (the embedded sprites_full_png) carries the
// arcade's 7x7 white glyph set: uppercase A-Z on one row and digits plus a
// little punctuation on the next. We crop each glyph into its own texture in
// loadAssets (buildFont) and tint it per draw with a colour mod. Glyphs are 7
// pixels square with one column of spacing, so the cell advance is 8.
constexpr int FONT_W = 7, FONT_H = 7, FONT_ADV = 8;
std::unordered_map<char, SDL_Texture*> g_font;

// ---------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------
void setCol(SDL_Renderer* r, Color c, Uint8 a = 255) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, a);
}
void fillR(SDL_Renderer* r, float x, float y, float w, float h) {
    SDL_FRect q{x, y, w, h};
    SDL_RenderFillRect(r, &q);
}
// Render one glyph from the ripped font, tinted with colour c. Unknown glyphs
// (e.g. space) draw nothing; the caller still advances the pen.
void drawChar(SDL_Renderer* r, char ch, float x, float y, int s, Color c) {
    auto it = g_font.find(static_cast<char>(std::toupper((unsigned char)ch)));
    if (it == g_font.end() || !it->second) return;
    SDL_SetTextureColorMod(it->second, c.r, c.g, c.b);
    SDL_FRect dst{x, y, (float)FONT_W * s, (float)FONT_H * s};
    SDL_RenderTexture(r, it->second, nullptr, &dst);
}
int textWidth(const char* t, int s) { return (int)std::strlen(t) * FONT_ADV * s; }
void drawText(SDL_Renderer* r, const char* t, float x, float y, int s, Color c) {
    for (const char* p = t; *p; ++p) {
        drawChar(r, *p, x, y, s, c);
        x += FONT_ADV * s;
    }
}
void drawTextCentered(SDL_Renderer* r, const char* t, float cx, float y, int s, Color c) {
    drawText(r, t, cx - textWidth(t, s) / 2.0f, y, s, c);
}

// Confine drawing to the central 224x224 play band (below the top HUD strip)
// and scale the 512x448 world into it. SDL multiplies the viewport rect by the
// render scale, so the viewport is given in world units: y = HUD_H / scaleY.
void useWorld(SDL_Renderer* r) {
    SDL_SetRenderScale(r, (float)GAME_W / LOGW, (float)GAME_H / LOGH);
    SDL_Rect vp{0, HUD_H * LOGH / GAME_H, LOGW, LOGH};   // y = 16 * 448/224 = 32
    SDL_SetRenderViewport(r, &vp);
}

// Draw directly in 224x256 screen pixels — used for the HUD strips.
void useScreen(SDL_Renderer* r) {
    SDL_SetRenderScale(r, 1.0f, 1.0f);
    SDL_SetRenderViewport(r, nullptr);
}

// ---------------------------------------------------------------------------
// Sprites — Jack, bombs and enemies are cropped from the embedded original
// Bomb Jack atlas (sprites_full_png.h). Platforms are drawn procedurally
// (drawPlatformShaded), so they need no sprite here. No runtime files.
// ---------------------------------------------------------------------------
enum SpriteId {
    SP_BOMB, SP_ENEMY1, SP_ENEMY2,
    SP_MUMMY, SP_MUMMY_WALK, SP_MUMMY_FALL,
    SP_SPHERE1, SP_SPHERE2, SP_ORB1, SP_ORB2, SP_HORN1, SP_HORN2,
    SP_CLUB1, SP_CLUB2, SP_UFO1, SP_UFO2,
    SP_COUNT
};

struct Sprite { int w = 0, h = 0; SDL_Texture* tex = nullptr; };
Sprite g_sprites[SP_COUNT];

// Bonus coins (4-frame spin each: B/E/S) and the boxed multiplier indicators
// (index 0 is the "x" box, 1..5 are the digits) — cropped in loadAssets.
Sprite g_bonusFrames[4];   // B 
Sprite g_bonusE[4];        // E 
Sprite g_bonusS[4];        // S 
Sprite g_bonusTaken[6];    // collect flash 
SDL_Texture* g_multTex[6] = {};

// Coin spin frames that P-frozen enemies turn into.
Sprite g_coinFrames[7];

// Bomb sprites: frame 0 is the resting bomb, frames 1-6 are the lit
// fuse animation (the "fired" bomb cycling once it's the active target).
Sprite g_bombFrames[7];

// Power-orb sprite (PowerBall.png) prebaked per colour family [0..6] and per
// colour-cycle phase [0..3]: each frame recolours the ball's bands with 4
// shades near the family's base colour, and stepping the phase cycles them.
Sprite g_orbCycle[7][4];

// Bomb-clear explosion frames (explosion.png): small -> medium -> big.
Sprite g_explFrames[3];

// Coin-pickup sparkle (pickCoin.png): 4 frames, baked as white masks so the
// draw can tint them yellow.
Sprite g_pickCoinFrames[4];

// "START!" intro halves (atlas "start_left"/"start_right"): each carries
// alternate scanlines of the word, so overlapping them in the centre spells it
// out. Each half is split into a background mask (the black box, white so it can
// be tinted with a cycling arcade colour) and the yellow text layer, [0]=left.
Sprite g_startBg[2], g_startText[2];

// Jack's animation frames:
// an idle pose, a 4-frame walk cycle per direction, and directional flying
// (rising) / falling poses. Frames are cropped from the shared atlas ("bj_*")
// in buildSprites, in this order.
enum JackFrame {
    JF_IDLE,
    JF_WALK_R0, JF_WALK_R1, JF_WALK_R2, JF_WALK_R3,
    JF_WALK_L0, JF_WALK_L1, JF_WALK_L2, JF_WALK_L3,
    JF_FLY, JF_FLY_R, JF_FLY_L,
    JF_FALL, JF_FALL_R, JF_FALL_L,
    JF_COUNT
};
constexpr int JACK_FW = 16, JACK_FH = 15;     // native frame size in the strip
constexpr float JACK_WALK_FRAME = 0.125f;     // 0.5s / 4 frames (arcade rate)
SDL_Texture* g_jackTex[JF_COUNT] = {};
// During the freeze, Jack is monochrome in the orb's colour with his shades
// cycling. Per frame [JF_COUNT] and per cycle phase [4]: a white banded mask
// (4 brightness bands rotated by phase) that a colour-mod tints to the hue.
SDL_Texture* g_jackPhase[JF_COUNT][4] = {};

struct JackVarFrame { int w = 0, h = 0; SDL_Texture* tex = nullptr; };
JackVarFrame g_jackDance[3] = {};
JackVarFrame g_jackPlf[4] = {};
JackVarFrame g_jackDead[4] = {};
// Round-clear victory dance: four 16x16 poses from sprites.png, indexed
// Normal / Left / Right / Up, cycled through a fixed 16-step routine.
enum JackWinPose { JW_NORMAL, JW_LEFT, JW_RIGHT, JW_UP };
JackVarFrame g_jackWin[4] = {};
constexpr int   VICTORY_STEPS = 16;
constexpr float VICTORY_FRAME = CLEAR_TIME / VICTORY_STEPS;   // 0.2s/step over the banner
constexpr int   VICTORY_SEQ[VICTORY_STEPS] = {
    JW_NORMAL, JW_LEFT,  JW_NORMAL, JW_RIGHT, JW_NORMAL, JW_UP, JW_NORMAL, JW_UP,
    JW_NORMAL, JW_LEFT,  JW_NORMAL, JW_RIGHT, JW_NORMAL, JW_UP, JW_NORMAL, JW_UP,
};

// Bird enemy: a 3-frame wing-flap cycle per heading (left / right / vertical),
// plus the arcade's pulsing-red recolour (BirdToColors). Frames are cropped from
// the shared atlas ("bird_move_*") in buildSprites, in this order.
enum BirdFrame {
    BF_LEFT0, BF_LEFT1, BF_LEFT2,
    BF_RIGHT0, BF_RIGHT1, BF_RIGHT2,
    BF_VERT0, BF_VERT1, BF_VERT2,
    BF_COUNT
};
constexpr int BIRD_FW = 16, BIRD_FH = 16;
constexpr int BIRD_DECIDE_FRAMES = 250;         // re-aim heading toward Jack every N frames
constexpr float BIRD_FLAP_FRAME = 0.1f;       // 0.3s / 3 frames (arcade rate)
constexpr float BIRD_PULSE_STEP = 0.06f;      // colour-pulse cadence (arcade rate)
// Rectangle the bird may spawn within (random angle or direction-forced corner).
constexpr float BIRD_SPAWN_L = 60.0f,        BIRD_SPAWN_R = LOGW - 60.0f;
constexpr float BIRD_SPAWN_T = 70.0f,        BIRD_SPAWN_B = LOGH - 90.0f;
// Bird travel speed for a level (already includes the half-speed tuning).
inline float birdSpeed(int level) {
    return std::min(72.0f + level * 8.0f, 150.0f) * 0.5f;
}
// Red intensity of the eye pulse, looping bright -> black -> bright (BirdToColors).
constexpr Uint8 EYE_PULSE[] = {255, 222, 189, 156, 115, 82, 49, 0,
                               49, 82, 115, 156, 189, 222};
SDL_Texture* g_birdTex[BF_COUNT] = {};        // bird bodies (natural colours)
SDL_Texture* g_birdEye[BF_COUNT] = {};        // bird eye overlays

// Mummy enemy: idle pose, a 3-frame walk cycle per direction, and a falling
// pose, mirroring the original (mummy.lua). Like the bird, only the eyes pulse.
enum MummyFrame {
    MF_IDLE,
    MF_WALK_R0, MF_WALK_R1, MF_WALK_R2,
    MF_WALK_L0, MF_WALK_L1, MF_WALK_L2,
    MF_FALL,
    MF_COUNT
};
constexpr int MUMMY_FW = 16, MUMMY_FH = 16;
constexpr float MUMMY_WALK_FRAME = 0.1f;      // 0.3s / 3 frames (arcade rate)
SDL_Texture* g_mummyTex[MF_COUNT] = {};       // mummy bodies (natural colours)
SDL_Texture* g_mummyEye[MF_COUNT] = {};       // mummy eye overlays

// Spawn-flash frames (atlas "ennemy_appearing"): 4 frames, baked as white masks
// so they can be tinted to any basic colour per spawn. INIT_FW/FH is the common
// cell each (variable-size) atlas frame is centred into.
constexpr int INIT_FW = 33, INIT_FH = 32;
SDL_Texture* g_initEnemyTex[4] = {};

// Current red intensity of the pulsing enemy eyes at time t.
inline Uint8 eyePulse(float t) {
    return EYE_PULSE[(int)(t / BIRD_PULSE_STEP) % (int)std::size(EYE_PULSE)];
}


static SDL_Texture* texFromSurface(SDL_Renderer* ren, SDL_Surface* s) {
    SDL_Texture* t = SDL_CreateTextureFromSurface(ren, s);
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
    return t;
}

// Crop the arcade's white 7x7 glyphs out of the sprite atlas into one tinted
// texture each. Letters sit on a 12px grid at y=212 (A-Z, ink left-aligned in
// the cell); digits share the same grid at y=224, with a handful of hand-placed
// punctuation windows after them.
static void buildFont(SDL_Renderer* ren, SDL_Surface* atlas) {
    auto add = [&](char c, int sx, int sy) {
        SDL_Surface* f = SDL_CreateSurface(FONT_W, FONT_H, SDL_PIXELFORMAT_RGBA32);
        SDL_Rect src{sx, sy, FONT_W, FONT_H};
        SDL_BlitSurface(atlas, &src, f, nullptr);
        g_font[c] = texFromSurface(ren, f);
        SDL_DestroySurface(f);
    };
    const char* letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    for (int i = 0; letters[i]; ++i) add(letters[i], 5 + 12 * i, 212);
    for (int i = 0; i < 10; ++i) add('0' + i, 5 + 12 * i, 224);
    // Punctuation isn't on the grid; these windows centre each glyph in its cell.
    add('\'', 169, 224);
    add('!', 180, 224);
    add('.', 190, 224);
    add('-', 204, 224);
    add('(', 215, 224);
    add(')', 224, 224);
}

// Build an "eye" overlay from a sprite surface: opaque white wherever the
// source is pure red (255,0,0) — the eye pixels the arcade recolours — and
// transparent everywhere else. Returns nullptr if the sprite has no eyes.
static SDL_Texture* makeEyeMask(SDL_Renderer* ren, SDL_Surface* s) {
    SDL_Surface* m = SDL_CreateSurface(s->w, s->h, SDL_PIXELFORMAT_RGBA32);
    bool any = false;
    for (int y = 0; y < s->h; ++y)
        for (int x = 0; x < s->w; ++x) {
            const Uint8* sp = (const Uint8*)s->pixels + y * s->pitch + x * 4;
            Uint8* mp = (Uint8*)m->pixels + y * m->pitch + x * 4;
            bool eye = sp[0] == 255 && sp[1] == 0 && sp[2] == 0 && sp[3] == 255;
            mp[0] = mp[1] = mp[2] = 255;
            mp[3] = eye ? 255 : 0;
            any |= eye;
        }
    SDL_Texture* t = any ? texFromSurface(ren, m) : nullptr;
    SDL_DestroySurface(m);
    return t;
}

// Draw an arbitrary texture at dst with a colour modulation and optional flip.
void drawTexTinted(SDL_Renderer* r, SDL_Texture* tex, const SDL_FRect& dst,
                   bool flip, Uint8 cr, Uint8 cg, Uint8 cb) {
    SDL_SetTextureColorMod(tex, cr, cg, cb);
    SDL_RenderTextureRotated(r, tex, nullptr, &dst, 0.0, nullptr,
                             flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    SDL_SetTextureColorMod(tex, 255, 255, 255);
}

// "GAME OVER" sprite (sprites.png x=2,y=155, 34x28), baked as a white mask so it
// can be colour-cycled. Slides to the screen centre on game over, then the
// attract title returns.
constexpr int   GAMEOVER_W = 34, GAMEOVER_H = 28;
constexpr float GAMEOVER_SLIDE = 110.0f;   // screen px/s the sprite travels
constexpr float GAMEOVER_HOLD  = 3.0f;     // seconds held at centre before the title
SDL_Texture* g_gameOverTex = nullptr;

// 3-stage arcade colour cycle (red->yellow->blue->...), used by the GAME OVER
// sprite. Mirrors guitext.lua getColorFromCycle3.
Color colorCycle3(float t) {
    const float CYCLE = 0.4f;                       // seconds per stage
    int stage = (int)(t / CYCLE);
    float f = t / CYCLE - (float)stage;             // 0..1 within the stage
    Uint8 a = (Uint8)(255.0f * f), b = (Uint8)(255.0f * (1.0f - f));
    switch (((stage % 3) + 3) % 3) {
        case 0:  return {255, a, 0};                // red   -> yellow
        case 1:  return {b, b, a};                  // yellow-> blue
        default: return {a, 0, b};                  // blue  -> red
    }
}

void buildSprites(SDL_Renderer* ren) {
    // The transformed chasers (sphere/orb/horn/club/ufo) are cropped from the
    // shared atlas below, alongside everything else. The bomb/bird/mummy SpriteId
    // entries are legacy and no longer drawn (those have dedicated loaders).

    // Jack's living animation frames are cropped from the shared atlas below.

    // Jack death sequence frames (bj_dancing, bj_PLF, bj_dead) from full atlas.
    int sw = 0, sh = 0, sc = 0;
    stbi_uc* spx = stbi_load_from_memory(sprites_full_png, (int)sprites_full_png_len,
                                         &sw, &sh, &sc, 4);
    if (spx) {
        SDL_Surface* atlas =
            SDL_CreateSurfaceFrom(sw, sh, SDL_PIXELFORMAT_RGBA32, spx, sw * 4);
        auto cropJackVar = [&](int x, int y, int w, int h, JackVarFrame& out) {
            SDL_Surface* f = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
            SDL_Rect src{x, y, w, h};
            SDL_BlitSurface(atlas, &src, f, nullptr);
            out = {w, h, texFromSurface(ren, f)};
            SDL_DestroySurface(f);
        };

        // Jack's living frames (all 16x15 in JackFrame order; sprites.json "bj_*").
        static const int jackRect[JF_COUNT][2] = {
            {4, 4},                                       // IDLE
            {24, 4}, {44, 4}, {64, 4}, {84, 4},           // WALK_R0..3
            {104, 4}, {124, 4}, {144, 4}, {164, 4},       // WALK_L0..3
            {224, 4}, {264, 4}, {304, 4},                 // FLY, FLY_R, FLY_L
            {204, 4}, {284, 4}, {324, 4},                 // FALL, FALL_R, FALL_L
        };
        for (int i = 0; i < JF_COUNT; ++i) {
            SDL_Surface* f = SDL_CreateSurface(JACK_FW, JACK_FH, SDL_PIXELFORMAT_RGBA32);
            SDL_Rect src{jackRect[i][0], jackRect[i][1], JACK_FW, JACK_FH};
            SDL_BlitSurface(atlas, &src, f, nullptr);
            g_jackTex[i] = texFromSurface(ren, f);
            // Frozen twin: classify pixels into 4 luminance bands, then bake 4
            // white phase-frames whose bands rotate. A colour-mod tints them to
            // the orb's hue at draw time, so Jack's shades cycle in that colour.
            float lo = 1e9f, hi = -1e9f;
            auto lum = [](const Uint8* p) { return 0.30f*p[0] + 0.59f*p[1] + 0.11f*p[2]; };
            for (int y = 0; y < JACK_FH; ++y)
                for (int x = 0; x < JACK_FW; ++x) {
                    const Uint8* sp = (const Uint8*)f->pixels + y * f->pitch + x * 4;
                    if (sp[3] > 0) { float L = lum(sp); lo = std::min(lo, L); hi = std::max(hi, L); }
                }
            float span = std::max(1.0f, hi - lo);
            const float bf[4] = {0.45f, 0.65f, 0.83f, 1.0f};   // 4 brightness shades
            for (int ph = 0; ph < 4; ++ph) {
                SDL_Surface* sv = SDL_CreateSurface(JACK_FW, JACK_FH, SDL_PIXELFORMAT_RGBA32);
                for (int y = 0; y < JACK_FH; ++y)
                    for (int x = 0; x < JACK_FW; ++x) {
                        const Uint8* sp = (const Uint8*)f->pixels + y * f->pitch + x * 4;
                        Uint8* dp = (Uint8*)sv->pixels + y * sv->pitch + x * 4;
                        if (sp[3] == 0) { dp[0]=dp[1]=dp[2]=dp[3]=0; continue; }
                        int band = std::min(3, (int)((lum(sp) - lo) / span * 4.0f));
                        Uint8 v = (Uint8)(bf[(band + ph) & 3] * 255.0f);
                        dp[0] = dp[1] = dp[2] = v; dp[3] = 255;
                    }
                g_jackPhase[i][ph] = texFromSurface(ren, sv);
                SDL_DestroySurface(sv);
            }
            SDL_DestroySurface(f);
        }
        static const int danceRect[3][4] = {
            {249, 31, 16, 17}, {269, 31, 16, 17}, {64, 34, 15, 14}
        };
        static const int plfRect[4][4] = {
            {44, 32, 15, 15}, {64, 32, 15, 16}, {84, 32, 15, 16}, {124, 32, 15, 15}
        };
        static const int deadRect[4][4] = {
            {145, 27, 16, 18}, {166, 29, 13, 16}, {184, 24, 17, 24}, {205, 27, 23, 21}
        };
        // Victory-dance poses (Normal / Left / Right / Up), each 16x16.
        static const int winRect[4][2] = {{4, 32}, {24, 32}, {286, 32}, {44, 32}};
        for (int i = 0; i < 4; ++i)
            cropJackVar(winRect[i][0], winRect[i][1], 16, 16, g_jackWin[i]);
        for (int i = 0; i < 3; ++i)
            cropJackVar(danceRect[i][0], danceRect[i][1], danceRect[i][2], danceRect[i][3],
                        g_jackDance[i]);
        for (int i = 0; i < 4; ++i) {
            cropJackVar(plfRect[i][0], plfRect[i][1], plfRect[i][2], plfRect[i][3],
                        g_jackPlf[i]);
            cropJackVar(deadRect[i][0], deadRect[i][1], deadRect[i][2], deadRect[i][3],
                        g_jackDead[i]);
        }
        buildFont(ren, atlas);   // the white glyph rows live in this same atlas

        // Boxed multiplier indicators (sprites.json "multiplier", y=194): the
        // "x" box then digits 1..5, each 12x12.
        static const int multX[6] = {8, 24, 44, 64, 84, 104};
        for (int i = 0; i < 6; ++i) {
            SDL_Surface* f = SDL_CreateSurface(12, 12, SDL_PIXELFORMAT_RGBA32);
            SDL_Rect src{multX[i], 194, 12, 12};
            SDL_BlitSurface(atlas, &src, f, nullptr);
            g_multTex[i] = texFromSurface(ren, f);
            SDL_DestroySurface(f);
        }

        // "GAME OVER" sprite -> white mask (every opaque pixel becomes white)
        // so the colour cycle can drive it across the full hue range.
        {
            SDL_Surface* f = SDL_CreateSurface(GAMEOVER_W, GAMEOVER_H,
                                               SDL_PIXELFORMAT_RGBA32);
            SDL_Rect src{2, 155, GAMEOVER_W, GAMEOVER_H};
            SDL_BlitSurface(atlas, &src, f, nullptr);
            for (int y = 0; y < f->h; ++y)
                for (int x = 0; x < f->w; ++x) {
                    Uint8* p = (Uint8*)f->pixels + y * f->pitch + x * 4;
                    if (p[3] > 0) { p[0] = p[1] = p[2] = 255; }
                }
            g_gameOverTex = texFromSurface(ren, f);
            SDL_DestroySurface(f);
        }

        // Coin spin a frozen enemy becomes (sprites.json "coin"): 7 cells of
        // 14x14 at x=307, y=115. Same source atlas as everything above.
        for (int i = 0; i < 7; ++i) {
            SDL_Surface* f = SDL_CreateSurface(14, 14, SDL_PIXELFORMAT_RGBA32);
            SDL_Rect src{307 + i * 14, 115, 14, 14};
            SDL_BlitSurface(atlas, &src, f, nullptr);
            g_coinFrames[i] = {14, 14, texFromSurface(ren, f)};
            SDL_DestroySurface(f);
        }

        // Generic crop from the atlas into a Sprite (used for the strips below).
        auto cropSprite = [&](int x, int y, int w, int h) -> Sprite {
            SDL_Surface* f = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
            SDL_Rect src{x, y, w, h};
            SDL_BlitSurface(atlas, &src, f, nullptr);
            Sprite sp{w, h, texFromSurface(ren, f)};
            SDL_DestroySurface(f);
            return sp;
        };
        // Bonus B / E / S spin (4 frames each; sprites.json "bonus_B/E/S"), all
        // on the y=114 row sharing the same per-frame widths.
        static const int bonusBX[4] = {78, 98, 117, 130};
        static const int bonusEX[4] = {150, 170, 189, 202};
        static const int bonusSX[4] = {222, 242, 261, 274};
        static const int bonusW[4]  = {13, 12, 7, 12};
        for (int i = 0; i < 4; ++i) {
            g_bonusFrames[i] = cropSprite(bonusBX[i], 114, bonusW[i], 13);
            g_bonusE[i]      = cropSprite(bonusEX[i], 114, bonusW[i], 13);
            g_bonusS[i]      = cropSprite(bonusSX[i], 114, bonusW[i], 13);
        }

        // Transformed chasers (sprites.json sphere/orb/club/ufo/horn): two frames
        // each for the 2-step animation. drawSprite stretches them to a fixed
        // on-screen size, so sources only need to match within a type.
        g_sprites[SP_SPHERE1] = cropSprite(5, 73, 14, 14);   g_sprites[SP_SPHERE2] = cropSprite(85, 73, 14, 14);
        g_sprites[SP_ORB1]    = cropSprite(185, 74, 14, 13); g_sprites[SP_ORB2]    = cropSprite(245, 74, 14, 13);
        g_sprites[SP_CLUB1]   = cropSprite(305, 92, 15, 16); g_sprites[SP_CLUB2]   = cropSprite(345, 92, 15, 16);
        g_sprites[SP_UFO1]    = cropSprite(4, 98, 16, 10);   g_sprites[SP_UFO2]    = cropSprite(64, 98, 16, 10);
        // Horn frames vary in size; centre two into a common 16x16 cell.
        auto cropCentered = [&](int x, int y, int w, int h) -> Sprite {
            SDL_Surface* f = SDL_CreateSurface(16, 16, SDL_PIXELFORMAT_RGBA32);
            SDL_Rect src{x, y, w, h};
            SDL_Rect dst{(16 - w) / 2, (16 - h) / 2, w, h};
            SDL_BlitSurface(atlas, &src, f, &dst);
            Sprite sp{16, 16, texFromSurface(ren, f)};
            SDL_DestroySurface(f);
            return sp;
        };
        g_sprites[SP_HORN1] = cropCentered(280, 54, 15, 14);
        g_sprites[SP_HORN2] = cropCentered(318, 52, 11, 16);
        // Bonus collect flash ("power_explosions"): 6 cells of 32x32, pitch 33.
        for (int i = 0; i < 6; ++i) g_bonusTaken[i] = cropSprite(266 + i * 33, 327, 32, 32);
        // Bombs ("bomb" + "bomb_activated"): resting bomb then 6 lit frames, 12x16.
        for (int i = 0; i < 7; ++i) g_bombFrames[i] = cropSprite(46 + i * 20, 136, 12, 16);
        // Bomb-clear explosion ("bomb_explosion"): 3 growing frames.
        static const int er[3][4] = {{24, 120, 8, 8}, {38, 114, 12, 12}, {56, 112, 16, 16}};
        for (int i = 0; i < 3; ++i) g_explFrames[i] = cropSprite(er[i][0], er[i][1], er[i][2], er[i][3]);

        // Bird flap frames ("bird_move_left/right/vertical", 3 each). The atlas
        // boxes are tightly cropped at varying heights, so each is centred into a
        // common 16x16 cell to keep the body anchored through the wing-flap.
        static const int birdRect[BF_COUNT][4] = {
            {140, 53, 16, 16}, {160, 54, 16, 13}, {180, 55, 16, 11},   // left 0..2
            {352, 53, 16, 16}, {373, 54, 16, 13}, {393, 54, 16, 11},   // right 0..2
            {201, 53, 15, 15}, {221, 55, 15, 12}, {241, 55, 15, 11},   // vertical 0..2
        };
        for (int i = 0; i < BF_COUNT; ++i) {
            const int w = birdRect[i][2], h = birdRect[i][3];
            SDL_Surface* f = SDL_CreateSurface(BIRD_FW, BIRD_FH, SDL_PIXELFORMAT_RGBA32);
            SDL_Rect src{birdRect[i][0], birdRect[i][1], w, h};
            SDL_Rect dst{(BIRD_FW - w) / 2, (BIRD_FH - h) / 2, w, h};   // centre in the cell
            SDL_BlitSurface(atlas, &src, f, &dst);
            g_birdTex[i] = texFromSurface(ren, f);
            g_birdEye[i] = makeEyeMask(ren, f);
            SDL_DestroySurface(f);
        }

        // Mummy frames ("mummy_idle/move_*/falling"): tight boxes top-aligned at
        // y=53, centred into a common 16x16 cell (same scheme as the bird).
        static const int mummyRect[MF_COUNT][4] = {
            {6, 53, 12, 15},                                          // IDLE
            {45, 53, 11, 15}, {61, 53, 11, 15}, {76, 53, 13, 15},     // WALK_R0..2
            {92, 53, 11, 15}, {108, 53, 11, 15}, {123, 53, 13, 15},   // WALK_L0..2
            {25, 53, 14, 16},                                         // FALL
        };
        for (int i = 0; i < MF_COUNT; ++i) {
            const int w = mummyRect[i][2], h = mummyRect[i][3];
            SDL_Surface* f = SDL_CreateSurface(MUMMY_FW, MUMMY_FH, SDL_PIXELFORMAT_RGBA32);
            SDL_Rect src{mummyRect[i][0], mummyRect[i][1], w, h};
            SDL_Rect dst{(MUMMY_FW - w) / 2, (MUMMY_FH - h) / 2, w, h};   // centre in the cell
            SDL_BlitSurface(atlas, &src, f, &dst);
            g_mummyTex[i] = texFromSurface(ren, f);
            g_mummyEye[i] = makeEyeMask(ren, f);
            SDL_DestroySurface(f);
        }

        // Power orb ("power" 12x12): classify pixels into 4 luminance bands, then
        // prebake a recoloured ball per colour family and cycle phase. Stepping
        // the phase at runtime cycles the shades (the orb itself stays upright).
        {
            const int PX = 292, PY = 116, ow = 12, oh = 12;
            std::vector<Uint8> opx((size_t)ow * oh * 4);
            for (int y = 0; y < oh; ++y)
                std::memcpy(&opx[(size_t)y * ow * 4],
                            (Uint8*)atlas->pixels + (PY + y) * atlas->pitch + PX * 4,
                            (size_t)ow * 4);
            auto lum = [](const Uint8* p) { return 0.30f*p[0] + 0.59f*p[1] + 0.11f*p[2]; };
            float lo = 1e9f, hi = -1e9f;
            for (int i = 0; i < ow * oh; ++i)
                if (opx[i*4 + 3] > 0) { float L = lum(&opx[i*4]); lo = std::min(lo, L); hi = std::max(hi, L); }
            float span = std::max(1.0f, hi - lo);
            std::vector<int> band((size_t)ow * oh, -1);
            for (int i = 0; i < ow * oh; ++i)
                if (opx[i*4 + 3] > 0) band[i] = std::min(3, (int)((lum(&opx[i*4]) - lo) / span * 4.0f));
            const float bf[4] = {0.50f, 0.68f, 0.84f, 1.0f};   // 4 brightness shades
            for (int fam = 0; fam < 7; ++fam) {
                Color bc = POWER_COLORS[fam];
                for (int ph = 0; ph < 4; ++ph) {
                    SDL_Surface* s = SDL_CreateSurface(ow, oh, SDL_PIXELFORMAT_RGBA32);
                    for (int i = 0; i < ow * oh; ++i) {
                        Uint8* dp = (Uint8*)s->pixels + i * 4;
                        if (band[i] < 0) { dp[0] = dp[1] = dp[2] = dp[3] = 0; continue; }
                        float f = bf[(band[i] + ph) & 3];      // cycle the shade by phase
                        dp[0] = (Uint8)(bc.r * f); dp[1] = (Uint8)(bc.g * f);
                        dp[2] = (Uint8)(bc.b * f); dp[3] = 255;
                    }
                    g_orbCycle[fam][ph] = {ow, oh, texFromSurface(ren, s)};
                    SDL_DestroySurface(s);
                }
            }
        }

        // Spawn flash ("ennemy_appearing"): 4 growing frames, magenta-shaded
        // (green=0). Centre each tight box into a common INIT cell and bake a
        // white mask whose brightness is (red+blue)/2 so a per-spawn colour mod
        // can tint it to any basic colour.
        static const int initRect[4][4] = {
            {40, 158, 18, 29}, {65, 157, 29, 31}, {101, 158, 29, 29}, {138, 159, 28, 28}
        };
        for (int i = 0; i < 4; ++i) {
            const int w = initRect[i][2], h = initRect[i][3];
            SDL_Surface* f = SDL_CreateSurface(INIT_FW, INIT_FH, SDL_PIXELFORMAT_RGBA32);
            SDL_Rect src{initRect[i][0], initRect[i][1], w, h};
            SDL_Rect dst{(INIT_FW - w) / 2, (INIT_FH - h) / 2, w, h};   // centre in the cell
            SDL_BlitSurface(atlas, &src, f, &dst);
            for (int y = 0; y < f->h; ++y)
                for (int x = 0; x < f->w; ++x) {
                    Uint8* p = (Uint8*)f->pixels + y * f->pitch + x * 4;
                    Uint8 v = (Uint8)((p[0] + p[2]) / 2);   // red+blue -> brightness
                    p[0] = p[1] = p[2] = v;
                }
            g_initEnemyTex[i] = texFromSurface(ren, f);
            SDL_DestroySurface(f);
        }

        // "START!" intro halves ("start_left" 240, "start_right" 300): each
        // carries alternate scanlines of the word; overlapping them spells it.
        // Both span y=260..271, so a common 56x12 box aligns them. Each is split
        // into a white box mask (colour-cycled) and the yellow text layer.
        auto splitStartHalf = [&](int x0, Sprite& bg, Sprite& text) {
            const int sw = 56, sh = 12;
            SDL_Surface* b = SDL_CreateSurface(sw, sh, SDL_PIXELFORMAT_RGBA32);
            SDL_Surface* t = SDL_CreateSurface(sw, sh, SDL_PIXELFORMAT_RGBA32);
            for (int y = 0; y < sh; ++y)
                for (int x = 0; x < sw; ++x) {
                    const Uint8* sp = (const Uint8*)atlas->pixels +
                                      (260 + y) * atlas->pitch + (x0 + x) * 4;
                    Uint8* bp = (Uint8*)b->pixels + y * b->pitch + x * 4;
                    Uint8* tp = (Uint8*)t->pixels + y * t->pitch + x * 4;
                    bool opaque = sp[3] > 0;
                    bool yellow = opaque && sp[0] > 150 && sp[1] > 150 && sp[2] < 120;
                    bool box    = opaque && !yellow;          // the black background
                    bp[0] = bp[1] = bp[2] = 255; bp[3] = box ? 255 : 0;
                    tp[0] = sp[0]; tp[1] = sp[1]; tp[2] = sp[2]; tp[3] = yellow ? 255 : 0;
                }
            bg   = {sw, sh, texFromSurface(ren, b)};
            text = {sw, sh, texFromSurface(ren, t)};
            SDL_DestroySurface(b);
            SDL_DestroySurface(t);
        };
        splitStartHalf(240, g_startBg[0], g_startText[0]);
        splitStartHalf(300, g_startBg[1], g_startText[1]);

        // Coin-pickup sparkle ("ennemy_explosions"): 4 white burst frames (drawn
        // centred and tinted yellow). Force opaque pixels white for a clean tint.
        static const int pickRect[4][4] = {
            {175, 159, 26, 26}, {209, 159, 30, 28}, {244, 159, 32, 29}, {282, 156, 32, 32}
        };
        for (int i = 0; i < 4; ++i) {
            const int w = pickRect[i][2], h = pickRect[i][3];
            SDL_Surface* f = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
            SDL_Rect src{pickRect[i][0], pickRect[i][1], w, h};
            SDL_BlitSurface(atlas, &src, f, nullptr);
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x) {
                    Uint8* p = (Uint8*)f->pixels + y * f->pitch + x * 4;
                    if (p[3]) { p[0] = p[1] = p[2] = 255; }
                }
            g_pickCoinFrames[i] = {w, h, texFromSurface(ren, f)};
            SDL_DestroySurface(f);
        }

        SDL_DestroySurface(atlas);
        stbi_image_free(spx);
    } else {
        std::fprintf(stderr, "full sprites atlas decode failed\n");
    }

    // Bonus B/E/S coins, the collect flash, bombs and the explosion are cropped
    // from the shared atlas in the block above.

    // The "START!" intro halves are cropped from the shared atlas in the block above.

    // The power orb is prebaked from the shared atlas in the block above.

    // The coin-pickup sparkle is cropped from the shared atlas in the block above.

    // Bird flap frames are cropped from the shared atlas in the block above.

    // Mummy frames are cropped from the shared atlas in the block above.

    // The spawn flash is cropped from the shared atlas in the block above.
}

void drawSprite(SDL_Renderer* r, SpriteId id, float x, float y, float w, float h,
                bool flip) {
    SDL_FRect dst{x, y, w, h};
    SDL_RenderTextureRotated(r, g_sprites[id].tex, nullptr, &dst, 0.0, nullptr,
                             flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}

// Backgrounds — the embedded 1120x224 strip holds five 224-wide arcade
// backdrops; rounds cycle through them (round 1 = first, then the next, ...).
constexpr int BG_W = 224, BG_H = 224;
SDL_Texture* g_bgTex = nullptr;
int g_bgCount = 0;

// Title logo (192x80, black colour-keyed to transparent), shown on the welcome
// screen. The ray backdrop uses three blues that the arcade cycles to make the
// rays appear to spin; we bake one texture per rotation phase and flip between
// them. Decoded once alongside the backgrounds.
constexpr int BANNER_W = 192, BANNER_H = 80;
constexpr int BANNER_PHASES = 3;
constexpr float BANNER_ROT = 0.05f;   // seconds per rotation step (arcade rate)
SDL_Texture* g_bannerTex[BANNER_PHASES] = {};

// Jack life icon (31x32, black colour-keyed to transparent) shown in the HUD.
constexpr int LIVE_W = 16, LIVE_H = 16;   // atlas life icon at (1,3)
SDL_Texture* g_liveTex = nullptr;

const std::array<Color, 8>& borderPaletteForScreen(int screen) {
    static const std::array<Color, 8> platform1 = {{
        {255, 223, 0}, {255, 191, 0}, {255, 159, 0}, {255, 127, 0},
        {255, 95, 0},  {255, 63, 0},  {255, 63, 0},  {255, 31, 0}
    }};
    static const std::array<Color, 8> platform2 = {{
        {0, 223, 0}, {0, 191, 0}, {0, 191, 0}, {0, 159, 0},
        {0, 127, 0}, {0, 95, 0},  {0, 63, 0},  {0, 31, 0}
    }};
    static const std::array<Color, 8> platform3 = {{
        {252, 252, 80}, {252, 252, 0}, {252, 252, 0}, {216, 216, 0},
        {180, 180, 0},  {144, 144, 0}, {108, 108, 0}, {72, 72, 0}
    }};
    static const std::array<Color, 8> platform4 = {{
        {0, 255, 255}, {0, 204, 255}, {0, 204, 255}, {0, 170, 255},
        {0, 136, 255}, {0, 0, 255},   {0, 0, 204},   {0, 0, 136}
    }};

    // Original levels map screens to border palettes as follows:
    // screen 1->Platform1, 2->Platform2, 3->Platform1, 4->Platform3, 5->Platform4.
    switch (screen) {
        case 1: return platform2;
        case 2: return platform1;
        case 3: return platform3;
        case 4: return platform4;
        default: return platform1;
    }
}

void drawPlayfieldBorder(SDL_Renderer* r, int screen) {
    const auto& pal = borderPaletteForScreen(screen);
    const float x0 = 0.0f;
    const float y0 = (float)HUD_H;
    const float w = (float)GAME_W;
    const float h = (float)GAME_H;
    for (int i = 0; i < 8; ++i) {
        setCol(r, pal[i]);
        fillR(r, x0 + i, y0 + i, w - 2.0f * i, 1.0f);                     // top
        fillR(r, x0 + i, y0 + i, 1.0f, h - 2.0f * i);                      // left

        // Invert shading on bottom/right for a beveled volume look.
        setCol(r, pal[7 - i]);
        fillR(r, x0 + i, y0 + h - 1.0f - i, w - 2.0f * i, 1.0f);          // bottom
        fillR(r, x0 + w - 1.0f - i, y0 + i, 1.0f, h - 2.0f * i);           // right
    }
}

// A platform whose edge reaches a side frame gets a small tab drawn over the
// frame (drawPlatformFrameTab); these helpers detect that and shape its corners.
constexpr int PLAT_TAB_EXT   = 3;   // screen px the platform pokes into the frame
constexpr int PLAT_TAB_NOTCH = 2;   // corner pixels dropped to round the tab tip

struct EdgeTouch { bool left, right; };
EdgeTouch platformEdgeTouch(int x, int w) {
    const int b = (int)std::lround(BORDER_SOLID_X);
    return { x <= b + 1, (x + w) >= LOGW - b - 1 };
}

// How many pixels the tab (or rounding) pulls in at row j: PLAT_TAB_NOTCH at the
// very top/bottom, tapering to 0 a couple of rows in — the "corner + 2 neighbours".
int platCornerPull(int j, int h) {
    int top = PLAT_TAB_NOTCH - j;
    int bot = PLAT_TAB_NOTCH - (h - 1 - j);
    return std::clamp(std::max(top, bot), 0, PLAT_TAB_EXT);
}

void drawPlatformShaded(SDL_Renderer* r, const SDL_FRect& pl, int screen) {
    const auto& pal = borderPaletteForScreen(screen);
    const int x = (int)std::round(pl.x);
    const int y = (int)std::round(pl.y);
    const int w = std::max(1, (int)std::round(pl.w));
    const int h = std::max(1, (int)std::round(pl.h));
    // Vertical girders (taller than wide) shade across their width — bright left
    // -> dark right — to match the playfield frame's vertical bars, rather than
    // along their length. Mirrors the reference platform.lua "V" branch (a column
    // per colour band). The top/bottom of the outer columns are inset 1px so the
    // four corners round off like the horizontal case.
    if (h > w) {
        for (int i = 0; i < w; ++i) {
            int band = std::min(7, (i * 8) / w);
            setCol(r, pal[band]);
            bool edge = (i == 0 || i == w - 1);
            float top = (float)y + (edge ? 1.0f : 0.0f);
            float bot = (float)(y + h) - (edge ? 1.0f : 0.0f);
            if (bot > top) fillR(r, (float)(x + i), top, 1.0f, bot - top);
        }
        return;
    }

    // A side that meets the frame is snapped to the exact frame inner edge (not
    // the rounded platform edge, which can fall a sub-pixel short and leave a gap)
    // and left square — its over-frame tab carries the rounding instead.
    const EdgeTouch touch = platformEdgeTouch(x, w);

    // Top bright -> bottom dark, matching original platform shading feel. Drawn
    // scanline by scanline so the top and bottom edge rows can be inset by 1px,
    // dropping the four corner pixels for a slightly rounded look.
    for (int j = 0; j < h; ++j) {
        int band = std::min(7, (j * 8) / h);
        setCol(r, pal[band]);
        bool edge = (j == 0 || j == h - 1);
        float left  = touch.left  ? BORDER_SOLID_X          : (float)x + (edge ? 1.0f : 0.0f);
        float right = touch.right ? LOGW - BORDER_SOLID_X    : (float)(x + w) - (edge ? 1.0f : 0.0f);
        if (right > left) fillR(r, left, (float)(y + j), right - left, 1.0f);
    }
}

// Draw the over-frame extension for platforms that reach a side frame. Called in
// screen space after the border so the tab sits on top of it; the tip is notched
// so the frame shows through, rounding the join.
void drawPlatformFrameTab(SDL_Renderer* r, const SDL_FRect& pl, int screen) {
    const int wx = (int)std::round(pl.x);
    const int ww = std::max(1, (int)std::round(pl.w));
    const EdgeTouch touch = platformEdgeTouch(wx, ww);
    if (!touch.left && !touch.right) return;
    const auto& pal = borderPaletteForScreen(screen);
    // The platform's on-screen rect (world scaled into the HUD-offset play band).
    const int sy = (int)std::round(HUD_H + pl.y * GAME_H / (float)LOGH);
    const int sh = std::max(1, (int)std::round(pl.h * GAME_H / (float)LOGH));
    const int leftEdge  = (int)std::round(pl.x * GAME_W / (float)LOGW);          // ~8
    const int rightEdge = (int)std::round((pl.x + pl.w) * GAME_W / (float)LOGW); // ~216
    for (int j = 0; j < sh; ++j) {
        int band = std::min(7, (j * 8) / sh);
        int len = PLAT_TAB_EXT - platCornerPull(j, sh);   // narrower near the corners
        if (len <= 0) continue;
        setCol(r, pal[band]);
        if (touch.left)  fillR(r, (float)(leftEdge - len), (float)(sy + j), (float)len, 1.0f);
        if (touch.right) fillR(r, (float)rightEdge,        (float)(sy + j), (float)len, 1.0f);
    }
}

void buildBackground(SDL_Renderer* ren) {
    int w = 0, h = 0, comp = 0;
    stbi_uc* px = stbi_load_from_memory(screens_png, (int)screens_png_len,
                                        &w, &h, &comp, 4);
    if (!px) {
        std::fprintf(stderr, "background decode failed\n");
        return;
    }
    SDL_Surface* s = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_RGBA32, px, w * 4);
    g_bgTex = SDL_CreateTextureFromSurface(ren, s);
    SDL_SetTextureScaleMode(g_bgTex, SDL_SCALEMODE_NEAREST);  // keep the pixel look
    SDL_DestroySurface(s);
    stbi_image_free(px);
    g_bgCount = w / BG_W;

    // Title logo: cropped from the shared atlas ("logo" at 4,300, 192x80), then
    // recoloured per phase to spin the three ray blues (BombjackLogoColors).
    int aw = 0, ah = 0, ac = 0;
    stbi_uc* apx = stbi_load_from_memory(sprites_full_png, (int)sprites_full_png_len,
                                         &aw, &ah, &ac, 4);
    if (!apx) {
        std::fprintf(stderr, "atlas decode failed (banner)\n");
        return;
    }
    const int LX = 4, LY = 300, bw = BANNER_W, bh = BANNER_H;
    std::vector<Uint8> logo((size_t)bw * bh * 4);
    for (int y = 0; y < bh; ++y)
        std::memcpy(&logo[(size_t)y * bw * 4],
                    apx + ((size_t)(LY + y) * aw + LX) * 4, (size_t)bw * 4);

    // Jack life icon for the HUD (atlas life sprite at 1,3, 16x16) from the same atlas.
    {
        const int LIX = 1, LIY = 3;
        std::vector<Uint8> li((size_t)LIVE_W * LIVE_H * 4);
        for (int y = 0; y < LIVE_H; ++y)
            std::memcpy(&li[(size_t)y * LIVE_W * 4],
                        apx + ((size_t)(LIY + y) * aw + LIX) * 4, (size_t)LIVE_W * 4);
        SDL_Surface* ls = SDL_CreateSurfaceFrom(LIVE_W, LIVE_H, SDL_PIXELFORMAT_RGBA32,
                                                li.data(), LIVE_W * 4);
        g_liveTex = SDL_CreateTextureFromSurface(ren, ls);
        SDL_SetTextureScaleMode(g_liveTex, SDL_SCALEMODE_NEAREST);
        SDL_DestroySurface(ls);
    }
    stbi_image_free(apx);

    const Uint8 blues[BANNER_PHASES][3] = {{0, 82, 255}, {0, 140, 255}, {0, 189, 255}};
    std::vector<Uint8> buf((size_t)bw * bh * 4);
    for (int phase = 0; phase < BANNER_PHASES; ++phase) {
        std::memcpy(buf.data(), logo.data(), buf.size());
        for (size_t p = 0; p < buf.size(); p += 4) {
            for (int i = 0; i < BANNER_PHASES; ++i)
                if (buf[p] == blues[i][0] && buf[p + 1] == blues[i][1] &&
                    buf[p + 2] == blues[i][2]) {
                    const Uint8* c = blues[(i + BANNER_PHASES - phase) % BANNER_PHASES];
                    buf[p] = c[0]; buf[p + 1] = c[1]; buf[p + 2] = c[2];
                    break;
                }
        }
        SDL_Surface* bs = SDL_CreateSurfaceFrom(bw, bh, SDL_PIXELFORMAT_RGBA32,
                                                buf.data(), bw * 4);
        g_bannerTex[phase] = SDL_CreateTextureFromSurface(ren, bs);
        SDL_SetTextureScaleMode(g_bannerTex[phase], SDL_SCALEMODE_NEAREST);
        SDL_DestroySurface(bs);
    }

}

// ---------------------------------------------------------------------------
// Level / entity setup
// ---------------------------------------------------------------------------
constexpr float FLOOR_TOP = 432.0f;       // solid ground at the bottom of the field

// The hand-authored layout for the current round (levels loop after the last).
const leveldata::Layout& currentLayout(const Game& g) {
    int idx = (g.level - 1) % leveldata::LEVEL_COUNT;
    return leveldata::layouts[leveldata::levels[idx].layout];
}

// Which of the five backdrops the current round's layout calls for.
int currentScreen(const Game& g) {
    int idx = (g.level - 1) % leveldata::LEVEL_COUNT;
    return leveldata::levels[idx].screen;
}

void initPlatforms(Game& g) {
    g.platforms.clear();
    g.platforms.push_back({0, FLOOR_TOP, LOGW, LOGH - FLOOR_TOP});   // ground
    if (currentScreen(g) == 4) return;   // California (screen 5) has no platforms
    const leveldata::Layout& lay = currentLayout(g);
    for (int i = 0; i < lay.nplats; ++i) {
        const leveldata::Plat& p = lay.plats[i];
        g.platforms.push_back({p.x, p.y, p.w, p.h});
    }
}

void spawnBombs(Game& g) {
    g.bombs.clear();
    const leveldata::Layout& lay = currentLayout(g);
    for (int i = 0; i < lay.nbombs; ++i)   // already sorted by lit-up order
        g.bombs.push_back({lay.bombs[i].x, lay.bombs[i].y, false});
    g.bombsLeft = (int)g.bombs.size();
    g.litBomb = -1;        // arcade: nothing is lit until the first bomb is grabbed
    g.catched = 0;         // fire-bomb count resets each level
}

Enemy makeBird(Game& g, float ang) {
    std::uniform_real_distribution<float> sign(-1.0f, 1.0f);
    float speed = birdSpeed(g.level);
    Enemy e;
    e.kind = EK_BIRD;
    e.r = 11.0f;
    e.x = BIRD_SPAWN_L + (BIRD_SPAWN_R - BIRD_SPAWN_L) * (0.5f + 0.5f * std::cos(ang));
    e.y = BIRD_SPAWN_T + (BIRD_SPAWN_B - BIRD_SPAWN_T) * (0.5f + 0.5f * std::sin(ang));
    e.vx = std::cos(ang) * speed + sign(g.rng) * 10.0f;
    e.vy = std::sin(ang) * speed + sign(g.rng) * 10.0f;
    return e;
}

void spawnEnemies(Game& g) {
    g.enemies.clear();
    int idx = (g.level - 1) % leveldata::LEVEL_COUNT;
    int birds = leveldata::levels[idx].birds;
    for (int i = 0; i < birds; ++i)
        g.enemies.push_back(makeBird(g, (i + 0.5f) / birds * 6.2831853f));
}

// Delete every enemy and respawn the phase's initial set, resetting the
// mummy-drop progression and any freeze/orb state. Used at the start of a round
// and whenever a life restarts mid-phase, so each life faces the same opening.
void restartEnemies(Game& g) {
    spawnEnemies(g);
    g.mummyTimer = 0.0f;
    g.mummiesSpawned = 0;
    g.transformIdx = 0;
    g.freezeTimer = 0.0f;
    g.killCount = 0;
    g.orbActive = false;
}

// Arcade spawn-forcing: a direction held before Jack starts moving makes the
// bird appear on the opposite side / corner. Encodes the speedrun table exactly
// — horizontal is always mirrored; a pure up/down also mirrors vertically, but a
// diagonal keeps its vertical (e.g. up+right -> top-left). Applied to the first
// bird once the START intro ends; no hold leaves the random spawn untouched.
void placeBirdFromHold(Game& g) {
    int hx = g.spawnHoldX, hy = g.spawnHoldY;
    if (hx == 0 && hy == 0) return;
    Enemy* bird = nullptr;
    for (Enemy& e : g.enemies) if (e.kind == EK_BIRD) { bird = &e; break; }
    if (!bird) return;

    const float leftX = BIRD_SPAWN_L, rightX = BIRD_SPAWN_R;
    const float topY  = BIRD_SPAWN_T, bottomY = BIRD_SPAWN_B;
    const float midX  = LOGW / 2.0f, midY = (topY + bottomY) / 2.0f;
    float bx = (hx > 0) ? leftX : (hx < 0) ? rightX : midX;   // horizontal: mirror
    float by;
    if (hx == 0) by = (hy < 0) ? bottomY : (hy > 0) ? topY    : midY;  // pure vert: mirror
    else         by = (hy < 0) ? topY    : (hy > 0) ? bottomY : midY;  // diagonal: keep

    float speed = birdSpeed(g.level);
    bird->x = bx; bird->y = by;
    bird->vx = (bx < midX ? 1.0f : -1.0f) * speed;           // head into the field
    bird->vy = (by < midY ? 1.0f : -1.0f) * speed * 0.5f;
    bird->tick = 0; bird->dirX = 0; bird->dirY = 0;
}

void resetPlayer(Game& g, bool invuln) {
    // Jack starts suspended in mid-air at the centre of the play area — right
    // where the "START!" banner sits — and only begins to fall once the intro
    // ends and the simulation resumes.
    g.p.x = LOGW / 2.0f - PW / 2.0f;
    g.p.y = LOGH / 2.0f - PH / 2.0f;
    g.p.vx = g.p.vy = 0.0f;
    g.p.onGround = false;
    g.p.invuln = invuln ? INVULN_TIME : 0.0f;
}

// Drop a bonus "B" coin onto a random (non-floor) platform; it patrols that
// platform horizontally until Jack grabs it. Falls back to the floor band if
// the layout has no platforms (e.g. the California screen).
void spawnBonus(Game& g, int kind) {
    g.bonusKind = kind;
    std::vector<const SDL_FRect*> eligible;
    for (const SDL_FRect& pl : g.platforms)
        if (pl.y < FLOOR_TOP - 1.0f) eligible.push_back(&pl);

    if (!eligible.empty()) {
        const SDL_FRect* pl = eligible[g.rng() % eligible.size()];
        g.bonusPlatX = pl->x;
        g.bonusPlatW = pl->w;
        g.bonusY = pl->y - BONUS_H * 0.5f - 1.0f;
    } else {
        g.bonusPlatX = BORDER_SOLID_X;
        g.bonusPlatW = LOGW - 2 * BORDER_SOLID_X;
        g.bonusY = FLOOR_TOP - BONUS_H * 0.5f - 1.0f;
    }
    g.bonusX = g.bonusPlatX + g.bonusPlatW * 0.5f;
    g.bonusVx = (g.rng() & 1) ? BONUS_SPEED : -BONUS_SPEED;
    g.bonusAnim = 0.0f;
    g.bonusActive = true;
}

void startRound(Game& g) {
    initPlatforms(g);     // load this round's hand-authored layout
    spawnBombs(g);
    spawnEnemies(g);
    resetPlayer(g, true);
    g.orbActive = false;
    g.freezeTimer = 0.0f;
    g.killCount = 0;
    g.playerDying = false;
    g.deathTimer = 0.0f;
    g.deathPhase = DP_NONE;
    g.deathFrame = 0;
    g.deathLoops = 0;
    g.deathAnim = 0.0f;
    g.mummyTimer = 0.0f;
    g.mummiesSpawned = 0;
    g.transformIdx = 0;
    g.popups.clear();
    g.explosions.clear();
    g.coinPickups.clear();
    g.bonusTakens.clear();
    g.phaseStart = g.score;            // start counting this phase's points fresh
    g.multiplier = 1;                  // the points multiplier resets each level
    g.bonusActive = false;
    g.nextBonusScore = (g.score / BONUS_LIMIT + 1) * BONUS_LIMIT;
    g.startAnim = START_TOTAL;          // play the "START!" intro for this phase
    g.birdSpawnPending = true;          // hold a direction during it to place the bird
    g.spawnHoldX = g.spawnHoldY = 0;
}

void startGame(Game& g) {
    g.score = 0;
    g.lives = 3;
    g.level = 1;
    g.state = PLAYING;
    g.bCoins = 0;                      // E/S progress is per-game, not per-round
    g.nextEAt = BONUS_E_EVERY;
    g.livesLost = 0;
    initPlatforms(g);
    startRound(g);
}

// ---------------------------------------------------------------------------
// Enemies — mummies drop in, walk/fall down the platforms, then transform into
// one of five flying chasers (Bomb Jack's signature enemy progression).
// ---------------------------------------------------------------------------
const leveldata::LevelDef& levelDef(const Game& g) {
    return leveldata::levels[(g.level - 1) % leveldata::LEVEL_COUNT];
}

void spawnMummy(Game& g) {
    const leveldata::LevelDef& L = levelDef(g);
    const leveldata::Layout& lay = currentLayout(g);
    Enemy e;
    e.kind = EK_MUMMY;
    e.phase = EP_INIT;                            // spawn flash, then EP_APPEAR
    e.timer = INIT_ENEMY_TIME;
    e.r = 10.0f;
    e.x = lay.mummyx[g.p.x < LOGW / 2 ? 1 : 0];   // drop in away from Jack
    // Drop in from the default height, but never start at/below the topmost
    // platform in this column (some columns sit under a y=48/96 platform, which
    // would spawn the mummy embedded in it and let it fall straight through).
    float topY = FLOOR_TOP;
    for (const SDL_FRect& pl : g.platforms)
        if (e.x > pl.x && e.x < pl.x + pl.w) topY = std::min(topY, pl.y);
    e.y = std::min(MUMMY_SPAWN_Y, topY - MUMMY_HALF_H - MUMMY_DROP_GAP);
    e.vx = e.vy = 0.0f;
    e.bounces = L.bouncing;
    e.becomes = L.mummies[g.transformIdx % L.nmummies];
    std::uniform_int_distribution<int> pick(0, (int)std::size(BASIC_COLORS) - 1);
    e.spawnTint = BASIC_COLORS[pick(g.rng)];
    g.transformIdx++;
    g.enemies.push_back(e);
}

void transformMummy(Game& g, Enemy& e) {
    e.kind = e.becomes;
    e.phase = EP_FLY;
    float s = FLY_SPEED + g.level * 2.0f;
    std::uniform_int_distribution<int> coin(0, 1);
    float sx = coin(g.rng) ? 1.0f : -1.0f, sy = coin(g.rng) ? 1.0f : -1.0f;
    switch (e.kind) {
        case EK_SPHERE: e.vx = 0;          e.vy = sy * s * 0.7f; break;
        case EK_ORB:    e.vx = sx * s*0.7f; e.vy = 0;            break;
        case EK_CLUB:   e.vx = sx * s*0.5f; e.vy = sy * s * 0.5f; break;
        case EK_HORN:   e.vx = sx * s*0.6f; e.vy = sy * s * 0.6f; break;
        case EK_UFO:    e.vx = e.vy = 0;                          break;
        default:        e.vx = sx * s*0.5f; e.vy = sy * s * 0.5f; break;
    }
    e.y -= 6.0f;   // lift off the ground
}

// Mummy gravity + platform walk/fall, transforming when it reaches the ground.
void updateMummy(Game& g, Enemy& e, float dt) {
    if (e.phase == EP_INIT) {                     // spawn flash before the mummy
        e.timer -= dt;
        if (e.timer <= 0) {
            e.phase = EP_APPEAR;
            e.timer = MUMMY_APPEAR_TIME;
        }
        return;
    }
    if (e.phase == EP_APPEAR) {
        e.timer -= dt;
        if (e.timer <= 0) {
            e.phase = EP_WALK;
            e.vx = (e.x < LOGW / 2) ? MUMMY_WALK_SPEED : -MUMMY_WALK_SPEED;
        }
        return;
    }
    if (e.phase == EP_DISAPPEAR) {
        e.timer -= dt;
        if (e.timer <= 0) transformMummy(g, e);
        return;
    }
    const float halfH = MUMMY_HALF_H, halfW = 8.0f;
    e.vy += GRAVITY * dt;
    if (e.vy > MAXFALL) e.vy = MAXFALL;
    float feetOld = e.y + halfH;
    e.y += e.vy * dt;
    float feet = e.y + halfH;
    if (e.vy >= 0) {                                   // land on a platform top
        for (const SDL_FRect& pl : g.platforms)
            if (e.x > pl.x && e.x < pl.x + pl.w && feetOld <= pl.y + 1 && feet >= pl.y) {
                // Only a real landing (arriving from a fall) refreshes the patrol
                // budget. Gravity re-triggers this check every standing frame, so
                // resetting unconditionally would pin bounces at max and the mummy
                // would never run out of turns to walk off an edge.
                bool landedFromFall = e.phase == EP_FALL;
                e.y = pl.y - halfH; e.vy = 0;
                if (landedFromFall && pl.y < FLOOR_TOP - 1)
                    e.bounces = levelDef(g).bouncing;
                break;
            }
    }
    const SDL_FRect* ground = nullptr;                 // platform we now stand on
    for (const SDL_FRect& pl : g.platforms)
        if (e.x > pl.x && e.x < pl.x + pl.w && std::fabs((e.y + halfH) - pl.y) < 2.0f) {
            ground = &pl; break;
        }
    if (!ground) { e.phase = EP_FALL; return; }
    if (ground->y >= FLOOR_TOP - 1.0f) {
        // On bottom floor, vanish briefly, then transform into the next chaser.
        e.phase = EP_DISAPPEAR;
        e.timer = MUMMY_DISAPPEAR_TIME;
        e.vx = e.vy = 0.0f;
        return;
    }
    e.phase = EP_WALK;
    if (e.vx == 0) e.vx = MUMMY_WALK_SPEED;
    e.x += e.vx * dt;
    // Frame walls: the body radius (e.r) exceeds the foot half-width, so on a
    // platform that reaches the frame the leading foot can never pass the edge.
    // Detect the wall pin directly and turn around there — a wall has no edge to
    // fall off, so always reverse (consuming a bounce if any are left).
    const float wallL = BORDER_SOLID_X + e.r, wallR = LOGW - BORDER_SOLID_X - e.r;
    bool hitWall = (e.vx < 0 && e.x <= wallL) || (e.vx > 0 && e.x >= wallR);
    e.x = std::clamp(e.x, wallL, wallR);
    if (hitWall) {
        if (e.bounces > 0) e.bounces--;
        e.vx = -e.vx;
    } else {
        float ahead = e.x + (e.vx > 0 ? halfW : -halfW);   // leading foot
        bool atEdge = ahead <= ground->x || ahead >= ground->x + ground->w;
        // Patrol: reverse at an edge while bounces remain. Once they're spent,
        // keep walking off — the centre clears the platform and next frame's
        // centre-based ground check drops it into EP_FALL. (Reversing on the
        // foot but falling on the centre matches the arcade; nudging it a couple
        // of pixels here would just let the centre re-land and reset bounces.)
        if (atEdge && e.bounces > 0) { e.vx = -e.vx; e.bounces--; }
    }
}

// AABB overlap of a body of radius r centred at (x,y) with a platform rect.
inline bool circleOverlapsRect(float x, float y, float r, const SDL_FRect& pl) {
    return x + r > pl.x && x - r < pl.x + pl.w &&
           y + r > pl.y && y - r < pl.y + pl.h;
}

// Block a flying enemy against the frame borders and the platforms, bouncing it
// off the face it crossed so it can't pass through them (like Jack / the orb).
// (ox, oy) is the position before this step, used to pick the entry face.
void blockEnemy(Game& g, Enemy& e, float ox, float oy) {
    const float L = BORDER_SOLID_X + e.r, R = LOGW - BORDER_SOLID_X - e.r;
    const float T = BORDER_SOLID_Y + e.r, B = FLOOR_TOP - e.r;
    if (e.x < L) { e.x = L; e.vx = std::fabs(e.vx); }
    if (e.x > R) { e.x = R; e.vx = -std::fabs(e.vx); }
    if (e.y < T) { e.y = T; e.vy = std::fabs(e.vy); }
    if (e.y > B) { e.y = B; e.vy = -std::fabs(e.vy); }

    for (const SDL_FRect& pl : g.platforms) {
        if (pl.y >= FLOOR_TOP - 1.0f) continue;          // floor handled by B above
        if (!circleOverlapsRect(e.x, e.y, e.r, pl)) continue;

        const float eps = 0.001f;
        bool hitTop = oy + e.r <= pl.y + eps && e.y + e.r > pl.y + eps;
        bool hitBottom = oy - e.r >= pl.y + pl.h - eps && e.y - e.r < pl.y + pl.h - eps;
        if (hitTop) {
            e.y = pl.y - e.r;          e.vy = -std::fabs(e.vy);
        } else if (hitBottom) {
            e.y = pl.y + pl.h + e.r;   e.vy =  std::fabs(e.vy);
        } else {
            bool hitLeft = ox + e.r <= pl.x + eps && e.x + e.r > pl.x + eps;
            bool hitRight = ox - e.r >= pl.x + pl.w - eps && e.x - e.r < pl.x + pl.w - eps;
            if (hitLeft) {
                e.x = pl.x - e.r;          e.vx = -std::fabs(e.vx);
            } else if (hitRight) {
                e.x = pl.x + pl.w + e.r;   e.vx =  std::fabs(e.vx);
            } else if (oy <= pl.y) {       // corner: prefer the top face
                e.y = pl.y - e.r;          e.vy = -std::fabs(e.vy);
            } else if (ox <= pl.x) {
                e.x = pl.x - e.r;          e.vx = -std::fabs(e.vx);
            } else {
                e.x = pl.x + pl.w + e.r;   e.vx =  std::fabs(e.vx);
            }
        }
    }
}

// Transformed chasers: each kind flies and bounces off the walls differently.
// True if a body of radius r centred at (x,y) overlaps the frame or a platform.
// Used by the bird to probe a step before committing to it.
bool flyerBlocked(const Game& g, float r, float x, float y) {
    const float L = BORDER_SOLID_X + r, R = LOGW - BORDER_SOLID_X - r;
    const float T = BORDER_SOLID_Y + r, B = FLOOR_TOP - r;
    if (x < L || x > R || y < T || y > B) return true;
    for (const SDL_FRect& pl : g.platforms) {
        if (pl.y >= FLOOR_TOP - 1.0f) continue;        // bottom floor is the B edge
        if (circleOverlapsRect(x, y, r, pl)) return true;
    }
    return false;
}

// The mechanical bird: an obstacle-aware homing chaser. It re-acquires Jack's
// position every BIRD_DECIDE_FRAMES frames, then flies in a straight line toward
// that point — heading at Jack's true angle (genuine diagonals) instead of
// snapping to 8 compass directions, which off 45 degrees degrades into a mostly
// cardinal staircase. Platforms are solid: if the direct step is blocked it
// slides along the obstacle on whichever axis still makes progress.
void updateBird(Game& g, Enemy& e, float dt, float pcx, float pcy) {
    const float s = birdSpeed(g.level);

    if (++e.tick >= BIRD_DECIDE_FRAMES || (e.tgtX == 0.0f && e.tgtY == 0.0f)) {
        e.tick = 0;
        e.tgtX = pcx; e.tgtY = pcy;                    // re-acquire Jack
    }

    float dx = e.tgtX - e.x, dy = e.tgtY - e.y;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1.0f) { e.vx = e.vy = 0.0f; return; }    // basically on target
    const float ux = dx / len, uy = dy / len;          // unit heading toward Jack
    const float step = s * dt;

    const float nx = e.x + ux * step, ny = e.y + uy * step;
    if (!flyerBlocked(g, e.r, nx, ny)) {               // direct line is clear
        e.x = nx; e.y = ny;
        e.vx = ux * s; e.vy = uy * s;                  // heading drives the sprite
        return;
    }
    // Blocked: slide on a single axis, trying the dominant one first.
    const int sx = (dx > 0) - (dx < 0), sy = (dy > 0) - (dy < 0);
    int order[2][2] = {{sx, 0}, {0, sy}};
    if (std::fabs(dy) > std::fabs(dx)) {
        order[0][0] = 0;  order[0][1] = sy;
        order[1][0] = sx; order[1][1] = 0;
    }
    for (const auto& m : order) {
        if (m[0] == 0 && m[1] == 0) continue;
        const float tx = e.x + m[0] * step, ty = e.y + m[1] * step;
        if (!flyerBlocked(g, e.r, tx, ty)) {
            e.x = tx; e.y = ty;
            e.vx = m[0] * s; e.vy = m[1] * s;
            return;
        }
    }
    e.vx = ux * s; e.vy = uy * s;                      // boxed in: hold, keep facing Jack
}

void updateFlyer(Game& g, Enemy& e, float dt, float pcx, float pcy) {
    float s = FLY_SPEED + g.level * 2.0f;
    switch (e.kind) {
        case EK_BIRD:
            updateBird(g, e, dt, pcx, pcy);
            return;                                     // does its own movement + collision
        case EK_SPHERE:                                // homes on X, bounces on Y
            e.vx = std::clamp(e.vx + (pcx > e.x ? 1 : -1) * s*2*dt, -s*0.7f, s*0.7f); break;
        case EK_ORB:                                   // homes on Y, bounces on X
            e.vy = std::clamp(e.vy + (pcy > e.y ? 1 : -1) * s*2*dt, -s*0.7f, s*0.7f); break;
        case EK_CLUB:                                  // homes on both axes
            e.vx = std::clamp(e.vx + (pcx > e.x ? 1 : -1) * s*1.5f*dt, -s*0.6f, s*0.6f);
            e.vy = std::clamp(e.vy + (pcy > e.y ? 1 : -1) * s*1.5f*dt, -s*0.6f, s*0.6f); break;
        case EK_HORN: break;                           // constant diagonal drift
        case EK_UFO: {                                 // straight at Jack, slowing when near
            float dx = pcx - e.x, dy = pcy - e.y, len = std::sqrt(dx*dx + dy*dy) + 1e-3f;
            float k = (len < 70 ? 0.4f : 1.0f) * s;
            e.vx = dx/len * k; e.vy = dy/len * k; break;
        }
    }
    float ox = e.x, oy = e.y;
    e.x += e.vx * dt; e.y += e.vy * dt;
    blockEnemy(g, e, ox, oy);   // borders + platforms, like Jack
}

// ---------------------------------------------------------------------------
// Simulation
// ---------------------------------------------------------------------------

// The next bomb to light after one is caught: the lowest-order remaining bomb
// with order >= orderMin, wrapping to the lowest remaining if none qualify.
// Bombs are stored sorted by lit-up order (index == order), mirroring the
// reference getNextBombToActivate. Returns -1 when the field is clear.
int nextLitBomb(const Game& g, int orderMin) {
    for (int j = orderMin; j < (int)g.bombs.size(); ++j)
        if (!g.bombs[j].collected) return j;
    for (int j = 0; j < (int)g.bombs.size(); ++j)
        if (!g.bombs[j].collected) return j;
    return -1;
}

// Special-bonus payout for the fire-bomb count (reference score.lua table).
int specialBonusFor(int catched) {
    switch (catched) {
        case 20: return 10000;
        case 21: return 20000;
        case 22: return 30000;
        case 23: return 50000;
        default: return 0;
    }
}

// All bombs cleared: award the Special Bonus (count-up screen) when enough fire
// bombs were caught, otherwise go straight to the round-clear dance.
void finishLevel(Game& g) {
    int bonus = specialBonusFor(g.catched);
    if (bonus > 0) {
        g.state = SPECIALBONUS;
        g.sbState = 0;
        g.sbTimer = SB_BEGIN_TIME;
        g.sbRemaining = bonus;
        g.sbCatched = g.catched;
    } else {
        g.state = ROUNDCLEAR;
        g.clearTimer = CLEAR_TIME;
    }
}

void updatePlaying(Game& g, const Input& in, float dt) {
    Player& p = g.p;
    if (g.playerDying) {
        if (g.deathPhase == DP_DANCING) {
            g.deathAnim += dt;
            const float step = DEATH_DANCE_TOTAL / 3.0f;
            while (g.deathAnim >= step) {
                g.deathAnim -= step;
                g.deathFrame = (g.deathFrame + 1) % 3;
                if (g.deathFrame == 0) {
                    g.deathLoops++;
                    if (g.deathLoops >= DEATH_DANCE_LOOPS) {
                        g.deathPhase = DP_FALLING;
                        g.deathFrame = 0;
                        g.deathAnim = 0.0f;
                        p.vx = 0.0f;
                        p.vy = 0.0f;
                        break;
                    }
                }
            }
        } else if (g.deathPhase == DP_FALLING) {
            g.deathAnim += dt;
            const float step = DEATH_PLF_TOTAL / 4.0f;
            while (g.deathAnim >= step) {
                g.deathAnim -= step;
                g.deathFrame = (g.deathFrame + 1) % 4;
            }
            p.vy += GRAVITY * dt;
            if (p.vy > MAXFALL) p.vy = MAXFALL;
            p.y += p.vy * dt;
            if (p.y + PH >= FLOOR_TOP) {
                p.y = FLOOR_TOP - PH;
                p.vy = 0.0f;
                p.onGround = true;
                g.deathPhase = DP_DEAD;
                g.deathFrame = 0;
                g.deathAnim = 0.0f;
            } else {
                p.onGround = false;
            }
        } else if (g.deathPhase == DP_DEAD) {
            g.deathAnim += dt;
            const float step = DEATH_DEAD_TOTAL / 4.0f;
            while (g.deathAnim >= step) {
                g.deathAnim -= step;
                if (g.deathFrame < 3) g.deathFrame++;
                else {
                    g.deathPhase = DP_WAIT;
                    g.deathTimer = DEATH_WAIT_TIME;
                    break;
                }
            }
        } else if (g.deathPhase == DP_WAIT) {
            g.deathTimer -= dt;
            if (g.deathTimer <= 0.0f) {
                g.playerDying = false;
                g.deathPhase = DP_NONE;
                g.lives--;
                g.livesLost++;                          // brings the next E forward
                if (g.lives <= 0) {
                    g.state = GAMEOVER;
                    // Spawn the GAME OVER sprite where Jack died (world -> screen
                    // px), then it slides to mid-height and on to the centre.
                    g.goX = (p.x + PW / 2) * (float)GAME_W / LOGW;
                    g.goY = HUD_H + (p.y + PH / 2) * (float)GAME_H / LOGH;
                    g.goStage = 0;
                    g.goTimer = GAMEOVER_HOLD;
                }
                else {                                  // new life: fresh enemies + intro
                    resetPlayer(g, true);
                    restartEnemies(g);
                    g.startAnim = START_TOTAL;
                    g.birdSpawnPending = true;
                    g.spawnHoldX = g.spawnHoldY = 0;
                }
            }
        }
        return;
    }
    // Hold the simulation while the "START!" intro slides in and settles.
    if (g.startAnim > 0.0f) {
        g.startAnim -= dt;
        // Latch the last direction held during the intro: it forces where the
        // bird appears (and naturally carries into Jack's first move on resume).
        if (in.left || in.right || in.up || in.down) {
            g.spawnHoldX = (in.right ? 1 : 0) - (in.left ? 1 : 0);
            g.spawnHoldY = (in.down ? 1 : 0) - (in.up ? 1 : 0);
        }
        return;
    }
    if (g.birdSpawnPending) {            // intro just ended: place the bird from the hold
        g.birdSpawnPending = false;
        placeBirdFromHold(g);
    }
    if (p.invuln > 0) p.invuln -= dt;
    const bool oldOnGround = p.onGround;
    // The Power orb's colour steps once per "action": a jump, a wall bump, or
    // landing/headbutting a barrier. Collected into orbStep below.
    bool orbStep = in.jumpPressed;

    // Horizontal movement.
    p.vx = (in.right ? MOVE : 0.0f) - (in.left ? MOVE : 0.0f);
    if (p.vx > 0) p.face = 1;
    else if (p.vx < 0) p.face = -1;
    p.x += p.vx * dt;
    float clampedX = std::clamp(p.x, BORDER_SOLID_X, LOGW - PW - BORDER_SOLID_X);
    if (clampedX != p.x && p.vx != 0.0f) orbStep = true;   // ran into a side wall
    p.x = clampedX;

    // Jump / flutter.
    if (in.jumpPressed) {
        if (p.onGround) {
            p.vy = JUMP_VEL;
            p.onGround = false;
        } else {
            p.vy -= FLUTTER;
            if (p.vy < FLUTTER_MIN) p.vy = FLUTTER_MIN;
        }
    }

    // Gravity (gentler while gliding down with jump held).
    float grav = (in.jumpHeld && p.vy > 0) ? GLIDE : GRAVITY;
    p.vy += grav * dt;
    if (p.vy > MAXFALL) p.vy = MAXFALL;

    float oldTop = p.y;
    float oldBottom = p.y + PH;
    p.y += p.vy * dt;
    if (p.y < BORDER_SOLID_Y) {
        p.y = BORDER_SOLID_Y;
        if (p.vy < 0) p.vy = 0;
    }

    // Solid platforms: collide with both top and bottom faces.
    p.onGround = false;
    for (const SDL_FRect& pl : g.platforms) {
        bool overlapX = (p.x + PW - 3 > pl.x) && (p.x + 3 < pl.x + pl.w);
        if (!overlapX) continue;
        if (p.vy >= 0 && oldBottom <= pl.y + 1.0f && p.y + PH >= pl.y) {
                p.y = pl.y - PH;
                p.vy = 0;
                p.onGround = true;
                break;
        }
        if (p.vy < 0 && oldTop >= pl.y + pl.h - 1.0f && p.y <= pl.y + pl.h) {
            p.y = pl.y + pl.h;
            p.vy = 0;
            orbStep = true;                          // bonked a barrier from below
            break;
        }
    }
    if (!oldOnGround && p.onGround) orbStep = true;  // landed on a barrier
    // Step the Power orb's colour on this action (fixed cycle, not random).
    if (g.orbActive && orbStep) g.orbFamily = (g.orbFamily + 1) % 7;

    // Bomb collection. The arcade lights bombs in a fixed order: nothing is lit
    // until the first bomb is grabbed, then catching the lit ("fire") bomb lights
    // the next in sequence. A fire bomb scores 200 (vs 100) and counts toward the
    // end-of-level Special Bonus; grabbing an unlit bomb breaks the chain but
    // leaves the lit one lit. (Mirrors bombjack.lua checkCollisionWithBombs.)
    for (int i = 0; i < (int)g.bombs.size(); ++i) {
        Bomb& b = g.bombs[i];
        if (b.collected) continue;
        if (b.x > p.x - BOMB_HALF_W && b.x < p.x + PW + BOMB_HALF_W &&
            b.y > p.y - BOMB_HALF_H && b.y < p.y + PH + BOMB_HALF_H) {
            b.collected = true;
            g.bombsLeft--;
            g.explosions.push_back({b.x, b.y, 0.0f});   // quick clear burst
            const bool wasLit  = (i == g.litBomb);
            const bool relight = (g.litBomb < 0) || wasLit;  // first grab, or grabbed the lit one
            int gain;
            if (wasLit) {                         // a "fire" bomb -> feeds the Special Bonus
                g.catched++;
                gain = 200;
                g.powerMeter += 1.0f;             // lit bombs charge the P orb faster
            } else {
                gain = 100;
                g.powerMeter += 0.5f;
            }
            if (relight) g.litBomb = nextLitBomb(g, i + 1);
            gain *= g.multiplier;
            g.score += gain;
            g.popups.push_back({b.x, b.y - 6, 0.0f, gain});
            // Enough charge spawns a Power orb (once, while none is active).
            if (!g.orbActive && g.freezeTimer <= 0 && g.powerMeter >= POWER_NEEDED) {
                g.powerMeter -= POWER_NEEDED;
                g.orbActive = true;
                g.orbX = LOGW / 2.0f;             // appears at the centre of the
                g.orbY = LOGH / 2.0f - 16.0f;     // play area (as in the original)
                g.orbVx = ORB_SPEED * 0.5f;
                g.orbVy = ORB_SPEED * 0.8660254f;
                g.orbFamily = 0;                    // starts blue; cycles on Jack's actions
            }
        }
    }

    // Move the orb, bouncing it around the play area.
    if (g.orbActive) {
        float oldOrbX = g.orbX;
        float oldOrbY = g.orbY;
        g.orbX += g.orbVx * dt;
        g.orbY += g.orbVy * dt;
        // Bounce off the inner frame border (same edges the flyers use), not the
        // raw world edge — otherwise the orb slides behind the frame/banner.
        const float orbL = BORDER_SOLID_X + ORB_R;
        const float orbRgt = LOGW - BORDER_SOLID_X - ORB_R;
        const float orbT = BORDER_SOLID_Y + ORB_R;
        if (g.orbX < orbL)   { g.orbX = orbL;   g.orbVx = std::fabs(g.orbVx); }
        if (g.orbX > orbRgt) { g.orbX = orbRgt; g.orbVx = -std::fabs(g.orbVx); }
        if (g.orbY < orbT)   { g.orbY = orbT;   g.orbVy = std::fabs(g.orbVy); }
        if (g.orbY > FLOOR_TOP - ORB_R) {
            g.orbY = FLOOR_TOP - ORB_R;
            g.orbVy = -std::fabs(g.orbVy);
        }

        // Bounce on platform faces (excluding the ground platform handled above).
        for (const SDL_FRect& pl : g.platforms) {
            if (pl.y >= FLOOR_TOP - 1.0f) continue;
            if (!circleOverlapsRect(g.orbX, g.orbY, ORB_R, pl)) continue;

            // Match the Lua collision feel: resolve vertical faces first, then sides.
            const float eps = 0.001f;
            bool hitTop = oldOrbY + ORB_R <= pl.y + eps && g.orbY + ORB_R > pl.y + eps;
            bool hitBottom = oldOrbY - ORB_R >= pl.y + pl.h - eps &&
                             g.orbY - ORB_R < pl.y + pl.h - eps;
            if (hitTop) {
                g.orbY = pl.y - ORB_R;
                g.orbVy = -std::fabs(g.orbVy);
            } else if (hitBottom) {
                g.orbY = pl.y + pl.h + ORB_R;
                g.orbVy = std::fabs(g.orbVy);
            } else {
                bool hitLeft = oldOrbX + ORB_R <= pl.x + eps && g.orbX + ORB_R > pl.x + eps;
                bool hitRight = oldOrbX - ORB_R >= pl.x + pl.w - eps &&
                                g.orbX - ORB_R < pl.x + pl.w - eps;
                if (hitLeft) {
                    g.orbX = pl.x - ORB_R;
                    g.orbVx = -std::fabs(g.orbVx);
                } else if (hitRight) {
                    g.orbX = pl.x + pl.w + ORB_R;
                    g.orbVx = std::fabs(g.orbVx);
                } else if (std::fabs(g.orbVy) >= std::fabs(g.orbVx) && oldOrbY <= pl.y) {
                    // Corner/large-step overlap: allow vertical correction only
                    // for top-face contacts. Bottom-face bounce is crossing-only.
                    g.orbY = pl.y - ORB_R;
                    g.orbVy = -std::fabs(g.orbVy);
                } else {
                    // Fallback to horizontal correction to avoid false underside bounces.
                    if (oldOrbX <= pl.x) {
                        g.orbX = pl.x - ORB_R;
                        g.orbVx = -std::fabs(g.orbVx);
                    } else {
                        g.orbX = pl.x + pl.w + ORB_R;
                        g.orbVx = std::fabs(g.orbVx);
                    }
                }
            }
            break;
        }
        // The orb keeps the colour it spawned with — only its shades cycle.
    }

    // Power orb pickup -> freeze every enemy and make them killable for a while.
    if (g.orbActive) {
        if (g.orbX > p.x - ORB_R && g.orbX < p.x + PW + ORB_R &&
            g.orbY > p.y - ORB_R && g.orbY < p.y + PH + ORB_R) {
            g.orbActive = false;
            g.freezeTimer = FREEZE_TIME;
            g.killCount = 0;
            int idx = (g.orbFamily % (int)std::size(POWER_POINTS) +
                       (int)std::size(POWER_POINTS)) % (int)std::size(POWER_POINTS);
            g.freezeColor = POWER_COLORS[idx];   // Jack takes the orb's colour
            int gain = POWER_POINTS[idx] * g.multiplier;
            g.score += gain;
            g.popups.push_back({g.orbX, g.orbY, 0.0f, gain});
        }
    }

    // Every BONUS_LIMIT points, offer a bonus coin (while the field is clear of
    // one). Most are B (capped to the multiplier), but the slot may instead carry
    // a rare S jackpot or an E extra-life once enough B coins are in. The
    // threshold always advances so it never sticks even when nothing is dropped.
    if (g.score >= g.nextBonusScore) {
        g.nextBonusScore += BONUS_LIMIT;
        if (!g.bonusActive && g.freezeTimer <= 0.0f) {
            int kind = -1;
            if ((int)(g.rng() % 100) < BONUS_S_CHANCE)           kind = BK_S;
            else if (g.bCoins + g.livesLost >= g.nextEAt)        kind = BK_E;
            else if (g.multiplier < MAX_MULT)                    kind = BK_B;
            if (kind >= 0) spawnBonus(g, kind);
        }
    }

    // Active bonus coin: patrol its platform, and apply its effect on pickup.
    if (g.bonusActive) {
        g.bonusAnim += dt;
        g.bonusX += g.bonusVx * dt;
        const float lo = g.bonusPlatX + BONUS_W * 0.5f;
        const float hi = g.bonusPlatX + g.bonusPlatW - BONUS_W * 0.5f;
        if (g.bonusX < lo) { g.bonusX = lo; g.bonusVx = std::fabs(g.bonusVx); }
        if (g.bonusX > hi) { g.bonusX = hi; g.bonusVx = -std::fabs(g.bonusVx); }

        if (g.bonusX > p.x - BONUS_W * 0.5f && g.bonusX < p.x + PW + BONUS_W * 0.5f &&
            g.bonusY > p.y - BONUS_H * 0.5f && g.bonusY < p.y + PH + BONUS_H * 0.5f) {
            g.bonusActive = false;
            g.bonusTakens.push_back({g.bonusX, g.bonusY, 0.0f});   // collect flash
            switch (g.bonusKind) {
                case BK_B: {
                    int gain = BONUS_POINTS * g.multiplier;
                    g.score += gain;
                    g.popups.push_back({g.bonusX, g.bonusY - 6, 0.0f, gain});
                    g.multiplier = std::min(g.multiplier + 1, MAX_MULT);
                    g.bCoins++;
                    break;
                }
                case BK_E:                                  // extra life
                    g.lives++;
                    g.nextEAt += BONUS_E_EVERY;             // raise the bar for the next E
                    break;
                case BK_S: {                                // special jackpot + stage skip
                    int gain = BONUS_S_POINTS * g.multiplier;
                    g.score += gain;
                    g.popups.push_back({g.bonusX, g.bonusY - 6, 0.0f, gain});
                    g.lives++;                              // "free credit" ~= an extra life
                    g.state = ROUNDCLEAR;                   // advance to the next stage
                    g.clearTimer = CLEAR_TIME;
                    break;
                }
            }
        }
    }

    // Age out floating score popups.
    for (auto it = g.popups.begin(); it != g.popups.end();) {
        it->age += dt;
        it->y -= 18.0f * dt;
        if (it->age > 1.0f) it = g.popups.erase(it); else ++it;
    }

    // Age out coin-pickup sparkles (4 quick frames, then gone).
    for (auto it = g.coinPickups.begin(); it != g.coinPickups.end();) {
        it->age += dt;
        if (it->age >= 4 * PICKCOIN_FRAME) it = g.coinPickups.erase(it); else ++it;
    }

    // Age out bonus collect flashes (6 quick frames, then gone).
    for (auto it = g.bonusTakens.begin(); it != g.bonusTakens.end();) {
        it->age += dt;
        if (it->age >= 6 * BONUSTAKEN_FRAME) it = g.bonusTakens.erase(it); else ++it;
    }

    // Age out clear explosions (3 quick frames, then gone).
    for (auto it = g.explosions.begin(); it != g.explosions.end();) {
        it->age += dt;
        if (it->age >= 3 * EXPL_FRAME) it = g.explosions.erase(it); else ++it;
    }

    if (g.bombsLeft <= 0) {
        finishLevel(g);            // Special Bonus screen, or straight to round-clear
        return;
    }

    // While frozen, enemies hold still and any touch kills them for points.
    const bool frozen = g.freezeTimer > 0.0f;
    if (frozen) g.freezeTimer -= dt;

    // Drop a fresh mummy in every so often, up to this level's quota.
    if (!frozen) {
        const leveldata::LevelDef& L = levelDef(g);
        g.mummyTimer += dt;
        if (g.mummyTimer >= MUMMY_SPAWN_DELAY && g.mummiesSpawned < L.nmummies) {
            g.mummyTimer = 0.0f;
            g.mummiesSpawned++;
            spawnMummy(g);
        }
    }

    float pcx = p.x + PW / 2, pcy = p.y + PH / 2;
    for (auto it = g.enemies.begin(); it != g.enemies.end();) {
        Enemy& e = *it;
        if (!frozen) {
            if (e.kind == EK_MUMMY) updateMummy(g, e, dt);
            else                    updateFlyer(g, e, dt, pcx, pcy);
        }

        // Intangible while flashing in / appearing / disappearing. But once
        // frozen these phases are drawn as collectible coins (only EP_INIT keeps
        // its spawn flash), so let Jack grab them — otherwise a mummy frozen
        // mid drop-in shows a coin that can't be taken and never advances.
        bool intangible = e.phase == EP_INIT ||
            (!frozen && (e.phase == EP_APPEAR || e.phase == EP_DISAPPEAR));
        if (intangible) {
            ++it;
            continue;
        }

        float ex = pcx - e.x, ey = pcy - e.y;
        bool touching = ex * ex + ey * ey < (e.r + 9.0f) * (e.r + 9.0f);
        if (frozen) {
            if (touching) {                             // kill the frozen chaser
                int idx = std::min(g.killCount, (int)(std::size(KILL_POINTS)) - 1);
                int gain = KILL_POINTS[idx] * g.multiplier;
                g.score += gain;
                g.popups.push_back({e.x, e.y - 6, 0.0f, gain});
                g.coinPickups.push_back({e.x, e.y, 0.0f});   // pickup sparkle
                g.killCount++;
                it = g.enemies.erase(it);
                continue;
            }
        } else if (p.invuln <= 0 && touching) {
            g.playerDying = true;
            g.deathPhase = DP_DANCING;
            g.deathFrame = 0;
            g.deathLoops = 0;
            g.deathAnim = 0.0f;
            g.deathTimer = 0.0f;
            p.invuln = 0.0f;
            p.onGround = false;
            p.vx = 0.0f;
            p.vy = 0.0f;
            return;
        }
        ++it;
    }
}

void update(Game& g, const Input& in, float dt) {
    g.time += dt;
    switch (g.state) {
        case TITLE:
            if (in.jumpPressed) startGame(g);
            break;
        case PLAYING:
            updatePlaying(g, in, dt);
            break;
        case ROUNDCLEAR:
            g.clearTimer -= dt;
            if (g.clearTimer <= 0) {
                g.level++;
                startRound(g);
                g.state = PLAYING;
            }
            break;
        case SPECIALBONUS:
            // Three phases: hold the box, count the bonus up onto the score
            // 1000 at a time, hold again, then advance to the next round.
            g.sbTimer -= dt;
            if (g.sbState == 0) {                        // begin hold
                if (g.sbTimer <= 0) { g.sbState = 1; g.sbTimer = SB_SCORE_TIME; }
            } else if (g.sbState == 1) {                 // count-up
                while (g.sbState == 1 && g.sbTimer <= 0) {
                    g.sbRemaining -= 1000;
                    g.score += 1000;
                    if (g.sbRemaining <= 0) { g.sbState = 2; g.sbTimer += SB_END_TIME; }
                    else                    { g.sbTimer += SB_SCORE_TIME; }
                }
            } else {                                     // end hold
                if (g.sbTimer <= 0) {
                    g.level++;
                    startRound(g);
                    g.state = PLAYING;
                }
            }
            break;
        case GAMEOVER: {
            const float cx = SCREEN_W / 2.0f;            // centre x
            const float cy = SCREEN_H / 2.0f;            // mid-height of the play view
            const float move = GAMEOVER_SLIDE * dt;
            if (g.goStage == 0) {                        // first: up/down to mid-height
                if (g.goY < cy) g.goY = std::min(cy, g.goY + move);
                else            g.goY = std::max(cy, g.goY - move);
                if (g.goY == cy) g.goStage = 1;
            } else if (g.goStage == 1) {                 // then: across to the centre
                if (g.goX < cx) g.goX = std::min(cx, g.goX + move);
                else            g.goX = std::max(cx, g.goX - move);
                if (g.goX == cx) g.goStage = 2;
            } else {                                     // hold, then back to attract title
                g.goTimer -= dt;
                if (g.goTimer <= 0.0f) g.state = TITLE;
            }
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------
void drawBomb(SDL_Renderer* r, const Bomb& b, bool lit, float t) {
    // Keep render size consistent with collision extents. No SPRITE_AR widening
    // here: the bomb is left to the screen's 3:4 squeeze (drawn narrow/tall).
    const float dw = BOMB_HALF_W * 2.0f;
    const float dh = BOMB_HALF_H * 2.0f;
    // Frame 0 is the resting bomb; the lit (active) bomb cycles frames 1-6.
    // Original bomb_activated has 6 frames over 0.3s -> 0.05s per frame.
    int frame = lit ? 1 + (int)(std::fmod(t, 0.3f) / 0.05f) % 6 : 0;
    SDL_Texture* tex = g_bombFrames[frame].tex;
    if (tex) {
        SDL_FRect dst{b.x - dw / 2, b.y - dh / 2, dw, dh};
        SDL_RenderTexture(r, tex, nullptr, &dst);
    }
}

void drawPlayer(SDL_Renderer* r, const Player& p, float t, bool dying,
                int deathPhase, int deathFrame, bool frozen, Color freezeColor) {
    if (dying) {
        const JackVarFrame* fr = nullptr;
        if (deathPhase == DP_DANCING) fr = &g_jackDance[std::clamp(deathFrame, 0, 2)];
        else if (deathPhase == DP_FALLING) fr = &g_jackPlf[std::clamp(deathFrame, 0, 3)];
        else if (deathPhase == DP_DEAD || deathPhase == DP_WAIT)
            fr = &g_jackDead[std::clamp(deathFrame, 0, 3)];
        if (fr && fr->tex) {
            const float scale = 26.0f / 16.0f;
            const float dw = fr->w * scale * SPRITE_AR;
            const float dh = fr->h * scale;
            SDL_FRect dst{p.x + PW / 2 - dw / 2, p.y + PH - dh, dw, dh};
            SDL_RenderTexture(r, fr->tex, nullptr, &dst);
        }
        return;
    }
    if (p.invuln > 0 && std::fmod(t, 0.16f) < 0.08f) return;  // blink when hit
    const bool moving = std::fabs(p.vx) > 1.0f;
    const bool left   = p.face < 0;
    int f;
    if (p.onGround) {
        if (moving) {
            int step = (int)(t / JACK_WALK_FRAME) % 4;        // 4-frame cycle
            f = (left ? JF_WALK_L0 : JF_WALK_R0) + step;
        } else {
            f = JF_IDLE;
        }
    } else if (p.vy < 0.0f) {                                 // rising -> flying
        f = moving ? (left ? JF_FLY_L : JF_FLY_R) : JF_FLY;
    } else {                                                  // descending -> falling
        f = moving ? (left ? JF_FALL_L : JF_FALL_R) : JF_FALL;
    }
    // Draw bigger than the collision box (feet anchored to its bottom) so Jack
    // reads at a similar scale to the bombs and chasers. The frames are already
    // drawn facing the right way, so no horizontal flip is needed.
    const float dw = 26.0f * SPRITE_AR, dh = 28.0f;
    SDL_FRect dst{p.x + PW / 2 - dw / 2, p.y + PH - dh, dw, dh};
    // While enemies are frozen, Jack is monochrome in the grabbed orb's colour
    // with his shades cycling, so he shimmers in that colour.
    if (frozen && g_jackPhase[f][0]) {
        int ph = (int)(t / 0.1f) & 3;                 // rotate the shades ~10/s
        SDL_Texture* tex = g_jackPhase[f][ph];
        SDL_SetTextureColorMod(tex, freezeColor.r, freezeColor.g, freezeColor.b);
        SDL_RenderTexture(r, tex, nullptr, &dst);
        SDL_SetTextureColorMod(tex, 255, 255, 255);
    } else if (g_jackTex[f]) {
        SDL_RenderTexture(r, g_jackTex[f], nullptr, &dst);
    }
}

// Jack's round-clear victory dance: step through the fixed VICTORY_SEQ over the
// clear banner, drawn at the same scale/anchor as the live Jack.
void drawJackVictory(SDL_Renderer* r, const Player& p, float clearTimer) {
    float elapsed = CLEAR_TIME - clearTimer;
    int step = std::clamp((int)(elapsed / VICTORY_FRAME), 0, VICTORY_STEPS - 1);
    const JackVarFrame& fr = g_jackWin[VICTORY_SEQ[step]];
    if (!fr.tex) return;
    const float scale = 26.0f / 16.0f;
    const float dw = fr.w * scale * SPRITE_AR, dh = fr.h * scale;
    SDL_FRect dst{p.x + PW / 2 - dw / 2, p.y + PH - dh, dw, dh};
    SDL_RenderTexture(r, fr.tex, nullptr, &dst);
}

// Pick the sprite + draw box for an enemy based on its kind and phase.
SpriteId enemySprite(const Enemy& e, float t, float& w, float& h) {
    bool a = std::fmod(t, 0.3f) < 0.15f;       // 2-frame animation toggle
    w = 28; h = 24;
    switch (e.kind) {
        case EK_MUMMY:
            w = 26; h = 28;
            if (e.phase == EP_FALL) return SP_MUMMY_FALL;
            if (e.phase == EP_WALK) return a ? SP_MUMMY_WALK : SP_MUMMY;
            return SP_MUMMY;                    // appearing / idle
        case EK_SPHERE: w = h = 26; return a ? SP_SPHERE1 : SP_SPHERE2;
        case EK_ORB:    w = h = 26; return a ? SP_ORB1 : SP_ORB2;
        case EK_HORN:   w = h = 26; return a ? SP_HORN1 : SP_HORN2;
        case EK_CLUB:   w = h = 26; return a ? SP_CLUB1 : SP_CLUB2;
        case EK_UFO:    w = 28; h = 20; return a ? SP_UFO1 : SP_UFO2;
        default:        return a ? SP_ENEMY1 : SP_ENEMY2;   // bird
    }
}

// The bird flaps through a 3-frame cycle for its current heading. Its body
// keeps its natural (grey) colours; only the eyes pulse red, mirroring the
// original (bird.lua).
void drawBird(SDL_Renderer* r, const Enemy& e, float t) {
    int step = (int)(t / BIRD_FLAP_FRAME) % 3;
    int base;
    if (std::fabs(e.vx) >= std::fabs(e.vy))
        base = (e.vx < 0) ? BF_LEFT0 : BF_RIGHT0;   // moving horizontally
    else
        base = BF_VERT0;                            // moving vertically
    SDL_Texture* body = g_birdTex[base + step];
    SDL_Texture* eye  = g_birdEye[base + step];
    if (!body) return;
    const float w = 26.0f * SPRITE_AR, h = 24.0f;
    SDL_FRect dst{e.x - w / 2, e.y - h / 2, w, h};
    drawTexTinted(r, body, dst, false, 255, 255, 255);
    if (eye) drawTexTinted(r, eye, dst, false, eyePulse(t), 0, 0);
}

// The mummy walks a 3-frame cycle (directional), with distinct idle and falling
// poses, mirroring the original (mummy.lua). Body keeps its colours; eyes pulse.
void drawMummy(SDL_Renderer* r, const Enemy& e, float t) {
    int frame;
    if (e.phase == EP_FALL) {
        frame = MF_FALL;
    } else if (e.phase == EP_WALK && std::fabs(e.vx) > 1.0f) {
        int step = (int)(t / MUMMY_WALK_FRAME) % 3;
        frame = (e.vx < 0 ? MF_WALK_L0 : MF_WALK_R0) + step;
    } else {
        frame = MF_IDLE;                            // appearing / standing still
    }
    SDL_Texture* body = g_mummyTex[frame];
    SDL_Texture* eye  = g_mummyEye[frame];
    if (!body) return;
    const float w = 26.0f * SPRITE_AR, h = 28.0f;
    SDL_FRect dst{e.x - w / 2, e.y - h / 2, w, h};
    drawTexTinted(r, body, dst, false, 255, 255, 255);
    if (eye) drawTexTinted(r, eye, dst, false, eyePulse(t), 0, 0);
}

// The spawn flash: the 4 InitEnemy frames play once over INIT_ENEMY_TIME, tinted
// to this spawn's random basic colour, where the mummy is about to appear.
void drawInitEnemy(SDL_Renderer* r, const Enemy& e) {
    float prog = 1.0f - std::clamp(e.timer / INIT_ENEMY_TIME, 0.0f, 1.0f);
    int frame = std::min(3, (int)(prog * 4));
    SDL_Texture* tex = g_initEnemyTex[frame];
    if (!tex) return;
    const float w = INIT_FW * SPRITE_AR, h = (float)INIT_FH;
    SDL_FRect dst{e.x - w / 2, e.y - h / 2, w, h};
    drawTexTinted(r, tex, dst, false, e.spawnTint.r, e.spawnTint.g, e.spawnTint.b);
}

void drawEnemy(SDL_Renderer* r, const Enemy& e, float t, bool frozen,
               float freezeTimer) {
    if (e.phase == EP_INIT) { drawInitEnemy(r, e); return; }
    // Mummies flash white as they pop in; flyers blink as the freeze wears off.
    if (e.phase == EP_APPEAR && std::fmod(t, 0.12f) < 0.06f) return;
    if (frozen && freezeTimer < 1.0f && std::fmod(t, 0.16f) < 0.08f) return;
    if (frozen) {
        // Grabbing the P turns the chasers into spinning collectible coins.
        const Sprite& c = g_coinFrames[(int)(t * 10.0f) % 7];
        if (c.tex) {
            const float w = 24.0f * SPRITE_AR, h = 24.0f;
            SDL_FRect dst{e.x - w / 2, e.y - h / 2, w, h};
            SDL_RenderTexture(r, c.tex, nullptr, &dst);
        }
        return;
    }
    if (e.kind == EK_BIRD)  { drawBird(r, e, t);  return; }
    if (e.kind == EK_MUMMY) { drawMummy(r, e, t); return; }
    float w, h;
    SpriteId id = enemySprite(e, t, w, h);
    w *= SPRITE_AR;
    bool flip = e.vx < 0;
    drawSprite(r, id, e.x - w / 2, e.y - h / 2, w, h, flip);
}

// The Power orb: a pulsing, colour-cycling ball. Grab it to freeze the chasers.
void drawPowerOrb(SDL_Renderer* r, float x, float y, float t, int family) {
    int fam = (family % 7 + 7) % 7;
    int phase = (int)(t / 0.1f) & 3;                   // cycle the 4 shades ~10/s
    const Sprite& f = g_orbCycle[fam][phase];
    if (f.tex) {
        const float w = 28.8f * SPRITE_AR, h = 28.8f;  // 20% larger than the coin
        SDL_FRect dst{x - w / 2, y - h / 2, w, h};      // drawn upright, no rotation
        SDL_RenderTexture(r, f.tex, nullptr, &dst);
    }
}

// Draw the backdrop the current level's layout calls for (screen is 0-based).
void drawBackground(SDL_Renderer* r, int screen) {
    setCol(r, {18, 16, 38});
    SDL_RenderClear(r);
    if (g_bgTex && g_bgCount > 0) {
        int idx = screen % g_bgCount;
        if (idx < 0) idx += g_bgCount;
        SDL_FRect src{(float)(idx * BG_W), 0, (float)BG_W, (float)BG_H};
        SDL_FRect dst{0, 0, (float)LOGW, (float)LOGH};
        SDL_RenderTexture(r, g_bgTex, &src, &dst);
    }
}

// HUD strips drawn in raw screen pixels (224 wide, 16 tall each). Top strip:
// running score (left) and the power gauge (right). Bottom strip mirrors the
// arcade: Jack life icons in the bottom-left, then ROUND / -N- and HI-SCORE /
// score (both two-line, centered) toward the right.
void drawHud(SDL_Renderer* r, const Game& g) {
    char buf[64];
    setCol(r, {0, 0, 0});
    fillR(r, 0, 0, SCREEN_W, HUD_H);                 // top strip
    fillR(r, 0, HUD_H + GAME_H, SCREEN_W, HUD_H);    // bottom strip

    // --- Top strip: current-phase points (left) + power gauge (right). ---
    // SIDE-ONE label (yellow) over the points earned in the current phase
    // (white), stacked two rows in the top-left corner.
    const float sideX = 3.0f;
    drawText(r, "SIDE-ONE", sideX, 1, 1, {255, 230, 60});
    std::snprintf(buf, sizeof(buf), "%d", g.score - g.phaseStart);  // no leading 0s
    const float sideRight = sideX + textWidth("SIDE-ONE", 1);
    drawText(r, buf, sideRight - textWidth(buf, 1), 9, 1, {255, 255, 255});
    {
        // Points multiplier, drawn as the arcade's boxed "x" + digit (x1..x5).
        int m = std::clamp(g.multiplier, 1, 5);
        if (g_multTex[0] && g_multTex[m]) {
            SDL_FRect xbox{96, 2, 12, 12};   SDL_RenderTexture(r, g_multTex[0], nullptr, &xbox);
            SDL_FRect dbox{108, 2, 12, 12};  SDL_RenderTexture(r, g_multTex[m], nullptr, &dbox);
        }
    }
    {
        // POWER gauge — fills as bombs are caught; a full bar spawns a P orb.
        float frac = std::min(g.powerMeter / POWER_NEEDED, 1.0f);
        const float gx = SCREEN_W - 64;
        setCol(r, {40, 40, 70});    fillR(r, gx, 5, 60, 6);
        setCol(r, {120, 200, 255}); fillR(r, gx, 5, 60 * frac, 6);
    }

    // --- Bottom strip. ---
    const float top = HUD_H + GAME_H;                // 240
    const float l1 = top, l2 = top + 8;              // two stacked text rows

    // Spare lives: the Jack currently in play isn't counted, so 3 lives shows 2
    // icons. Up to 7 icons in the bottom-left corner; g.lives may climb higher
    // internally but only the cap is drawn.
    int shown = std::clamp(g.lives - 1, 0, 7);
    for (int i = 0; i < shown; ++i) {
        SDL_FRect dst{2.0f + i * 15.0f, top + 1, (float)LIVE_W * 14 / LIVE_H, 14};
        if (g_liveTex) SDL_RenderTexture(r, g_liveTex, nullptr, &dst);
    }

    // ROUND (green) over -N- (white), centered just right of the life icons.
    drawTextCentered(r, "ROUND", 132, l1, 1, {80, 230, 90});
    std::snprintf(buf, sizeof(buf), "-%d-", g.level);
    drawTextCentered(r, buf, 132, l2, 1, {0, 139, 255});

    // HI-SCORE (yellow) over the current score (white), at the right. The
    // score has no leading zeros and is right-aligned under the label.
    drawTextCentered(r, "HI-SCORE", 192, l1, 1, {255, 230, 60});
    const float hiRight = 192 + textWidth("HI-SCORE", 1) / 2.0f;
    std::snprintf(buf, sizeof(buf), "%d", g.score);
    drawText(r, buf, hiRight - textWidth(buf, 1), l2, 1, {255, 255, 255});
}

// The "START!" intro: the two interlaced halves slide in from the screen edges
// and overlap in the centre of the play area to spell the word. Raw screen px.
void drawStartIntro(SDL_Renderer* r, const Game& g) {
    if (g.startAnim <= 0.0f || !g_startBg[0].tex) return;
    const float scale = 1.0f;            // drawn at the source image's native size
    const float w = 56.0f * scale, h = g_startBg[0].h * scale;
    const float finalX = (SCREEN_W - w) / 2.0f;
    const float cy = HUD_H + (GAME_H - h) / 2.0f;
    float elapsed = START_TOTAL - g.startAnim;
    float p = std::min(elapsed / START_SLIDE, 1.0f);
    p = 1.0f - (1.0f - p) * (1.0f - p);                   // ease-out
    float xs[2] = {-w       + (finalX - (-w))     * p,    // left half: in from left
                   SCREEN_W + (finalX - SCREEN_W) * p};   // right half: in from right

    // Arcade colour cycling: the black box background steps through a palette.
    static const Color cyc[] = {
        {220, 40, 40}, {230, 130, 30}, {220, 200, 40}, {60, 200, 70},
        {40, 180, 220}, {70, 90, 230}, {180, 60, 220}
    };
    const int N = (int)(sizeof(cyc) / sizeof(cyc[0]));
    Color bgc = cyc[(int)(g.time * 12.0f) % N];

    // The background dims smoothly to black, reaching full black at t=1s (and
    // staying black afterwards); the yellow text is unaffected.
    float fade = (elapsed <= START_SLIDE)
                     ? 1.0f
                     : std::max(0.0f, 1.0f - (elapsed - START_SLIDE) /
                                                 (START_BLACK - START_SLIDE));

    for (int i = 0; i < 2; ++i) {
        SDL_FRect d{xs[i], cy, w, h};
        SDL_SetTextureColorMod(g_startBg[i].tex,                        // cycling box
                               (Uint8)(bgc.r * fade), (Uint8)(bgc.g * fade),
                               (Uint8)(bgc.b * fade));
        SDL_RenderTexture(r, g_startBg[i].tex, nullptr, &d);
        SDL_SetTextureColorMod(g_startText[i].tex, 255, 255, 255);      // yellow text
        SDL_RenderTexture(r, g_startText[i].tex, nullptr, &d);
    }
}

// The between-level Special Bonus box: "YOU'VE GOTTEN n FIRE BOMBS / SPECIAL
// BONUS nnnnn PTS". The number cycles colour and the remaining total counts down
// to zero (the score counts up to match). Drawn in raw screen pixels.
void drawSpecialBonus(SDL_Renderer* r, const Game& g) {
    const float w = 178.0f, h = 92.0f;
    const float x = (SCREEN_W - w) / 2.0f;
    const float y = HUD_H + (GAME_H - h) / 2.0f;
    const float cx = x + w / 2.0f;
    Color hl = colorCycle3(g.time);
    setCol(r, hl);         fillR(r, x - 2, y - 2, w + 4, h + 4);   // bright frame
    setCol(r, {0, 0, 28}); fillR(r, x, y, w, h);                  // dark interior
    char buf[40];
    drawTextCentered(r, "YOU'VE GOTTEN", cx, y + 14, 1, {255, 230, 60});
    std::snprintf(buf, sizeof(buf), "%d FIRE BOMBS", g.sbCatched);
    drawTextCentered(r, buf, cx, y + 30, 1, hl);
    drawTextCentered(r, "SPECIAL BONUS", cx, y + 54, 1, {255, 255, 255});
    if (g.sbRemaining > 0) {
        std::snprintf(buf, sizeof(buf), "%d PTS", g.sbRemaining);
        drawTextCentered(r, buf, cx, y + 70, 1, hl);
    }
}

void render(SDL_Renderer* r, const Game& g) {
    useWorld(r);

    if (g.state == SPECIALBONUS) {
        useScreen(r);
        setCol(r, {0, 0, 0});
        SDL_RenderClear(r);
        drawSpecialBonus(r, g);
        drawHud(r, g);
        return;
    }

    if (g.state == TITLE) {
        // Welcome screen: solid black background with the logo centered in the
        // play area. Drawn in raw screen pixels so the logo keeps its true
        // proportions.
        useScreen(r);
        setCol(r, {0, 0, 0});
        SDL_RenderClear(r);
        const float bx = (SCREEN_W - BANNER_W) / 2.0f;
        const float by = HUD_H + (GAME_H / 4.0f) - (BANNER_H / 2.0f);   // play-area upper middle
        int phase = (int)(g.time / BANNER_ROT) % BANNER_PHASES;
        if (g_bannerTex[phase]) {
            SDL_FRect dst{bx, by, (float)BANNER_W, (float)BANNER_H};
            SDL_RenderTexture(r, g_bannerTex[phase], nullptr, &dst);
        }
        drawTextCentered(r, "PRESS SPACE TO START", SCREEN_W / 2.0f, by + BANNER_H + 14,
                         1, std::fmod(g.time, 1.0f) < 0.5f ? Color{230, 230, 230}
                                                           : Color{120, 120, 120});
        drawTextCentered(r, "ARROWS MOVE   SPACE FLOAT", SCREEN_W / 2.0f,
                         by + BANNER_H + 28, 1, {150, 150, 170});
        drawHud(r, g);
        return;
    }

    drawBackground(r, currentScreen(g));

    // Platforms — level-coloured shading with darker bottom edge.
    for (const SDL_FRect& pl : g.platforms) {
        if (pl.y >= FLOOR_TOP - 1.0f) continue;  // keep floor as collision-only
        drawPlatformShaded(r, pl, currentScreen(g));
    }

    // Bombs (the lit one is the bomb whose fuse is currently active, if any).
    for (int i = 0; i < (int)g.bombs.size(); ++i)
        if (!g.bombs[i].collected) drawBomb(r, g.bombs[i], i == g.litBomb, g.time);

    // Bomb-clear explosions: play the 3 frames quickly, small -> big.
    for (const Explosion& ex : g.explosions) {
        int fi = std::min(2, (int)(ex.age / EXPL_FRAME));
        const Sprite& f = g_explFrames[fi];
        if (!f.tex) continue;
        const float s = 1.6f;                          // a touch bigger than a bomb
        SDL_FRect dst{ex.x - f.w * s / 2, ex.y - f.h * s / 2, f.w * s, f.h * s};
        SDL_RenderTexture(r, f.tex, nullptr, &dst);
    }

    // Coin-pickup sparkles: play the 4 frames once, tinted yellow.
    for (const CoinPickup& cp : g.coinPickups) {
        int fi = std::min(3, (int)(cp.age / PICKCOIN_FRAME));
        const Sprite& f = g_pickCoinFrames[fi];
        if (!f.tex) continue;
        const float s = 2.0f;                          // drawn at 2x the coin size
        const float w = f.w * SPRITE_AR * s, h = f.h * s;
        SDL_FRect dst{cp.x - w / 2, cp.y - h / 2, w, h};
        drawTexTinted(r, f.tex, dst, false, 255, 215, 0);   // gold/yellow
    }

    if (g.orbActive) drawPowerOrb(r, g.orbX, g.orbY, g.time, g.orbFamily);

    // Active bonus coin (B/E/S), spinning through its four frames (drawn at 2x).
    if (g.bonusActive) {
        const Sprite* set = g.bonusKind == BK_E ? g_bonusE
                          : g.bonusKind == BK_S ? g_bonusS : g_bonusFrames;
        const Sprite& f = set[(int)(g.bonusAnim * 12.0f) % 4];
        if (f.tex) {
            SDL_FRect dst{g.bonusX - f.w, g.bonusY - f.h,
                          f.w * 2.0f, f.h * 2.0f};
            SDL_RenderTexture(r, f.tex, nullptr, &dst);
        }
    }

    // Bonus collect flashes: play the 6 frames once where a bonus was grabbed.
    for (const BonusTaken& bt : g.bonusTakens) {
        int fi = std::min(5, (int)(bt.age / BONUSTAKEN_FRAME));
        const Sprite& f = g_bonusTaken[fi];
        if (!f.tex) continue;
        // Drawn large enough to wreath the 26px bonus coin rather than sit inside it.
        const float w = f.w * SPRITE_AR * BONUSTAKEN_SCALE, h = f.h * BONUSTAKEN_SCALE;
        SDL_FRect dst{bt.x - w / 2, bt.y - h / 2, w, h};
        SDL_RenderTexture(r, f.tex, nullptr, &dst);
    }

    bool frozen = g.freezeTimer > 0.0f;
    for (const Enemy& e : g.enemies) drawEnemy(r, e, g.time, frozen, g.freezeTimer);
    if (g.state == ROUNDCLEAR)
        drawJackVictory(r, g.p, g.clearTimer);   // victory dance on level finish
    else if (g.state != GAMEOVER)                // game over hides Jack; shows the sprite
        drawPlayer(r, g.p, g.time, g.playerDying, g.deathPhase, g.deathFrame, frozen,
                   g.freezeColor);

    useScreen(r);
    drawPlayfieldBorder(r, currentScreen(g));
    // Platforms meeting a side frame poke a rounded tab over it (drawn on top).
    for (const SDL_FRect& pl : g.platforms) {
        if (pl.y >= FLOOR_TOP - 1.0f) continue;
        drawPlatformFrameTab(r, pl, currentScreen(g));
    }
    useWorld(r);

    // Floating score popups (kills, bombs, power).
    for (const Popup& pp : g.popups) {
        char sb[16];
        std::snprintf(sb, sizeof(sb), "%d", pp.value);
        drawTextCentered(r, sb, pp.x, pp.y, 1, {255, 240, 140});
    }

    char buf[64];
    if (g.state == ROUNDCLEAR) {
        std::snprintf(buf, sizeof(buf), "ROUND %d CLEAR!", g.level);
        drawTextCentered(r, buf, LOGW / 2.0f, LOGH / 2.0f - 10, 3, {120, 255, 140});
    }

    useScreen(r);
    drawStartIntro(r, g);
    // GAME OVER: the colour-cycling sprite slides to the screen centre (screen px).
    if (g.state == GAMEOVER && g_gameOverTex) {
        Color c = colorCycle3(g.time);
        const SDL_FRect dst{g.goX - GAMEOVER_W / 2.0f, g.goY - GAMEOVER_H / 2.0f,
                            (float)GAMEOVER_W, (float)GAMEOVER_H};
        drawTexTinted(r, g_gameOverTex, dst, false, c.r, c.g, c.b);
    }
    drawHud(r, g);
}

// ---------------------------------------------------------------------------
// Headless self-test: run the simulation with scripted input, no window.
// ---------------------------------------------------------------------------
int selfTest(int steps) {
    Game g;
    initPlatforms(g);
    startGame(g);
    const float dt = 1.0f / 120.0f;
    for (int i = 0; i < steps; ++i) {
        Input in{};
        in.right = (i / 60) % 2 == 0;
        in.left  = (i / 60) % 2 == 1;
        in.jumpHeld = (i % 20) < 10;
        in.jumpPressed = (i % 25) == 0;
        update(g, in, dt);
        if (g.p.y < -1 || g.p.y > LOGH || g.p.x < -1 || g.p.x > LOGW) {
            std::printf("FAIL: player escaped bounds at step %d (%.1f,%.1f)\n",
                        i, g.p.x, g.p.y);
            return 1;
        }
    }
    std::printf("selftest ok: %d steps, score=%d lives=%d level=%d state=%d\n",
                steps, g.score, g.lives, g.level, g.state);
    return 0;
}

// Headless render smoke test: builds the textures and draws every game state
// once against a software renderer (run with SDL_VIDEODRIVER=dummy for CI).
int renderTest() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window* win = SDL_CreateWindow("rendertest", SCREEN_W * WIN_SCALE,
                                       SCREEN_H * WIN_SCALE, 0);
    SDL_Renderer* ren = win ? SDL_CreateRenderer(win, nullptr) : nullptr;
    if (!ren) {
        std::fprintf(stderr, "render init failed: %s\n", SDL_GetError());
        if (win) SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderLogicalPresentation(ren, SCREEN_W, SCREEN_H,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);
    buildSprites(ren);
    buildBackground(ren);

    Game g;
    initPlatforms(g);
    startGame(g);
    int frames = 0;
    for (State st : {TITLE, PLAYING, ROUNDCLEAR, SPECIALBONUS, GAMEOVER}) {
        g.state = st;
        render(ren, g);
        SDL_RenderPresent(ren);
        ++frames;
    }

    if (g_bgTex) SDL_DestroyTexture(g_bgTex);
    for (Sprite& s : g_sprites)
        if (s.tex) SDL_DestroyTexture(s.tex);
    for (SDL_Texture*& t : g_jackTex)
        if (t) SDL_DestroyTexture(t);
    for (JackVarFrame& f : g_jackDance)
        if (f.tex) SDL_DestroyTexture(f.tex);
    for (JackVarFrame& f : g_jackPlf)
        if (f.tex) SDL_DestroyTexture(f.tex);
    for (JackVarFrame& f : g_jackDead)
        if (f.tex) SDL_DestroyTexture(f.tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    std::printf("rendertest ok: %d frames, all states drawn\n", frames);
    return 0;
}

}  // namespace

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--selftest") == 0)
            return selfTest(i + 1 < argc ? std::atoi(argv[i + 1]) : 600);
        if (std::strcmp(argv[i], "--rendertest") == 0)
            return renderTest();
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* win = SDL_CreateWindow("Bomb Jack", WIN_W, WIN_H,
                                       SDL_WINDOW_RESIZABLE);
    if (!win) {
        std::fprintf(stderr, "CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    // Lock the window to a 3:4 aspect so a resize keeps the squeezed-width look.
    SDL_SetWindowAspectRatio(win, 3.0f / 4.0f, 3.0f / 4.0f);
    // SDL3's Vulkan render backend flickers on some Linux/X11 setups (it
    // presents alternating swapchain images), so pick a stable backend in
    // preference order. SDL_CreateRenderer silently falls back to the default
    // (often Vulkan) for an unknown driver name, so reject Vulkan explicitly.
    SDL_Renderer* ren = nullptr;
    for (const char* drv : {"opengl", "opengles2", "gpu", "software"}) {
        SDL_Renderer* r = SDL_CreateRenderer(win, drv);
        if (!r) continue;
        const char* name = SDL_GetRendererName(r);
        if (name && SDL_strcmp(name, "vulkan") == 0) { SDL_DestroyRenderer(r); continue; }
        ren = r;
        break;
    }
    if (!ren) ren = SDL_CreateRenderer(win, nullptr);  // last resort
    if (!ren) {
        std::fprintf(stderr, "CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderVSync(ren, 1);
    // STRETCH maps the 224x256 render onto the whole (3:4) window, producing the
    // non-square arcade pixels; the window's locked aspect keeps it undistorted.
    SDL_SetRenderLogicalPresentation(ren, SCREEN_W, SCREEN_H,
                                     SDL_LOGICAL_PRESENTATION_STRETCH);

    buildSprites(ren);
    buildBackground(ren);

    Game g;
    initPlatforms(g);

    bool running = true;
    bool jumpEdge = false;
    Uint64 prev = SDL_GetTicks();
    double acc = 0.0;
    const double STEP = 1.0 / 120.0;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = false;
            if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
                SDL_Scancode sc = e.key.scancode;
                if (sc == SDL_SCANCODE_ESCAPE || sc == SDL_SCANCODE_Q)
                    running = false;
                bool jump = (sc == SDL_SCANCODE_SPACE || sc == SDL_SCANCODE_UP ||
                             sc == SDL_SCANCODE_W);
                if (jump) {
                    if (g.state == TITLE) startGame(g);
                    else jumpEdge = true;
                }
                if (sc == SDL_SCANCODE_R && g.state == GAMEOVER) startGame(g);
            }
        }

        const bool* ks = SDL_GetKeyboardState(nullptr);
        Input in{};
        in.left = ks[SDL_SCANCODE_LEFT] || ks[SDL_SCANCODE_A];
        in.right = ks[SDL_SCANCODE_RIGHT] || ks[SDL_SCANCODE_D];
        in.up = ks[SDL_SCANCODE_UP] || ks[SDL_SCANCODE_W];
        in.down = ks[SDL_SCANCODE_DOWN] || ks[SDL_SCANCODE_S];
        in.jumpHeld = ks[SDL_SCANCODE_SPACE] || ks[SDL_SCANCODE_UP] ||
                      ks[SDL_SCANCODE_W];

        Uint64 now = SDL_GetTicks();
        double frame = (now - prev) / 1000.0;
        prev = now;
        if (frame > 0.25) frame = 0.25;
        acc += frame;
        while (acc >= STEP) {
            in.jumpPressed = jumpEdge;
            jumpEdge = false;
            update(g, in, (float)STEP);
            acc -= STEP;
        }

        render(ren, g);
        SDL_RenderPresent(ren);
    }

    if (g_bgTex) SDL_DestroyTexture(g_bgTex);
    for (Sprite& s : g_sprites)
        if (s.tex) SDL_DestroyTexture(s.tex);
    for (SDL_Texture*& t : g_jackTex)
        if (t) SDL_DestroyTexture(t);
    for (JackVarFrame& f : g_jackDance)
        if (f.tex) SDL_DestroyTexture(f.tex);
    for (JackVarFrame& f : g_jackPlf)
        if (f.tex) SDL_DestroyTexture(f.tex);
    for (JackVarFrame& f : g_jackDead)
        if (f.tex) SDL_DestroyTexture(f.tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
