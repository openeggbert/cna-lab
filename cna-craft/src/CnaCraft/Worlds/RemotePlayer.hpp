#pragma once

#include <string>

// Remote-player state + interpolation (MULTIPLAYER_PLAN.md §7, plan.md
// §12.1 item 18 phase M5) -- a direct port of Craft's Player/State pair and
// its update_player/interpolate_player (src/main.c:85-101, 421-461): each
// inbound position sample shifts sample2 -> sample1 and becomes the new
// sample2 (stamped with its arrival time); every frame the RENDERED state
// lerps sample1 -> sample2 by (now - t2) / clamp(t2 - t1, 0.1, 1) -- i.e.
// rendering runs one update-interval behind the newest sample, trading a
// ~100ms display delay (the position-send cadence) for perfectly smooth
// motion between discrete network updates. Yaw wraparound across ±pi is
// normalized at sample-push time, exactly like Craft's own s1->rx
// adjustment, so a player turning through the seam doesn't spin the long
// way around.
//
// Engine-agnostic (no CNA/SDL) and pure math -- unit-tested in
// worlds_smoke_test like PlayerController.
namespace CnaCraft::Worlds {

struct RemotePlayerSample {
    float x = 0, y = 0, z = 0;   // eye position (Craft's State semantics)
    float rx = 0, ry = 0;        // yaw, pitch (radians)
    double t = 0;                // arrival time (game-clock seconds)
};

class RemotePlayer {
public:
    int id = 0;
    std::string name;  // arrives separately via N; defaults to "player<id>"

    // Craft's update_player(..., interpolate=1). The first sample seeds
    // BOTH slots (Craft calls update_player twice on first sight), so a
    // just-joined player renders immediately at their reported position.
    void PushSample(float x, float y, float z, float rx, float ry, double now);

    // Craft's interpolate_player: the smoothed state to render this frame.
    [[nodiscard]] RemotePlayerSample Interpolated(double now) const;

    [[nodiscard]] bool HasSample() const { return hasSample_; }

private:
    RemotePlayerSample s1_, s2_;
    bool hasSample_ = false;
};

}
