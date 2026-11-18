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

// Visible pages: honors the admin gate (admin-only pages are hidden if the
// discord gate didn't match).
std::vector<std::string> visible_pages();

// Mark a page as admin-only. Only visible when state::is_admin() is true.
void mark_admin_only(const std::string& category);

// Search: if active, page_mods_visible filters by case-insensitive substring
// match on mod name.
void set_search_active(bool on);
bool search_active();
void refresh_search_query(); // reads /sdcard/iiMenu/search.txt
const std::string& search_query();
std::vector<Mod*> page_mods_visible(const std::string& category);

void tick_enabled();

// Load/save enabled-state map to /sdcard/iiMenu/config.ini (auto-created).
void load_config();
void save_config();

// Registers the core mod set (called once from main init).
void register_core();

} // namespace registry
