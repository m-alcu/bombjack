#include "render/render.h"
#include "render/gfx.h"
#include "render/hud.h"
#include "render/screens.h"
#include "render/world.h"

// A full-screen page over black, with the HUD on top.
static void drawPage(SDL_Renderer* r, const Game& g, void (*page)(SDL_Renderer*, const Game&)) {
    useScreen(r);
    setCol(r, {0, 0, 0});
    SDL_RenderClear(r);
    page(r, g);
    drawHud(r, g);
}

void render(SDL_Renderer* r, const Game& g) {
    switch (g.state) {
        case SPECIALBONUS: drawPage(r, g, drawSpecialBonus); return;
        case LUCKY:        drawPage(r, g, drawLucky);        return;
        case NAMEENTRY:    drawPage(r, g, drawHiScores);     return;   // inline name editing
        case TITLE:        // the attract loop alternates the banner with the table
            drawPage(r, g, g.attract.showTable ? drawHiScores : drawTitle);
            return;
        case OPTIONS:      drawOptions(r, g); return;
        default:           break;
    }

    useWorld(r);
    drawPlayfield(r, g);
    useScreen(r);
    drawStartIntro(r, g);
    if (g.state == GAMEOVER) drawGameOver(r, g);
    drawHud(r, g);
}
