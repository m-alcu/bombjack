#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "core/constants.h"
#include "render/gfx.h"
#include "render/textures.h"
#include "render/background.h"
#include "game/game.h"
#include "render/render.h"
#include "render/world.h"
#include "platform/audio.h"

// ---------------------------------------------------------------------------
// Headless simulation smoke test
// ---------------------------------------------------------------------------
static int selfTest(int steps) {
    Game g;
    g.rng.seed(1);   // reproducible runs
    initPlatforms(g);
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
    std::printf("selftest ok: %d steps, score=%d lives=%d level=%d state=%d\n",
                steps, g.score, g.lives, g.round, g.state);
    return 0;
}

// ---------------------------------------------------------------------------
// Headless render smoke test (run with SDL_VIDEODRIVER=dummy for CI)
// ---------------------------------------------------------------------------
static int renderTest() {
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
    buildTextures(ren);
    buildBackground(ren);

    Game g;
    g.rng.seed(1);
    initPlatforms(g);
    buildJackSprite();
    loadHiScores(g);
    startGame(g);
    int frames = 0;
    for (State st : {TITLE, OPTIONS, PLAYING, ROUNDCLEAR, SPECIALBONUS, LUCKY, GAMEOVER, NAMEENTRY}) {
        g.state = st;
        render(ren, g);
        SDL_RenderPresent(ren);
        ++frames;
    }

    destroyTextures();
    destroyBackground();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    std::printf("rendertest ok: %d frames, all states drawn\n", frames);
    return 0;
}

// Render one level's PLAYING view to a BMP (debug tool).
static int shot(int level, const char* out) {
    if (!SDL_Init(SDL_INIT_VIDEO)) return 1;
    SDL_Window* win = SDL_CreateWindow("shot", SCREEN_W * WIN_SCALE, SCREEN_H * WIN_SCALE, 0);
    SDL_Renderer* ren = win ? SDL_CreateRenderer(win, nullptr) : nullptr;
    if (!ren) { SDL_Quit(); return 1; }
    SDL_SetRenderLogicalPresentation(ren, SCREEN_W, SCREEN_H, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    buildTextures(ren); buildBackground(ren);
    Game g; g.rng.seed(1); initPlatforms(g); buildJackSprite(); loadHiScores(g);
    g.opt.firstRound = level; startGame(g); g.startAnim = 0.0f;
    render(ren, g);
    SDL_Surface* s = SDL_RenderReadPixels(ren, nullptr);
    if (s) { SDL_SaveBMP(s, out); SDL_DestroySurface(s); std::printf("shot %d -> %s\n", level, out); }
    destroyTextures(); destroyBackground();
    SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();
    return 0;
}

// Replay a demo recording headless and print Jack's position after each arcade
// frame in arcade pixels, plus each bomb pickup (debug tool, for comparing
// against the arcade in MAME). `cycle` parity picks recording A or B.
static int demoTrace(int round, int cycle) {
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

// ---------------------------------------------------------------------------
// Entry point
// Play a level headless at the arcade frame rate with an untouchable, idle
// Jack and print every enemy's sprite each frame (debug tool, for comparing
// enemy animation against the arcade in MAME).
static int enemyTrace(int level, int frames) {
    Game g;
    g.rng.seed(1);
    initPlatforms(g);
    g.opt.firstRound = level;
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

// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--selftest") == 0)
            return selfTest(i + 1 < argc ? std::atoi(argv[i + 1]) : 600);
        if (std::strcmp(argv[i], "--rendertest") == 0)
            return renderTest();
        if (std::strcmp(argv[i], "--demotrace") == 0)
            return demoTrace(i + 1 < argc ? std::atoi(argv[i + 1]) : 1,
                             i + 2 < argc ? std::atoi(argv[i + 2]) : 0);
        if (std::strcmp(argv[i], "--enemytrace") == 0)
            return enemyTrace(i + 1 < argc ? std::atoi(argv[i + 1]) : 1,
                              i + 2 < argc ? std::atoi(argv[i + 2]) : 3000);
        if (std::strcmp(argv[i], "--shot") == 0)
            return shot(i+1 < argc ? std::atoi(argv[i+1]) : 1, i+2 < argc ? argv[i+2] : "shot.bmp");
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
    SDL_SetWindowAspectRatio(win, 3.0f / 4.0f, 3.0f / 4.0f);

    // Prefer stable backends; Vulkan can flicker on some Linux/X11 setups.
    SDL_Renderer* ren = nullptr;
    for (const char* drv : {"opengl", "opengles2", "gpu", "software"}) {
        SDL_Renderer* r = SDL_CreateRenderer(win, drv);
        if (!r) continue;
        const char* name = SDL_GetRendererName(r);
        if (name && SDL_strcmp(name, "vulkan") == 0) { SDL_DestroyRenderer(r); continue; }
        ren = r;
        break;
    }
    if (!ren) ren = SDL_CreateRenderer(win, nullptr);
    if (!ren) {
        std::fprintf(stderr, "CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderVSync(ren, 1);
    // STRETCH maps the 224×256 render onto the whole 3:4 window (non-square pixels).
    SDL_SetRenderLogicalPresentation(ren, SCREEN_W, SCREEN_H,
                                     SDL_LOGICAL_PRESENTATION_STRETCH);

    buildTextures(ren);
    buildBackground(ren);
    audio::init();   // silent no-op fallback if no audio device is available

    Game g;
    initPlatforms(g);
    buildJackSprite();
    loadHiScores(g);   // seeds the Godot default table on first run

    bool running  = true;
    bool jumpEdge = false;
    Uint64 prev   = SDL_GetTicks();
    double acc    = 0.0;
    const double STEP = 1.0 / 120.0;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = false;
            if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
                SDL_Scancode sc = e.key.scancode;
                if (sc == SDL_SCANCODE_Q && g.state != NAMEENTRY) running = false;
                else if (g.state == NAMEENTRY) {
                    // High-score name entry: type A-Z / 0-9, backspace, enter.
                    if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z) {
                        if (g.nameEntry.len < NAME_MAX)
                            g.nameEntry.text[g.nameEntry.len++] = (char)('A' + (sc - SDL_SCANCODE_A));
                        g.nameEntry.text[g.nameEntry.len] = '\0';
                    } else if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9) {
                        if (g.nameEntry.len < NAME_MAX)
                            g.nameEntry.text[g.nameEntry.len++] = (char)('1' + (sc - SDL_SCANCODE_1));
                        g.nameEntry.text[g.nameEntry.len] = '\0';
                    } else if (sc == SDL_SCANCODE_0) {
                        if (g.nameEntry.len < NAME_MAX) g.nameEntry.text[g.nameEntry.len++] = '0';
                        g.nameEntry.text[g.nameEntry.len] = '\0';
                    } else if (sc == SDL_SCANCODE_BACKSPACE) {
                        if (g.nameEntry.len > 0) g.nameEntry.text[--g.nameEntry.len] = '\0';
                    } else if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER) {
                        if (g.nameEntry.len > 0) confirmNameEntry(g);
                    } else if (sc == SDL_SCANCODE_ESCAPE) {
                        audio::stopTag("nameentry");   // cancel: back to title
                        enterAttract(g, 0);
                    }
                }
                else if (g.state == OPTIONS) {
                    switch (sc) {
                        case SDL_SCANCODE_ESCAPE:
                            enterAttract(g, 2); break;
                        case SDL_SCANCODE_UP: case SDL_SCANCODE_W:
                            g.opt.sel = (g.opt.sel + OPT_COUNT - 1) % OPT_COUNT; break;
                        case SDL_SCANCODE_DOWN: case SDL_SCANCODE_S:
                            g.opt.sel = (g.opt.sel + 1) % OPT_COUNT; break;
                        case SDL_SCANCODE_LEFT: case SDL_SCANCODE_A:
                            optionAdjust(g, -1); break;
                        case SDL_SCANCODE_RIGHT: case SDL_SCANCODE_D:
                            optionAdjust(g, +1); break;
                        case SDL_SCANCODE_SPACE: case SDL_SCANCODE_RETURN:
                            startGame(g); break;
                        default: break;
                    }
                } else {
                    if (sc == SDL_SCANCODE_ESCAPE) running = false;
                    // The attract cycle (title screens and demo) waits for start.
                    const bool attract = g.state == TITLE || g.demo.active;
                    if (sc == SDL_SCANCODE_SPACE) {
                        if (attract) startGame(g);
                        else jumpEdge = true;
                    }
                    if (sc == SDL_SCANCODE_O && attract) {
                        stopDemo(g);
                        g.opt.sel = 0;
                        g.state  = OPTIONS;
                    }
                    if (sc == SDL_SCANCODE_R && g.state == GAMEOVER) startGame(g);
                }
            }
        }

        const bool* ks = SDL_GetKeyboardState(nullptr);
        Input in{};
        in.left       = ks[SDL_SCANCODE_LEFT]  || ks[SDL_SCANCODE_A];
        in.right      = ks[SDL_SCANCODE_RIGHT] || ks[SDL_SCANCODE_D];
        in.up         = ks[SDL_SCANCODE_UP]    || ks[SDL_SCANCODE_W];
        in.down       = ks[SDL_SCANCODE_DOWN]  || ks[SDL_SCANCODE_S];
        in.jumpHeld   = ks[SDL_SCANCODE_SPACE];

        Uint64 now    = SDL_GetTicks();
        double frame  = (now - prev) / 1000.0;
        prev = now;
        if (frame > 0.25) frame = 0.25;
        acc += frame;
        while (acc >= STEP) {
            in.jumpPressed = jumpEdge;
            jumpEdge = false;
            update(g, in, (float)STEP);
            acc -= STEP;
        }

        audio::update();   // reap finished one-shots, keep loops fed
        render(ren, g);
        SDL_RenderPresent(ren);
    }

    audio::shutdown();
    destroyTextures();
    destroyBackground();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
