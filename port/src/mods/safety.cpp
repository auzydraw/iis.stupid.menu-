// Safety page port. Portable subset of PC Safety.cs: no finger, fake oculus
// menu, fake power off, fake broken controller, auto clear cache, change
// identity (local), anti predictions scaffold.

#include "safety.h"
#include "../registry.h"
#include "../state.h"
#include "../rig.h"
#include "../input.h"
#include "../net.h"
#include "../il2cpp.h"
#include "../unity.h"

#include <android/log.h>
#include <cmath>
#include <cstdlib>
#include <cstring>

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "iistupid/safety", __VA_ARGS__)

using unity::Vector3;

namespace mods::safety {

namespace {

float dt() { return state::g().delta_time.load(); }

// --- No Finger: zero all fields on ControllerInputPoller.instance --------

void no_finger_tick() {
    rig::poller_no_finger();
}

// --- Fake Oculus Menu: puts GTPlayer in overlay, pins controllers --------

void fake_oculus_menu_tick() {
    const auto& s = input::state();
    if (!s.leftPrimary) return;
    rig::poller_no_finger();
    rig::set_in_overlay(true);

    Vector3 head = rig::head_position();
    Vector3 up   = rig::head_up();
    rig::set_controller_position(true,  head + up * -0.5f);
    rig::set_controller_position(false, head + up *  0.3f);
}

void fake_oculus_menu_toggled(bool on) {
    if (!on) rig::set_in_overlay(false);
}

// --- Fake Power Off: freezes body + hides rig while left stick clicked ---

Vector3 s_dead_pos{};
bool    s_dead_cap = false;

void fake_power_off_tick() {
    const auto& s = input::state();
    if (s.leftJoystickClick) {
        if (!s_dead_cap) { s_dead_pos = rig::body_position(); s_dead_cap = true; }
        rig::set_rig_enabled(false);
        rig::set_body_position(s_dead_pos);
        rig::zero_velocity();
    } else {
        if (s_dead_cap) {
            rig::set_rig_enabled(true);
            s_dead_cap = false;
        }
    }
}

void fake_power_off_toggled(bool on) {
    if (!on) {
        rig::set_rig_enabled(true);
        s_dead_cap = false;
    }
}

// --- Fake Broken Controller: pins left hand to a bad spot, kills input ---

void fake_broken_controller_tick() {
    Vector3 head = rig::head_position();
    Vector3 up   = rig::head_up();
    rig::set_controller_position(true, head + up * -0.5f);

    // Blank the left controller only.
    rig::poller_no_finger();
}

// --- Auto Clear Cache: GC.Collect() once per minute ----------------------

double s_next_gc = 60.0;
void auto_clear_cache_tick() {
    double now = state::g().frame_time.load();
    if (now < s_next_gc) return;
    rig::gc_collect();
    s_next_gc = now + 60.0;
    LOG("GC.Collect() fired");
}

// --- Change Identity (local): swap NickName via Photon LocalPlayer -------
// Covers Safety.ChangeIdentity() locally. A full ChangeColor would need the
// RPC_InitializeNoobMaterial networked path; that's deferred with the Console
// protocol port.

const char* kNames[] = {
    "SHIBA", "PBBV", "J3VU", "BEES", "NAMO", "MANGO", "FROSTY", "LEMMING",
    "MINIGAMES", "JMANCURLY", "ELLIOT", "POLAR", "DAISY09", "SHARKPUPPET",
    "DUCKY", "EDDIE", "RAKZZ", "CASEOH", "SKETCH", "SKY", "WATERMELON",
    "MONK", "MONKI", "MONKEY", "GORILLA", "RUFUS", "BANANA", "PEANUTBUTTER",
    "NOVA", "LUNAR", "GLITCH", "HACKER", "BLUE", "RED", "PURPLE", "CHEETO",
    nullptr
};
const char* kPrefixes[] = {"EPIC", "REAL", "NOT", "SILLY", "LITTLE", "BIG", "MONKE", "SUB2", "OG", nullptr};
const char* kSuffixes[] = {"GT", "VR", "LOL", "GTVR", "FAN", "XD", "YT", "FR", nullptr};

int count(const char* const* a) { int n = 0; while (a[n]) ++n; return n; }

static void (*s_set_nickname)(void* player, void* str) = nullptr;
static bool  s_set_nickname_resolved = false;

bool resolve_nickname() {
    if (s_set_nickname_resolved) return s_set_nickname != nullptr;
    s_set_nickname_resolved = true;
    using namespace il2cpp;
    const char* photon_imgs[] = {
        "PhotonRealtime.dll", "Photon3Unity3D.dll", "PhotonUnityNetworking.dll",
        "Assembly-CSharp.dll", nullptr
    };
    for (int i = 0; photon_imgs[i]; ++i) {
        Il2CppImage* img = find_image(photon_imgs[i]);
        if (!img) continue;
        Il2CppClass* k = class_from_name(img, "Photon.Realtime", "Player");
        if (!k) continue;
        const MethodInfo* m = class_get_method_from_name(k, "set_NickName", 1);
        if (m) {
            s_set_nickname = (decltype(s_set_nickname))method_get_pointer(m);
            break;
        }
    }
    LOG("Player.set_NickName resolved: %p", s_set_nickname);
    return s_set_nickname != nullptr;
}

void set_local_nickname(const char* name) {
    if (!resolve_nickname()) return;
    void* player = net::local_player();
    if (!player) return;
    void* str = il2cpp::string_new(name);
    if (!str) return;
    s_set_nickname(player, str);
}

char s_current_name[32];

void change_identity_tick() {
    // Cycle every 2 seconds.
    static double next = 0;
    double now = state::g().frame_time.load();
    if (now < next) return;
    next = now + 2.0;

    int nn = count(kNames);
    int np = count(kPrefixes);
    int ns = count(kSuffixes);
    const char* pfx = (rand() % 3 == 0) ? kPrefixes[rand() % np] : "";
    const char* sfx = (rand() % 3 == 0) ? kSuffixes[rand() % ns] : "";
    const char* core = kNames[rand() % nn];
    snprintf(s_current_name, sizeof(s_current_name), "%s%s%s", pfx, core, sfx);
    s_current_name[12] = 0; // 12-char cap matches PC behavior
    set_local_nickname(s_current_name);
}

// --- Anti Predictions scaffold ------------------------------------------
// PC version intercepts SerializePatch.OverrideSerialization and smooths
// the rig target local positions before sending. Needs the serialize hook
// which isn't ported. Scaffold logs when enabled so downstream work can
// find the integration point.

bool s_anti_pred_warned = false;
void anti_predictions_tick() {
    if (!s_anti_pred_warned) {
        LOG("Anti Predictions enabled; full impl needs the SerializePatch hook.");
        s_anti_pred_warned = true;
    }
}

} // namespace

void register_all() {
    using registry::add;
    add("No Finger",               "Safety Mods", no_finger_tick);
    add("Fake Oculus Menu",        "Safety Mods", fake_oculus_menu_tick, fake_oculus_menu_toggled);
    add("Fake Power Off",          "Safety Mods", fake_power_off_tick,   fake_power_off_toggled);
    add("Fake Broken Controller",  "Safety Mods", fake_broken_controller_tick);
    add("Auto Clear Cache",        "Safety Mods", auto_clear_cache_tick);
    add("Change Identity",         "Safety Mods", change_identity_tick);
    add("Anti Predictions",        "Safety Mods", anti_predictions_tick);
}

} // namespace mods::safety
