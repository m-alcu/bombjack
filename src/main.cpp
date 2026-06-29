#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "constants.h"
#include "types.h"
#include "renderer.h"
#include "sprites.h"
#include "background.h"
#include "game.h"
#include "draw.h"

// ---------------------------------------------------------------------------
// Headless simulation smoke test
// ---------------------------------------------------------------------------
static int selfTest(int steps) {
    Game g;
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
                steps, g.score, g.lives, g.level, g.state);
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
    buildSprites(ren);
    buildBackground(ren);

    Game g;
    initPlatforms(g);
    buildJackSprite(g.jackSprite);
    startGame(g);
    int frames = 0;
    for (State st : {TITLE, OPTIONS, PLAYING, ROUNDCLEAR, SPECIALBONUS, GAMEOVER}) {
        g.state = st;
        render(ren, g);
        SDL_RenderPresent(ren);
        ++frames;
    }

    destroySprites();
    destroyBackground();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    std::printf("rendertest ok: %d frames, all states drawn\n", frames);
    return 0;
}

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

    buildSprites(ren);
    buildBackground(ren);

    Game g;
    initPlatforms(g);
    buildJackSprite(g.jackSprite);

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
                if (sc == SDL_SCANCODE_Q) running = false;
                else if (g.state == OPTIONS) {
                    switch (sc) {
                        case SDL_SCANCODE_ESCAPE:
                            g.state = TITLE; break;
                        case SDL_SCANCODE_UP: case SDL_SCANCODE_W:
                            g.optSel = (g.optSel + OPT_COUNT - 1) % OPT_COUNT; break;
                        case SDL_SCANCODE_DOWN: case SDL_SCANCODE_S:
                            g.optSel = (g.optSel + 1) % OPT_COUNT; break;
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
                    if (sc == SDL_SCANCODE_SPACE) {
                        if (g.state == TITLE) startGame(g);
                        else jumpEdge = true;
                    }
                    if (sc == SDL_SCANCODE_O && g.state == TITLE) {
                        g.optSel = 0;
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

        render(ren, g);
        SDL_RenderPresent(ren);
    }

    destroySprites();
    destroyBackground();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
