#include "advantages.h"
#include "../registry.h"
#include "../state.h"
#include "../rig.h"
#include "../input.h"
#include "../il2cpp.h"
#include "../unity.h"

#include <cmath>
#include <vector>

using unity::Vector3;

namespace mods::advantages {

namespace {

float dt() { return state::g().delta_time.load(); }

// Pulls the local body toward the nearest non-local VRRig when trigger held.
// Uses the same list traversal as mods::fun.

struct ListLayout { uint8_t h[0x10]; void* items; int32_t size; int32_t ver; };
struct ArrLayout  { uint8_t h[0x10]; void* bounds; uintptr_t len; void* data[1]; };

static void*(*gp_get_instance)() = nullptr;
static void*(*gp_get_rigs)(void*) = nullptr;
static int32_t off_islocal = -1;
static int32_t off_headmesh = -1;
static void*(*comp_get_tf)(void*) = nullptr;
static void (*tf_get_pos)(void*, Vector3*) = nullptr;

void resolve() {
    static bool done = false;
    if (done) return;
    using namespace il2cpp;
    Il2CppImage* game = find_image("Assembly-CSharp.dll");
    Il2CppImage* core = find_image("UnityEngine.CoreModule.dll");
    if (!game || !core) return;

    Il2CppClass* gp = class_from_name(game, "", "GorillaParent");
    if (gp) {
        gp_get_instance = (void*(*)())method_get_pointer(class_get_method_from_name(gp, "get_instance", 0));
        gp_get_rigs     = (void*(*)(void*))method_get_pointer(class_get_method_from_name(gp, "GetRigs", 0));
    }
    Il2CppClass* vr = class_from_name(game, "", "VRRig");
    if (vr) {
        FieldInfo* f1 = class_get_field_from_name(vr, "isLocal");
        FieldInfo* f2 = class_get_field_from_name(vr, "headMesh");
        off_islocal = f1 ? field_get_offset(f1) : -1;
        off_headmesh = f2 ? field_get_offset(f2) : -1;
    }
    Il2CppClass* cp = class_from_name(core, "UnityEngine", "Component");
    if (cp) comp_get_tf = (void*(*)(void*))method_get_pointer(class_get_method_from_name(cp, "get_transform", 0));
    Il2CppClass* tf = class_from_name(core, "UnityEngine", "Transform");
    if (tf) tf_get_pos = (void(*)(void*, Vector3*))method_get_pointer(class_get_method_from_name(tf, "get_position", 0));
    done = true;
}

std::vector<void*> enumerate_rigs() {
    resolve();
    std::vector<void*> out;
    if (!gp_get_instance || !gp_get_rigs) return out;
    void* inst = gp_get_instance(); if (!inst) return out;
    void* list = gp_get_rigs(inst); if (!list) return out;
    auto* L = reinterpret_cast<ListLayout*>(list);
    if (!L->items || L->size <= 0) return out;
    auto* A = reinterpret_cast<ArrLayout*>(L->items);
    void** entries = reinterpret_cast<void**>(&A->data[0]);
    out.reserve(L->size);
    for (int i = 0; i < L->size; ++i) if (entries[i]) out.push_back(entries[i]);
    return out;
}

bool rig_islocal(void* v) {
    if (off_islocal < 0) return false;
    return *reinterpret_cast<bool*>(reinterpret_cast<uint8_t*>(v) + off_islocal);
}
Vector3 rig_head_pos(void* v) {
    if (off_headmesh < 0 || !comp_get_tf || !tf_get_pos) return {};
    void* mesh = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(v) + off_headmesh);
    void* tfp  = comp_get_tf(mesh);
    Vector3 p{}; if (tfp) tf_get_pos(tfp, &p); return p;
}

void* nearest_non_local(Vector3& out_pos) {
    auto rigs = enumerate_rigs();
    float best = 1e9f;
    void* winner = nullptr;
    Vector3 me = rig::body_position();
    for (void* v : rigs) {
        if (rig_islocal(v)) continue;
        Vector3 p = rig_head_pos(v);
        float d2 = (p - me).sqrmag();
        if (d2 < best) { best = d2; winner = v; out_pos = p; }
    }
    return winner;
}

// --- Fast Tag: zip straight to the nearest player on trigger ----------

void fast_tag_tick() {
    if (input::state().rightTrigger <= 0.5f) return;
    Vector3 tgt{};
    if (!nearest_non_local(tgt)) return;
    Vector3 dir = (tgt - rig::body_position()).normalized();
    rig::translate(dir * (dt() * state::g().fly_speed.load() * 2.f));
    rig::zero_velocity();
}

// --- Reach extension via arm length ramp -------------------------------

void reach_ext_tick() {
    // Boost arm length only while trigger held, restore to 1.0 otherwise.
    // Writes GTPlayer.armLengthMultiplier directly each frame.
    using namespace il2cpp;
    static int32_t off = -2;
    if (off == -2) {
        Il2CppImage* game = find_image("Assembly-CSharp.dll");
        Il2CppClass* gtp = game ? class_from_name(game, "GorillaLocomotion", "GTPlayer") : nullptr;
        FieldInfo* f = gtp ? class_get_field_from_name(gtp, "armLengthMultiplier") : nullptr;
        off = f ? field_get_offset(f) : -1;
    }
    if (off < 0 || !rig::gtplayer()) return;
    float v = input::state().rightTrigger > 0.3f ? state::g().arm_length.load() : 1.f;
    *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(rig::gtplayer()) + off) = v;
}

// --- Air control: let left stick steer while airborne ------------------

void air_control_tick() {
    // We cannot reliably detect grounded without more offsets; always allow.
    unity::Vector2 j = input::state().leftJoystick;
    if (std::fabs(j.x) < 0.2f && std::fabs(j.y) < 0.2f) return;
    Vector3 fwd = rig::head_forward(); fwd.y = 0; fwd = fwd.normalized();
    Vector3 right = rig::head_right(); right.y = 0; right = right.normalized();
    rig::add_force((fwd * j.y + right * j.x) * 20.f);
}

// --- Follow player (slower, constant) ---------------------------------

void follow_player_tick() {
    Vector3 tgt{};
    if (!nearest_non_local(tgt)) return;
    Vector3 dir = (tgt - rig::body_position()).normalized();
    rig::translate(dir * (dt() * 3.f));
}

// --- Anti tag (push away from nearest non-local) ----------------------

void anti_tag_tick() {
    Vector3 tgt{};
    if (!nearest_non_local(tgt)) return;
    float d = (tgt - rig::body_position()).mag();
    if (d > 4.f) return;
    Vector3 dir = (rig::body_position() - tgt).normalized();
    rig::translate(dir * (dt() * state::g().fly_speed.load()));
    rig::zero_velocity();
}

} // namespace

void register_all() {
    using registry::add;
    add("Fast Tag",       "Advantage Mods", fast_tag_tick);
    add("Reach Extension","Advantage Mods", reach_ext_tick);
    add("Air Control",    "Advantage Mods", air_control_tick);
    add("Follow Player",  "Advantage Mods", follow_player_tick);
    add("Anti Tag",       "Advantage Mods", anti_tag_tick);
}

} // namespace mods::advantages
