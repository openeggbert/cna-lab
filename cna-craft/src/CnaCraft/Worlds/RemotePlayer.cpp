#include "RemotePlayer.hpp"

#include <algorithm>
#include <cmath>

namespace CnaCraft::Worlds {

namespace {
constexpr float kPi = 3.14159265358979323846f;
}

void RemotePlayer::PushSample(float x, float y, float z, float rx, float ry, double now) {
    if (!hasSample_) {
        // Seed both slots (Craft's double update_player on first sight,
        // main.c:2514/2517) -- interpolation between two identical samples
        // is a stable point, no sliding in from the origin.
        s1_ = RemotePlayerSample{x, y, z, rx, ry, now};
        s2_ = s1_;
        hasSample_ = true;
        return;
    }
    s1_ = s2_;
    s2_ = RemotePlayerSample{x, y, z, rx, ry, now};
    // Yaw wraparound (Craft main.c:430-435): if the new sample crossed the
    // ±pi seam, shift the OLD sample by a full turn so the lerp takes the
    // short way around.
    if (s2_.rx - s1_.rx > kPi) s1_.rx += 2.0f * kPi;
    if (s1_.rx - s2_.rx > kPi) s1_.rx -= 2.0f * kPi;
}

RemotePlayerSample RemotePlayer::Interpolated(double now) const {
    if (!hasSample_) return {};
    // Craft's interpolate_player (main.c:445-461), verbatim math: the lerp
    // window is the gap between the last two samples, clamped to
    // [0.1, 1.0] seconds, and progress saturates at 1 (a stalled sender
    // leaves the player parked at their last reported spot).
    float window = static_cast<float>(s2_.t - s1_.t);
    window = std::clamp(window, 0.1f, 1.0f);
    const float progress = std::min(static_cast<float>(now - s2_.t) / window, 1.0f);
    RemotePlayerSample out;
    out.x = s1_.x + (s2_.x - s1_.x) * progress;
    out.y = s1_.y + (s2_.y - s1_.y) * progress;
    out.z = s1_.z + (s2_.z - s1_.z) * progress;
    out.rx = s1_.rx + (s2_.rx - s1_.rx) * progress;
    out.ry = s1_.ry + (s2_.ry - s1_.ry) * progress;
    out.t = now;
    return out;
}

}
