#include "game/effects.h"
#include "game/game.h"

template <typename T, typename Done>
static void age(std::vector<T>& v, float dt, Done done) {
    for (auto it = v.begin(); it != v.end();) {
        it->age += dt;
        if (done(*it)) it = v.erase(it); else ++it;
    }
}

void updateEffects(Game& g, float dt) {
    // Score popups float up for a second.
    age(g.popups, dt, [dt](Popup& pp) { pp.y -= 18.0f * dt; return pp.age > 1.0f; });
    age(g.coinPickups, dt, [](const TimedEffect& e) { return e.age >= 4 * PICKCOIN_FRAME; });
    age(g.bonusTakens, dt, [](const TimedEffect& e) { return e.age >= 6 * BONUSTAKEN_FRAME; });
    age(g.explosions,  dt, [](const TimedEffect& e) { return e.age >= 3 * EXPL_FRAME; });
}
