#include "movement.h"
#include "../registry.h"
#include "../state.h"
#include "../rig.h"
#include "../input.h"
#include "../unity.h"
#include "../il2cpp.h"

#include <cmath>
#include <vector>

using unity::Vector3;
using unity::Vector2;

namespace mods::movement {

namespace {

float fly_speed_eff() {
    float s = state::g().fly_speed.load();
    return state::g().scale_with_player.load() ? s * rig::scale() : s;
}

float dt() { return state::g().delta_time.load(); }

// --- Fly family --------------------------------------------------------

void fly_tick() {
    if (!input::state().rightPrimary) return;
    rig::translate(rig::head_forward() * (dt() * fly_speed_eff()));
    rig::zero_velocity();
}

void trigger_fly_tick() {
    if (input::state().rightTrigger <= 0.5f) return;
    rig::translate(rig::head_forward() * (dt() * fly_speed_eff()));
    rig::zero_velocity();
}

void noclip_fly_tick() {
    bool down = input::state().rightPrimary;
    if (down) {
        rig::translate(rig::head_forward() * (dt() * fly_speed_eff()));
        rig::zero_velocity();
    }
    state::g().noclip_active.store(down);
}

void joystick_fly_tick() {
    Vector2 j = input::state().leftJoystick;
    if (std::fabs(j.x) < 0.3f && std::fabs(j.y) < 0.3f) return;
    Vector3 d = rig::head_forward() * (dt() * j.y * fly_speed_eff())
              + rig::head_right()   * (dt() * j.x * fly_speed_eff());
    rig::translate(d);
    rig::zero_velocity();
}

void hand_fly_tick() {
    if (!input::state().rightPrimary) return;
    rig::translate(rig::right_hand_forward() * (dt() * fly_speed_eff()));
    rig::zero_velocity();
}

void bark_fly_tick() {
    Vector2 lj = input::state().leftJoystick;
    Vector2 rj = input::state().rightJoystick;
    Vector3 in = {lj.x, rj.y, lj.y};
    if (std::fabs(in.x) < 0.01f && std::fabs(in.y) < 0.01f && std::fabs(in.z) < 0.01f) return;
    Vector3 fwd = rig::head_forward(); fwd.y = 0;
    Vector3 right = rig::head_right(); right.y = 0;
    Vector3 vel = right * in.x + Vector3{0, 1, 0} * in.y + fwd * in.z;
    vel = vel * fly_speed_eff();
    rig::translate(vel * dt());
    rig::add_force({0, 9.81f, 0});
}

void slingshot_fly_tick() {
    if (!input::state().rightPrimary) return;
    Vector3 dir = (rig::right_hand_position() - rig::body_position()).normalized();
    rig::translate(dir * (dt() * fly_speed_eff()));
    rig::zero_velocity();
}

void dash_tick() {
    if (!input::just_pressed_right_primary()) return;
    rig::translate(rig::head_forward() * (fly_speed_eff() * 0.4f));
    rig::zero_velocity();
}

void wasd_fly_tick() {
    Vector2 j = input::state().leftJoystick;
    Vector3 fwd = rig::head_forward(); fwd.y = 0; fwd = fwd.normalized();
    Vector3 right = rig::head_right(); right.y = 0; right = right.normalized();
    Vector3 v = fwd * j.y + right * j.x;
    if (input::state().rightPrimary)   v.y += 1;
    if (input::state().rightSecondary) v.y -= 1;
    if (v.sqrmag() < 0.001f) return;
    rig::translate(v * (dt() * fly_speed_eff()));
    rig::zero_velocity();
}

void iron_man_tick() {
    if (input::state().leftTrigger > 0.3f)
        rig::add_force({0, input::state().leftTrigger * 25.f, 0});
    if (input::state().rightTrigger > 0.3f)
        rig::add_force({0, input::state().rightTrigger * 25.f, 0});
}

void spider_man_tick() {
    // Pull toward hand if trigger held while far from a surface target.
    const auto& s = input::state();
    if (s.leftTrigger > 0.5f) {
        Vector3 d = (rig::left_hand_position() - rig::body_position()).normalized();
        rig::translate(d * (dt() * fly_speed_eff() * 1.5f));
    }
    if (s.rightTrigger > 0.5f) {
        Vector3 d = (rig::right_hand_position() - rig::body_position()).normalized();
        rig::translate(d * (dt() * fly_speed_eff() * 1.5f));
    }
}

// --- Gravity ----------------------------------------------------------

void zero_gravity_tick() { rig::add_force({0, 9.81f, 0}); }
void low_gravity_tick()  { rig::add_force({0, 6.66f, 0}); }
void high_gravity_tick() { rig::add_force({0, -7.77f, 0}); }
void reverse_gravity_tick() { rig::add_force({0, 19.62f, 0}); }
void feather_falling_tick() {
    // Soft-cap downward velocity. Approximation via counter-force.
    rig::add_force({0, 8.0f, 0});
}

// --- Platforms --------------------------------------------------------

void platforms_tick() {
    const auto& s = input::state();
    if (s.leftTrigger > 0.5f) {
        Vector3 under = rig::left_hand_position() + Vector3{0, -0.1f, 0};
        Vector3 body  = rig::body_position();
        if (body.y < under.y + 1.2f) { rig::set_body_position({body.x, under.y + 1.2f, body.z}); rig::zero_velocity(); }
    }
    if (s.rightTrigger > 0.5f) {
        Vector3 under = rig::right_hand_position() + Vector3{0, -0.1f, 0};
        Vector3 body  = rig::body_position();
        if (body.y < under.y + 1.2f) { rig::set_body_position({body.x, under.y + 1.2f, body.z}); rig::zero_velocity(); }
    }
}

void trigger_platforms_tick() {
    const auto& s = input::state();
    if (!(s.leftTrigger > 0.5f || s.rightTrigger > 0.5f)) return;
    platforms_tick();
}

// --- Jumps ------------------------------------------------------------

void high_jump_tick() {
    if (!input::just_pressed_right_primary()) return;
    float jp = state::g().jump_power.load() * state::g().jump_multi.load();
    rig::zero_velocity();
    rig::add_force({0, jp * 40.f, 0});
}

void infinite_jump_tick() {
    if (!input::state().rightPrimary) return;
    rig::add_force({0, 30.f, 0});
}

void super_jump_tick() {
    if (!input::just_pressed_right_primary()) return;
    rig::zero_velocity();
    rig::add_force({0, 1000.f, 0});
}

// --- Long arms --------------------------------------------------------

static int32_t off_arm_mult = -2;
static void resolve_arm_offset() {
    if (off_arm_mult != -2) return;
    using namespace il2cpp;
    Il2CppImage* game = find_image("Assembly-CSharp.dll");
    if (!game) { off_arm_mult = -1; return; }
    Il2CppClass* gtp = class_from_name(game, "GorillaLocomotion", "GTPlayer");
    if (!gtp) { off_arm_mult = -1; return; }
    FieldInfo* f = class_get_field_from_name(gtp, "armLengthMultiplier");
    off_arm_mult = f ? field_get_offset(f) : -1;
}

void long_arms_tick() {
    resolve_arm_offset();
    if (off_arm_mult < 0 || !rig::gtplayer()) return;
    float* p = reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(rig::gtplayer()) + off_arm_mult);
    *p = state::g().arm_length.load();
}
void long_arms_toggled(bool on) {
    resolve_arm_offset();
    if (off_arm_mult < 0 || !rig::gtplayer()) return;
    float* p = reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(rig::gtplayer()) + off_arm_mult);
    *p = on ? state::g().arm_length.load() : 1.f;
}

// --- Speed boost ------------------------------------------------------

void speed_boost_tick() {
    Vector2 lj = input::state().leftJoystick;
    if (std::fabs(lj.x) < 0.2f && std::fabs(lj.y) < 0.2f) return;
    float boost = state::g().speed_boost.load();
    Vector3 fwd = rig::head_forward(); fwd.y = 0; fwd = fwd.normalized();
    Vector3 right = rig::head_right(); right.y = 0; right = right.normalized();
    Vector3 d = fwd * (lj.y * boost * dt()) + right * (lj.x * boost * dt());
    rig::translate(d);
}

void playspace_abuse_tick() {
    Vector2 lj = input::state().leftJoystick;
    Vector2 rj = input::state().rightJoystick;
    float ps = state::g().playspace_speed.load();
    Vector3 fwd = rig::head_forward(); fwd.y = 0; fwd = fwd.normalized();
    Vector3 right = rig::head_right(); right.y = 0; right = right.normalized();
    Vector3 d = (fwd * lj.y + right * lj.x + Vector3{0, rj.y, 0}) * (ps * dt());
    rig::translate(d);
}

// --- Up / Down / Left / Right / Fwd / Back toggles ---------------------

void up_down_tick() {
    static bool dir = true;
    static float t = 0;
    t += dt();
    if (t > 1.0f) { t = 0; dir = !dir; }
    rig::translate({0, (dir ? 1.f : -1.f) * dt() * 2.f, 0});
}

void left_right_tick() {
    static bool dir = true;
    static float t = 0;
    t += dt();
    if (t > 1.0f) { t = 0; dir = !dir; }
    rig::translate(rig::head_right() * ((dir ? 1.f : -1.f) * dt() * 2.f));
}

void fwd_back_tick() {
    static bool dir = true;
    static float t = 0;
    t += dt();
    if (t > 1.0f) { t = 0; dir = !dir; }
    Vector3 fwd = rig::head_forward(); fwd.y = 0; fwd = fwd.normalized();
    rig::translate(fwd * ((dir ? 1.f : -1.f) * dt() * 2.f));
}

// --- Teleport ---------------------------------------------------------

Vector3 checkpoint_pos{};
bool    checkpoint_set = false;

void checkpoint_tick() {
    if (input::state().leftGrip) {
        checkpoint_pos = rig::body_position();
        checkpoint_set = true;
    }
    if (input::just_pressed_left_primary() && checkpoint_set) {
        rig::set_body_position(checkpoint_pos);
        rig::zero_velocity();
    }
}

void teleport_gun_tick() {
    if (!input::just_pressed_right_primary()) return;
    // Teleport body to 5m along head forward (no raycast collision).
    rig::set_body_position(rig::body_position() + rig::head_forward() * 5.f);
    rig::zero_velocity();
}

void rewind_tick() {
    static std::vector<Vector3> trail;
    const auto& s = input::state();
    if (s.rightTrigger > 0.5f && !trail.empty()) {
        rig::set_body_position(trail.back());
        trail.pop_back();
        rig::zero_velocity();
        return;
    }
    if (trail.size() < 240) trail.push_back(rig::body_position());
    else { trail.erase(trail.begin()); trail.push_back(rig::body_position()); }
}

// --- Ragdoll via velocity scrub ---------------------------------------

void ragdoll_tick() {
    // Randomly nudge body around.
    static float t = 0;
    t += dt();
    float w = std::sin(t * 3.f);
    rig::translate({w * dt() * 0.2f, 0, std::cos(t * 2.f) * dt() * 0.2f});
}

} // namespace

void register_all() {
    using registry::add;
    // The category name doubles as the menu page name.
    add("Platforms",            "Movement Mods", platforms_tick);
    add("Trigger Platforms",    "Movement Mods", trigger_platforms_tick);
    add("Fly",                  "Movement Mods", fly_tick);
    add("Trigger Fly",          "Movement Mods", trigger_fly_tick);
    add("Noclip Fly",           "Movement Mods", noclip_fly_tick);
    add("Joystick Fly",         "Movement Mods", joystick_fly_tick);
    add("Hand Fly",             "Movement Mods", hand_fly_tick);
    add("Bark Fly",             "Movement Mods", bark_fly_tick);
    add("Slingshot Fly",        "Movement Mods", slingshot_fly_tick);
    add("WASD Fly",             "Movement Mods", wasd_fly_tick);
    add("Dash",                 "Movement Mods", dash_tick);
    add("Iron Man",             "Movement Mods", iron_man_tick);
    add("Spider Man",           "Movement Mods", spider_man_tick);
    add("Speed Boost",          "Movement Mods", speed_boost_tick);
    add("Playspace Abuse",      "Movement Mods", playspace_abuse_tick);
    add("Long Arms",            "Movement Mods", long_arms_tick, long_arms_toggled);
    add("High Jump",            "Movement Mods", high_jump_tick);
    add("Infinite Jump",        "Movement Mods", infinite_jump_tick);
    add("Super Jump",           "Movement Mods", super_jump_tick);
    add("Low Gravity",          "Movement Mods", low_gravity_tick);
    add("Zero Gravity",         "Movement Mods", zero_gravity_tick);
    add("High Gravity",         "Movement Mods", high_gravity_tick);
    add("Reverse Gravity",      "Movement Mods", reverse_gravity_tick);
    add("Feather Falling",      "Movement Mods", feather_falling_tick);
    add("Up And Down",          "Movement Mods", up_down_tick);
    add("Left And Right",       "Movement Mods", left_right_tick);
    add("Forwards And Backwards","Movement Mods", fwd_back_tick);
    add("Checkpoint",           "Movement Mods", checkpoint_tick);
    add("Teleport Gun",         "Movement Mods", teleport_gun_tick);
    add("Rewind",               "Movement Mods", rewind_tick);
    add("Ragdoll",              "Movement Mods", ragdoll_tick);
}

} // namespace mods::movement
