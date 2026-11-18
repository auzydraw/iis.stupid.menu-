#pragma once
#include <cstddef>

namespace hook {

// Install an inline hook via Dobby. orig receives the trampoline pointer.
bool install(void* target, void* replacement, void** orig);
bool install_sym(const char* module, const char* symbol, void* replacement, void** orig);

// Install the Unity update tick that drives all mods.
// Called once il2cpp is ready. Hooks GTPlayer.LateUpdate (or an equivalent).
bool install_tick();

} // namespace hook
