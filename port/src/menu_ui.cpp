// In-world menu panel. Spawns a floating panel in front of the player on
// open, destroys everything on close. Click detection via AABB test on the
// cursor sphere (parented to the left hand, since right B holds the menu).
//
// Layout:
//   left column:   Prev, Next, Search, Verify
//   right column:  mod list (scrolls when > LIST_ROWS)
//   header:        current page title
//   footer (sel):  selected mod name + ON/OFF
//
// Keyboard: opens as an overlay when Search or Verify is clicked. Typing
// writes to a buffer. Enter dispatches to the chosen target.

#include "menu_ui.h"
#include "ui.h"
#include "input.h"
#include "rig.h"
#include "registry.h"
#include "state.h"
#include "unity.h"

#include <android/log.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "iistupid/menu_ui", __VA_ARGS__)

using unity::Vector3;
using unity::Color;

namespace menu_ui {

namespace {

// -------- Colors --------
const Color C_BG      = {0.10f, 0.10f, 0.12f, 1};
const Color C_BTN     = {0.20f, 0.25f, 0.35f, 1};
const Color C_HOVER   = {0.40f, 0.55f, 0.90f, 1};
const Color C_ON      = {0.30f, 0.80f, 0.35f, 1};
const Color C_PREV    = {0.90f, 0.55f, 0.20f, 1};
const Color C_NEXT    = {0.90f, 0.55f, 0.20f, 1};
const Color C_SEARCH  = {0.25f, 0.75f, 0.45f, 1};
const Color C_VERIFY  = {0.60f, 0.35f, 0.85f, 1};
const Color C_KBD_KEY = {0.22f, 0.25f, 0.30f, 1};
const Color C_KBD_ENT = {0.30f, 0.70f, 0.40f, 1};
const Color C_KBD_BS  = {0.70f, 0.30f, 0.30f, 1};
const Color C_TXT_BG  = {0.05f, 0.05f, 0.08f, 1};

// -------- Button --------
struct Button {
    void* go = nullptr;
    Vector3 center{};
    Vector3 half{};
    Color base = C_BTN;
    bool  hovered = false;
    std::string label;
};

bool aabb_hit(const Vector3& p, const Button& b) {
    Vector3 d = p - b.center;
    return std::fabs(d.x) <= b.half.x
        && std::fabs(d.y) <= b.half.y
        && std::fabs(d.z) <= b.half.z;
}

void apply_hover_color(Button& b) {
    ui::set_color(b.go, b.hovered ? C_HOVER : b.base);
}

// -------- Globals --------
bool s_open = false;

void* s_cursor = nullptr; // sphere, parented to left hand
void* s_panel  = nullptr; // bg plane (cube flattened)

Button s_btn_prev;
Button s_btn_next;
Button s_btn_search;
Button s_btn_verify;

constexpr int LIST_ROWS = 10;
std::vector<Button> s_mod_buttons;

int s_page_idx = 0;
int s_scroll   = 0;

// Keyboard
enum class KeyboardTarget { None, Search, Verify };
KeyboardTarget s_kb_target = KeyboardTarget::None;
bool s_kb_visible = false;
void* s_kb_panel = nullptr;
std::vector<Button> s_kb_buttons;
std::string s_typed;
void* s_typed_display_go = nullptr;

// Edge-detect right trigger (click button).
bool s_prev_trigger_down = false;

std::vector<std::string> s_visible_pages;

// -------- Build helpers --------

void refresh_pages() {
    s_visible_pages = registry::visible_pages();
    if (s_visible_pages.empty()) {
        s_page_idx = 0;
    } else if (s_page_idx >= (int)s_visible_pages.size()) {
        s_page_idx = 0;
    }
    s_scroll = 0;
}

std::vector<registry::Mod*> current_mods() {
    if (s_visible_pages.empty()) return {};
    return registry::page_mods_visible(s_visible_pages[s_page_idx]);
}

void* spawn_button(Vector3 pos, Vector3 half, Color c, const char* label) {
    void* go = ui::create_cube();
    if (!go) return nullptr;
    ui::destroy_collider_of(go);
    ui::set_position(go, pos);
    ui::set_local_scale(go, {half.x * 2, half.y * 2, half.z * 2});
    ui::set_color(go, c);
    if (label && *label) {
        void* label_go = ui::create_sphere(); // placeholder host to add TMP on; cube+TMP has weird parenting
        if (label_go) {
            ui::destroy_collider_of(label_go);
            ui::set_position(label_go, pos + Vector3{0, 0, -half.z * 1.2f});
            ui::set_local_scale(label_go, {0.001f, 0.001f, 0.001f});
            ui::set_label(label_go, label, 48.f, {1, 1, 1, 1});
        }
    }
    return go;
}

// -------- Panel layout --------

constexpr float PANEL_DIST = 1.2f;
constexpr float BTN_H = 0.10f;
constexpr float BTN_W = 0.35f;
constexpr float BTN_Z = 0.015f;
constexpr float LIST_BTN_H = 0.07f;
constexpr float LIST_BTN_W = 0.45f;

Vector3 panel_anchor_pos() {
    Vector3 head = rig::head_position();
    Vector3 fwd  = rig::head_forward();
    fwd.y = 0;
    float mag = fwd.mag();
    if (mag < 0.001f) fwd = {0, 0, 1}; else fwd = fwd * (1.f / mag);
    return head + fwd * PANEL_DIST;
}

void build_panel() {
    Vector3 origin = panel_anchor_pos();

    // BG
    s_panel = ui::create_cube();
    ui::destroy_collider_of(s_panel);
    ui::set_position(s_panel, origin);
    ui::set_local_scale(s_panel, {1.1f, 0.9f, 0.02f});
    ui::set_color(s_panel, C_BG);

    // Left column buttons
    float lx = origin.x - 0.35f;
    float y  = origin.y + 0.25f;
    Vector3 half = {BTN_W / 2, BTN_H / 2, BTN_Z};

    s_btn_prev.center = {lx, y, origin.z - 0.02f};
    s_btn_prev.half   = half;
    s_btn_prev.base   = C_PREV;
    s_btn_prev.label  = "PREV";
    s_btn_prev.go     = spawn_button(s_btn_prev.center, half, C_PREV, "PREV");
    y -= 0.14f;

    s_btn_next.center = {lx, y, origin.z - 0.02f};
    s_btn_next.half   = half;
    s_btn_next.base   = C_NEXT;
    s_btn_next.label  = "NEXT";
    s_btn_next.go     = spawn_button(s_btn_next.center, half, C_NEXT, "NEXT");
    y -= 0.14f;

    s_btn_search.center = {lx, y, origin.z - 0.02f};
    s_btn_search.half   = half;
    s_btn_search.base   = C_SEARCH;
    s_btn_search.label  = "SEARCH";
    s_btn_search.go     = spawn_button(s_btn_search.center, half, C_SEARCH, "SEARCH");
    y -= 0.14f;

    s_btn_verify.center = {lx, y, origin.z - 0.02f};
    s_btn_verify.half   = half;
    s_btn_verify.base   = C_VERIFY;
    s_btn_verify.label  = state::is_admin() ? "ADMIN" : "VERIFY";
    s_btn_verify.go     = spawn_button(s_btn_verify.center, half, s_btn_verify.base, s_btn_verify.label.c_str());

    // Right column: mod list
    s_mod_buttons.clear();
    s_mod_buttons.reserve(LIST_ROWS);
    float rx = origin.x + 0.22f;
    float ry = origin.y + 0.30f;
    Vector3 lhalf = {LIST_BTN_W / 2, LIST_BTN_H / 2, BTN_Z};
    for (int i = 0; i < LIST_ROWS; ++i) {
        Button b;
        b.center = {rx, ry - i * (LIST_BTN_H + 0.015f), origin.z - 0.02f};
        b.half   = lhalf;
        b.base   = C_BTN;
        b.go     = spawn_button(b.center, lhalf, C_BTN, "");
        s_mod_buttons.push_back(b);
    }
}

void destroy_panel() {
    for (auto& b : s_mod_buttons) ui::destroy(b.go);
    s_mod_buttons.clear();
    ui::destroy(s_btn_prev.go);   s_btn_prev.go   = nullptr;
    ui::destroy(s_btn_next.go);   s_btn_next.go   = nullptr;
    ui::destroy(s_btn_search.go); s_btn_search.go = nullptr;
    ui::destroy(s_btn_verify.go); s_btn_verify.go = nullptr;
    ui::destroy(s_panel);         s_panel         = nullptr;
}

// -------- Keyboard --------

// 6 rows x 5 cols of letters + bottom row backspace / enter / close
const char* kKbRows[] = {
    "QWERTY",
    "UIOPAS",
    "DFGHJK",
    "LZXCVB",
    "NM.-_ ",
};
constexpr int KB_COLS = 6;
constexpr int KB_ROWS = 5;

void build_keyboard() {
    if (s_kb_visible) return;
    s_kb_visible = true;
    s_typed.clear();

    Vector3 origin = panel_anchor_pos();
    origin = origin + Vector3{0, -0.05f, -0.15f}; // slightly in front, below center

    s_kb_panel = ui::create_cube();
    ui::destroy_collider_of(s_kb_panel);
    ui::set_position(s_kb_panel, origin);
    ui::set_local_scale(s_kb_panel, {0.75f, 0.55f, 0.02f});
    ui::set_color(s_kb_panel, C_TXT_BG);

    s_kb_buttons.clear();
    s_kb_buttons.reserve(KB_COLS * KB_ROWS + 3);

    float key_w = 0.09f, key_h = 0.08f, key_gap = 0.012f;
    float total_w = KB_COLS * key_w + (KB_COLS - 1) * key_gap;
    float start_x = origin.x - total_w / 2 + key_w / 2;

    for (int r = 0; r < KB_ROWS; ++r) {
        for (int c = 0; c < KB_COLS; ++c) {
            char ch = kKbRows[r][c];
            if (ch == 0 || ch == ' ') continue;
            char label[4] = {ch, 0, 0, 0};
            Button b;
            b.center = {
                start_x + c * (key_w + key_gap),
                origin.y + 0.18f - r * (key_h + key_gap),
                origin.z - 0.02f,
            };
            b.half = {key_w / 2, key_h / 2, BTN_Z};
            b.base = C_KBD_KEY;
            b.label = label;
            b.go   = spawn_button(b.center, b.half, b.base, label);
            s_kb_buttons.push_back(b);
        }
    }

    float bottom_y = origin.y + 0.18f - KB_ROWS * (key_h + key_gap);
    Button bs;
    bs.center = {origin.x - 0.21f, bottom_y, origin.z - 0.02f};
    bs.half   = {0.09f, 0.055f, BTN_Z};
    bs.base   = C_KBD_BS;
    bs.label  = "<-";
    bs.go     = spawn_button(bs.center, bs.half, bs.base, "<-");
    s_kb_buttons.push_back(bs);

    Button en;
    en.center = {origin.x, bottom_y, origin.z - 0.02f};
    en.half   = {0.09f, 0.055f, BTN_Z};
    en.base   = C_KBD_ENT;
    en.label  = "ENTER";
    en.go     = spawn_button(en.center, en.half, en.base, "ENTER");
    s_kb_buttons.push_back(en);

    Button cl;
    cl.center = {origin.x + 0.21f, bottom_y, origin.z - 0.02f};
    cl.half   = {0.09f, 0.055f, BTN_Z};
    cl.base   = C_KBD_BS;
    cl.label  = "CLOSE";
    cl.go     = spawn_button(cl.center, cl.half, cl.base, "CLOSE");
    s_kb_buttons.push_back(cl);

    // Typed buffer display (sphere + TMP label).
    s_typed_display_go = ui::create_sphere();
    if (s_typed_display_go) {
        ui::destroy_collider_of(s_typed_display_go);
        ui::set_position(s_typed_display_go, {origin.x, origin.y + 0.22f, origin.z - 0.03f});
        ui::set_local_scale(s_typed_display_go, {0.001f, 0.001f, 0.001f});
        ui::set_label(s_typed_display_go, "[empty]", 72.f, {1, 1, 1, 1});
    }
}

void destroy_keyboard() {
    for (auto& b : s_kb_buttons) ui::destroy(b.go);
    s_kb_buttons.clear();
    ui::destroy(s_kb_panel);        s_kb_panel = nullptr;
    ui::destroy(s_typed_display_go); s_typed_display_go = nullptr;
    s_kb_visible = false;
    s_kb_target = KeyboardTarget::None;
}

void update_typed_display() {
    if (!s_typed_display_go) return;
    ui::destroy(s_typed_display_go);
    Vector3 origin = panel_anchor_pos();
    origin = origin + Vector3{0, -0.05f, -0.15f};
    s_typed_display_go = ui::create_sphere();
    if (!s_typed_display_go) return;
    ui::destroy_collider_of(s_typed_display_go);
    ui::set_position(s_typed_display_go, {origin.x, origin.y + 0.22f, origin.z - 0.03f});
    ui::set_local_scale(s_typed_display_go, {0.001f, 0.001f, 0.001f});
    ui::set_label(s_typed_display_go, s_typed.empty() ? "[empty]" : s_typed.c_str(),
                  72.f, {1, 1, 1, 1});
}

// -------- Cursor --------

void build_cursor() {
    if (s_cursor) return;
    s_cursor = ui::create_sphere();
    if (!s_cursor) return;
    ui::destroy_collider_of(s_cursor);
    ui::set_local_scale(s_cursor, {0.025f, 0.025f, 0.025f});
    ui::set_color(s_cursor, {1.0f, 0.9f, 0.2f, 1});
}

void destroy_cursor() {
    ui::destroy(s_cursor);
    s_cursor = nullptr;
}

Vector3 cursor_world_position() {
    // Place at left-hand position + a bit forward (laser-pointer feel).
    Vector3 p = rig::left_hand_position() + rig::left_hand_forward() * 0.08f;
    return p;
}

void update_cursor() {
    if (!s_cursor) return;
    ui::set_position(s_cursor, cursor_world_position());
}

// -------- Mod list rendering --------

void refresh_mod_buttons() {
    auto mods = current_mods();
    int shown = (int)std::min<size_t>(mods.size() - (size_t)s_scroll, (size_t)LIST_ROWS);
    for (int i = 0; i < LIST_ROWS; ++i) {
        Button& b = s_mod_buttons[i];
        if (i < shown) {
            auto* m = mods[s_scroll + i];
            b.base = m->enabled ? C_ON : C_BTN;
            ui::set_active(b.go, true);
        } else {
            ui::set_active(b.go, false);
        }
    }
}

// -------- Interaction --------

void on_click_prev() {
    if (s_visible_pages.empty()) return;
    s_page_idx = (s_page_idx - 1 + (int)s_visible_pages.size()) % (int)s_visible_pages.size();
    s_scroll = 0;
    LOG("page = %s", s_visible_pages[s_page_idx].c_str());
}

void on_click_next() {
    if (s_visible_pages.empty()) return;
    s_page_idx = (s_page_idx + 1) % (int)s_visible_pages.size();
    s_scroll = 0;
    LOG("page = %s", s_visible_pages[s_page_idx].c_str());
}

void on_click_search() {
    s_kb_target = KeyboardTarget::Search;
    build_keyboard();
}

void on_click_verify() {
    s_kb_target = KeyboardTarget::Verify;
    build_keyboard();
}

void on_click_mod(int list_idx) {
    auto mods = current_mods();
    int mod_i = s_scroll + list_idx;
    if (mod_i < 0 || mod_i >= (int)mods.size()) return;
    auto* m = mods[mod_i];
    bool now = !registry::get(m->name);
    registry::set(m->name, now);
    LOG("toggle %s -> %d", m->name.c_str(), now);
}

void on_kb_key(const std::string& label) {
    if (label == "<-") {
        if (!s_typed.empty()) s_typed.pop_back();
    } else if (label == "ENTER") {
        LOG("kb enter: '%s' target=%d", s_typed.c_str(), (int)s_kb_target);
        if (s_kb_target == KeyboardTarget::Search) {
            registry::set_search_query_direct(s_typed);
            registry::set_search_active(!s_typed.empty());
            s_scroll = 0;
        } else if (s_kb_target == KeyboardTarget::Verify) {
            bool ok = state::try_verify(s_typed.c_str());
            LOG("verify '%s' -> %d", s_typed.c_str(), ok);
            refresh_pages(); // admin page may become visible
            // rebuild panel to update Verify label color/text
            destroy_panel();
            build_panel();
        }
        destroy_keyboard();
        return;
    } else if (label == "CLOSE") {
        destroy_keyboard();
        return;
    } else {
        if (s_typed.size() < 24) s_typed += label;
    }
    update_typed_display();
}

void handle_click(const Vector3& cursor_pos) {
    // Keyboard first (overlays panel).
    if (s_kb_visible) {
        for (auto& b : s_kb_buttons) {
            if (aabb_hit(cursor_pos, b)) {
                on_kb_key(b.label);
                return;
            }
        }
        return;
    }
    if (aabb_hit(cursor_pos, s_btn_prev))   { on_click_prev();   return; }
    if (aabb_hit(cursor_pos, s_btn_next))   { on_click_next();   return; }
    if (aabb_hit(cursor_pos, s_btn_search)) { on_click_search(); return; }
    if (aabb_hit(cursor_pos, s_btn_verify)) { on_click_verify(); return; }
    for (int i = 0; i < (int)s_mod_buttons.size(); ++i) {
        if (aabb_hit(cursor_pos, s_mod_buttons[i])) { on_click_mod(i); return; }
    }
}

void update_hover_colors(const Vector3& cursor_pos) {
    auto do_btn = [&](Button& b) {
        bool h = aabb_hit(cursor_pos, b);
        if (h != b.hovered) {
            b.hovered = h;
            apply_hover_color(b);
        }
    };
    if (s_kb_visible) {
        for (auto& b : s_kb_buttons) do_btn(b);
        return;
    }
    do_btn(s_btn_prev);
    do_btn(s_btn_next);
    do_btn(s_btn_search);
    do_btn(s_btn_verify);
    for (auto& b : s_mod_buttons) do_btn(b);
}

} // namespace

bool is_open() { return s_open; }

void open() {
    if (s_open) return;
    if (!ui::init()) { LOG("ui init failed; menu suppressed"); return; }
    refresh_pages();
    build_panel();
    build_cursor();
    refresh_mod_buttons();
    s_open = true;
    LOG("menu open; admin=%d pages=%zu", state::is_admin(), s_visible_pages.size());
}

void close() {
    if (!s_open) return;
    destroy_keyboard();
    destroy_panel();
    destroy_cursor();
    s_open = false;
}

void tick() {
    if (!s_open) return;
    refresh_mod_buttons();

    Vector3 cp = cursor_world_position();
    update_cursor();
    update_hover_colors(cp);

    // Right trigger = click (edge).
    bool down = input::state().rightTrigger > 0.5f;
    if (down && !s_prev_trigger_down) handle_click(cp);
    s_prev_trigger_down = down;
}

} // namespace menu_ui
