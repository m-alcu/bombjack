#include "platform/input.h"
#include "game/game.h"

// High-score name entry: type A-Z / 0-9, backspace, enter, escape.
static void nameEntryKey(Game& g, SDL_Scancode sc) {
    if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z)
        nameEntryType(g, (char)('A' + (sc - SDL_SCANCODE_A)));
    else if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9)
        nameEntryType(g, (char)('1' + (sc - SDL_SCANCODE_1)));
    else if (sc == SDL_SCANCODE_0)
        nameEntryType(g, '0');
    else if (sc == SDL_SCANCODE_BACKSPACE)
        nameEntryBackspace(g);
    else if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER)
        confirmNameEntry(g);
    else if (sc == SDL_SCANCODE_ESCAPE)
        cancelNameEntry(g);
}

static void optionsKey(Game& g, SDL_Scancode sc) {
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
}

// Title, demo, play and the screens between rounds.
static bool gameKey(Game& g, SDL_Scancode sc, bool& jumpEdge) {
    if (sc == SDL_SCANCODE_ESCAPE) return false;
    // The attract cycle (title screens and demo) waits for start.
    const bool attract = g.state == TITLE || g.demo.active;
    if (sc == SDL_SCANCODE_SPACE) {
        if (attract) startGame(g);
        else jumpEdge = true;
    }
    if (sc == SDL_SCANCODE_O && attract) {
        stopDemo(g);
        g.opt.sel = 0;
        g.state   = OPTIONS;
    }
    if (sc == SDL_SCANCODE_R && g.state == GAMEOVER) startGame(g);
    return true;
}

bool handleKeyDown(Game& g, SDL_Scancode sc, bool& jumpEdge) {
    if (g.state == NAMEENTRY) { nameEntryKey(g, sc); return true; }
    if (sc == SDL_SCANCODE_Q) return false;
    if (g.state == OPTIONS)   { optionsKey(g, sc); return true; }
    return gameKey(g, sc, jumpEdge);
}

Input readHeldKeys() {
    const bool* ks = SDL_GetKeyboardState(nullptr);
    Input in{};
    in.left     = ks[SDL_SCANCODE_LEFT]  || ks[SDL_SCANCODE_A];
    in.right    = ks[SDL_SCANCODE_RIGHT] || ks[SDL_SCANCODE_D];
    in.up       = ks[SDL_SCANCODE_UP]    || ks[SDL_SCANCODE_W];
    in.down     = ks[SDL_SCANCODE_DOWN]  || ks[SDL_SCANCODE_S];
    in.jumpHeld = ks[SDL_SCANCODE_SPACE];
    return in;
}
