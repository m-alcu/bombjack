#include "platform/audio.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace audio {
namespace {

// A decoded clip held in memory, in its own source format. SDL_AudioStream
// converts it to the device format on playback, so clips of mixed
// sample-rates/channel-counts happily coexist.
struct Clip {
    SDL_AudioSpec spec{};
    Uint8*        buf = nullptr;   // always SDL_malloc'd → freed with SDL_free
    Uint32        len = 0;
    // Optional loop region (see kLoop): a looping channel plays buf[0, headLen)
    // once, then repeats loopBuf, whose tail is crossfaded into the region start.
    Uint8*        loopBuf = nullptr;
    Uint32        loopLen = 0;
    Uint32        headLen = 0;
};

// Loop region for clips whose recording is not a clean loop, in seconds.
struct LoopSpec { float start, end, xfade; };

// One playing channel: a stream bound to the output device, mirroring a
// TEsound channel (a sound + its tag + optional loop / finished callback).
struct Channel {
    SDL_AudioStream* stream     = nullptr;
    Sid              id         = SND_COUNT;
    std::string      tag;
    bool             loop       = false;
    bool             paused     = false;
    bool             headFed    = false;
    void (*onFinished)()        = nullptr;
};

bool             g_ok = false;
bool             g_muted = false;
SDL_AudioDeviceID g_dev = 0;
SDL_AudioSpec    g_devSpec{};
Clip             g_clips[SND_COUNT];
std::vector<Channel> g_channels;

// Filename for each Sid, relative to the sounds asset directory.
const char* kFile[SND_COUNT] = {
    "jump.wav",
    "walking.wav",
    // Melodies: recorded from the arcade's sound board in MAME (command in
    // brackets), DC-filtered, one common gain. Loops are cut to one exact period.
    "11_Touched_an_Enemy.wav",       // [0x27]
    "05_Clear.wav",                  // [0x25]
    "09_Special_Bonus.wav",          // [0x2B]
    "12_Game_Over.wav",              // [0x26]
    "08_Power_Ball_Appearing.wav",   // [0x1B]
    "07_Got_a_Power_Ball.wav",       // [0x1A]
    "take_bomb.wav",
    "take_exploding_bomb.wav",
    "kill_ennemy.wav",
    "mummy_appear.wav",
    "mummy_fall.wav",
    "coin_bounce.wav",
    "take_bonus.wav",
    "04 Got a Special Coin.wav",                   // [0x28]
    "03_Ringo_no_Mori_no_Koneko_Tachi_BGM_1.wav",  // [0x23]
    "06_BGM_2.wav",                                // [0x22]
    "10_Lady_Madonna_BGM3.wav",                    // [0x24]
    "01 Title.wav",                                // [0x20]
    "13_Name_Entry_1.wav",                         // [0x29]
    // Recorded from the arcade's sound board in MAME (sound commands 0x17,
    // 0x2D, 0x14; coin_bounce.wav is 0x1D), DC-filtered and level-matched.
    "mummy_land.wav",
    "mummy_transform.wav",
    "jack_death_fall.wav",
    "14 Name Entry 2.wav",                         // [0x2A]
};

// Loop regions of the looping melodies: one period of the arcade tune as measured
// in MAME, after its intro (BGM 1 and BGM 2 open with a ~3 s intro that is not
// repeated). A short crossfade hides the sound chips' phase at the seam.
const LoopSpec kLoop[SND_COUNT] = {
    {}, {}, {}, {},
    {1.0f, 17.22292f, 0.015f},  // SND_SPECIAL_BONUS
    {},
    {1.0f, 16.20650f, 0.015f},  // SND_POWER_APPEARING (10 sweeps of 1.52 s)
    {}, {}, {}, {}, {}, {}, {}, {}, {},
    {5.0f, 39.06404f, 0.015f},  // SND_MUSIC
    {5.0f, 48.25654f, 0.015f},  // SND_MUSIC_2
    {1.0f, 39.92708f, 0.015f},  // SND_MUSIC_3
    {},
    {1.0f, 28.57256f, 0.015f},  // SND_NAME_ENTRY
    {}, {}, {},
    {1.0f, 13.97254f, 0.015f},  // SND_NAME_ENTRY_2
};

#ifndef ASSET_ROOT
#define ASSET_ROOT "."
#endif

// Locate a sound file. Try the build-time source root first (robust for local
// runs regardless of working directory), then the current directory.
std::string resolvePath(const char* name) {
    std::string candidates[] = {
        std::string(ASSET_ROOT) + "/assets/sounds/" + name,
        std::string("assets/sounds/") + name,
    };
    for (const std::string& c : candidates) {
        SDL_PathInfo info;
        if (SDL_GetPathInfo(c.c_str(), &info)) return c;
    }
    return candidates[0];  // report the primary path in the load error
}

bool loadClip(Sid id) {
    std::string path = resolvePath(kFile[id]);
    Clip& c = g_clips[id];

    // WAV via SDL — buffer comes back SDL_malloc'd, matching our free path.
    if (!SDL_LoadWAV(path.c_str(), &c.spec, &c.buf, &c.len)) {
        std::fprintf(stderr, "audio: failed to load %s: %s\n",
                     path.c_str(), SDL_GetError());
        return false;
    }
    return true;
}

// Build a clip's crossfaded loop buffer from its kLoop region. The clip is
// converted to F32 first if it was loaded in another format.
void buildLoop(Sid id) {
    const LoopSpec& ls = kLoop[id];
    Clip& c = g_clips[id];
    if (ls.end <= ls.start || !c.buf) return;
    if (c.spec.format != SDL_AUDIO_F32) {
        SDL_AudioSpec f32 = c.spec;
        f32.format = SDL_AUDIO_F32;
        Uint8* conv = nullptr;
        int    convLen = 0;
        if (!SDL_ConvertAudioSamples(&c.spec, c.buf, (int)c.len, &f32, &conv, &convLen))
            return;
        SDL_free(c.buf);
        c.buf  = conv;
        c.len  = (Uint32)convLen;
        c.spec = f32;
    }
    const int    chans  = c.spec.channels;
    const Uint32 frames = c.len / (Uint32)(chans * sizeof(float));
    const Uint32 s = (Uint32)(ls.start * c.spec.freq);
    const Uint32 e = (Uint32)(ls.end * c.spec.freq);
    const Uint32 n = (Uint32)(ls.xfade * c.spec.freq);
    if (e > frames || n > s || n > e - s) return;
    const float* src = (const float*)c.buf;
    float* lb = (float*)SDL_malloc((e - s) * chans * sizeof(float));
    if (!lb) return;
    std::memcpy(lb, src + s * chans, (e - s) * chans * sizeof(float));
    // Fade the region's tail into the audio just before its start, so the wrap
    // from the last sample back to the first is continuous.
    for (Uint32 i = 0; i < n; ++i) {
        const float t = (float)i / (float)n;
        for (int k = 0; k < chans; ++k) {
            float& out = lb[((e - s) - n + i) * chans + k];
            out = src[(e - n + i) * chans + k] * (1.0f - t) + src[(s - n + i) * chans + k] * t;
        }
    }
    c.loopBuf = (Uint8*)lb;
    c.loopLen = (e - s) * chans * sizeof(float);
    c.headLen = s * chans * sizeof(float);
}

// Feed one copy of the clip into a channel's stream: the whole clip, or for a
// looping clip with a loop region, its head once and then the loop buffer.
void feed(Channel& ch) {
    const Clip& c = g_clips[ch.id];
    if (ch.loop && c.loopBuf) {
        if (!ch.headFed) {
            SDL_PutAudioStreamData(ch.stream, c.buf, (int)c.headLen);
            ch.headFed = true;
        }
        SDL_PutAudioStreamData(ch.stream, c.loopBuf, (int)c.loopLen);
        return;
    }
    SDL_PutAudioStreamData(ch.stream, c.buf, (int)c.len);
}

// Bytes a looping channel needs queued ahead to stay gapless.
Uint32 loopChunk(Sid id) {
    const Clip& c = g_clips[id];
    return c.loopBuf ? c.loopLen : c.len;
}

}  // namespace

bool init() {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        std::fprintf(stderr, "audio: SDL_INIT_AUDIO failed: %s\n", SDL_GetError());
        return false;
    }
    g_dev = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (!g_dev) {
        std::fprintf(stderr, "audio: OpenAudioDevice failed: %s\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }
    SDL_GetAudioDeviceFormat(g_dev, &g_devSpec, nullptr);

    bool anyLoaded = false;
    for (int i = 0; i < SND_COUNT; ++i) {
        anyLoaded |= loadClip((Sid)i);
        buildLoop((Sid)i);
    }

    g_ok = anyLoaded;
    return g_ok;
}

void shutdown() {
    if (!g_ok) return;
    for (Channel& ch : g_channels) {
        SDL_UnbindAudioStream(ch.stream);
        SDL_DestroyAudioStream(ch.stream);
    }
    g_channels.clear();
    for (Clip& c : g_clips) {
        if (c.buf) SDL_free(c.buf);
        if (c.loopBuf) SDL_free(c.loopBuf);
        c.buf = nullptr;
        c.loopBuf = nullptr;
    }
    if (g_dev) SDL_CloseAudioDevice(g_dev);
    g_dev = 0;
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    g_ok = false;
}

void play(Sid id, const char* tag, bool loop, void (*onFinished)(), float gain) {
    if (!g_ok || g_muted || id < 0 || id >= SND_COUNT || !g_clips[id].buf) return;

    SDL_AudioStream* s = SDL_CreateAudioStream(&g_clips[id].spec, &g_devSpec);
    if (!s) return;
    if (!SDL_BindAudioStream(g_dev, s)) { SDL_DestroyAudioStream(s); return; }
    if (gain != 1.0f) SDL_SetAudioStreamGain(s, gain);

    Channel ch;
    ch.stream     = s;
    ch.id         = id;
    ch.tag        = tag ? tag : "";
    ch.loop       = loop;
    ch.onFinished = onFinished;
    feed(ch);
    if (!loop) SDL_FlushAudioStream(s);  // let it drain fully, then reap
    g_channels.push_back(std::move(ch));
}

void update() {
    if (!g_ok) return;
    for (size_t i = 0; i < g_channels.size();) {
        Channel& ch = g_channels[i];
        if (ch.loop) {
            // Keep at least one clip buffered ahead for a gapless loop. Paused
            // channels keep looping silently so they still count as "playing".
            if (!ch.paused &&
                SDL_GetAudioStreamQueued(ch.stream) < (int)loopChunk(ch.id))
                feed(ch);
            ++i;
            continue;
        }
        bool done = SDL_GetAudioStreamAvailable(ch.stream) == 0 &&
                    SDL_GetAudioStreamQueued(ch.stream) == 0;
        if (!done) { ++i; continue; }
        void (*cb)() = ch.onFinished;
        SDL_UnbindAudioStream(ch.stream);
        SDL_DestroyAudioStream(ch.stream);
        g_channels.erase(g_channels.begin() + i);
        if (cb) cb();  // after erase: callback may enqueue new channels
    }
}

void setMuted(bool muted) { g_muted = muted; }

void stopTag(const char* tag) {
    if (!g_ok || !tag) return;
    for (size_t i = 0; i < g_channels.size();) {
        if (g_channels[i].tag == tag) {
            SDL_UnbindAudioStream(g_channels[i].stream);
            SDL_DestroyAudioStream(g_channels[i].stream);
            g_channels.erase(g_channels.begin() + i);
        } else {
            ++i;
        }
    }
}

void pauseTag(const char* tag) {
    if (!g_ok || !tag) return;
    for (Channel& ch : g_channels)
        if (ch.tag == tag) {
            ch.paused = true;
            SDL_SetAudioStreamGain(ch.stream, 0.0f);
        }
}

void resumeTag(const char* tag) {
    if (!g_ok || !tag) return;
    for (Channel& ch : g_channels)
        if (ch.tag == tag) {
            ch.paused = false;
            SDL_SetAudioStreamGain(ch.stream, 1.0f);
        }
}

int tagCount(const char* tag) {
    if (!g_ok || !tag) return 0;
    int n = 0;
    for (const Channel& ch : g_channels)
        if (ch.tag == tag) ++n;
    return n;
}

}  // namespace audio
