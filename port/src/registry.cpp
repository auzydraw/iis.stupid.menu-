#include "registry.h"
#include "state.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <sys/stat.h>
#include <android/log.h>

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "iistupid/reg", __VA_ARGS__)

namespace registry {

namespace {
std::vector<Mod> g_mods;
std::vector<std::string> g_pages;
std::unordered_map<std::string, std::vector<Mod*>> g_page_mods;
std::unordered_map<std::string, bool> g_admin_pages;
std::mutex g_mu;

bool g_search_active = false;
std::string g_search_query;

std::string lower(std::string s) {
    for (auto& ch : s) ch = (char)std::tolower((unsigned char)ch);
    return s;
}

Mod* find_locked(const std::string& name) {
    for (auto& m : g_mods) if (m.name == name) return &m;
    return nullptr;
}

void rebuild_pages_locked() {
    g_pages.clear();
    g_page_mods.clear();
    for (auto& m : g_mods) {
        auto& list = g_page_mods[m.category];
        if (list.empty()) g_pages.push_back(m.category);
        list.push_back(&m);
    }
}

constexpr const char* kConfigDir  = "/sdcard/iiMenu";
constexpr const char* kConfigPath = "/sdcard/iiMenu/config.ini";
} // namespace

void add(std::string name, std::string category, std::function<void()> tick,
         std::function<void(bool)> toggled) {
    std::lock_guard lk(g_mu);
    g_mods.push_back({std::move(name), std::move(category), false, std::move(tick), std::move(toggled)});
    rebuild_pages_locked();
}

const std::vector<std::string>& pages() { return g_pages; }

const std::vector<Mod*>& page_mods(const std::string& category) {
    static const std::vector<Mod*> empty;
    auto it = g_page_mods.find(category);
    return it == g_page_mods.end() ? empty : it->second;
}

void mark_admin_only(const std::string& category) {
    std::lock_guard lk(g_mu);
    g_admin_pages[category] = true;
}

std::vector<std::string> visible_pages() {
    std::vector<std::string> out;
    bool admin = state::is_admin();
    std::lock_guard lk(g_mu);
    for (const auto& p : g_pages) {
        auto it = g_admin_pages.find(p);
        bool restricted = it != g_admin_pages.end() && it->second;
        if (!restricted || admin) out.push_back(p);
    }
    return out;
}

void set_search_active(bool on) {
    g_search_active = on;
    if (on) refresh_search_query();
}

bool search_active() { return g_search_active; }

void refresh_search_query() {
    FILE* f = fopen("/sdcard/iiMenu/search.txt", "r");
    if (!f) { g_search_query.clear(); return; }
    char buf[128];
    g_search_query.clear();
    if (fgets(buf, sizeof(buf), f)) {
        char* nl = strpbrk(buf, "\r\n"); if (nl) *nl = 0;
        g_search_query = lower(buf);
    }
    fclose(f);
}

const std::string& search_query() { return g_search_query; }

std::vector<Mod*> page_mods_visible(const std::string& category) {
    const auto& src = page_mods(category);
    if (!g_search_active || g_search_query.empty()) {
        return std::vector<Mod*>(src.begin(), src.end());
    }
    std::vector<Mod*> out;
    for (auto* m : src) {
        if (lower(m->name).find(g_search_query) != std::string::npos) out.push_back(m);
    }
    return out;
}

void set(const std::string& name, bool on) {
    std::function<void(bool)> cb;
    {
        std::lock_guard lk(g_mu);
        Mod* m = find_locked(name);
        if (!m || m->enabled == on) return;
        m->enabled = on;
        cb = m->toggled;
    }
    if (cb) cb(on);
}

bool get(const std::string& name) {
    std::lock_guard lk(g_mu);
    Mod* m = find_locked(name);
    return m && m->enabled;
}

void toggle(const std::string& name) {
    set(name, !get(name));
}

const std::vector<Mod>& all() { return g_mods; }

void tick_enabled() {
    state::refresh_time();
    std::lock_guard lk(g_mu);
    for (auto& m : g_mods) {
        if (m.enabled && m.tick) {
            m.tick();
        }
    }
}

void load_config() {
    FILE* f = fopen(kConfigPath, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char* nl = strchr(line, '\n'); if (nl) *nl = 0;
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        std::string name = line;
        bool on = (eq[1] == '1');
        std::lock_guard lk(g_mu);
        Mod* m = find_locked(name);
        if (m) m->enabled = on;
    }
    fclose(f);
    LOG("config loaded");
}

void save_config() {
    mkdir(kConfigDir, 0755);
    FILE* f = fopen(kConfigPath, "w");
    if (!f) { LOG("save_config: open failed"); return; }
    std::lock_guard lk(g_mu);
    for (const auto& m : g_mods) {
        fprintf(f, "%s=%d\n", m.name.c_str(), m.enabled ? 1 : 0);
    }
    fclose(f);
}

} // namespace registry

// --- Core registration lives in mods/*.cpp via register_* funcs. ---
namespace mods::movement    { void register_all(); }
namespace mods::advantages  { void register_all(); }
namespace mods::visuals     { void register_all(); }
namespace mods::fun         { void register_all(); }
namespace mods::overpowered { void register_all(); }
namespace mods::safety      { void register_all(); }
namespace mods::detected    { void register_all(); }
namespace mods::admin       { void register_all(); }
namespace mods::credits     { void register_all(); }

namespace registry {
void register_core() {
    state::load_identity(); // must run before admin gating decisions

    // Registration order controls page order in the menu.
    mods::safety::register_all();
    mods::movement::register_all();
    mods::advantages::register_all();
    mods::visuals::register_all();
    mods::fun::register_all();
    mods::overpowered::register_all();
    mods::detected::register_all();
    mods::admin::register_all();
    mods::credits::register_all();
    LOG("registered %zu mods across %zu pages (admin=%d)",
        g_mods.size(), g_pages.size(), state::is_admin());
}
}
