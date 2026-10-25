#pragma once

// ---------------------------------------------------------------------------
// Sound system — a thin port of the original game's TEsound layer.
//
// The Lua original plays sounds through TEsound with optional string "tags"
// used to group, count, stop, pause and resume channels. We mirror that:
// each Sid names a clip; sounds can be played one-shot or looping, optionally
// under a tag, and tags can be queried/stopped/paused/resumed.
//
// If the audio device can't be opened the whole module goes silent and every
// call becomes a no-op, so the game still runs.
// ---------------------------------------------------------------------------

namespace audio {

enum Sid {
    SND_JUMP,                // jump.wav
    SND_WALKING,             // walking.wav
    SND_TOUCHED,             // 11_Touched_an_Enemy.mp3 — Jack hits an enemy
    SND_CLEAR,               // 05_Clear.mp3 — level cleared
    SND_SPECIAL_BONUS,       // 09_Special_Bonus.mp3 — special-bonus tally
    SND_GAMEOVER,            // 12_Game_Over.mp3
    SND_POWER_APPEARING,     // 08_Power_Ball_Appearing.mp3 — orb on screen
    SND_GOT_POWER,           // 07_Got_a_Power_Ball.mp3 — orb collected
    SND_TAKE_BOMB,           // take_bomb.wav — grab an unlit bomb
    SND_TAKE_EXPLODING_BOMB, // take_exploding_bomb.wav — grab the lit bomb
    SND_KILL_ENEMY,          // kill_ennemy.wav — collect a frozen enemy
    SND_MUMMY_APPEAR,        // mummy_appear.wav
    SND_MUMMY_FALL,          // mummy_fall.wav
    SND_BONUS,               // coin_bounce.wav — B/E/S coin bounces (arcade cmd 0x1D)
    SND_TAKE_BONUS,          // take_bonus.wav — B/E/S coin collected
    SND_GOT_SPECIAL,         // 04 Got a Special Coin.wav — S-bonus "lucky" screen
    SND_MUSIC,               // 03_Ringo..._BGM_1.mp3 — in-game music, levels %3==1
    SND_MUSIC_2,             // 06_BGM_2.mp3 — in-game music, levels %3==0 (3,6,9…)
    SND_MUSIC_3,             // 10_Lady_Madonna_BGM3.mp3 — in-game music, levels %3==2
    SND_TITLE,               // 01 Title.wav — title-screen music loop
    SND_NAME_ENTRY,          // 13_Name_Entry_1.mp3 — high-score name-entry screen
    SND_MUMMY_LAND,          // mummy_land.wav — mummy lands on a platform (arcade cmd 0x17)
    SND_MUMMY_TRANSFORM,     // mummy_transform.wav — mummy hits the floor and transforms (0x2D)
    SND_DEATH_FALL,          // jack_death_fall.wav — Jack falls after being hit (0x14)
    SND_COUNT
};

// Open the audio device and decode every clip. Returns false (and leaves the
// module muted) if audio is unavailable; the caller can ignore the result.
bool init();
void shutdown();

// Per-frame housekeeping: reaps finished one-shots (firing their finished
// callback) and tops up looping streams. Call once per rendered frame.
void update();

// Play a clip. `tag` (optional) groups the channel for stop/pause/count.
// `loop` keeps it repeating until stopped. `onFinished` fires from update()
// when a non-looping channel drains. `gain` scales volume (1.0 = full).
void play(Sid id, const char* tag = nullptr, bool loop = false,
          void (*onFinished)() = nullptr, float gain = 1.0f);

// Tag operations mirror TEsound.stop / pause / resume / findTag.
void stopTag(const char* tag);
void pauseTag(const char* tag);
void resumeTag(const char* tag);
int  tagCount(const char* tag);

}  // namespace audio
