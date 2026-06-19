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
#include "banner_png.h"     // the colorful "BOMB JACK" title logo (191x79)
#include "live_png.h"        // little Jack life icon for the HUD (31x32)
#include "jack_png.h"        // Jack animation frames (15 x 16x15) from the original
#include "bird_png.h"        // bird enemy flap frames (9 x 16x16) from the original
#include "mummy_png.h"       // mummy enemy frames (8 x 16x16) from the original
#include "bonus_png.h"       // bonus "B" coin spin frames (4 x, from bonusSprite.png)
#include "coins_png.h"       // coin spin frames frozen enemies turn into (7 x, coins.png)
#include "start_png.h"       // "START!" intro: two interlaced halves (start.png)
#include "powerball_png.h"   // the P power-orb sprite (16x16), tinted per type
#include "explosion_png.h"   // 3-frame bomb-clear explosion (small -> big)
#include "sprites_pack.h"   // official Bomb Jack character sprites, packed
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
constexpr float CLEAR_TIME  = 1.6f;     // round-clear banner duration (s)
constexpr float FREEZE_TIME = 5.0f;     // enemy freeze after grabbing the P orb
constexpr float POWER_NEEDED = 8.0f;    // lit-bomb "charge" needed to spawn a P orb
constexpr float ORB_SPEED = 60.0f;      // moving Power orb speed (world px/s)
// Bonus "B" coin: every BONUS_LIMIT points of score one appears; catching it
// bumps the points multiplier x1 -> x5 (capped). Mirrors the reference game's
// score.lua (BONUS_LIMIT=5000, multiplier resets each level). The coin patrols
// a platform until grabbed.
constexpr int   BONUS_LIMIT  = 5000;    // score per bonus-B opportunity
constexpr int   BONUS_POINTS = 1000;    // base value of a caught B coin
constexpr int   MAX_MULT     = 5;       // multiplier cap
constexpr float BONUS_W = 26.0f, BONUS_H = 26.0f;   // drawn 2x the 13px frame
constexpr float BONUS_SPEED = 45.0f;    // horizontal patrol speed (world px/s)
// "START!" intro: two interlaced halves slide in from the screen edges, meet in
// the centre, and hold briefly. Plays at every phase start and on a new life.
constexpr float START_SLIDE = 0.55f;    // slide-in duration (s)
constexpr float START_HOLD  = 0.65f;    // centre hold duration (s)
constexpr float START_BLACK = 1.0f;     // the background reaches black at t=1s
constexpr float START_TOTAL = START_SLIDE + START_HOLD;   // 1.2s, then Jack drops
constexpr float ORB_R = 10.0f;          // Power orb collision / bounce radius
constexpr float DEATH_DANCE_TOTAL = 0.5f; // bj_dancing duration (3 frames)
constexpr int   DEATH_DANCE_LOOPS = 2;    // bj_dancing repeats
constexpr float DEATH_PLF_TOTAL = 1.0f;   // bj_PLF loop duration (4 frames)
constexpr float DEATH_DEAD_TOTAL = 1.0f;  // bj_dead one-shot duration (4 frames)
constexpr float DEATH_WAIT_TIME = 3.0f;   // wait after bj_dead before losing life

// Escalating points for each enemy killed during a single freeze (Bomb Jack).
constexpr int KILL_POINTS[] = {100, 200, 300, 500, 800, 1200, 2000};
// Original Power orb score tiers (indexed by orb colour family).
constexpr int POWER_POINTS[] = {100, 200, 300, 500, 800, 1200, 2000};

// Mummies drop in over time and transform into flying chasers on the ground.
constexpr float MUMMY_APPEAR_TIME = 1.1f;   // pop-in pause before it moves
constexpr float MUMMY_DISAPPEAR_TIME = 0.5f; // bottom-floor vanish before transform
constexpr float MUMMY_SPAWN_DELAY = 3.6f;   // seconds between mummy spawns
constexpr float MUMMY_WALK_SPEED  = 70.0f;  // platform walk speed (world px/s)
constexpr float FLY_SPEED         = 120.0f; // base speed of transformed chasers

enum State { TITLE, PLAYING, ROUNDCLEAR, GAMEOVER };
enum DeathPhase { DP_NONE, DP_DANCING, DP_FALLING, DP_DEAD, DP_WAIT };

struct Color { Uint8 r, g, b; };

// The 7 Power-orb base colours, by family index (matching POWER_POINTS):
// 0 Red=100, 1 Blue=200, 2 Purple=300, 3 Green=500, 4 Aqua=800, 5 Gold=1200,
// 6 Silver=2000. The orb's family rotates as Jack acts, changing colour/value.
constexpr Color POWER_COLORS[7] = {
    {235, 30, 45}, {45, 80, 240}, {165, 45, 225}, {45, 205, 65},
    {40, 215, 215}, {240, 195, 45}, {205, 210, 220}
};

struct Input { bool left, right, jumpHeld, jumpPressed; };

struct Player {
    float x, y, vx, vy;
    bool  onGround;
    float invuln;
    int   face = 1;   // 1 = facing right, -1 = facing left
};

struct Bomb { float x, y; bool collected; };  // x,y = center

// Enemy kinds match the level data's transform sequence (see levels_data.h).
enum EKind  { EK_BIRD, EK_MUMMY, EK_SPHERE, EK_ORB, EK_HORN, EK_CLUB, EK_UFO };
enum EPhase { EP_FLY, EP_APPEAR, EP_WALK, EP_FALL, EP_DISAPPEAR };  // mummy lifecycle; others fly

struct Enemy {
    float x, y, vx, vy, r;
    int   kind   = EK_BIRD;
    int   phase  = EP_FLY;
    float timer  = 0.0f;     // appear countdown
    int   bounces = 0;       // platform direction-changes left before falling
    int   becomes = EK_SPHERE;  // what this mummy transforms into
};

struct Popup { float x, y, age = 0.0f; int value = 0; };  // floating score text
struct Explosion { float x, y, age = 0.0f; };            // bomb-clear burst (3 frames)
constexpr float EXPL_FRAME = 0.06f;                       // per-frame time (quick)

struct Game {
    int   state = TITLE;
    int   score = 0, lives = 3, level = 1, streak = 1, bombsLeft = 0;
    int   phaseStart = 0;              // total score when the current phase began
    float clearTimer = 0.0f;
    float time = 0.0f;                 // animation clock
    // Power orb / enemy-freeze state.
    float powerMeter = 0.0f;           // charge from caught bombs; spawns orb at POWER_NEEDED
    bool  orbActive = false;           // a P orb is on the field
    float orbX = 0.0f, orbY = 0.0f;
    float orbVx = 0.0f, orbVy = 0.0f;
    int   orbFamily = 0;               // 0..6 PowerToColors family index
    float freezeTimer = 0.0f;          // >0 while enemies are frozen & killable
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
    // Bonus "B" coin + points multiplier.
    int   multiplier = 1;              // x1..x5, multiplies all point gains
    int   nextBonusScore = BONUS_LIMIT;// next score threshold that spawns a B coin
    bool  bonusActive = false;         // a B coin is on the field
    float bonusX = 0.0f, bonusY = 0.0f, bonusVx = 0.0f;
    float bonusPlatX = 0.0f, bonusPlatW = 0.0f;  // platform the coin patrols
    float bonusAnim = 0.0f;            // spin-animation clock
    float startAnim = 0.0f;            // >0 while the "START!" intro plays
    Player p{};
    std::vector<SDL_FRect> platforms;
    std::vector<Bomb>      bombs;
    std::vector<Enemy>     enemies;
    std::vector<Popup>     popups;
    std::vector<Explosion> explosions;
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
void fillCircle(SDL_Renderer* r, float cx, float cy, float rad) {
    int ir = static_cast<int>(rad);
    for (int dy = -ir; dy <= ir; ++dy) {
        float dx = std::sqrt(rad * rad - static_cast<float>(dy * dy));
        fillR(r, cx - dx, cy + static_cast<float>(dy), 2 * dx + 1, 1);
    }
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
// Sprites — Jack, bombs and enemies are cropped from the embedded official
// Bomb Jack sprite sheet (sprites_pack.h); the girder platform is still a small
// pixel map (its colour is stage-specific in the arcade). No runtime files.
// ---------------------------------------------------------------------------
enum SpriteId {
    SP_BOMB, SP_ENEMY1, SP_ENEMY2,
    SP_MUMMY, SP_MUMMY_WALK, SP_MUMMY_FALL,
    SP_SPHERE1, SP_SPHERE2, SP_ORB1, SP_ORB2, SP_HORN1, SP_HORN2,
    SP_CLUB1, SP_CLUB2, SP_UFO1, SP_UFO2,
    SP_PLAT, SP_COUNT
};

struct Sprite { int w = 0, h = 0; SDL_Texture* tex = nullptr; };
Sprite g_sprites[SP_COUNT];

// Bonus "B" coin (4-frame spin) and the boxed multiplier indicators (index 0 is
// the "x" box, 1..5 are the digits) — cropped from the full atlas in loadAssets.
Sprite g_bonusFrames[4];
SDL_Texture* g_multTex[6] = {};

// Coin spin frames that P-frozen enemies turn into (from coins.png).
Sprite g_coinFrames[7];

// Power-orb sprite (PowerBall.png) prebaked per colour family [0..6] and per
// colour-cycle phase [0..3]: each frame recolours the ball's bands with 4
// shades near the family's base colour, and stepping the phase cycles them.
Sprite g_orbCycle[7][4];

// Bomb-clear explosion frames (explosion.png): small -> medium -> big.
Sprite g_explFrames[3];

// "START!" intro halves (start.png): each carries alternate scanlines of the
// word, so overlapping them in the centre spells it out. Each half is split into
// a background mask (the black box, white so it can be tinted with a cycling
// arcade colour) and the yellow text layer, indexed [0]=left, [1]=right.
Sprite g_startBg[2], g_startText[2];

// Jack's animation frames, mirroring the original game (bombjack-resources):
// an idle pose, a 4-frame walk cycle per direction, and directional flying
// (rising) / falling poses. Frame order matches the embedded jack_png strip.
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

struct JackVarFrame { int w = 0, h = 0; SDL_Texture* tex = nullptr; };
JackVarFrame g_jackDance[3] = {};
JackVarFrame g_jackPlf[4] = {};
JackVarFrame g_jackDead[4] = {};

// Bird enemy: a 3-frame wing-flap cycle per heading (left / right / vertical),
// plus the arcade's pulsing-red recolour (BirdToColors). Frame order matches
// the embedded bird_png strip.
enum BirdFrame {
    BF_LEFT0, BF_LEFT1, BF_LEFT2,
    BF_RIGHT0, BF_RIGHT1, BF_RIGHT2,
    BF_VERT0, BF_VERT1, BF_VERT2,
    BF_COUNT
};
constexpr int BIRD_FW = 16, BIRD_FH = 16;
constexpr float BIRD_FLAP_FRAME = 0.1f;       // 0.3s / 3 frames (arcade rate)
constexpr float BIRD_PULSE_STEP = 0.06f;      // colour-pulse cadence (arcade rate)
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

// Current red intensity of the pulsing enemy eyes at time t.
inline Uint8 eyePulse(float t) {
    return EYE_PULSE[(int)(t / BIRD_PULSE_STEP) % (int)std::size(EYE_PULSE)];
}

SDL_Color paletteColor(char c) {
    switch (c) {
        case 'B': return {50, 100, 235, 255};   // blue girder
        case 'b': return {25, 55, 160, 255};    // dark blue
        case 'L': return {150, 165, 245, 255};  // platform top edge
        case 'D': return {40, 55, 120, 255};    // platform shadow
        default:  return {0, 0, 0, 0};          // transparent
    }
}

// The girder platform tile (tiled across each platform rect).
std::vector<std::string> platformRows() {
    return {
        "LLLLLLLLLLLLLLLL", "BBBBBBBBBBBBBBBB", "BBBBBBBBBBBBBBBB",
        "BBBbbBBBBBBbbBBB", "BbbDDbbbbbbDDbbB", "BBBBBBBBBBBBBBBB",
        "BBBBBBBBBBBBBBBB", "DDDDDDDDDDDDDDDD", "BBBBBBBBBBBBBBBB",
        "BBBbbBBBBBBbbBBB", "BbbDDbbbbbbDDbbB", "DDDDDDDDDDDDDDDD"};
}

// Verify the platform pixel map is rectangular (catches authoring miscounts).
int validateSprites() {
    int bad = 0;
    auto rows = platformRows();
    size_t w = rows.empty() ? 0 : rows[0].size();
    for (size_t r = 0; r < rows.size(); ++r)
        if (rows[r].size() != w) {
            std::printf("platform row %zu width %zu != %zu\n", r, rows[r].size(), w);
            ++bad;
        }
    return bad;
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

void buildSprites(SDL_Renderer* ren) {
    // Character sprites: crop each from the embedded official sprite sheet.
    struct PackRect { SpriteId id; int x, y, w, h; };
    static const PackRect packRects[] = {
        {SP_BOMB, 45, 0, 16, 18},
        {SP_ENEMY1, 61, 0, 18, 16},    {SP_ENEMY2, 79, 0, 18, 16},
        {SP_MUMMY, 97, 0, 14, 16},     {SP_MUMMY_WALK, 111, 0, 14, 16},
        {SP_MUMMY_FALL, 125, 0, 14, 16},
        {SP_SPHERE1, 139, 0, 16, 16},  {SP_SPHERE2, 155, 0, 16, 16},
        {SP_ORB1, 171, 0, 16, 16},     {SP_ORB2, 187, 0, 16, 16},
        {SP_HORN1, 203, 0, 16, 16},    {SP_HORN2, 219, 0, 16, 16},
        {SP_CLUB1, 235, 0, 16, 16},    {SP_CLUB2, 251, 0, 16, 16},
        {SP_UFO1, 267, 0, 16, 12},     {SP_UFO2, 283, 0, 16, 12},
    };
    int w = 0, h = 0, comp = 0;
    stbi_uc* px = stbi_load_from_memory(spritepack_png, (int)spritepack_png_len,
                                        &w, &h, &comp, 4);
    if (px) {
        SDL_Surface* sheet =
            SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_RGBA32, px, w * 4);
        for (const PackRect& pr : packRects) {
            SDL_Surface* s = SDL_CreateSurface(pr.w, pr.h, SDL_PIXELFORMAT_RGBA32);
            SDL_Rect src{pr.x, pr.y, pr.w, pr.h};
            SDL_BlitSurface(sheet, &src, s, nullptr);
            g_sprites[pr.id] = {pr.w, pr.h, texFromSurface(ren, s)};
            SDL_DestroySurface(s);
        }
        SDL_DestroySurface(sheet);
        stbi_image_free(px);
    } else {
        std::fprintf(stderr, "sprite sheet decode failed\n");
    }

    // Platform girder: built from its pixel map.
    auto rows = platformRows();
    int ph = (int)rows.size(), pw = (int)rows[0].size();
    SDL_Surface* s = SDL_CreateSurface(pw, ph, SDL_PIXELFORMAT_RGBA32);
    Uint8* sp = (Uint8*)s->pixels;
    for (int y = 0; y < ph; ++y)
        for (int x = 0; x < pw; ++x) {
            SDL_Color c = paletteColor(rows[y][x]);
            Uint8* p = sp + y * s->pitch + x * 4;  // RGBA32 byte order
            p[0] = c.r; p[1] = c.g; p[2] = c.b; p[3] = c.a;
        }
    g_sprites[SP_PLAT] = {pw, ph, texFromSurface(ren, s)};
    SDL_DestroySurface(s);

    // Jack animation frames: slice the embedded strip into JF_COUNT cells.
    int jw = 0, jh = 0, jc = 0;
    stbi_uc* jpx = stbi_load_from_memory(jack_png, (int)jack_png_len,
                                         &jw, &jh, &jc, 4);
    if (jpx) {
        SDL_Surface* strip =
            SDL_CreateSurfaceFrom(jw, jh, SDL_PIXELFORMAT_RGBA32, jpx, jw * 4);
        for (int i = 0; i < JF_COUNT; ++i) {
            SDL_Surface* f = SDL_CreateSurface(JACK_FW, JACK_FH, SDL_PIXELFORMAT_RGBA32);
            SDL_Rect src{i * JACK_FW, 0, JACK_FW, JACK_FH};
            SDL_BlitSurface(strip, &src, f, nullptr);
            g_jackTex[i] = texFromSurface(ren, f);
            SDL_DestroySurface(f);
        }
        SDL_DestroySurface(strip);
        stbi_image_free(jpx);
    } else {
        std::fprintf(stderr, "jack sprite decode failed\n");
    }

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
        static const int danceRect[3][4] = {
            {249, 31, 16, 17}, {269, 31, 16, 17}, {64, 34, 15, 14}
        };
        static const int plfRect[4][4] = {
            {44, 32, 15, 15}, {64, 32, 15, 16}, {84, 32, 15, 16}, {124, 32, 15, 15}
        };
        static const int deadRect[4][4] = {
            {145, 27, 16, 18}, {166, 29, 13, 16}, {184, 24, 17, 24}, {205, 27, 23, 21}
        };
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
        SDL_DestroySurface(atlas);
        stbi_image_free(spx);
    } else {
        std::fprintf(stderr, "full sprites atlas decode failed\n");
    }

    // Bonus "B" coin: 4 spin frames from the dedicated bonusSprite.png strip.
    int bw = 0, bh = 0, bc = 0;
    stbi_uc* bpx = stbi_load_from_memory(bonus_png, (int)bonus_png_len,
                                         &bw, &bh, &bc, 4);
    if (bpx) {
        SDL_Surface* strip =
            SDL_CreateSurfaceFrom(bw, bh, SDL_PIXELFORMAT_RGBA32, bpx, bw * 4);
        static const int bonusRect[4][4] = {   // x, y, w, h within the strip
            {3, 1, 13, 13}, {23, 1, 12, 13}, {42, 1, 7, 13}, {55, 1, 12, 13}
        };
        for (int i = 0; i < 4; ++i) {
            SDL_Surface* f = SDL_CreateSurface(bonusRect[i][2], bonusRect[i][3],
                                               SDL_PIXELFORMAT_RGBA32);
            SDL_Rect src{bonusRect[i][0], bonusRect[i][1], bonusRect[i][2], bonusRect[i][3]};
            SDL_BlitSurface(strip, &src, f, nullptr);
            g_bonusFrames[i] = {bonusRect[i][2], bonusRect[i][3], texFromSurface(ren, f)};
            SDL_DestroySurface(f);
        }
        SDL_DestroySurface(strip);
        stbi_image_free(bpx);
    } else {
        std::fprintf(stderr, "bonus sprite decode failed\n");
    }

    // Coin frames (coins.png): 7 cells, 12x12, what frozen enemies become.
    int cw = 0, ch = 0, cc = 0;
    stbi_uc* cpx = stbi_load_from_memory(coins_png, (int)coins_png_len,
                                         &cw, &ch, &cc, 4);
    if (cpx) {
        SDL_Surface* strip =
            SDL_CreateSurfaceFrom(cw, ch, SDL_PIXELFORMAT_RGBA32, cpx, cw * 4);
        for (int i = 0; i < 7; ++i) {
            SDL_Surface* f = SDL_CreateSurface(12, 12, SDL_PIXELFORMAT_RGBA32);
            SDL_Rect src{1 + i * 14, 1, 12, 12};   // frames at x=1,15,...,85
            SDL_BlitSurface(strip, &src, f, nullptr);
            g_coinFrames[i] = {12, 12, texFromSurface(ren, f)};
            SDL_DestroySurface(f);
        }
        SDL_DestroySurface(strip);
        stbi_image_free(cpx);
    } else {
        std::fprintf(stderr, "coin sprite decode failed\n");
    }

    // "START!" intro halves (start.png): left at x=2, right at x=62, each 56x14.
    int tw = 0, th = 0, tc = 0;
    stbi_uc* tpx = stbi_load_from_memory(start_png, (int)start_png_len,
                                         &tw, &th, &tc, 4);
    if (tpx) {
        // Split a half into: a white background mask (over the black box pixels,
        // so it can be colour-cycled) and the yellow text layer.
        auto splitHalf = [&](int x0, Sprite& bg, Sprite& text) {
            SDL_Surface* b = SDL_CreateSurface(56, th, SDL_PIXELFORMAT_RGBA32);
            SDL_Surface* t = SDL_CreateSurface(56, th, SDL_PIXELFORMAT_RGBA32);
            for (int y = 0; y < th; ++y)
                for (int x = 0; x < 56; ++x) {
                    const stbi_uc* sp = tpx + ((size_t)y * tw + (x0 + x)) * 4;
                    Uint8* bp = (Uint8*)b->pixels + y * b->pitch + x * 4;
                    Uint8* tp = (Uint8*)t->pixels + y * t->pitch + x * 4;
                    bool opaque = sp[3] > 0;
                    bool yellow = opaque && sp[0] > 150 && sp[1] > 150 && sp[2] < 120;
                    bool box    = opaque && !yellow;          // the black background
                    bp[0] = bp[1] = bp[2] = 255; bp[3] = box ? 255 : 0;
                    tp[0] = sp[0]; tp[1] = sp[1]; tp[2] = sp[2]; tp[3] = yellow ? 255 : 0;
                }
            bg   = {56, th, texFromSurface(ren, b)};
            text = {56, th, texFromSurface(ren, t)};
            SDL_DestroySurface(b);
            SDL_DestroySurface(t);
        };
        splitHalf(2,  g_startBg[0], g_startText[0]);
        splitHalf(62, g_startBg[1], g_startText[1]);
        stbi_image_free(tpx);
    } else {
        std::fprintf(stderr, "start sprite decode failed\n");
    }

    // Power orb (PowerBall.png): the pinwheel "P" ball. We classify each pixel
    // into one of 4 bands by luminance, then prebake, for every colour family
    // and every cycle phase, a recoloured ball whose 4 bands use 4 shades near
    // the base colour. Stepping the phase at runtime cycles the colours (no
    // sprite rotation; the orb stays upright).
    int ow = 0, oh = 0, oc = 0;
    stbi_uc* opx = stbi_load_from_memory(powerball_png, (int)powerball_png_len,
                                         &ow, &oh, &oc, 4);
    if (opx) {
        auto lum = [](const stbi_uc* p) {
            return 0.30f * p[0] + 0.59f * p[1] + 0.11f * p[2];
        };
        float lo = 1e9f, hi = -1e9f;                  // luminance range of the ball
        for (int i = 0; i < ow * oh; ++i)
            if (opx[i * 4 + 3] > 0) {
                float L = lum(opx + i * 4);
                lo = std::min(lo, L); hi = std::max(hi, L);
            }
        float span = std::max(1.0f, hi - lo);
        // Per-pixel band 0..3 (dark -> bright); -1 = transparent.
        std::vector<int> band((size_t)ow * oh, -1);
        for (int i = 0; i < ow * oh; ++i)
            if (opx[i * 4 + 3] > 0)
                band[i] = std::min(3, (int)((lum(opx + i * 4) - lo) / span * 4.0f));
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
        stbi_image_free(opx);
    } else {
        std::fprintf(stderr, "power orb sprite decode failed\n");
    }

    // Bomb-clear explosion (explosion.png): 3 frames, growing small -> big.
    int ew = 0, eh = 0, ec = 0;
    stbi_uc* epx = stbi_load_from_memory(explosion_png, (int)explosion_png_len,
                                         &ew, &eh, &ec, 4);
    if (epx) {
        SDL_Surface* strip =
            SDL_CreateSurfaceFrom(ew, eh, SDL_PIXELFORMAT_RGBA32, epx, ew * 4);
        static const int er[3][4] = {{4, 5, 8, 8}, {18, 3, 12, 12}, {36, 1, 16, 16}};
        for (int i = 0; i < 3; ++i) {
            SDL_Surface* f = SDL_CreateSurface(er[i][2], er[i][3], SDL_PIXELFORMAT_RGBA32);
            SDL_Rect src{er[i][0], er[i][1], er[i][2], er[i][3]};
            SDL_BlitSurface(strip, &src, f, nullptr);
            g_explFrames[i] = {er[i][2], er[i][3], texFromSurface(ren, f)};
            SDL_DestroySurface(f);
        }
        SDL_DestroySurface(strip);
        stbi_image_free(epx);
    } else {
        std::fprintf(stderr, "explosion sprite decode failed\n");
    }

    // Bird frames: slice the embedded strip into BF_COUNT cells.
    int dw = 0, dh = 0, dc = 0;
    stbi_uc* dpx = stbi_load_from_memory(bird_png, (int)bird_png_len,
                                         &dw, &dh, &dc, 4);
    if (dpx) {
        SDL_Surface* strip =
            SDL_CreateSurfaceFrom(dw, dh, SDL_PIXELFORMAT_RGBA32, dpx, dw * 4);
        for (int i = 0; i < BF_COUNT; ++i) {
            SDL_Surface* f = SDL_CreateSurface(BIRD_FW, BIRD_FH, SDL_PIXELFORMAT_RGBA32);
            SDL_Rect src{i * BIRD_FW, 0, BIRD_FW, BIRD_FH};
            SDL_BlitSurface(strip, &src, f, nullptr);
            g_birdTex[i] = texFromSurface(ren, f);
            g_birdEye[i] = makeEyeMask(ren, f);
            SDL_DestroySurface(f);
        }
        SDL_DestroySurface(strip);
        stbi_image_free(dpx);
    } else {
        std::fprintf(stderr, "bird sprite decode failed\n");
    }

    // Mummy frames: slice the embedded strip into MF_COUNT cells.
    int mw = 0, mh = 0, mc = 0;
    stbi_uc* mpx = stbi_load_from_memory(mummy_png, (int)mummy_png_len,
                                         &mw, &mh, &mc, 4);
    if (mpx) {
        SDL_Surface* strip =
            SDL_CreateSurfaceFrom(mw, mh, SDL_PIXELFORMAT_RGBA32, mpx, mw * 4);
        for (int i = 0; i < MF_COUNT; ++i) {
            SDL_Surface* f = SDL_CreateSurface(MUMMY_FW, MUMMY_FH, SDL_PIXELFORMAT_RGBA32);
            SDL_Rect src{i * MUMMY_FW, 0, MUMMY_FW, MUMMY_FH};
            SDL_BlitSurface(strip, &src, f, nullptr);
            g_mummyTex[i] = texFromSurface(ren, f);
            g_mummyEye[i] = makeEyeMask(ren, f);
            SDL_DestroySurface(f);
        }
        SDL_DestroySurface(strip);
        stbi_image_free(mpx);
    } else {
        std::fprintf(stderr, "mummy sprite decode failed\n");
    }
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
constexpr int LIVE_W = 31, LIVE_H = 32;
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

void drawPlatformShaded(SDL_Renderer* r, const SDL_FRect& pl, int screen) {
    const auto& pal = borderPaletteForScreen(screen);
    const int x = (int)std::round(pl.x);
    const int y = (int)std::round(pl.y);
    const int w = std::max(1, (int)std::round(pl.w));
    const int h = std::max(1, (int)std::round(pl.h));

    // Top bright -> bottom dark, matching original platform shading feel.
    for (int i = 0; i < 8; ++i) {
        int y0 = y + (h * i) / 8;
        int y1 = y + (h * (i + 1)) / 8;
        int bh = std::max(1, y1 - y0);
        setCol(r, pal[i]);
        fillR(r, (float)x, (float)y0, (float)w, (float)bh);
    }

    // Subtle underside accent to reinforce the bottom shadow.
    setCol(r, pal[7]);
    fillR(r, (float)x, (float)(y + h - 1), (float)w, 1.0f);

    // Pin exact corner pixels so platforms keep square arcade corners.
    setCol(r, pal[0]);
    fillR(r, (float)x, (float)y, 1.0f, 1.0f);
    fillR(r, (float)(x + w - 1), (float)y, 1.0f, 1.0f);
    setCol(r, pal[7]);
    fillR(r, (float)x, (float)(y + h - 1), 1.0f, 1.0f);
    fillR(r, (float)(x + w - 1), (float)(y + h - 1), 1.0f, 1.0f);
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

    // Title logo. Build one texture per rotation phase, cycling the three ray
    // blues (BombjackLogoColors from the original game) so the rays spin.
    int bw = 0, bh = 0, bc = 0;
    stbi_uc* bpx = stbi_load_from_memory(banner_png, (int)banner_png_len,
                                         &bw, &bh, &bc, 4);
    if (!bpx) {
        std::fprintf(stderr, "banner decode failed\n");
        return;
    }
    const Uint8 blues[BANNER_PHASES][3] = {{0, 82, 255}, {0, 140, 255}, {0, 189, 255}};
    std::vector<Uint8> buf(bw * bh * 4);
    for (int phase = 0; phase < BANNER_PHASES; ++phase) {
        std::memcpy(buf.data(), bpx, buf.size());
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
    stbi_image_free(bpx);

    // Jack life icon.
    int lw = 0, lh = 0, lc = 0;
    stbi_uc* lpx = stbi_load_from_memory(live_png, (int)live_png_len,
                                         &lw, &lh, &lc, 4);
    if (lpx) {
        SDL_Surface* ls = SDL_CreateSurfaceFrom(lw, lh, SDL_PIXELFORMAT_RGBA32,
                                                lpx, lw * 4);
        g_liveTex = SDL_CreateTextureFromSurface(ren, ls);
        SDL_SetTextureScaleMode(g_liveTex, SDL_SCALEMODE_NEAREST);
        SDL_DestroySurface(ls);
        stbi_image_free(lpx);
    } else {
        std::fprintf(stderr, "life icon decode failed\n");
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
}

Enemy makeBird(Game& g, float ang) {
    std::uniform_real_distribution<float> sign(-1.0f, 1.0f);
    float speed = std::min(72.0f + g.level * 8.0f, 150.0f);
    Enemy e;
    e.kind = EK_BIRD;
    e.r = 11.0f;
    e.x = 60.0f + (LOGW - 120.0f) * (0.5f + 0.5f * std::cos(ang));
    e.y = 70.0f + (LOGH - 160.0f) * (0.5f + 0.5f * std::sin(ang));
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
void spawnBonus(Game& g) {
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
    g.streak = 1;
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
    g.phaseStart = g.score;            // start counting this phase's points fresh
    g.multiplier = 1;                  // the points multiplier resets each level
    g.bonusActive = false;
    g.nextBonusScore = (g.score / BONUS_LIMIT + 1) * BONUS_LIMIT;
    g.startAnim = START_TOTAL;          // play the "START!" intro for this phase
}

void startGame(Game& g) {
    g.score = 0;
    g.lives = 3;
    g.level = 1;
    g.state = PLAYING;
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
    e.phase = EP_APPEAR;
    e.timer = MUMMY_APPEAR_TIME;
    e.r = 10.0f;
    e.x = lay.mummyx[g.p.x < LOGW / 2 ? 1 : 0];   // drop in away from Jack
    e.y = 90.0f;
    e.vx = e.vy = 0.0f;
    e.bounces = L.bouncing;
    e.becomes = L.mummies[g.transformIdx % L.nmummies];
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
    const float halfH = 14.0f, halfW = 8.0f;
    e.vy += GRAVITY * dt;
    if (e.vy > MAXFALL) e.vy = MAXFALL;
    float feetOld = e.y + halfH;
    e.y += e.vy * dt;
    float feet = e.y + halfH;
    if (e.vy >= 0) {                                   // land on a platform top
        for (const SDL_FRect& pl : g.platforms)
            if (e.x > pl.x && e.x < pl.x + pl.w && feetOld <= pl.y + 1 && feet >= pl.y) {
                e.y = pl.y - halfH; e.vy = 0;
                if (pl.y < FLOOR_TOP - 1) e.bounces = levelDef(g).bouncing;
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
    e.x = std::clamp(e.x, e.r, LOGW - e.r);
    float ahead = e.x + (e.vx > 0 ? halfW : -halfW);   // ground under the leading foot?
    if (ahead <= ground->x || ahead >= ground->x + ground->w) {
        if (e.bounces > 0) { e.vx = -e.vx; e.bounces--; }
        else { e.phase = EP_FALL; e.x += (e.vx > 0 ? 2.0f : -2.0f); }
    }
}

// Transformed chasers: each kind flies and bounces off the walls differently.
void updateFlyer(Game& g, Enemy& e, float dt, float pcx, float pcy) {
    float s = FLY_SPEED + g.level * 2.0f;
    switch (e.kind) {
        case EK_BIRD: {                                // gentle homing, constant speed
            float dx = pcx - e.x, dy = pcy - e.y, len = std::sqrt(dx*dx + dy*dy) + 1e-3f;
            float spd = std::sqrt(e.vx*e.vx + e.vy*e.vy) + 1e-3f;
            e.vx += dx/len * 40*dt; e.vy += dy/len * 40*dt;
            float n = std::sqrt(e.vx*e.vx + e.vy*e.vy);
            e.vx = e.vx/n*spd; e.vy = e.vy/n*spd; break;
        }
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
    e.x += e.vx * dt; e.y += e.vy * dt;
    if (e.x < e.r)        { e.x = e.r;        e.vx = std::fabs(e.vx); }
    if (e.x > LOGW - e.r) { e.x = LOGW - e.r; e.vx = -std::fabs(e.vx); }
    if (e.y < e.r)        { e.y = e.r;        e.vy = std::fabs(e.vy); }
    if (e.y > LOGH - e.r) { e.y = LOGH - e.r; e.vy = -std::fabs(e.vy); }
}

// ---------------------------------------------------------------------------
// Simulation
// ---------------------------------------------------------------------------
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
                if (g.lives <= 0) g.state = GAMEOVER;
                else { resetPlayer(g, true); g.startAnim = START_TOTAL; }  // new life intro
            }
        }
        return;
    }
    // Hold the simulation while the "START!" intro slides in and settles.
    if (g.startAnim > 0.0f) {
        g.startAnim -= dt;
        return;
    }
    if (p.invuln > 0) p.invuln -= dt;

    // Horizontal movement.
    p.vx = (in.right ? MOVE : 0.0f) - (in.left ? MOVE : 0.0f);
    if (p.vx > 0) p.face = 1;
    else if (p.vx < 0) p.face = -1;
    p.x += p.vx * dt;
    p.x = std::clamp(p.x, BORDER_SOLID_X, LOGW - PW - BORDER_SOLID_X);

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
            break;
        }
    }

    // Bomb collection (lowest remaining index is the lit one).
    int lit = -1;
    for (int i = 0; i < (int)g.bombs.size(); ++i)
        if (!g.bombs[i].collected) { lit = i; break; }

    for (int i = 0; i < (int)g.bombs.size(); ++i) {
        Bomb& b = g.bombs[i];
        if (b.collected) continue;
        if (b.x > p.x - BOMB_HALF_W && b.x < p.x + PW + BOMB_HALF_W &&
            b.y > p.y - BOMB_HALF_H && b.y < p.y + PH + BOMB_HALF_H) {
            b.collected = true;
            g.bombsLeft--;
            g.explosions.push_back({b.x, b.y, 0.0f});   // quick clear burst
            int gain;
            if (i == lit) {                       // grabbed the lit one, in sequence
                g.streak = std::min(g.streak + 1, 5);
                gain = 100 * g.streak;
                g.powerMeter += 1.0f;             // lit bombs charge the P orb faster
            } else {
                g.streak = 1;
                gain = 100;
                g.powerMeter += 0.5f;
            }
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
                g.orbFamily = (int)(g.rng() % 7);   // fixed colour/value for its life
            }
        }
    }

    // Move the orb, bouncing it around the play area.
    if (g.orbActive) {
        float oldOrbX = g.orbX;
        float oldOrbY = g.orbY;
        g.orbX += g.orbVx * dt;
        g.orbY += g.orbVy * dt;
        if (g.orbX < ORB_R) { g.orbX = ORB_R; g.orbVx = std::fabs(g.orbVx); }
        if (g.orbX > LOGW - ORB_R) {
            g.orbX = LOGW - ORB_R;
            g.orbVx = -std::fabs(g.orbVx);
        }
        if (g.orbY < ORB_R) { g.orbY = ORB_R; g.orbVy = std::fabs(g.orbVy); }
        if (g.orbY > FLOOR_TOP - ORB_R) {
            g.orbY = FLOOR_TOP - ORB_R;
            g.orbVy = -std::fabs(g.orbVy);
        }

        // Bounce on platform faces (excluding the ground platform handled above).
        for (const SDL_FRect& pl : g.platforms) {
            if (pl.y >= FLOOR_TOP - 1.0f) continue;
            bool overlapX = g.orbX + ORB_R > pl.x && g.orbX - ORB_R < pl.x + pl.w;
            bool overlapY = g.orbY + ORB_R > pl.y && g.orbY - ORB_R < pl.y + pl.h;
            if (!overlapX || !overlapY) continue;

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
            int gain = POWER_POINTS[idx] * g.multiplier;
            g.score += gain;
            g.popups.push_back({g.orbX, g.orbY, 0.0f, gain});
        }
    }

    // Every BONUS_LIMIT points, offer a B coin (while the multiplier can still
    // grow and the field is clear of one). The threshold always advances so it
    // never sticks even when no coin is dropped.
    if (g.score >= g.nextBonusScore) {
        g.nextBonusScore += BONUS_LIMIT;
        if (!g.bonusActive && g.multiplier < MAX_MULT && g.freezeTimer <= 0.0f)
            spawnBonus(g);
    }

    // Bonus "B" coin: patrol its platform, and on pickup bump the multiplier.
    if (g.bonusActive) {
        g.bonusAnim += dt;
        g.bonusX += g.bonusVx * dt;
        const float lo = g.bonusPlatX + BONUS_W * 0.5f;
        const float hi = g.bonusPlatX + g.bonusPlatW - BONUS_W * 0.5f;
        if (g.bonusX < lo) { g.bonusX = lo; g.bonusVx = std::fabs(g.bonusVx); }
        if (g.bonusX > hi) { g.bonusX = hi; g.bonusVx = -std::fabs(g.bonusVx); }

        if (g.bonusX > p.x - BONUS_W * 0.5f && g.bonusX < p.x + PW + BONUS_W * 0.5f &&
            g.bonusY > p.y - BONUS_H * 0.5f && g.bonusY < p.y + PH + BONUS_H * 0.5f) {
            int gain = BONUS_POINTS * g.multiplier;   // valued at the current x
            g.score += gain;
            g.popups.push_back({g.bonusX, g.bonusY - 6, 0.0f, gain});
            g.multiplier = std::min(g.multiplier + 1, MAX_MULT);   // then it grows
            g.bonusActive = false;
        }
    }

    // Age out floating score popups.
    for (auto it = g.popups.begin(); it != g.popups.end();) {
        it->age += dt;
        it->y -= 18.0f * dt;
        if (it->age > 1.0f) it = g.popups.erase(it); else ++it;
    }

    // Age out clear explosions (3 quick frames, then gone).
    for (auto it = g.explosions.begin(); it != g.explosions.end();) {
        it->age += dt;
        if (it->age >= 3 * EXPL_FRAME) it = g.explosions.erase(it); else ++it;
    }

    if (g.bombsLeft <= 0) {
        g.score += 500;            // round-clear bonus
        g.state = ROUNDCLEAR;
        g.clearTimer = CLEAR_TIME;
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

        if (e.phase == EP_APPEAR || e.phase == EP_DISAPPEAR) {
            ++it;
            continue;   // intangible while appearing/disappearing
        }

        float ex = pcx - e.x, ey = pcy - e.y;
        bool touching = ex * ex + ey * ey < (e.r + 9.0f) * (e.r + 9.0f);
        if (frozen) {
            if (touching) {                             // kill the frozen chaser
                int idx = std::min(g.killCount, (int)(std::size(KILL_POINTS)) - 1);
                int gain = KILL_POINTS[idx] * g.streak * g.multiplier;
                g.score += gain;
                g.popups.push_back({e.x, e.y - 6, 0.0f, gain});
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
        case GAMEOVER:
            break;
    }
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------
void drawBomb(SDL_Renderer* r, const Bomb& b, bool lit, float t) {
    // Keep render size consistent with collision extents.
    const float dw = BOMB_HALF_W * 2.0f;
    const float dh = BOMB_HALF_H * 2.0f;
    drawSprite(r, SP_BOMB, b.x - dw / 2, b.y - dh / 2, dw, dh, false);
    if (lit) {                                       // animated fuse spark
        // Original bomb_activated has 6 frames over 0.3s -> 0.05s per frame.
        int step = (int)(std::fmod(t, 0.3f) / 0.05f) % 6;
        const float fx = b.x + dw * 0.10f;
        const float fy = b.y - dh * 0.36f;
        static const Color sparkCol[6] = {
            {255, 190, 70}, {255, 225, 110}, {255, 245, 160},
            {255, 225, 110}, {255, 190, 70}, {255, 145, 40}
        };
        static const float sparkR[6] = {1.8f, 2.3f, 2.7f, 2.3f, 2.0f, 1.6f};
        setCol(r, sparkCol[step]);
        fillCircle(r, fx, fy, sparkR[step]);
        setCol(r, Color{255, 255, 255});
        fillCircle(r, fx, fy, step == 2 ? 1.0f : 0.8f);
    }
}

void drawPlayer(SDL_Renderer* r, const Player& p, float t, bool dying,
                int deathPhase, int deathFrame) {
    if (dying) {
        const JackVarFrame* fr = nullptr;
        if (deathPhase == DP_DANCING) fr = &g_jackDance[std::clamp(deathFrame, 0, 2)];
        else if (deathPhase == DP_FALLING) fr = &g_jackPlf[std::clamp(deathFrame, 0, 3)];
        else if (deathPhase == DP_DEAD || deathPhase == DP_WAIT)
            fr = &g_jackDead[std::clamp(deathFrame, 0, 3)];
        if (fr && fr->tex) {
            const float scale = 26.0f / 16.0f;
            const float dw = fr->w * scale;
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
    const float dw = 26.0f, dh = 28.0f;
    SDL_FRect dst{p.x + PW / 2 - dw / 2, p.y + PH - dh, dw, dh};
    if (g_jackTex[f]) SDL_RenderTexture(r, g_jackTex[f], nullptr, &dst);
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
    const float w = 26.0f, h = 24.0f;
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
    const float w = 26.0f, h = 28.0f;
    SDL_FRect dst{e.x - w / 2, e.y - h / 2, w, h};
    drawTexTinted(r, body, dst, false, 255, 255, 255);
    if (eye) drawTexTinted(r, eye, dst, false, eyePulse(t), 0, 0);
}

void drawEnemy(SDL_Renderer* r, const Enemy& e, float t, bool frozen,
               float freezeTimer) {
    // Mummies flash white as they pop in; flyers blink as the freeze wears off.
    if (e.phase == EP_APPEAR && std::fmod(t, 0.12f) < 0.06f) return;
    if (frozen && freezeTimer < 1.0f && std::fmod(t, 0.16f) < 0.08f) return;
    if (frozen) {
        // Grabbing the P turns the chasers into spinning collectible coins.
        const Sprite& c = g_coinFrames[(int)(t * 10.0f) % 7];
        if (c.tex) {
            const float w = 24.0f, h = 24.0f;
            SDL_FRect dst{e.x - w / 2, e.y - h / 2, w, h};
            SDL_RenderTexture(r, c.tex, nullptr, &dst);
        }
        return;
    }
    if (e.kind == EK_BIRD)  { drawBird(r, e, t);  return; }
    if (e.kind == EK_MUMMY) { drawMummy(r, e, t); return; }
    float w, h;
    SpriteId id = enemySprite(e, t, w, h);
    bool flip = e.vx < 0;
    drawSprite(r, id, e.x - w / 2, e.y - h / 2, w, h, flip);
}

// The Power orb: a pulsing, colour-cycling ball. Grab it to freeze the chasers.
void drawPowerOrb(SDL_Renderer* r, float x, float y, float t, int family) {
    int fam = (family % 7 + 7) % 7;
    int phase = (int)(t / 0.1f) & 3;                   // cycle the 4 shades ~10/s
    const Sprite& f = g_orbCycle[fam][phase];
    if (f.tex) {
        const float d = f.w * 1.5f;                    // 1.5x the 16px source sprite
        SDL_FRect dst{x - d / 2, y - d / 2, d, d};     // drawn upright, no rotation
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

    // Lives: up to 7 Jack icons in the bottom-left corner.
    int shown = std::min(g.lives, 7);
    for (int i = 0; i < shown; ++i) {
        SDL_FRect dst{2.0f + i * 15.0f, top + 1, (float)LIVE_W * 14 / LIVE_H, 14};
        if (g_liveTex) SDL_RenderTexture(r, g_liveTex, nullptr, &dst);
    }

    // ROUND (green) over -N- (white), centered just right of the life icons.
    drawTextCentered(r, "ROUND", 132, l1, 1, {80, 230, 90});
    std::snprintf(buf, sizeof(buf), "-%d-", g.level);
    drawTextCentered(r, buf, 132, l2, 1, {255, 255, 255});

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

void render(SDL_Renderer* r, const Game& g) {
    useWorld(r);

    if (g.state == TITLE) {
        // Welcome screen: solid black background with the logo centered in the
        // play area. Drawn in raw screen pixels so the logo keeps its true
        // proportions.
        useScreen(r);
        setCol(r, {0, 0, 0});
        SDL_RenderClear(r);
        const float bx = (SCREEN_W - BANNER_W) / 2.0f;
        const float by = HUD_H + (GAME_H - BANNER_H) / 2.0f;   // play-area middle
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

    // Bombs (lit = lowest remaining index).
    int lit = -1;
    for (int i = 0; i < (int)g.bombs.size(); ++i)
        if (!g.bombs[i].collected) { lit = i; break; }
    for (int i = 0; i < (int)g.bombs.size(); ++i)
        if (!g.bombs[i].collected) drawBomb(r, g.bombs[i], i == lit, g.time);

    // Bomb-clear explosions: play the 3 frames quickly, small -> big.
    for (const Explosion& ex : g.explosions) {
        int fi = std::min(2, (int)(ex.age / EXPL_FRAME));
        const Sprite& f = g_explFrames[fi];
        if (!f.tex) continue;
        const float s = 1.6f;                          // a touch bigger than a bomb
        SDL_FRect dst{ex.x - f.w * s / 2, ex.y - f.h * s / 2, f.w * s, f.h * s};
        SDL_RenderTexture(r, f.tex, nullptr, &dst);
    }

    if (g.orbActive) drawPowerOrb(r, g.orbX, g.orbY, g.time, g.orbFamily);

    // Bonus "B" coin, spinning through its four frames (drawn at 2x).
    if (g.bonusActive) {
        const Sprite& f = g_bonusFrames[(int)(g.bonusAnim * 12.0f) % 4];
        if (f.tex) {
            SDL_FRect dst{g.bonusX - f.w, g.bonusY - f.h,
                          f.w * 2.0f, f.h * 2.0f};
            SDL_RenderTexture(r, f.tex, nullptr, &dst);
        }
    }

    bool frozen = g.freezeTimer > 0.0f;
    for (const Enemy& e : g.enemies) drawEnemy(r, e, g.time, frozen, g.freezeTimer);
    drawPlayer(r, g.p, g.time, g.playerDying, g.deathPhase, g.deathFrame);

    useScreen(r);
    drawPlayfieldBorder(r, currentScreen(g));
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
    } else if (g.state == GAMEOVER) {
        drawTextCentered(r, "GAME OVER", LOGW / 2.0f, LOGH / 2.0f - 40, 5,
                         {255, 80, 80});
        std::snprintf(buf, sizeof(buf), "FINAL SCORE %06d", g.score);
        drawTextCentered(r, buf, LOGW / 2.0f, LOGH / 2.0f + 20, 2, {255, 255, 255});
        drawTextCentered(r, "PRESS R TO PLAY AGAIN", LOGW / 2.0f,
                         LOGH / 2.0f + 60, 2, {200, 200, 200});
    }

    useScreen(r);
    drawStartIntro(r, g);
    drawHud(r, g);
}

// ---------------------------------------------------------------------------
// Headless self-test: run the simulation with scripted input, no window.
// ---------------------------------------------------------------------------
int selfTest(int steps) {
    if (int bad = validateSprites()) {
        std::printf("FAIL: %d malformed sprite row(s)\n", bad);
        return 1;
    }
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
    for (State st : {TITLE, PLAYING, ROUNDCLEAR, GAMEOVER}) {
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

    SDL_Window* win = SDL_CreateWindow("Bomb Jack", SCREEN_W * WIN_SCALE,
                                       SCREEN_H * WIN_SCALE, SDL_WINDOW_RESIZABLE);
    if (!win) {
        std::fprintf(stderr, "CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
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
    SDL_SetRenderLogicalPresentation(ren, SCREEN_W, SCREEN_H,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);

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
