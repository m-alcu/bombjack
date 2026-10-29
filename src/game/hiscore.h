#pragma once
// High-score table: persistence and name entry, mirroring the Godot game.

struct Game;

constexpr int HISCORE_COUNT = 10;
constexpr int NAME_MAX      = 3;   // 3 initials fit the frame hole at 1px gaps

// One persistent high-score row: score, player name, round reached.
struct HiScoreEntry {
    int  score = 0;
    char name[NAME_MAX + 1] = {};
    int  round = 1;
};

// The name being typed on the NAMEENTRY screen.
struct NameEntry {
    char text[NAME_MAX + 1] = {};
    int  len  = 0;
    int  rank = 0;   // table row the new score lands on
};

// Load the table from disk (seeding defaults on first run) and persist it.
void loadHiScores(Game& g);
void saveHiScores(const Game& g);

// Whether the current score makes the table, and insert it under `name`.
bool hiScoreQualifies(const Game& g);
void insertHiScore(Game& g, const char* name);

// Go to the NAMEENTRY screen for the current score.
void startNameEntry(Game& g);

// Typing on the NAMEENTRY screen: add a character, delete the last one,
// confirm (insert, save, back to the title) or cancel.
void nameEntryType(Game& g, char c);
void nameEntryBackspace(Game& g);
void confirmNameEntry(Game& g);
void cancelNameEntry(Game& g);
