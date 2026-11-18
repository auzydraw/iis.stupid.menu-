#include "overpowered.h"
#include "../registry.h"
#include "../state.h"
#include "../rig.h"
#include "../input.h"
#include "../il2cpp.h"
#include "../unity.h"

#include <cmath>
#include <vector>

using unity::Vector3;

namespace mods::overpowered {

// Forward-declare the rig enumeration from the advantages TU.
namespace mods::advantages { /* nothing exposed */ }

namespace {

float dt() { return state::g().delta_time.load(); }

struct ListLayout { uint8_t h[0x10]; void* items; int32_t size; int32_t ver; };
struct ArrLayout  { uint8_t h[0x10]; void* bounds; uintptr_t len; void* data[1]; };

static void*(*gp_get_instance)() = nullptr;
static void*(*gp_get_rigs)(void*) = nullptr;
static int32_t off_islocal = -1;
static int32_t off_headmesh = -1;
static int32_t off_tf = -1;
static void*(*comp_get_tf)(void*) = nullptr;
static void (*tf_get_pos)(void*, Vector3*) = nullptr;
static void (*tf_set_pos)(void*, Vector3) = nullptr;

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
        off_islocal = a ? field_get_offset(a) : -1;
        off_headmesh = b ? field_get_offset(b) : -1;
    }
    if (Il2CppClass* cp = class_from_name(core, "UnityEngine", "Component"))
        comp_get_tf = (void*(*)(void*))method_get_pointer(class_get_method_from_name(cp, "get_transform", 0));
    if (Il2CppClass* tf = class_from_name(core, "UnityEngine", "Transform")) {
        tf_get_pos = (void(*)(void*, Vector3*))method_get_pointer(class_get_method_from_name(tf, "get_position", 0));
        tf_set_pos = (void(*)(void*, Vector3))method_get_pointer(class_get_method_from_name(tf, "set_position", 1));
    }
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

bool is_local(void* v) {
    return off_islocal >= 0 && *reinterpret_cast<bool*>(reinterpret_cast<uint8_t*>(v) + off_islocal);
}
Vector3 head_pos(void* v) {
    if (off_headmesh < 0 || !comp_get_tf || !tf_get_pos) return {};
    void* m = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(v) + off_headmesh);
    void* t = comp_get_tf(m);
    Vector3 p{}; if (t) tf_get_pos(t, &p); return p;
}

// --- Tag Everyone (client-side): teleports body to each non-local rig
// position in a sweep. Does NOT send a tag RPC - that's a different fight.
// Useful for tag chaining when client-sided tag detection is in place.

void tag_everyone_tick() {
    static int idx = 0;
    static float t = 0;
    t += dt();
    if (t < 0.05f) return;
    t = 0;
    auto r = rigs();
    std::vector<void*> targets;
    for (void* v : r) if (!is_local(v)) targets.push_back(v);
    if (targets.empty()) return;
    idx = (idx + 1) % (int)targets.size();
    rig::set_body_position(head_pos(targets[idx]) + Vector3{0, 0.2f, 0});
    rig::zero_velocity();
}

// --- Freeze everyone: hold their heads at a fixed offset. Visible to
// local only - remote positions are server-authoritative.

void freeze_everyone_tick() {
    if (!comp_get_tf || !tf_set_pos) return;
    auto r = rigs();
    for (void* v : r) {
        if (is_local(v)) continue;
        if (off_headmesh < 0) continue;
        void* m = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(v) + off_headmesh);
        void* t = comp_get_tf(m);
        if (!t) continue;
        Vector3 me = rig::head_position();
        tf_set_pos(t, me + Vector3{0, 2, 0});
    }
}

// --- Rig puppet: teleport yourself behind each player sequentially ----

void stalker_tick() {
    Vector3 nearest{};
    float best = 1e9f;
    for (void* v : rigs()) {
        if (is_local(v)) continue;
        Vector3 p = head_pos(v);
        float d = (p - rig::body_position()).sqrmag();
        if (d < best) { best = d; nearest = p; }
    }
    if (best >= 1e9f) return;
    Vector3 behind = nearest + (rig::head_forward() * -0.8f);
    rig::set_body_position(behind);
    rig::zero_velocity();
}

// --- Macro recorder playback (idle scaffold, needs command binding) ---

void macro_tick() {
    // Reserved. Macro system needs a recorder UI not yet wired.
}

} // namespace

void register_all() {
    using registry::add;
    add("Tag Everyone (client)", "Overpowered Mods", tag_everyone_tick);
    add("Freeze Everyone",       "Overpowered Mods", freeze_everyone_tick);
    add("Stalker",               "Overpowered Mods", stalker_tick);
}

} // namespace mods::overpowered
