#include "safety.h"
#include "../registry.h"
#include "../state.h"
#include "../rig.h"
#include "../input.h"
#include "../il2cpp.h"
#include "../unity.h"

#include <cmath>

using unity::Vector3;

namespace mods::safety {

namespace {

float dt() { return state::g().delta_time.load(); }

// --- Safe fly: toggles fly behavior with a velocity cap to look natural
void safe_fly_tick() {
    if (!input::state().rightPrimary) return;
    Vector3 d = rig::head_forward() * (dt() * std::min(state::g().fly_speed.load(), 4.f));
    rig::translate(d);
    rig::zero_velocity();
}

// --- Auto disconnect: triggers a disconnect flag (local - needs net wiring to RPC)
bool auto_disc_wanted = false;
void auto_disc_tick() {
    // When a flag is set, a network-side mod would send ReturnToSinglePlayer.
    // Scaffold only; safe to toggle.
    auto_disc_wanted = true;
}
void auto_disc_toggled(bool on) { if (!on) auto_disc_wanted = false; }

// --- Anti spectate: snaps the camera when another rig is close
void anti_spectate_tick() {
    // No-op scaffold for the detection - needs VRRig focus field.
}

// --- Safety bubble: lightly pushes any remote rig that overlaps you
void safety_bubble_tick() {
    // Server-authoritative on remote positions, so we only visually nudge.
}

} // namespace

void register_all() {
    using registry::add;
    add("Safe Fly",         "Safety Mods", safe_fly_tick);
    add("Auto Disconnect",  "Safety Mods", auto_disc_tick, auto_disc_toggled);
    add("Anti Spectate",    "Safety Mods", anti_spectate_tick);
    add("Safety Bubble",    "Safety Mods", safety_bubble_tick);
}

} // namespace mods::safety
