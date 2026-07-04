#include "audio.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"  // keep STDIO enabled for drmp3_open_file_and_read_pcm_frames_f32

namespace audio {
namespace {

// A decoded clip held in memory, in its own source format. SDL_AudioStream
// converts it to the device format on playback, so clips of mixed
// sample-rates/channel-counts happily coexist.
struct Clip {
    SDL_AudioSpec spec{};
    Uint8*        buf = nullptr;   // always SDL_malloc'd → freed with SDL_free
    Uint32        len = 0;
};

// One playing channel: a stream bound to the output device, mirroring a
// TEsound channel (a sound + its tag + optional loop / finished callback).
struct Channel {
    SDL_AudioStream* stream     = nullptr;
    Sid              id         = SND_COUNT;
    std::string      tag;
    bool             loop       = false;
    bool             paused     = false;
    void (*onFinished)()        = nullptr;
};

bool             g_ok = false;
SDL_AudioDeviceID g_dev = 0;
SDL_AudioSpec    g_devSpec{};
Clip             g_clips[SND_COUNT];
std::vector<Channel> g_channels;

// Filename for each Sid, relative to the sounds asset directory.
const char* kFile[SND_COUNT] = {
    "jump.wav",
    "walking.wav",
    "11_Touched_an_Enemy.mp3",
    "05_Clear.mp3",
    "09_Special_Bonus.mp3",
    "12_Game_Over.mp3",
    "08_Power_Ball_Appearing.mp3",
    "07_Got_a_Power_Ball.mp3",
    "take_bomb.wav",
    "take_exploding_bomb.wav",
    "kill_ennemy.wav",
    "mummy_appear.wav",
    "mummy_fall.wav",
    "bonus.wav",
    "take_bonus.wav",
    "04 Got a Special Coin.wav",
    "03_Ringo_no_Mori_no_Koneko_Tachi_BGM_1.mp3",
    "06_BGM_2.mp3",
    "10_Lady_Madonna_BGM3.mp3",
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

bool endsWith(const char* s, const char* suffix) {
    size_t ls = std::strlen(s), lx = std::strlen(suffix);
    return ls >= lx && std::strcmp(s + ls - lx, suffix) == 0;
}

bool loadClip(Sid id) {
    std::string path = resolvePath(kFile[id]);
    Clip& c = g_clips[id];

    if (endsWith(kFile[id], ".mp3")) {
        drmp3_config    cfg{};
        drmp3_uint64    frames = 0;
        float* pcm = drmp3_open_file_and_read_pcm_frames_f32(
            path.c_str(), &cfg, &frames, nullptr);
        if (!pcm) {
            std::fprintf(stderr, "audio: failed to decode %s\n", path.c_str());
            return false;
        }
        c.spec.format   = SDL_AUDIO_F32;
        c.spec.channels = (int)cfg.channels;
        c.spec.freq     = (int)cfg.sampleRate;
        c.len = (Uint32)(frames * cfg.channels * sizeof(float));
        c.buf = (Uint8*)SDL_malloc(c.len);
        if (c.buf) std::memcpy(c.buf, pcm, c.len);
        drmp3_free(pcm, nullptr);
        return c.buf != nullptr;
    }

    // WAV via SDL — buffer comes back SDL_malloc'd, matching our free path.
    if (!SDL_LoadWAV(path.c_str(), &c.spec, &c.buf, &c.len)) {
        std::fprintf(stderr, "audio: failed to load %s: %s\n",
                     path.c_str(), SDL_GetError());
        return false;
    }
    return true;
}

// Feed one full copy of the clip into a channel's stream.
void feed(Channel& ch) {
    const Clip& c = g_clips[ch.id];
    SDL_PutAudioStreamData(ch.stream, c.buf, (int)c.len);
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
    for (int i = 0; i < SND_COUNT; ++i)
        anyLoaded |= loadClip((Sid)i);

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
        c.buf = nullptr;
    }
    if (g_dev) SDL_CloseAudioDevice(g_dev);
    g_dev = 0;
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    g_ok = false;
}

void play(Sid id, const char* tag, bool loop, void (*onFinished)(), float gain) {
    if (!g_ok || id < 0 || id >= SND_COUNT || !g_clips[id].buf) return;

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
                SDL_GetAudioStreamQueued(ch.stream) < (int)g_clips[ch.id].len)
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
