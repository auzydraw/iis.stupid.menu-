#include "credits.h"
#include "../registry.h"

#include <android/log.h>

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "iistupid/credits", __VA_ARGS__)

namespace mods::credits {

namespace {

// Credit "mods" are inert - highlighting them logs the credit line.
// They never change game state; they just fill the Credits page.

void rick_tick()  { LOG("Rick  shouts for his work on ii's Stupid Menu"); }
void nkhhi_tick() { LOG("nkhhi  shouts for his work on ii's Stupid Menu"); }
void ii_tick()    { LOG("iiDk / Goldentrophy  original PC mod author");   }
void port_tick()  { LOG("Native Android port  built for auzydraw");       }

} // namespace

void register_all() {
    using registry::add;
    add("Rick",                 "Credits", rick_tick);
    add("nkhhi",                "Credits", nkhhi_tick);
    add("iiDk / Goldentrophy",  "Credits", ii_tick);
    add("Android Port",         "Credits", port_tick);
}

} // namespace mods::credits
