#include "registry.h"
#include "state.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <sys/stat.h>
#include <android/log.h>

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "iistupid/reg", __VA_ARGS__)

namespace registry {

namespace {
std::vector<Mod> g_mods;
std::vector<std::string> g_pages;
std::unordered_map<std::string, std::vector<Mod*>> g_page_mods;
std::mutex g_mu;

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

namespace registry {
void register_core() {
    // Registration order controls page order in the menu.
    mods::safety::register_all();
    mods::movement::register_all();
    mods::advantages::register_all();
    mods::visuals::register_all();
    mods::fun::register_all();
    mods::overpowered::register_all();
    LOG("registered %zu mods across %zu pages", g_mods.size(), g_pages.size());
}
}
