#include "menu.h"
#include "input.h"
#include "registry.h"

#include <android/log.h>

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "iistupid/menu", __VA_ARGS__)

namespace menu {

namespace {

int    s_cursor = 0;
bool   s_menu_open = false;
double s_save_at = 0;

// Open menu: left grip + right secondary (B).
// Navigate: left joystick up/down (edge).
// Toggle:   right primary (A).
// Save:     left primary (X).
// Close:    left grip + right secondary again.

bool s_lj_up_latched = false;
bool s_lj_dn_latched = false;

} // namespace

void tick() {
    const auto& s = input::state();

    // Open / close.
    if (s.leftGrip && input::just_pressed_right_secondary()) {
        s_menu_open = !s_menu_open;
        LOG("menu %s cursor=%d", s_menu_open ? "OPEN" : "CLOSE", s_cursor);
    }
    if (!s_menu_open) return;

    const auto& mods = registry::all();
    if (mods.empty()) return;

    // Navigation (edge-triggered).
    if (s.leftJoystick.y > 0.6f) {
        if (!s_lj_up_latched) {
            s_cursor = (s_cursor - 1 + (int)mods.size()) % (int)mods.size();
            LOG("cursor=%d (%s)", s_cursor, mods[s_cursor].name.c_str());
            s_lj_up_latched = true;
        }
    } else s_lj_up_latched = false;

    if (s.leftJoystick.y < -0.6f) {
        if (!s_lj_dn_latched) {
            s_cursor = (s_cursor + 1) % (int)mods.size();
            LOG("cursor=%d (%s)", s_cursor, mods[s_cursor].name.c_str());
            s_lj_dn_latched = true;
        }
    } else s_lj_dn_latched = false;

    // Toggle currently highlighted.
    if (input::just_pressed_right_primary()) {
        const auto& m = mods[s_cursor];
        bool now = !registry::get(m.name);
        registry::set(m.name, now);
        LOG("toggle %s -> %d", m.name.c_str(), now);
    }

    // Save config.
    if (input::just_pressed_left_primary()) {
        registry::save_config();
        LOG("config saved");
    }
}

} // namespace menu
