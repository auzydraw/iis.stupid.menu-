#include "menu.h"
#include "input.h"
#include "registry.h"

#include <android/log.h>

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "iistupid/menu", __VA_ARGS__)

namespace menu {

namespace {

bool s_open = false;
int  s_page_idx = 0;
int  s_cursor   = 0;

bool s_lj_up = false;
bool s_lj_dn = false;
bool s_lj_lt = false;
bool s_lj_rt = false;

void log_state() {
    const auto& pages = registry::pages();
    if (pages.empty()) { LOG("menu: no pages"); return; }
    const auto& page = pages[s_page_idx];
    const auto& mods = registry::page_mods(page);
    if (mods.empty()) { LOG("page=%s (empty)", page.c_str()); return; }
    const auto* m = mods[s_cursor];
    LOG("page=%s [%d/%zu]  >  %s [%s]",
        page.c_str(), s_cursor + 1, mods.size(),
        m->name.c_str(), m->enabled ? "ON" : "OFF");
}

} // namespace

void tick() {
    const auto& s = input::state();

    // Open / close: left grip + B.
    if (s.leftGrip && input::just_pressed_right_secondary()) {
        s_open = !s_open;
        LOG("menu %s", s_open ? "OPEN" : "CLOSE");
        if (s_open) log_state();
    }
    if (!s_open) return;

    const auto& pages = registry::pages();
    if (pages.empty()) return;

    // Horizontal (page swap) edge-triggered.
    if (s.leftJoystick.x < -0.6f) {
        if (!s_lj_lt) {
            s_page_idx = (s_page_idx - 1 + (int)pages.size()) % (int)pages.size();
            s_cursor = 0;
            log_state();
            s_lj_lt = true;
        }
    } else s_lj_lt = false;

    if (s.leftJoystick.x > 0.6f) {
        if (!s_lj_rt) {
            s_page_idx = (s_page_idx + 1) % (int)pages.size();
            s_cursor = 0;
            log_state();
            s_lj_rt = true;
        }
    } else s_lj_rt = false;

    const auto& mods = registry::page_mods(pages[s_page_idx]);
    if (mods.empty()) return;
    if (s_cursor >= (int)mods.size()) s_cursor = 0;

    // Vertical (mod cursor) edge-triggered.
    if (s.leftJoystick.y > 0.6f) {
        if (!s_lj_up) {
            s_cursor = (s_cursor - 1 + (int)mods.size()) % (int)mods.size();
            log_state();
            s_lj_up = true;
        }
    } else s_lj_up = false;

    if (s.leftJoystick.y < -0.6f) {
        if (!s_lj_dn) {
            s_cursor = (s_cursor + 1) % (int)mods.size();
            log_state();
            s_lj_dn = true;
        }
    } else s_lj_dn = false;

    // Toggle highlighted.
    if (input::just_pressed_right_primary()) {
        const auto* m = mods[s_cursor];
        bool now = !registry::get(m->name);
        registry::set(m->name, now);
        LOG("toggle %s -> %d", m->name.c_str(), now);
    }

    // Save config.
    if (input::just_pressed_left_primary()) {
        registry::save_config();
        LOG("config saved");
    }
}

} // namespace menu
