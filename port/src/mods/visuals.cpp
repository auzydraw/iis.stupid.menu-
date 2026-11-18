#include "visuals.h"
#include "../registry.h"
#include "../state.h"
#include "../rig.h"
#include "../il2cpp.h"
#include "../unity.h"

#include <android/log.h>
#include <vector>

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "iistupid/visuals", __VA_ARGS__)

using unity::Vector3;

namespace mods::visuals {

namespace {

struct ListLayout { uint8_t h[0x10]; void* items; int32_t size; int32_t ver; };
struct ArrLayout  { uint8_t h[0x10]; void* bounds; uintptr_t len; void* data[1]; };

static void*(*gp_get_instance)() = nullptr;
static void*(*gp_get_rigs)(void*) = nullptr;
static int32_t off_islocal = -1;
static int32_t off_headmesh = -1;
static int32_t off_mainskin = -1;
static void*(*comp_get_tf)(void*) = nullptr;
static void (*tf_get_pos)(void*, Vector3*) = nullptr;
static void*(*comp_get_go)(void*) = nullptr;
static void (*go_set_active)(void*, bool) = nullptr;

void resolve() {
    static bool done = false; if (done) return;
    using namespace il2cpp;
    Il2CppImage* game = find_image("Assembly-CSharp.dll");
    Il2CppImage* core = find_image("UnityEngine.CoreModule.dll");
    if (!game || !core) return;
    if (Il2CppClass* gp = class_from_name(game, "", "GorillaParent")) {
        gp_get_instance = (void*(*)())method_get_pointer(class_get_method_from_name(gp, "get_instance", 0));
        gp_get_rigs     = (void*(*)(void*))method_get_pointer(class_get_method_from_name(gp, "GetRigs", 0));
    }
    if (Il2CppClass* vr = class_from_name(game, "", "VRRig")) {
        FieldInfo* a = class_get_field_from_name(vr, "isLocal");
        FieldInfo* b = class_get_field_from_name(vr, "headMesh");
        FieldInfo* c = class_get_field_from_name(vr, "mainSkin");
        off_islocal  = a ? field_get_offset(a) : -1;
        off_headmesh = b ? field_get_offset(b) : -1;
        off_mainskin = c ? field_get_offset(c) : -1;
    }
    if (Il2CppClass* cp = class_from_name(core, "UnityEngine", "Component")) {
        comp_get_tf = (void*(*)(void*))method_get_pointer(class_get_method_from_name(cp, "get_transform", 0));
        comp_get_go = (void*(*)(void*))method_get_pointer(class_get_method_from_name(cp, "get_gameObject", 0));
    }
    if (Il2CppClass* tf = class_from_name(core, "UnityEngine", "Transform"))
        tf_get_pos = (void(*)(void*, Vector3*))method_get_pointer(class_get_method_from_name(tf, "get_position", 0));
    if (Il2CppClass* go = class_from_name(core, "UnityEngine", "GameObject"))
        go_set_active = (void(*)(void*, bool))method_get_pointer(class_get_method_from_name(go, "SetActive", 1));
    done = true;
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
bool is_local(void* v) { return off_islocal >= 0 && *reinterpret_cast<bool*>(reinterpret_cast<uint8_t*>(v) + off_islocal); }
Vector3 head_pos(void* v) {
    if (off_headmesh < 0 || !comp_get_tf || !tf_get_pos) return {};
    void* m = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(v) + off_headmesh);
    void* t = comp_get_tf(m);
    Vector3 p{}; if (t) tf_get_pos(t, &p); return p;
}

// --- Player Dot (logcat) -----------------------------------------------

int frame = 0;
void player_dot_tick() {
    if ((++frame % 30) != 0) return;
    auto r = rigs();
    LOG("rigs=%zu", r.size());
    for (size_t i = 0; i < r.size(); ++i) {
        Vector3 p = head_pos(r[i]);
        LOG("  rig[%zu] pos=(%.2f, %.2f, %.2f) local=%d", i, p.x, p.y, p.z, is_local(r[i]));
    }
}

// --- Hide Players (SetActive(false) on mainSkin while enabled) ---------

void hide_players_tick() {
    if (!comp_get_go || !go_set_active) return;
    for (void* v : rigs()) {
        if (is_local(v)) continue;
        if (off_mainskin < 0) continue;
        void* skin = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(v) + off_mainskin);
        if (!skin) continue;
        void* go = comp_get_go(skin);
        if (go) go_set_active(go, false);
    }
}
void hide_players_toggled(bool on) {
    if (on) return;
    if (!comp_get_go || !go_set_active) return;
    for (void* v : rigs()) {
        if (off_mainskin < 0) continue;
        void* skin = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(v) + off_mainskin);
        if (!skin) continue;
        void* go = comp_get_go(skin);
        if (go) go_set_active(go, true);
    }
}

// --- Third Person Push (body offset back) ------------------------------

void third_person_push_tick() {
    rig::translate(rig::head_forward() * (-0.4f * state::g().delta_time.load()));
}

// --- Show Local Position (logcat, periodic) ----------------------------

void where_am_i_tick() {
    if ((++frame % 60) != 0) return;
    Vector3 p = rig::body_position();
    LOG("me pos=(%.2f, %.2f, %.2f)", p.x, p.y, p.z);
}

} // namespace

void register_all() {
    using registry::add;
    add("Player Dot",         "Visual Mods", player_dot_tick);
    add("Hide Players",       "Visual Mods", hide_players_tick, hide_players_toggled);
    add("Third Person Push",  "Visual Mods", third_person_push_tick);
    add("Where Am I",         "Visual Mods", where_am_i_tick);
}

} // namespace mods::visuals
