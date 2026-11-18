#include "menu.h"
#include "input.h"
#include "registry.h"
#include "state.h"

#include <android/log.h>

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "iistupid/menu", __VA_ARGS__)

namespace menu {

// Branding header. Change these two strings to re-brand the in-game banner.
constexpr const char* kTitle = "ii's Stupid Menu  Native";
constexpr const char* kMotd  = "built for auzydraw  shouts to Rick and nkhhi";

namespace {

bool s_open = false;
int  s_page_idx = 0;
int  s_cursor   = 0;

bool s_lj_up = false;
bool s_lj_dn = false;
bool s_lj_lt = false;
bool s_lj_rt = false;

std::vector<std::string> s_pages;

void refresh_pages() {
    s_pages = registry::visible_pages();
    if (s_page_idx >= (int)s_pages.size()) s_page_idx = 0;
}

std::vector<registry::Mod*> current_mods() {
    if (s_pages.empty()) return {};
    return registry::page_mods_visible(s_pages[s_page_idx]);
}

void log_state() {
    if (s_pages.empty()) { LOG("menu: no pages"); return; }
    const auto& page = s_pages[s_page_idx];
    auto mods = current_mods();
    if (mods.empty()) {
        LOG("page=%s (empty%s%s)",
            page.c_str(),
            registry::search_active() ? ", search " : "",
            registry::search_active() ? registry::search_query().c_str() : "");
        return;
    }
    if (s_cursor >= (int)mods.size()) s_cursor = 0;
    const auto* m = mods[s_cursor];
    LOG("page=%s [%d/%zu]  >  %s [%s]%s",
        page.c_str(), s_cursor + 1, mods.size(),
        m->name.c_str(), m->enabled ? "ON" : "OFF",
        registry::search_active() ? "  [SEARCH]" : "");
}

void banner() {
    LOG("===== %s =====", kTitle);
    LOG("%s", kMotd);
    LOG("discord='%s' admin=%d", state::discord_username(), state::is_admin());
    if (registry::search_active())
        LOG("search active: query='%s'", registry::search_query().c_str());
}

} // namespace

void tick() {
    const auto& s = input::state();

    // Open / close: left grip + B.
    if (s.leftGrip && input::just_pressed_right_secondary()) {
        s_open = !s_open;
        refresh_pages();
        LOG("menu %s", s_open ? "OPEN" : "CLOSE");
        if (s_open) { banner(); log_state(); }
    }
    if (!s_open) return;

    // Toggle search mode: left grip + left joystick click (edge).
    // Each enter re-reads /sdcard/iiMenu/search.txt so the operator can edit
    // the query live.
    if (s.leftGrip && input::just_pressed_left_primary() == false) {
        // left primary is already used for save config; use a different gesture.
    }
    static bool s_prev_ljc = false;
    if (s.leftGrip && s.leftJoystickClick && !s_prev_ljc) {
        registry::set_search_active(!registry::search_active());
        s_cursor = 0;
        LOG("search %s", registry::search_active() ? "ON" : "OFF");
        if (registry::search_active()) LOG("query='%s'", registry::search_query().c_str());
        log_state();
    }
    s_prev_ljc = s.leftJoystickClick;

    if (s_pages.empty()) return;

    // Horizontal (page swap) edge-triggered.
    if (s.leftJoystick.x < -0.6f) {
        if (!s_lj_lt) {
            s_page_idx = (s_page_idx - 1 + (int)s_pages.size()) % (int)s_pages.size();
            s_cursor = 0;
            log_state();
            s_lj_lt = true;
        }
    } else s_lj_lt = false;

    if (s.leftJoystick.x > 0.6f) {
        if (!s_lj_rt) {
            s_page_idx = (s_page_idx + 1) % (int)s_pages.size();
            s_cursor = 0;
            log_state();
            s_lj_rt = true;
        }
    } else s_lj_rt = false;

    auto mods = current_mods();
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

    // Save config (left X, when NOT holding left grip — grip + click = search).
    if (!s.leftGrip && input::just_pressed_left_primary()) {
        registry::save_config();
        LOG("config saved");
    }
}

} // namespace menu
