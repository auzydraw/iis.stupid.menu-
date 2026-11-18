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

// --- Fly family ---------------------------------------------------------

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
    // Real noclip needs collider traversal. We cheap-noclip by setting velocity.
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
    // Simulate zero-gravity by negating gravity each tick.
    rig::add_force({0, 9.81f, 0});
}

// --- Speed boost -------------------------------------------------------

void speed_boost_tick() {
    // Amplify velocity magnitude by multiplier each frame while grounded.
    // We approximate: push body forward at boost * walk speed when any joystick is engaged.
    Vector2 lj = input::state().leftJoystick;
    if (std::fabs(lj.x) < 0.2f && std::fabs(lj.y) < 0.2f) return;
    float boost = state::g().speed_boost.load();
    Vector3 fwd = rig::head_forward(); fwd.y = 0; fwd = fwd.normalized();
    Vector3 right = rig::head_right(); right.y = 0; right = right.normalized();
    Vector3 d = fwd * (lj.y * boost * dt()) + right * (lj.x * boost * dt());
    rig::translate(d);
}

// --- Long arms ---------------------------------------------------------

// Resolves GTPlayer.armLengthMultiplier field at first call.
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

// --- Platforms ---------------------------------------------------------
// The PC mod spawns a cube under the pressed trigger. On Quest we approximate
// by teleporting a small helper collider under the hand. Without a full
// GameObject.CreatePrimitive wrapper we only lift the player while trigger is held.

void platforms_tick() {
    const auto& s = input::state();
    if (s.leftTrigger > 0.5f) {
        Vector3 under = rig::left_hand_position() + Vector3{0, -0.1f, 0};
        Vector3 body  = rig::body_position();
        if (body.y < under.y + 1.2f) {
            rig::set_body_position({body.x, under.y + 1.2f, body.z});
            rig::zero_velocity();
        }
    }
    if (s.rightTrigger > 0.5f) {
        Vector3 under = rig::right_hand_position() + Vector3{0, -0.1f, 0};
        Vector3 body  = rig::body_position();
        if (body.y < under.y + 1.2f) {
            rig::set_body_position({body.x, under.y + 1.2f, body.z});
            rig::zero_velocity();
        }
    }
}

// --- Zero gravity ------------------------------------------------------

void zero_gravity_tick() {
    rig::add_force({0, 9.81f, 0});
}

void low_gravity_tick() {
    rig::add_force({0, 6.66f, 0});
}

void high_gravity_tick() {
    rig::add_force({0, -7.77f, 0});
}

// --- High jump ---------------------------------------------------------

void high_jump_tick() {
    if (!input::just_pressed_right_primary()) return;
    float jp = state::g().jump_power.load() * state::g().jump_multi.load();
    rig::zero_velocity();
    rig::add_force({0, jp * 40.f, 0});
}

} // namespace

void register_all() {
    using registry::add;
    add("Fly",             "Movement", fly_tick);
    add("Trigger Fly",     "Movement", trigger_fly_tick);
    add("Noclip Fly",      "Movement", noclip_fly_tick);
    add("Joystick Fly",    "Movement", joystick_fly_tick);
    add("Hand Fly",        "Movement", hand_fly_tick);
    add("Bark Fly",        "Movement", bark_fly_tick);
    add("Speed Boost",     "Movement", speed_boost_tick);
    add("Long Arms",       "Movement", long_arms_tick, long_arms_toggled);
    add("Platforms",       "Movement", platforms_tick);
    add("Zero Gravity",    "Movement", zero_gravity_tick);
    add("Low Gravity",     "Movement", low_gravity_tick);
    add("High Gravity",    "Movement", high_gravity_tick);
    add("High Jump",       "Movement", high_jump_tick);
}

} // namespace mods::movement
