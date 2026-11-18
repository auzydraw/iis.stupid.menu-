#include "state.h"
#include "il2cpp.h"

namespace state {

namespace {
Globals g_state;

float (*g_get_delta_time)() = nullptr;

void resolve() {
    if (g_get_delta_time) return;
    using namespace il2cpp;
    const MethodInfo* m = find_method("UnityEngine.CoreModule.dll", "UnityEngine", "Time", "get_deltaTime", 0);
    if (m) g_get_delta_time = reinterpret_cast<float(*)()>(method_get_pointer(m));
}
} // namespace

Globals& g() { return g_state; }

void refresh_time() {
    resolve();
    if (g_get_delta_time) {
        float dt = g_get_delta_time();
        g_state.delta_time.store(dt, std::memory_order_relaxed);
        g_state.frame_time.store(g_state.frame_time.load(std::memory_order_relaxed) + dt, std::memory_order_relaxed);
    }
}

} // namespace state
