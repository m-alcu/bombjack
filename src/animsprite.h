#pragma once
// Generic animated sprite for this engine's prebaked-texture model.
//
// Unlike a classic atlas+sourceRect sprite, every frame here is a finished
// SDL_Texture produced once by buildSprites(). An AnimSprite is therefore just
// a named collection of frame-texture sequences plus the live state needed to
// pick one and blit it: position, size, flip and a colour-mod tint (this engine
// recolours via SDL_SetTextureColorMod rather than CPU palette rotation).
//
// Each frame may carry an optional `overlay` texture drawn on top with its own
// tint — used for layered effects such as the pulsing enemy eyes.

#include <SDL3/SDL.h>
#include <cmath>
#include <string>
#include <utility>
#include <vector>
#include <unordered_map>
#include "color.h"

class AnimSprite {
public:
    struct Frame {
        SDL_Texture* tex     = nullptr;
        SDL_Texture* overlay = nullptr; // optional second layer (e.g. eye)
        float        duration = 0.1f;   // seconds
    };
    struct Animation {
        std::vector<Frame> frames;
        bool loop = true;
    };

    void addAnimation(const std::string& name, Animation anim) {
        if (anim.frames.empty()) return;
        bool first = animations_.empty();
        animations_[name] = std::move(anim);
        if (first) play(name);
    }

    // Switch animation. `restart` rewinds even if already playing this one.
    void play(const std::string& name, bool restart = false) {
        if (!animations_.count(name)) return;
        if (current_ != name || restart) {
            current_  = name;
            clock_    = 0.0f;
            finished_ = false;
        }
    }

    // Advance the sprite's own clock (for entities that own their timing).
    void update(float dt) { setClock(clock_ + dt); }

    // Drive the frame straight off a shared/global clock (immediate-mode draw).
    void setClock(float seconds) {
        clock_ = seconds < 0.0f ? 0.0f : seconds;
        finished_ = false;
        const Animation* a = currentAnim();
        if (!a || a->frames.size() <= 1) return;
        if (a->loop) {
            float total = totalDuration(*a);
            if (total > 0.0f) clock_ = std::fmod(clock_, total);
        } else if (clock_ >= totalDuration(*a)) {
            finished_ = true;
        }
    }

    void setPosition(float cx, float cy) { x_ = cx; y_ = cy; } // centre
    void setSize(float w, float h)       { w_ = w;  h_ = h;  }
    void setFlip(bool fx, bool fy = false){ flipX_ = fx; flipY_ = fy; }
    void setTint(Color c)                { tint_ = c; }
    void setOverlayTint(Color c)         { overlayTint_ = c; }

    bool finished() const                { return finished_; }
    const std::string& animation() const { return current_; }

    void draw(SDL_Renderer* r) const {
        const Frame* f = currentFrame();
        if (!f || !f->tex) return;
        SDL_FRect dst{x_ - w_ / 2.0f, y_ - h_ / 2.0f, w_, h_};
        blit(r, f->tex, dst, tint_);
        if (f->overlay) blit(r, f->overlay, dst, overlayTint_);
    }

private:
    static float totalDuration(const Animation& a) {
        float t = 0.0f;
        for (const Frame& f : a.frames) t += f.duration;
        return t;
    }

    const Animation* currentAnim() const {
        auto it = animations_.find(current_);
        return it == animations_.end() ? nullptr : &it->second;
    }

    const Frame* currentFrame() const {
        const Animation* a = currentAnim();
        if (!a || a->frames.empty()) return nullptr;
        float t = clock_;
        for (const Frame& f : a->frames) {  // walk by accumulated duration
            if (t < f.duration) return &f;
            t -= f.duration;
        }
        return &a->frames.back();            // clamp (non-looping tail)
    }

    void blit(SDL_Renderer* r, SDL_Texture* tex, const SDL_FRect& dst,
              Color c) const {
        SDL_FlipMode flip = (SDL_FlipMode)((flipX_ ? SDL_FLIP_HORIZONTAL : 0) |
                                           (flipY_ ? SDL_FLIP_VERTICAL   : 0));
        SDL_SetTextureColorMod(tex, c.r, c.g, c.b);
        SDL_RenderTextureRotated(r, tex, nullptr, &dst, 0.0, nullptr, flip);
        SDL_SetTextureColorMod(tex, 255, 255, 255);
    }

    std::unordered_map<std::string, Animation> animations_;
    std::string current_;
    float clock_ = 0.0f;
    bool  finished_ = false;

    float x_ = 0.0f, y_ = 0.0f, w_ = 0.0f, h_ = 0.0f;
    bool  flipX_ = false, flipY_ = false;
    Color tint_{255, 255, 255};
    Color overlayTint_{255, 255, 255};
};
