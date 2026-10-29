// Bomb Jack clone: window, main loop and fixed-step simulation.
#include <SDL3/SDL.h>
#include <cstdio>

#include "debug_tools.h"
#include "game/game.h"
#include "platform/audio.h"
#include "platform/input.h"
#include "render/background.h"
#include "render/render.h"
#include "render/textures.h"
#include "render/world.h"

// Prefer stable backends; Vulkan can flicker on some Linux/X11 setups.
static SDL_Renderer* createRenderer(SDL_Window* win) {
    for (const char* drv : {"opengl", "opengles2", "gpu", "software"}) {
        SDL_Renderer* r = SDL_CreateRenderer(win, drv);
        if (!r) continue;
        const char* name = SDL_GetRendererName(r);
        if (name && SDL_strcmp(name, "vulkan") == 0) { SDL_DestroyRenderer(r); continue; }
        return r;
    }
    return SDL_CreateRenderer(win, nullptr);
}

int main(int argc, char** argv) {
    const int tool = runDebugTool(argc, argv);
    if (tool >= 0) return tool;

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

    SDL_Renderer* ren = createRenderer(win);
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
    buildJackSprite();
    audio::init();   // silent no-op fallback if no audio device is available

    Game g;
    initPlatforms(g);
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
            if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat &&
                !handleKeyDown(g, e.key.scancode, jumpEdge))
                running = false;
        }

        // Fixed 120 Hz simulation steps for the time that has passed.
        Input in = readHeldKeys();
        Uint64 now   = SDL_GetTicks();
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
