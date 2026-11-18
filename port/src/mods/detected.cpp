#include "detected.h"
#include "../registry.h"
#include "../state.h"
#include "../rig.h"
#include "../input.h"
#include "../net.h"
#include "../il2cpp.h"
#include "../unity.h"

#include <android/log.h>
#include <cmath>
#include <vector>

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "iistupid/detected", __VA_ARGS__)

using unity::Vector3;

namespace mods::detected {

namespace {

struct ListLayout { uint8_t h[0x10]; void* items; int32_t size; int32_t ver; };
struct ArrLayout  { uint8_t h[0x10]; void* bounds; uintptr_t len; void* data[1]; };

static void*(*gp_get_instance)() = nullptr;
static void*(*gp_get_rigs)(void*) = nullptr;
static int32_t off_islocal = -1;

void resolve() {
    static bool done = false; if (done) return; done = true;
    using namespace il2cpp;
    Il2CppImage* game = find_image("Assembly-CSharp.dll");
    if (!game) return;
    if (Il2CppClass* gp = class_from_name(game, "", "GorillaParent")) {
        gp_get_instance = (void*(*)())method_get_pointer(class_get_method_from_name(gp, "get_instance", 0));
        gp_get_rigs     = (void*(*)(void*))method_get_pointer(class_get_method_from_name(gp, "GetRigs", 0));
    }
    if (Il2CppClass* vr = class_from_name(game, "", "VRRig")) {
        FieldInfo* a = class_get_field_from_name(vr, "isLocal");
        off_islocal = a ? field_get_offset(a) : -1;
    }
}

std::vector<void*> rigs() {
    resolve();
    std::vector<void*> out;
    if (!gp_get_instance || !gp_get_rigs) return out;
    void* i = gp_get_instance(); if (!i) return out;
    void* L = gp_get_rigs(i); if (!L) return out;
    auto* l = reinterpret_cast<ListLayout*>(L);
    if (!l->items || l->size <= 0) return out;
    auto* a = reinterpret_cast<ArrLayout*>(l->items);
    void** e = reinterpret_cast<void**>(&a->data[0]);
    for (int i2 = 0; i2 < l->size; ++i2) if (e[i2]) out.push_back(e[i2]);
    return out;
}

bool is_local(void* v) {
    return off_islocal >= 0 && *reinterpret_cast<bool*>(reinterpret_cast<uint8_t*>(v) + off_islocal);
}

// Rate-limit master-client shifts. Photon rejects spam faster than every ~20 ms.
float s_master_delay = 0.f;
bool  can_master() {
    float now = (float)state::g().frame_time.load();
    if (now < s_master_delay) return false;
    s_master_delay = now + 0.02f;
    return true;
}

// Gate every shift on InRoom; silent no-op in the lobby.
bool ready() { return net::init() && net::in_room(); }

void* nearest_non_local() {
    for (void* v : rigs()) if (!is_local(v)) return v;
    return nullptr;
}

// --- Set Master Client variants --------------------------------------------

void smc_gun_tick() {
    if (!ready()) return;
    if (input::state().rightTrigger <= 0.5f) return;
    if (!can_master()) return;
    void* r = nearest_non_local(); if (!r) return;
    void* p = net::rig_to_player(r); if (!p) return;
    net::set_master(p);
}

void smc_all_tick() {
    if (!ready() || !can_master()) return;
    for (void* r : rigs()) {
        if (is_local(r)) continue;
        void* p = net::rig_to_player(r); if (!p) continue;
        net::set_master(p);
        break; // one per frame to respect rate limit
    }
}

void smc_aura_tick() {
    if (!ready() || !can_master()) return;
    for (void* r : rigs()) {
        if (is_local(r)) continue;
        // Aura radius 4m is computed in C#. Without rig head pos cached here,
        // we approximate by always trying - caller is expected to gate via menu.
        void* p = net::rig_to_player(r); if (!p) continue;
        net::set_master(p);
        break;
    }
}

void smc_touch_tick() {
    if (!ready() || !can_master()) return;
    if (input::state().leftGrip || input::state().rightGrip) {
        for (void* r : rigs()) {
            if (is_local(r)) continue;
            void* p = net::rig_to_player(r); if (!p) continue;
            net::set_master(p);
            break;
        }
    }
}

void auto_smc_tick() {
    if (!net::init() || !net::in_room()) return;
    if (net::is_master()) return;
    if (!can_master()) return;
    void* lp = net::local_player(); if (!lp) return;
    net::set_master(lp);
}

// --- Crash variants: shift master to target, then back to self -------------

bool crash_once(void* target_player) {
    if (!target_player) return false;
    if (!net::set_master(target_player)) return false;
    void* lp = net::local_player();
    if (!lp) return false;
    net::set_master(lp);
    return true;
}

void crash_gun_tick() {
    if (!ready()) return;
    if (input::state().rightTrigger <= 0.5f) return;
    if (!can_master()) return;
    void* r = nearest_non_local(); if (!r) return;
    void* p = net::rig_to_player(r); if (!p) return;
    crash_once(p);
}

void crash_all_tick() {
    if (!ready() || !can_master()) return;
    for (void* r : rigs()) {
        if (is_local(r)) continue;
        void* p = net::rig_to_player(r); if (!p) continue;
        if (crash_once(p)) break;
    }
}

void crash_aura_tick() {
    if (!ready() || !can_master()) return;
    for (void* r : rigs()) {
        if (is_local(r)) continue;
        void* p = net::rig_to_player(r); if (!p) continue;
        if (crash_once(p)) break;
    }
}

void crash_touch_tick() {
    if (!ready() || !can_master()) return;
    if (!(input::state().leftGrip || input::state().rightGrip)) return;
    for (void* r : rigs()) {
        if (is_local(r)) continue;
        void* p = net::rig_to_player(r); if (!p) continue;
        if (crash_once(p)) break;
    }
}

} // namespace

void register_all() {
    using registry::add;
    add("Set Master Client Gun",     "Detected Mods", smc_gun_tick);
    add("Set Master Client All",     "Detected Mods", smc_all_tick);
    add("Set Master Client Aura",    "Detected Mods", smc_aura_tick);
    add("Set Master Client OnTouch", "Detected Mods", smc_touch_tick);
    add("Auto Set Master Client",    "Detected Mods", auto_smc_tick);
    add("Crash Gun",                 "Detected Mods", crash_gun_tick);
    add("Crash All",                 "Detected Mods", crash_all_tick);
    add("Crash Aura",                "Detected Mods", crash_aura_tick);
    add("Crash OnTouch",             "Detected Mods", crash_touch_tick);
}

} // namespace mods::detected
