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

float dt() { return state::g().delta_time.load(); }

struct ListLayout { uint8_t h[0x10]; void* items; int32_t size; int32_t ver; };
struct ArrLayout  { uint8_t h[0x10]; void* bounds; uintptr_t len; void* data[1]; };

static void*(*gp_get_instance)() = nullptr;
static void*(*gp_get_rigs)(void*) = nullptr;
static int32_t off_islocal = -1;
static int32_t off_headmesh = -1;
static void*(*comp_get_tf)(void*) = nullptr;
static void (*tf_get_pos)(void*, Vector3*) = nullptr;

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
    if (Il2CppClass* tf = class_from_name(core, "UnityEngine", "Transform"))
        tf_get_pos = (void(*)(void*, Vector3*))method_get_pointer(class_get_method_from_name(tf, "get_position", 0));
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

// --- Tag Seek ---------------------------------------------------------

void tag_seek_tick() {
    auto r = rigs();
    if (r.empty()) return;
    Vector3 me = rig::body_position();
    float best = 1e9f;
    Vector3 tgt{};
    for (void* v : r) {
        if (is_local(v)) continue;
        Vector3 p = head_pos(v);
        float d2 = (p - me).sqrmag();
        if (d2 < best) { best = d2; tgt = p; }
    }
    if (best >= 1e9f) return;
    rig::translate((tgt - me).normalized() * (dt() * state::g().fly_speed.load()));
    rig::zero_velocity();
}

// --- Rapid Jump -------------------------------------------------------

void rapid_jump_tick() {
    if (!input::state().rightPrimary) return;
    rig::add_force({0, 60.f, 0});
}

// --- Freeze (self) ----------------------------------------------------

Vector3 freeze_pos{}; bool freeze_cap = false;
void freeze_tick() {
    if (!freeze_cap) { freeze_pos = rig::body_position(); freeze_cap = true; }
    rig::set_body_position(freeze_pos);
    rig::zero_velocity();
}
void freeze_toggled(bool on) { if (!on) freeze_cap = false; }

// --- Earthquake: wobble every rig visible to the local client ---------

void earthquake_tick() {
    static float t = 0;
    t += dt();
    float dx = std::sin(t * 12.f) * dt() * 0.3f;
    float dz = std::cos(t * 11.f) * dt() * 0.3f;
    rig::translate({dx, 0, dz});
}

// --- Head Bob ---------------------------------------------------------

void head_bob_tick() {
    static float t = 0;
    t += dt();
    rig::translate({0, std::sin(t * 6.f) * dt() * 0.2f, 0});
}

// --- Wobble (constant rotation nudge via body pos) --------------------

void wobble_tick() {
    static float t = 0;
    t += dt();
    rig::translate(rig::head_right() * (std::sin(t * 4.f) * dt() * 0.1f));
}

// --- Spin left -------------------------------------------------------

void spin_tick() {
    // Without transform set_rotation on the body collider resolved here
    // we approximate by circling the body around current head direction.
    static float t = 0;
    t += dt();
    Vector3 c = rig::body_position();
    float r = 0.1f;
    rig::set_body_position({c.x + std::cos(t * 4.f) * r * dt(),
                            c.y,
                            c.z + std::sin(t * 4.f) * r * dt()});
}

} // namespace

void register_all() {
    using registry::add;
    add("Tag Seek",   "Fun Mods", tag_seek_tick);
    add("Rapid Jump", "Fun Mods", rapid_jump_tick);
    add("Freeze",     "Fun Mods", freeze_tick, freeze_toggled);
    add("Earthquake", "Fun Mods", earthquake_tick);
    add("Head Bob",   "Fun Mods", head_bob_tick);
    add("Wobble",     "Fun Mods", wobble_tick);
    add("Spin",       "Fun Mods", spin_tick);
}

} // namespace mods::fun
