#include "debug_tools.h"
#include "game/game.h"
#include "render/background.h"
#include "render/render.h"
#include "render/textures.h"
#include "render/world.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

// A game with a fixed seed, so every run prints the same.
static void initTestGame(Game& g) {
    g.rng.seed(1);
    initPlatforms(g);
}

// Headless simulation smoke test.
static int selfTest(int steps) {
    Game g;
    initTestGame(g);
    startGame(g);
    const float dt = 1.0f / 120.0f;
    for (int i = 0; i < steps; ++i) {
        Input in{};
        in.right      = (i / 60) % 2 == 0;
        in.left       = (i / 60) % 2 == 1;
        in.jumpHeld   = (i % 20) < 10;
        in.jumpPressed = (i % 25) == 0;
        update(g, in, dt);
        if (g.p.y < -1 || g.p.y > LOGH || g.p.x < -1 || g.p.x > LOGW) {
            std::printf("FAIL: player escaped bounds at step %d (%.1f,%.1f)\n",
                        i, g.p.x, g.p.y);
            return 1;
        }
    }
    std::printf("selftest ok: %d steps, score=%d lives=%d round=%d state=%d\n",
                steps, g.score, g.lives, g.round, g.state);
    return 0;
}

// Window, renderer and textures for the tools that draw.
struct Gfx {
    SDL_Window*   win = nullptr;
    SDL_Renderer* ren = nullptr;

    bool open(const char* title) {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
            return false;
        }
        win = SDL_CreateWindow(title, SCREEN_W * WIN_SCALE, SCREEN_H * WIN_SCALE, 0);
        ren = win ? SDL_CreateRenderer(win, nullptr) : nullptr;
        if (!ren) {
            std::fprintf(stderr, "render init failed: %s\n", SDL_GetError());
            close();
            return false;
        }
        SDL_SetRenderLogicalPresentation(ren, SCREEN_W, SCREEN_H,
                                         SDL_LOGICAL_PRESENTATION_LETTERBOX);
        buildTextures(ren);
        buildBackground(ren);
        buildJackSprite();
        return true;
    }

    void save(const char* path) {
        SDL_Surface* s = SDL_RenderReadPixels(ren, nullptr);
        if (!s) return;
        SDL_SaveBMP(s, path);
        SDL_DestroySurface(s);
    }

    void close() {
        if (ren) {
            destroyTextures();
            destroyBackground();
            SDL_DestroyRenderer(ren);
        }
        if (win) SDL_DestroyWindow(win);
        SDL_Quit();
    }
};

// Draw every state once (run with SDL_VIDEODRIVER=dummy for CI). With an
// output directory, each frame is also saved there as <state>.bmp.
static int renderTest(const char* outDir) {
    Gfx gfx;
    if (!gfx.open("rendertest")) return 1;
    Game g;
    initTestGame(g);
    loadHiScores(g);
    startGame(g);
    int frames = 0;
    for (State st : {TITLE, OPTIONS, PLAYING, ROUNDCLEAR, SPECIALBONUS, LUCKY, GAMEOVER, NAMEENTRY}) {
        g.state = st;
        render(gfx.ren, g);
        if (outDir) {
            char path[512];
            std::snprintf(path, sizeof(path), "%s/%d.bmp", outDir, (int)st);
            gfx.save(path);
        }
        SDL_RenderPresent(gfx.ren);
        ++frames;
    }
    gfx.close();
    std::printf("rendertest ok: %d frames, all states drawn\n", frames);
    return 0;
}

// Render one round's PLAYING view to a BMP.
static int shot(int round, const char* out) {
    Gfx gfx;
    if (!gfx.open("shot")) return 1;
    Game g;
    initTestGame(g);
    loadHiScores(g);
    g.opt.firstRound = round;
    startGame(g);
    g.startAnim = 0.0f;
    render(gfx.ren, g);
    gfx.save(out);
    std::printf("shot %d -> %s\n", round, out);
    gfx.close();
    return 0;
}

// Replay a demo recording headless and print Jack's position after each arcade
// frame in arcade pixels, plus each bomb pickup (for comparing against the
// arcade in MAME). `cycle` parity picks recording A or B.
static int demoTrace(int round, int cycle) {
    if (round < 1 || round > 2) {
        std::fprintf(stderr, "demotrace: the arcade only has demos of rounds 1 and 2\n");
        return 1;
    }
    Game g;
    g.rng.seed(1);
    g.attract.cycle = cycle;
    startDemo(g, round);
    const float x0 = g.p.x, y0 = g.p.y;
    const float dt = 1.0f / 120.0f;
    int last = -1;
    std::printf("frame,bits,x,y,bombsLeft\n");
    std::vector<bool> had(g.bombs.size(), false);
    for (int i = 0; i < 120 * 120 && g.demo.active; ++i) {
        update(g, Input{}, dt);
        for (size_t b = 0; b < g.bombs.size() && b < had.size(); ++b)
            if (g.bombs[b].collected && !had[b]) {
                had[b] = true;
                std::printf("# bomb %zu at frame %d\n", b, g.demo.frame);
            }
        if (g.demo.frame != last) {
            last = g.demo.frame;
            std::printf("%d,%d,%.2f,%.2f,%d\n", g.demo.frame, g.demo.bits,
                        120.0f + (g.p.x - x0) / PX_TO_WU_X,
                        120.0f + (g.p.y - y0) / PX_TO_WU_Y, g.bombsLeft);
        }
    }
    return 0;
}

// Play a round headless at the arcade frame rate with an untouchable, idle
// Jack and print every enemy's sprite each frame (for comparing enemy
// animation against the arcade in MAME).
static int enemyTrace(int round, int frames) {
    Game g;
    initTestGame(g);
    g.opt.firstRound = round;
    startGame(g);
    g.startAnim = 0.0f;
    std::printf("frame,slot,kind,phase,code,flip,x,y\n");
    for (int f = 0; f < frames; ++f) {
        g.p.invuln = 1.0f;
        update(g, Input{}, 1.0f / ARCADE_HZ);
        for (size_t i = 0; i < g.enemies.size(); ++i) {
            const Enemy& e = g.enemies[i];
            std::printf("%d,%zu,%d,%d,%02x,%d,%.1f,%.1f\n", f, i, e.kind, e.phase,
                        e.code, e.flip, e.x, e.y);
        }
    }
    return 0;
}

int runDebugTool(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        auto num = [&](int k, int def) { return i + k < argc ? std::atoi(argv[i + k]) : def; };
        auto str = [&](int k, const char* def) { return i + k < argc ? argv[i + k] : def; };
        if (std::strcmp(argv[i], "--selftest") == 0)   return selfTest(num(1, 600));
        if (std::strcmp(argv[i], "--rendertest") == 0) return renderTest(str(1, nullptr));
        if (std::strcmp(argv[i], "--shot") == 0)       return shot(num(1, 1), str(2, "shot.bmp"));
        if (std::strcmp(argv[i], "--demotrace") == 0)  return demoTrace(num(1, 1), num(2, 0));
        if (std::strcmp(argv[i], "--enemytrace") == 0) return enemyTrace(num(1, 1), num(2, 3000));
    }
    return -1;
}
