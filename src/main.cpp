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
#include "sprites_pack.h"   // official Bomb Jack character sprites, packed

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
namespace {

constexpr int LOGW = 512;   // world coordinate space (game logic + drawing)
constexpr int LOGH = 448;

// The arcade screen is 224x224. We render the whole world into a 224x224 logical
// canvas (via a render scale) and present it pixel-doubled to a 448x448 window.
constexpr int VIEW      = 224;
constexpr int WIN_SCALE = 2;

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

struct Enemy { float x, y, vx, vy, r; };

struct Game {
    int   state = TITLE;
    int   score = 0, lives = 3, level = 1, streak = 1, bombsLeft = 0;
    float clearTimer = 0.0f;
    float time = 0.0f;                 // animation clock
    Player p{};
    std::vector<SDL_FRect> platforms;
    std::vector<Bomb>      bombs;
    std::vector<Enemy>     enemies;
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

// ---------------------------------------------------------------------------
// Sprites — Jack, bombs and enemies are cropped from the embedded official
// Bomb Jack sprite sheet (sprites_pack.h); the girder platform is still a small
// pixel map (its colour is stage-specific in the arcade). No runtime files.
// ---------------------------------------------------------------------------
enum SpriteId {
    SP_JACK_STAND, SP_JACK_WALK, SP_JACK_JUMP,
    SP_BOMB, SP_ENEMY1, SP_ENEMY2, SP_PLAT, SP_COUNT
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
        {SP_JACK_STAND, 0, 0, 24, 24}, {SP_JACK_WALK, 24, 0, 24, 24},
        {SP_JACK_JUMP, 48, 0, 24, 24}, {SP_BOMB, 72, 0, 16, 18},
        {SP_ENEMY1, 88, 0, 18, 16},    {SP_ENEMY2, 106, 0, 18, 16},
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
}

// ---------------------------------------------------------------------------
// Level / entity setup
// ---------------------------------------------------------------------------
void initPlatforms(Game& g) {
    g.platforms = {
        {0,   430, LOGW, 18},   // floor
        {54,  348, 132,  12},
        {326, 348, 132,  12},
        {190, 262, 132,  12},
        {40,  176, 118,  12},
        {354, 176, 118,  12},
        {198, 96,  116,  12},
    };
}

void spawnBombs(Game& g) {
    g.bombs.clear();
    for (const SDL_FRect& pl : g.platforms) {
        bool floor = (pl.w >= LOGW - 1);
        int n = floor ? 5 : std::clamp((int)(pl.w / 70.0f), 2, 4);
        for (int i = 0; i < n; ++i) {
            float t = (n == 1) ? 0.5f : (i + 1.0f) / (n + 1.0f);
            g.bombs.push_back({pl.x + t * pl.w, pl.y - BOMB_R - 1.0f, false});
        }
    }
    // A few mid-air bombs that demand some gliding to reach.
    const float air[][2] = {{256, 200}, {118, 122}, {394, 122}, {256, 46}};
    for (auto& a : air) g.bombs.push_back({a[0], a[1], false});
    g.bombsLeft = (int)g.bombs.size();
}

void spawnEnemies(Game& g) {
    g.enemies.clear();
    int count = std::min(1 + g.level, 4);
    float speed = std::min(72.0f + g.level * 12.0f, 160.0f);
    std::uniform_real_distribution<float> sign(-1.0f, 1.0f);
    for (int i = 0; i < count; ++i) {
        float ang = (i + 0.5f) / count * 6.2831853f;
        Enemy e;
        e.r  = 11.0f;
        e.x  = 60.0f + i * (LOGW - 120.0f) / std::max(1, count - 1);
        e.y  = 70.0f + (i % 2) * 40.0f;
        e.vx = std::cos(ang) * speed + sign(g.rng) * 10.0f;
        e.vy = std::sin(ang) * speed + sign(g.rng) * 10.0f;
        g.enemies.push_back(e);
    }
}

void resetPlayer(Game& g, bool invuln) {
    g.p.x = LOGW / 2.0f - PW / 2.0f;
    g.p.y = 430.0f - PH;
    g.p.vx = g.p.vy = 0.0f;
    g.p.onGround = true;
    g.p.invuln = invuln ? INVULN_TIME : 0.0f;
}

void startRound(Game& g) {
    spawnBombs(g);
    spawnEnemies(g);
    resetPlayer(g, true);
    g.streak = 1;
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
            if (i == lit) {
                g.streak = std::min(g.streak + 1, 5);
                g.score += 100 * g.streak;
            } else {
                g.streak = 1;
                g.score += 100;
            }
        }
    }

    if (g.bombsLeft <= 0) {
        g.score += 500;            // round-clear bonus
        g.state = ROUNDCLEAR;
        g.clearTimer = CLEAR_TIME;
        return;
    }

    // Enemies: drift, bounce off the screen edges, gently home in.
    float pcx = p.x + PW / 2, pcy = p.y + PH / 2;
    for (Enemy& e : g.enemies) {
        float dx = pcx - e.x, dy = pcy - e.y;
        float len = std::sqrt(dx * dx + dy * dy) + 1e-3f;
        float spd = std::sqrt(e.vx * e.vx + e.vy * e.vy) + 1e-3f;
        e.vx += (dx / len) * 40.0f * dt;
        e.vy += (dy / len) * 40.0f * dt;
        float nspd = std::sqrt(e.vx * e.vx + e.vy * e.vy);
        e.vx = e.vx / nspd * spd;     // keep speed constant
        e.vy = e.vy / nspd * spd;
        e.x += e.vx * dt;
        e.y += e.vy * dt;
        if (e.x < e.r)         { e.x = e.r;         e.vx = std::fabs(e.vx); }
        if (e.x > LOGW - e.r)  { e.x = LOGW - e.r;  e.vx = -std::fabs(e.vx); }
        if (e.y < e.r)         { e.y = e.r;         e.vy = std::fabs(e.vy); }
        if (e.y > LOGH - e.r)  { e.y = LOGH - e.r;  e.vy = -std::fabs(e.vy); }

        if (p.invuln <= 0) {
            float ex = pcx - e.x, ey = pcy - e.y;
            if (ex * ex + ey * ey < (e.r + 9.0f) * (e.r + 9.0f)) {
                g.lives--;
                if (g.lives <= 0) {
                    g.state = GAMEOVER;
                } else {
                    resetPlayer(g, true);
                }
                return;
            }
        }
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
    // Sprite is 12x14; draw it centred on the bomb (radius 7 -> ~16x18 box).
    drawSprite(r, SP_BOMB, b.x - 8, b.y - 9, 16, 18, false);
    if (lit) {                                       // animated fuse spark
        bool flick = std::fmod(t, 0.2f) < 0.1f;
        setCol(r, flick ? Color{255, 230, 90} : Color{255, 150, 40});
        fillCircle(r, b.x + 1, b.y - 11, flick ? 3.0f : 2.0f);
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
    drawSprite(r, id, p.x - 2, p.y, PW + 4, PH, p.face < 0);
}

void drawEnemy(SDL_Renderer* r, const Enemy& e, float t) {
    SpriteId id = (std::fmod(t, 0.3f) < 0.15f) ? SP_ENEMY1 : SP_ENEMY2;
    drawSprite(r, id, e.x - 14, e.y - 12, 28, 24, e.vx < 0);
}

// Night-sky backdrop with a crescent moon and Egyptian pyramids (round 1 vibe).
void drawBackground(SDL_Renderer* r, int level) {
    setCol(r, {18, 16, 38});
    SDL_RenderClear(r);
    if (g_bgTex && g_bgCount > 0) {
        int idx = (level - 1) % g_bgCount;          // round 1 -> first backdrop
        if (idx < 0) idx += g_bgCount;
        SDL_FRect src{(float)(idx * BG_W), 0, (float)BG_W, (float)BG_H};
        SDL_FRect dst{0, 0, (float)LOGW, (float)LOGH};
        SDL_RenderTexture(r, g_bgTex, &src, &dst);
    }
}

void render(SDL_Renderer* r, const Game& g) {
    drawBackground(r, g.level);

    if (g.state == TITLE) {
        drawSprite(r, SP_JACK_STAND, LOGW / 2.0f - 40, 70, 80, 80, false);
        drawTextCentered(r, "BOMB JACK", LOGW / 2.0f, 180, 6, {255, 210, 60});
        drawTextCentered(r, "PRESS SPACE TO START", LOGW / 2.0f, 260, 2,
                         std::fmod(g.time, 1.0f) < 0.5f ? Color{230, 230, 230}
                                                        : Color{120, 120, 120});
        drawTextCentered(r, "ARROWS MOVE   SPACE JUMP-FLOAT", LOGW / 2.0f, 310, 1,
                         {150, 150, 170});
        return;
    }

    // Platforms — tiled girder texture.
    for (const SDL_FRect& pl : g.platforms)
        SDL_RenderTextureTiled(r, g_sprites[SP_PLAT].tex, nullptr, 1.0f, &pl);

    // Bombs (lit = lowest remaining index).
    int lit = -1;
    for (int i = 0; i < (int)g.bombs.size(); ++i)
        if (!g.bombs[i].collected) { lit = i; break; }
    for (int i = 0; i < (int)g.bombs.size(); ++i)
        if (!g.bombs[i].collected) drawBomb(r, g.bombs[i], i == lit, g.time);

    for (const Enemy& e : g.enemies) drawEnemy(r, e, g.time);
    drawPlayer(r, g.p, g.time);

    // HUD.
    char buf[64];
    std::snprintf(buf, sizeof(buf), "SCORE %06d", g.score);
    drawText(r, buf, 8, 6, 2, {255, 255, 255});
    std::snprintf(buf, sizeof(buf), "ROUND %d", g.level);
    drawTextCentered(r, buf, LOGW / 2.0f, 6, 2, {200, 220, 255});
    std::snprintf(buf, sizeof(buf), "LIVES %d", g.lives);
    drawText(r, buf, LOGW - textWidth(buf, 2) - 8, 6, 2, {255, 180, 180});
    if (g.streak > 1) {
        std::snprintf(buf, sizeof(buf), "BONUS X%d", g.streak);
        drawText(r, buf, 8, 26, 1, {255, 230, 120});
    }

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
    SDL_Window* win = SDL_CreateWindow("rendertest", VIEW * WIN_SCALE,
                                       VIEW * WIN_SCALE, 0);
    SDL_Renderer* ren = win ? SDL_CreateRenderer(win, nullptr) : nullptr;
    if (!ren) {
        std::fprintf(stderr, "render init failed: %s\n", SDL_GetError());
        if (win) SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderLogicalPresentation(ren, VIEW, VIEW,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderScale(ren, (float)VIEW / LOGW, (float)VIEW / LOGH);
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

    SDL_Window* win = SDL_CreateWindow("Bomb Jack", VIEW * WIN_SCALE,
                                       VIEW * WIN_SCALE, SDL_WINDOW_RESIZABLE);
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
    SDL_SetRenderLogicalPresentation(ren, VIEW, VIEW,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderScale(ren, (float)VIEW / LOGW, (float)VIEW / LOGH);

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
