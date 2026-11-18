#include "hook.h"
#include "il2cpp.h"
#include "input.h"
#include "rig.h"
#include "state.h"
#include "registry.h"
#include "menu.h"

#include <dlfcn.h>
#include <shadowhook.h>
#include <android/log.h>

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "iistupid/hook", __VA_ARGS__)

namespace hook {

namespace { bool s_sh_init = false; }

static void ensure_init() {
    if (s_sh_init) return;
    shadowhook_init(SHADOWHOOK_MODE_UNIQUE, false);
    s_sh_init = true;
}

bool install(void* target, void* replacement, void** orig) {
    if (!target) return false;
    ensure_init();
    void* stub = shadowhook_hook_func_addr(target, replacement, orig);
    if (!stub) {
        LOG("shadowhook_hook_func_addr failed: %d (%s)",
            shadowhook_get_errno(), shadowhook_to_errmsg(shadowhook_get_errno()));
        return false;
    }
    return true;
}

bool install_sym(const char* module, const char* symbol, void* replacement, void** orig) {
    ensure_init();
    void* stub = shadowhook_hook_sym_name(module, symbol, replacement, orig);
    return stub != nullptr;
}

namespace {

using late_update_fn = void (*)(void* self);
late_update_fn g_orig_late_update = nullptr;

void late_update_hook(void* self) {
    // Set the local player rig pointer once, lazy.
    rig::capture_local_rig(self);

    // Poll controllers before mods tick so they see fresh input.
    input::poll();

    // Run the menu (handles toggle gesture + navigation).
    menu::tick();

    // Tick every enabled mod.
    registry::tick_enabled();

    if (g_orig_late_update) g_orig_late_update(self);
}

} // namespace

bool install_tick() {
    using namespace il2cpp;

    // GTPlayer lives in Assembly-CSharp.dll, namespace GorillaLocomotion.
    const MethodInfo* m = find_method("Assembly-CSharp.dll", "GorillaLocomotion", "GTPlayer", "LateUpdate", 0);
    if (!m) m = find_method("Assembly-CSharp.dll", "GorillaLocomotion", "GTPlayer", "LateUpdateLocal", 0);
    if (!m) m = find_method("Assembly-CSharp.dll", "GorillaLocomotion", "GTPlayer", "Update", 0);
    if (!m) { LOG("GTPlayer tick method not found"); return false; }

    void* target = method_get_pointer(m);
    if (!target) { LOG("GTPlayer tick ptr null"); return false; }

    bool ok = install(target, (void*)late_update_hook, (void**)&g_orig_late_update);
    LOG("tick hook: %s target=%p", ok ? "ok" : "fail", target);
    return ok;
}

} // namespace hook
