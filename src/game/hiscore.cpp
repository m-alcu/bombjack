#include "game/hiscore.h"
#include "game/game.h"
#include "platform/audio.h"
#include <cstdio>
#include <cstring>

#ifndef ASSET_ROOT
#define ASSET_ROOT "."
#endif
static const char* kHiScorePath = ASSET_ROOT "/hiscores.dat";

static void setEntryName(HiScoreEntry& e, const char* name) {
    std::strncpy(e.name, name, NAME_MAX);
    e.name[NAME_MAX] = '\0';
}

// Default table matching the arcade "BEST PLAYERS" screen: ten 100000 / ROUND 1
// entries with the classic initials (10th place left as "...").
static void seedDefaultHiScores(Game& g) {
    static const char* const kNames[HISCORE_COUNT] = {
        "T.T", "E.E", "H.H", "K.K", "A.A", "N.N", "L.L", "T.T", "D.D", "...",
    };
    for (int i = 0; i < HISCORE_COUNT; ++i) {
        g.hiscores[i].score = 100000;
        g.hiscores[i].round = 1;
        setEntryName(g.hiscores[i], kNames[i]);
    }
}

void saveHiScores(const Game& g) {
    std::FILE* f = std::fopen(kHiScorePath, "w");
    if (!f) return;
    for (const HiScoreEntry& e : g.hiscores)
        std::fprintf(f, "%d %s %d\n", e.score, e.name[0] ? e.name : "-", e.round);
    std::fclose(f);
}

void loadHiScores(Game& g) {
    std::FILE* f = std::fopen(kHiScorePath, "r");
    if (!f) { seedDefaultHiScores(g); saveHiScores(g); return; }
    int n = 0;
    for (; n < HISCORE_COUNT; ++n) {
        char name[64] = {};
        int score = 0, round = 0;
        if (std::fscanf(f, "%d %63s %d", &score, name, &round) != 3) break;
        g.hiscores[n].score = score;
        g.hiscores[n].round = round;
        setEntryName(g.hiscores[n], name);
    }
    std::fclose(f);
    if (n < HISCORE_COUNT) { seedDefaultHiScores(g); saveHiScores(g); }  // corrupt/short → reseed
}

bool hiScoreQualifies(const Game& g) {
    return g.score > g.hiscores[HISCORE_COUNT - 1].score;
}

// Insert the current score under `name`, shifting lower rows down (Godot addScore).
void insertHiScore(Game& g, const char* name) {
    for (int i = 0; i < HISCORE_COUNT; ++i) {
        if (g.score > g.hiscores[i].score) {
            for (int j = HISCORE_COUNT - 1; j > i; --j) g.hiscores[j] = g.hiscores[j - 1];
            g.hiscores[i].score = g.score;
            g.hiscores[i].round = g.round;
            setEntryName(g.hiscores[i], name && name[0] ? name : "-");
            break;
        }
    }
}

// Made the table: type a name inline at its rank row.
void startNameEntry(Game& g) {
    NameEntry& e = g.nameEntry;
    e.text[0] = '\0';
    e.len     = 0;
    e.rank    = 0;
    while (e.rank < HISCORE_COUNT && g.score <= g.hiscores[e.rank].score)
        e.rank++;
    // The arcade plays tune 0x29 for a new #1 score and 0x2A for the other
    // ranks (ROM 0x4865).
    audio::play(e.rank == 0 ? audio::SND_NAME_ENTRY : audio::SND_NAME_ENTRY_2,
                "nameentry", /*loop=*/true);
    g.state = NAMEENTRY;
}

void nameEntryType(Game& g, char c) {
    NameEntry& e = g.nameEntry;
    if (e.len < NAME_MAX) e.text[e.len++] = c;
    e.text[e.len] = '\0';
}

void nameEntryBackspace(Game& g) {
    NameEntry& e = g.nameEntry;
    if (e.len > 0) e.text[--e.len] = '\0';
}

void confirmNameEntry(Game& g) {
    if (g.nameEntry.len == 0) return;
    insertHiScore(g, g.nameEntry.text);
    saveHiScores(g);
    audio::stopTag("nameentry");
    enterAttract(g, 0);   // show the fresh table first
}

void cancelNameEntry(Game& g) {
    audio::stopTag("nameentry");   // back to the title
    enterAttract(g, 0);
}
