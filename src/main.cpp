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
#include "sprites_pack.h"   // official Bomb Jack character sprites, packed
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
constexpr float JUMP_VEL    = -355.0f;  // initial jump velocity
constexpr float FLUTTER     = 165.0f;   // upward kick from an in-air tap
constexpr float FLUTTER_MIN = -200.0f;  // cap on upward speed from fluttering
constexpr float GRAVITY     = 980.0f;
constexpr float GLIDE       = 150.0f;   // reduced gravity while holding jump
constexpr float MAXFALL     = 520.0f;

constexpr float PW = 18.0f;             // player size
constexpr float PH = 24.0f;
constexpr float BOMB_R = 7.0f;          // bomb radius

constexpr float INVULN_TIME = 2.0f;     // post-hit invulnerability (s)
constexpr float CLEAR_TIME  = 1.6f;     // round-clear banner duration (s)
constexpr float FREEZE_TIME = 5.0f;     // enemy freeze after grabbing the P orb
constexpr float POWER_NEEDED = 8.0f;    // lit-bomb "charge" needed to spawn a P orb

// Escalating points for each enemy killed during a single freeze (Bomb Jack).
constexpr int KILL_POINTS[] = {100, 200, 300, 500, 800, 1200, 2000};

// Mummies drop in over time and transform into flying chasers on the ground.
constexpr float MUMMY_APPEAR_TIME = 1.1f;   // pop-in pause before it moves
constexpr float MUMMY_SPAWN_DELAY = 3.6f;   // seconds between mummy spawns
constexpr float MUMMY_WALK_SPEED  = 70.0f;  // platform walk speed (world px/s)
constexpr float FLY_SPEED         = 120.0f; // base speed of transformed chasers

enum State { TITLE, PLAYING, ROUNDCLEAR, GAMEOVER };

struct Color { Uint8 r, g, b; };

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
enum EPhase { EP_FLY, EP_APPEAR, EP_WALK, EP_FALL };  // mummy lifecycle; others fly

struct Enemy {
    float x, y, vx, vy, r;
    int   kind   = EK_BIRD;
    int   phase  = EP_FLY;
    float timer  = 0.0f;     // appear countdown
    int   bounces = 0;       // platform direction-changes left before falling
    int   becomes = EK_SPHERE;  // what this mummy transforms into
};

struct Popup { float x, y, age = 0.0f; int value = 0; };  // floating score text

struct Game {
    int   state = TITLE;
    int   score = 0, lives = 3, level = 1, streak = 1, bombsLeft = 0;
    float clearTimer = 0.0f;
    float time = 0.0f;                 // animation clock
    // Power orb / enemy-freeze state.
    float powerMeter = 0.0f;           // charge from caught bombs; spawns orb at POWER_NEEDED
    bool  orbActive = false;           // a P orb is on the field
    float orbX = 0.0f, orbY = 0.0f;
    float freezeTimer = 0.0f;          // >0 while enemies are frozen & killable
    int   killCount = 0;               // enemies killed in the current freeze
    // Mummy spawning.
    float mummyTimer = 0.0f;           // counts up to MUMMY_SPAWN_DELAY
    int   mummiesSpawned = 0;          // mummies dropped this round
    int   transformIdx = 0;            // index into the level's mummy sequence
    Player p{};
    std::vector<SDL_FRect> platforms;
    std::vector<Bomb>      bombs;
    std::vector<Enemy>     enemies;
    std::vector<Popup>     popups;
    std::mt19937           rng{std::random_device{}()};
};

// ---------------------------------------------------------------------------
// 5x7 bitmap font (uppercase + digits + a little punctuation)
// ---------------------------------------------------------------------------
const std::unordered_map<char, std::array<uint8_t, 7>>& font() {
    struct Def { char c; const char* rows[7]; };
    static const Def defs[] = {
        {'0', {"01110","10001","10011","10101","11001","10001","01110"}},
        {'1', {"00100","01100","00100","00100","00100","00100","01110"}},
        {'2', {"01110","10001","00001","00010","00100","01000","11111"}},
        {'3', {"11111","00010","00100","00010","00001","10001","01110"}},
        {'4', {"00010","00110","01010","10010","11111","00010","00010"}},
        {'5', {"11111","10000","11110","00001","00001","10001","01110"}},
        {'6', {"00110","01000","10000","11110","10001","10001","01110"}},
        {'7', {"11111","00001","00010","00100","01000","01000","01000"}},
        {'8', {"01110","10001","10001","01110","10001","10001","01110"}},
        {'9', {"01110","10001","10001","01111","00001","00010","01100"}},
        {'A', {"01110","10001","10001","11111","10001","10001","10001"}},
        {'B', {"11110","10001","10001","11110","10001","10001","11110"}},
        {'C', {"01110","10001","10000","10000","10000","10001","01110"}},
        {'D', {"11100","10010","10001","10001","10001","10010","11100"}},
        {'E', {"11111","10000","10000","11110","10000","10000","11111"}},
        {'F', {"11111","10000","10000","11110","10000","10000","10000"}},
        {'G', {"01110","10001","10000","10111","10001","10001","01111"}},
        {'H', {"10001","10001","10001","11111","10001","10001","10001"}},
        {'I', {"01110","00100","00100","00100","00100","00100","01110"}},
        {'J', {"00111","00010","00010","00010","00010","10010","01100"}},
        {'K', {"10001","10010","10100","11000","10100","10010","10001"}},
        {'L', {"10000","10000","10000","10000","10000","10000","11111"}},
        {'M', {"10001","11011","10101","10101","10001","10001","10001"}},
        {'N', {"10001","11001","11001","10101","10011","10011","10001"}},
        {'O', {"01110","10001","10001","10001","10001","10001","01110"}},
        {'P', {"11110","10001","10001","11110","10000","10000","10000"}},
        {'Q', {"01110","10001","10001","10001","10101","10010","01101"}},
        {'R', {"11110","10001","10001","11110","10100","10010","10001"}},
        {'S', {"01111","10000","10000","01110","00001","00001","11110"}},
        {'T', {"11111","00100","00100","00100","00100","00100","00100"}},
        {'U', {"10001","10001","10001","10001","10001","10001","01110"}},
        {'V', {"10001","10001","10001","10001","10001","01010","00100"}},
        {'W', {"10001","10001","10001","10101","10101","11011","10001"}},
        {'X', {"10001","10001","01010","00100","01010","10001","10001"}},
        {'Y', {"10001","10001","01010","00100","00100","00100","00100"}},
        {'Z', {"11111","00001","00010","00100","01000","10000","11111"}},
        {' ', {"00000","00000","00000","00000","00000","00000","00000"}},
        {':', {"00000","00100","00100","00000","00100","00100","00000"}},
        {'!', {"00100","00100","00100","00100","00100","00000","00100"}},
        {'-', {"00000","00000","00000","11111","00000","00000","00000"}},
        {'.', {"00000","00000","00000","00000","00000","00100","00100"}},
    };
    static const auto map = [] {
        std::unordered_map<char, std::array<uint8_t, 7>> m;
        for (const auto& d : defs) {
            std::array<uint8_t, 7> g{};
            for (int r = 0; r < 7; ++r) {
                uint8_t bits = 0;
                for (int c = 0; c < 5; ++c)
                    if (d.rows[r][c] == '1') bits |= (1 << (4 - c));
                g[r] = bits;
            }
            m[d.c] = g;
        }
        return m;
    }();
    return map;
}

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
void drawChar(SDL_Renderer* r, char ch, float x, float y, int s) {
    auto it = font().find(static_cast<char>(std::toupper((unsigned char)ch)));
    if (it == font().end()) return;
    for (int row = 0; row < 7; ++row)
        for (int col = 0; col < 5; ++col)
            if (it->second[row] & (1 << (4 - col)))
                fillR(r, x + col * s, y + row * s, (float)s, (float)s);
}
int textWidth(const char* t, int s) { return (int)std::strlen(t) * 6 * s; }
void drawText(SDL_Renderer* r, const char* t, float x, float y, int s, Color c) {
    setCol(r, c);
    for (const char* p = t; *p; ++p) {
        drawChar(r, *p, x, y, s);
        x += 6 * s;
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
    SP_JACK_STAND, SP_JACK_WALK, SP_JACK_JUMP,
    SP_BOMB, SP_ENEMY1, SP_ENEMY2,
    SP_MUMMY, SP_MUMMY_WALK, SP_MUMMY_FALL,
    SP_SPHERE1, SP_SPHERE2, SP_ORB1, SP_ORB2, SP_HORN1, SP_HORN2,
    SP_CLUB1, SP_CLUB2, SP_UFO1, SP_UFO2,
    SP_PLAT, SP_COUNT
};

struct Sprite { int w = 0, h = 0; SDL_Texture* tex = nullptr; };
Sprite g_sprites[SP_COUNT];

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

void buildSprites(SDL_Renderer* ren) {
    // Character sprites: crop each from the embedded official sprite sheet.
    struct PackRect { SpriteId id; int x, y, w, h; };
    static const PackRect packRects[] = {
        {SP_JACK_STAND, 0, 0, 15, 16}, {SP_JACK_WALK, 15, 0, 15, 16},
        {SP_JACK_JUMP, 30, 0, 15, 16}, {SP_BOMB, 45, 0, 16, 18},
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
    g.p.x = LOGW / 2.0f - PW / 2.0f;
    g.p.y = FLOOR_TOP - PH;
    g.p.vx = g.p.vy = 0.0f;
    g.p.onGround = true;
    g.p.invuln = invuln ? INVULN_TIME : 0.0f;
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
    g.mummyTimer = 0.0f;
    g.mummiesSpawned = 0;
    g.transformIdx = 0;
    g.popups.clear();
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
    if (ground->y >= FLOOR_TOP - 1.0f) { transformMummy(g, e); return; }  // hit the floor
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
    if (p.invuln > 0) p.invuln -= dt;

    // Horizontal movement.
    p.vx = (in.right ? MOVE : 0.0f) - (in.left ? MOVE : 0.0f);
    if (p.vx > 0) p.face = 1;
    else if (p.vx < 0) p.face = -1;
    p.x += p.vx * dt;
    p.x = std::clamp(p.x, 0.0f, LOGW - PW);

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

    float oldBottom = p.y + PH;
    p.y += p.vy * dt;
    if (p.y < 0) { p.y = 0; if (p.vy < 0) p.vy = 0; }

    // One-way platforms: land only when descending onto the top edge.
    p.onGround = false;
    if (p.vy >= 0) {
        for (const SDL_FRect& pl : g.platforms) {
            bool overlapX = (p.x + PW - 3 > pl.x) && (p.x + 3 < pl.x + pl.w);
            if (overlapX && oldBottom <= pl.y + 1.0f && p.y + PH >= pl.y) {
                p.y = pl.y - PH;
                p.vy = 0;
                p.onGround = true;
                break;
            }
        }
    }

    // Bomb collection (lowest remaining index is the lit one).
    int lit = -1;
    for (int i = 0; i < (int)g.bombs.size(); ++i)
        if (!g.bombs[i].collected) { lit = i; break; }

    for (int i = 0; i < (int)g.bombs.size(); ++i) {
        Bomb& b = g.bombs[i];
        if (b.collected) continue;
        if (b.x > p.x - BOMB_R && b.x < p.x + PW + BOMB_R &&
            b.y > p.y - BOMB_R && b.y < p.y + PH + BOMB_R) {
            b.collected = true;
            g.bombsLeft--;
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
            g.score += gain;
            g.popups.push_back({b.x, b.y - 6, 0.0f, gain});
            // Enough charge spawns a Power orb (once, while none is active).
            if (!g.orbActive && g.freezeTimer <= 0 && g.powerMeter >= POWER_NEEDED) {
                g.powerMeter -= POWER_NEEDED;
                g.orbActive = true;
                g.orbX = b.x;
                g.orbY = b.y - 18.0f;             // float just above the grabbed bomb
            }
        }
    }

    // Power orb pickup -> freeze every enemy and make them killable for a while.
    if (g.orbActive) {
        if (g.orbX > p.x - 10 && g.orbX < p.x + PW + 10 &&
            g.orbY > p.y - 10 && g.orbY < p.y + PH + 10) {
            g.orbActive = false;
            g.freezeTimer = FREEZE_TIME;
            g.killCount = 0;
            g.score += 1000 * g.streak;
            g.popups.push_back({g.orbX, g.orbY, 0.0f, 1000 * g.streak});
        }
    }

    // Age out floating score popups.
    for (auto it = g.popups.begin(); it != g.popups.end();) {
        it->age += dt;
        it->y -= 18.0f * dt;
        if (it->age > 1.0f) it = g.popups.erase(it); else ++it;
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

        if (e.phase == EP_APPEAR) { ++it; continue; }   // intangible while popping in

        float ex = pcx - e.x, ey = pcy - e.y;
        bool touching = ex * ex + ey * ey < (e.r + 9.0f) * (e.r + 9.0f);
        if (frozen) {
            if (touching) {                             // kill the frozen chaser
                int idx = std::min(g.killCount, (int)(std::size(KILL_POINTS)) - 1);
                int gain = KILL_POINTS[idx] * g.streak;
                g.score += gain;
                g.popups.push_back({e.x, e.y - 6, 0.0f, gain});
                g.killCount++;
                it = g.enemies.erase(it);
                continue;
            }
        } else if (p.invuln <= 0 && touching) {
            g.lives--;
            if (g.lives <= 0) g.state = GAMEOVER;
            else resetPlayer(g, true);
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
    // Drawn larger than the collision radius, centred on the bomb, so it reads
    // at a similar scale to Jack and the chasers.
    const float dw = 24.0f, dh = 27.0f;
    drawSprite(r, SP_BOMB, b.x - dw / 2, b.y - dh / 2, dw, dh, false);
    if (lit) {                                       // animated fuse spark
        bool flick = std::fmod(t, 0.2f) < 0.1f;
        setCol(r, flick ? Color{255, 230, 90} : Color{255, 150, 40});
        fillCircle(r, b.x + 1, b.y - dh / 2 - 2, flick ? 4.0f : 3.0f);
    }
}

void drawPlayer(SDL_Renderer* r, const Player& p, float t) {
    if (p.invuln > 0 && std::fmod(t, 0.16f) < 0.08f) return;  // blink when hit
    SpriteId id;
    if (!p.onGround)
        id = SP_JACK_JUMP;
    else if (std::fabs(p.vx) > 1.0f)
        id = (std::fmod(t, 0.2f) < 0.1f) ? SP_JACK_WALK : SP_JACK_STAND;
    else
        id = SP_JACK_STAND;
    // Draw bigger than the collision box (feet anchored to its bottom) so Jack
    // reads at a similar scale to the bombs and chasers.
    const float dw = 26.0f, dh = 28.0f;
    drawSprite(r, id, p.x + PW / 2 - dw / 2, p.y + PH - dh, dw, dh, p.face < 0);
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

void drawEnemy(SDL_Renderer* r, const Enemy& e, float t, bool frozen,
               float freezeTimer) {
    // Mummies flash white as they pop in; flyers blink as the freeze wears off.
    if (e.phase == EP_APPEAR && std::fmod(t, 0.12f) < 0.06f) return;
    if (frozen && freezeTimer < 1.0f && std::fmod(t, 0.16f) < 0.08f) return;
    float w, h;
    SpriteId id = enemySprite(e, t, w, h);
    bool flip = e.vx < 0;
    SDL_Texture* tex = g_sprites[id].tex;
    if (frozen) SDL_SetTextureColorMod(tex, 90, 150, 255);   // frozen blue tint
    drawSprite(r, id, e.x - w / 2, e.y - h / 2, w, h, flip);
    if (frozen) SDL_SetTextureColorMod(tex, 255, 255, 255);
}

// The Power orb: a pulsing, colour-cycling ball. Grab it to freeze the chasers.
void drawPowerOrb(SDL_Renderer* r, float x, float y, float t) {
    static const Color cyc[] = {{255, 80, 80}, {255, 200, 60}, {120, 255, 120},
                                {90, 160, 255}, {220, 90, 230}};
    int k = (int)(t * 8) % 5;
    float rad = 10.0f + std::sin(t * 12.0f) * 1.5f;
    setCol(r, cyc[k]);             fillCircle(r, x, y, rad);
    setCol(r, {255, 255, 255});    fillCircle(r, x, y, rad * 0.5f);
    drawTextCentered(r, "P", x, y - 3, 1, cyc[k]);
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

    // --- Top strip: running score (left) + power gauge (right). ---
    std::snprintf(buf, sizeof(buf), "SCORE %06d", g.score);
    drawText(r, buf, 3, 1, 2, {255, 255, 255});
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

    // HI-SCORE (yellow) over the current score (white), at the right.
    drawTextCentered(r, "HI-SCORE", 192, l1, 1, {255, 230, 60});
    std::snprintf(buf, sizeof(buf), "%06d", g.score);
    drawTextCentered(r, buf, 192, l2, 1, {255, 255, 255});
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
        drawTextCentered(r, "ARROWS MOVE   SPACE JUMP-FLOAT", SCREEN_W / 2.0f,
                         by + BANNER_H + 28, 1, {150, 150, 170});
        drawHud(r, g);
        return;
    }

    drawBackground(r, currentScreen(g));

    // Platforms — tiled girder texture.
    for (const SDL_FRect& pl : g.platforms)
        SDL_RenderTextureTiled(r, g_sprites[SP_PLAT].tex, nullptr, 1.0f, &pl);

    // Bombs (lit = lowest remaining index).
    int lit = -1;
    for (int i = 0; i < (int)g.bombs.size(); ++i)
        if (!g.bombs[i].collected) { lit = i; break; }
    for (int i = 0; i < (int)g.bombs.size(); ++i)
        if (!g.bombs[i].collected) drawBomb(r, g.bombs[i], i == lit, g.time);

    if (g.orbActive) drawPowerOrb(r, g.orbX, g.orbY, g.time);

    bool frozen = g.freezeTimer > 0.0f;
    for (const Enemy& e : g.enemies) drawEnemy(r, e, g.time, frozen, g.freezeTimer);
    drawPlayer(r, g.p, g.time);

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
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
