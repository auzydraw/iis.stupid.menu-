#include "fun.h"
#include "../registry.h"
#include "../state.h"
#include "../rig.h"
#include "../input.h"
#include "../il2cpp.h"
#include "../unity.h"

#include <cmath>
#include <vector>

using unity::Vector3;

namespace mods::fun {

namespace {

// --- Resolvers for GorillaParent.instance.GetRigs() and VRRig fields. ---

struct FunCache {
    void* (*gp_get_instance)() = nullptr;
    void* (*gp_get_rigs)(void* self) = nullptr;
    int32_t off_vrrig_islocal = -1;
    int32_t off_vrrig_headmesh = -1;
    int32_t off_vrrig_transform = -1;
    int32_t off_vrrig_head = -1;
    int32_t off_head_tracking_rot = -1;
    void* (*component_get_transform)(void*) = nullptr;
    void (*transform_set_rot)(void*, unity::Quaternion) = nullptr;
    unity::Quaternion (*euler)(float, float, float) = nullptr;
    bool resolved{};

    void resolve() {
        if (resolved) return;
        using namespace il2cpp;
        Il2CppImage* game = find_image("Assembly-CSharp.dll");
        Il2CppImage* core = find_image("UnityEngine.CoreModule.dll");
        if (!game || !core) return;

        Il2CppClass* gp = class_from_name(game, "", "GorillaParent");
        if (gp) {
            gp_get_instance = (decltype(gp_get_instance))method_get_pointer(
                class_get_method_from_name(gp, "get_instance", 0));
            gp_get_rigs = (decltype(gp_get_rigs))method_get_pointer(
                class_get_method_from_name(gp, "GetRigs", 0));
        }

        Il2CppClass* vr = class_from_name(game, "", "VRRig");
        if (vr) {
            auto off = [&](const char* n) {
                FieldInfo* f = class_get_field_from_name(vr, n);
                return f ? field_get_offset(f) : -1;
            };
            off_vrrig_islocal   = off("isLocal");
            off_vrrig_headmesh  = off("headMesh");
            off_vrrig_head      = off("head");
        }

        Il2CppClass* cp = class_from_name(core, "UnityEngine", "Component");
        if (cp) component_get_transform = (decltype(component_get_transform))method_get_pointer(
            class_get_method_from_name(cp, "get_transform", 0));

        Il2CppClass* tf = class_from_name(core, "UnityEngine", "Transform");
        if (tf) transform_set_rot = (decltype(transform_set_rot))method_get_pointer(
            class_get_method_from_name(tf, "set_rotation", 1));

        Il2CppClass* q = class_from_name(core, "UnityEngine", "Quaternion");
        if (q) euler = (decltype(euler))method_get_pointer(
            class_get_method_from_name(q, "Euler", 3));

        resolved = true;
    }
};

FunCache cache;

// --- Head spin ---------------------------------------------------------

float head_spin_angle = 0.f;
void head_spin_tick() {
    cache.resolve();
    // Minimal implementation: rotate head via setting rotation each frame.
    // Full VRRig.trackingRotationOffset access would require resolving the HeadModel class.
    head_spin_angle += 240.f * state::g().delta_time.load();
    if (head_spin_angle > 360.f) head_spin_angle -= 360.f;
    // Without the VRRig head reference we can at least spin via GTPlayer transform:
    // Skipped unless head offset resolved. Left as scaffolding.
}

// --- Tag Seek (teleport toward nearest non-local rig) ------------------

// Simple array/list traversal of a managed List<VRRig>. The returned list
// uses Il2CppList layout: object header, int32 _size, Il2CppArray* _items.
struct ListLayout {
    uint8_t  header[0x10]; // obj header + sync block
    void*    _items;       // Il2CppArray*
    int32_t  _size;
    int32_t  _version;
};

struct ArrayLayout {
    uint8_t  header[0x10];
    void*    bounds;
    uintptr_t max_length;
    void*    vector[1];
};

std::vector<void*> enumerate_rigs() {
    std::vector<void*> out;
    if (!cache.gp_get_instance || !cache.gp_get_rigs) return out;
    void* gp = cache.gp_get_instance();
    if (!gp) return out;
    auto* list = reinterpret_cast<ListLayout*>(cache.gp_get_rigs(gp));
    if (!list || !list->_items) return out;
    auto* arr = reinterpret_cast<ArrayLayout*>(list->_items);
    void** data = reinterpret_cast<void**>(&arr->vector[0]);
    out.reserve(list->_size);
    for (int i = 0; i < list->_size; ++i) {
        if (data[i]) out.push_back(data[i]);
    }
    return out;
}

Vector3 rig_head_position(void* vrrig) {
    if (cache.off_vrrig_headmesh < 0 || !cache.component_get_transform) return {};
    void* headmesh = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(vrrig) + cache.off_vrrig_headmesh);
    void* tf = cache.component_get_transform(headmesh);
    if (!tf) return {};
    // Reuse rig's transform_get_position by calling UnityEngine.Transform.get_position.
    using fn_t = void (*)(void*, Vector3*);
    static fn_t gp = nullptr;
    if (!gp) {
        using namespace il2cpp;
        Il2CppImage* core = find_image("UnityEngine.CoreModule.dll");
        Il2CppClass* tfc = class_from_name(core, "UnityEngine", "Transform");
        gp = (fn_t)method_get_pointer(class_get_method_from_name(tfc, "get_position", 0));
    }
    Vector3 v{}; if (gp) gp(tf, &v); return v;
}

bool rig_is_local(void* vrrig) {
    if (cache.off_vrrig_islocal < 0) return false;
    return *reinterpret_cast<bool*>(reinterpret_cast<uint8_t*>(vrrig) + cache.off_vrrig_islocal);
}

void tag_seek_tick() {
    cache.resolve();
    auto rigs = enumerate_rigs();
    if (rigs.empty()) return;

    Vector3 me = rig::body_position();
    float best = 1e9f;
    Vector3 target{};
    for (void* v : rigs) {
        if (rig_is_local(v)) continue;
        Vector3 p = rig_head_position(v);
        float d2 = (p - me).sqrmag();
        if (d2 < best) { best = d2; target = p; }
    }
    if (best >= 1e9f) return;
    Vector3 dir = (target - me).normalized();
    rig::translate(dir * (state::g().delta_time.load() * state::g().fly_speed.load()));
    rig::zero_velocity();
}

// --- Rapid Jump --------------------------------------------------------

void rapid_jump_tick() {
    // Force a constant upward kick while button held -> stair stepping effect.
    if (!input::state().rightPrimary) return;
    rig::add_force({0, 60.f, 0});
}

// --- Freeze (snap to position when enabled) ----------------------------

Vector3 freeze_pos{};
bool    freeze_cap = false;

void freeze_tick() {
    if (!freeze_cap) {
        freeze_pos = rig::body_position();
        freeze_cap = true;
    }
    rig::set_body_position(freeze_pos);
    rig::zero_velocity();
}

void freeze_toggled(bool on) { if (!on) freeze_cap = false; }

} // namespace

void register_all() {
    using registry::add;
    add("Head Spin",   "Fun", head_spin_tick);
    add("Tag Seek",    "Fun", tag_seek_tick);
    add("Rapid Jump",  "Fun", rapid_jump_tick);
    add("Freeze",      "Fun", freeze_tick, freeze_toggled);
}

} // namespace mods::fun
