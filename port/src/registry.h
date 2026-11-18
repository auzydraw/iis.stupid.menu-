#pragma once
#include <functional>
#include <string>
#include <vector>

namespace registry {

struct Mod {
    std::string name;
    std::string category;
    bool enabled;
    std::function<void()> tick;       // runs every frame while enabled
    std::function<void(bool)> toggled; // optional hook on state change
};

void add(std::string name, std::string category, std::function<void()> tick,
         std::function<void(bool)> toggled = {});

// Flip a mod by name. Fires toggled callback.
void set(const std::string& name, bool on);
bool get(const std::string& name);
void toggle(const std::string& name);

// Enumerate (order of registration).
const std::vector<Mod>& all();

// Pages: distinct categories in registration order, and the mods in each.
const std::vector<std::string>& pages();
const std::vector<Mod*>& page_mods(const std::string& category);

void tick_enabled();

// Load/save enabled-state map to /sdcard/iiMenu/config.ini (auto-created).
void load_config();
void save_config();

// Registers the core mod set (called once from main init).
void register_core();

} // namespace registry
