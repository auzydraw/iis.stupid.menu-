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

// --- Player Dot: logs all non-local player positions once per 30 frames ---
// Hook this up to a world-space LineRenderer builder for a real ESP.

int frame = 0;

extern "C" std::vector<void*> fun_enumerate_rigs(); // provided by mods::fun

void player_dot_tick() {
    if ((++frame % 30) != 0) return;
    // Direct enumeration via Il2CppList layout (mirrors fun.cpp).
    using namespace il2cpp;
    static void* (*gp_get_instance)() = nullptr;
    static void* (*gp_get_rigs)(void*) = nullptr;
    if (!gp_get_instance) {
        Il2CppImage* game = find_image("Assembly-CSharp.dll");
        if (!game) return;
        Il2CppClass* gp = class_from_name(game, "", "GorillaParent");
        if (!gp) return;
        gp_get_instance = (void*(*)())method_get_pointer(class_get_method_from_name(gp, "get_instance", 0));
        gp_get_rigs     = (void*(*)(void*))method_get_pointer(class_get_method_from_name(gp, "GetRigs", 0));
    }
    if (!gp_get_instance) return;
    void* inst = gp_get_instance(); if (!inst) return;
    void* list = gp_get_rigs(inst); if (!list) return;

    struct ListLayout { uint8_t h[0x10]; void* items; int32_t size; int32_t ver; };
    struct ArrLayout  { uint8_t h[0x10]; void* bounds; uintptr_t len; void* data[1]; };
    auto* L = reinterpret_cast<ListLayout*>(list);
    auto* A = reinterpret_cast<ArrLayout*>(L->items);
    void** entries = reinterpret_cast<void**>(&A->data[0]);
    LOG("rigs=%d", L->size);
    for (int i = 0; i < L->size; ++i) {
        LOG("  rig[%d]=%p", i, entries[i]);
    }
}

// --- Third person forward (moves body away from head along head forward) ---
// A substitute for the camera-reparenting ThirdPerson of the PC build.

void third_person_push_tick() {
    Vector3 back = rig::head_forward() * -0.4f; // roughly behind head
    // Just translates model slightly; a real third person needs camera rig work.
    rig::translate(back * state::g().delta_time.load());
}

} // namespace

void register_all() {
    using registry::add;
    add("Player Dot",         "Visuals", player_dot_tick);
    add("Third Person Push",  "Visuals", third_person_push_tick);
}

} // namespace mods::visuals
