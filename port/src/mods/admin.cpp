// Admin-only page. Visible only when state::is_admin() is true, which the
// Discord gate decides at startup by reading /sdcard/iiMenu/discord_user.txt
// and matching against the admin allowlist in state.cpp.

#include "admin.h"
#include "../registry.h"
#include "../state.h"
#include "../rig.h"
#include "../net.h"
#include "../il2cpp.h"
#include "../unity.h"

#include <android/log.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "iistupid/admin", __VA_ARGS__)

using unity::Vector3;

namespace mods::admin {

namespace {

constexpr const char* kNotifyPath = "/sdcard/iiMenu/notify.txt";

std::string read_file(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return {};
    char buf[512];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = 0;
    // Trim trailing newlines / spaces.
    while (n > 0 && (buf[n-1]=='\n' || buf[n-1]=='\r' || buf[n-1]==' ')) buf[--n] = 0;
    return std::string(buf);
}

// --- Admin Notify All Menu Users ---------------------------------------
// Reads /sdcard/iiMenu/notify.txt, logs it. Full broadcast to other menu
// users needs the iiMenu Console byte protocol (PhotonNetwork.OpRaiseEvent
// with Console.ConsoleByte and a "notify" command). That marshalling layer
// is tracked in net.cpp as the next piece of work.

void notify_all_tick() {
    static double next = 0;
    double now = state::g().frame_time.load();
    if (now < next) return;
    next = now + 2.0;

    std::string msg = read_file(kNotifyPath);
    if (msg.empty()) {
        LOG("notify.txt empty; write a message to /sdcard/iiMenu/notify.txt");
        return;
    }
    LOG("[notify %s] %s", state::discord_username(), msg.c_str());
    // TODO(net): once Hashtable/RaiseEventOptions marshalling lands, replace
    // the LOG with PhotonNetwork.OpRaiseEvent(Console.ConsoleByte,
    // {"notify", msg}, ReceiverGroup.All, SendReliable).
}

// --- Admin Dump Rigs: logs every VRRig pointer + body position ----------

struct ListLayout { uint8_t h[0x10]; void* items; int32_t size; int32_t ver; };
struct ArrLayout  { uint8_t h[0x10]; void* bounds; uintptr_t len; void* data[1]; };

static void* (*gp_get_instance)() = nullptr;
static void* (*gp_get_rigs)(void*) = nullptr;
static int32_t off_islocal = -1;

void resolve_rigs() {
    static bool done = false; if (done) return; done = true;
    using namespace il2cpp;
    Il2CppImage* game = find_image("Assembly-CSharp.dll");
    if (!game) return;
    if (Il2CppClass* gp = class_from_name(game, "", "GorillaParent")) {
        gp_get_instance = (void*(*)())method_get_pointer(class_get_method_from_name(gp, "get_instance", 0));
        gp_get_rigs     = (void*(*)(void*))method_get_pointer(class_get_method_from_name(gp, "GetRigs", 0));
    }
    if (Il2CppClass* vr = class_from_name(game, "", "VRRig")) {
        FieldInfo* f = class_get_field_from_name(vr, "isLocal");
        off_islocal = f ? field_get_offset(f) : -1;
    }
}

void dump_rigs_tick() {
    static int sentinel = 0;
    if ((sentinel++ % 120) != 0) return; // every ~2s

    resolve_rigs();
    if (!gp_get_instance || !gp_get_rigs) return;
    void* inst = gp_get_instance(); if (!inst) return;
    void* list = gp_get_rigs(inst); if (!list) return;
    auto* L = reinterpret_cast<ListLayout*>(list);
    if (!L->items || L->size <= 0) return;
    auto* A = reinterpret_cast<ArrLayout*>(L->items);
    void** e = reinterpret_cast<void**>(&A->data[0]);
    LOG("=== rig dump (%d) ===", L->size);
    for (int i = 0; i < L->size; ++i) {
        void* r = e[i]; if (!r) continue;
        bool loc = off_islocal >= 0 && *reinterpret_cast<bool*>(reinterpret_cast<uint8_t*>(r) + off_islocal);
        LOG("  rig[%d] ptr=%p local=%d", i, r, loc);
    }
}

// --- Admin Lockdown: force non-admin targets to hard-freeze --------------
// Needs master-client status + per-target OpRaiseEvent for a real lockdown.
// Scaffold: locally freezes my own body so I can test. Expand when net is in.

void lockdown_tick() {
    static int once = 0;
    if ((once++) != 0) return;
    LOG("Lockdown enabled - full impl needs master client RPC marshalling.");
}

// --- Admin Status: logcat print of admin state --------------------------

void status_tick() {
    static int sentinel = 0;
    if ((sentinel++ % 180) != 0) return;
    LOG("admin=%d discord='%s' search_active=%d search='%s'",
        state::is_admin(),
        state::discord_username(),
        registry::search_active() ? 1 : 0,
        registry::search_query().c_str());
}

} // namespace

void register_all() {
    using registry::add;

    // Still register the mods even without admin so the page exists; the
    // page gate hides it from the menu for non-admins.
    add("Notify All Menu Users", "Admin Mods", notify_all_tick);
    add("Dump Rigs",             "Admin Mods", dump_rigs_tick);
    add("Lockdown Scaffold",     "Admin Mods", lockdown_tick);
    add("Admin Status",          "Admin Mods", status_tick);

    registry::mark_admin_only("Admin Mods");
}

} // namespace mods::admin
