// ii's Stupid Menu  Android port entry.
// Copyright (C) 2026  Goldentrophy Software. GPL-3.0.
//
// Loader: this .so is injected into com.AnotherAxiom.GorillaTag before
// libil2cpp.so loads (via libmain.so hijack / zygisk / lsplant / etc.).
// We spin up a watcher thread that waits for libil2cpp.so to show up in
// the proc maps, then installs the il2cpp wrapper, the tick hook, and
// registers every mod.

#include "il2cpp.h"
#include "hook.h"
#include "input.h"
#include "registry.h"
#include "state.h"

#include <android/log.h>
#include <dlfcn.h>
#include <jni.h>
#include <pthread.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "iistupid", __VA_ARGS__)
#define ERR(...) __android_log_print(ANDROID_LOG_ERROR, "iistupid", __VA_ARGS__)

namespace {

bool module_loaded(const char* name) {
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return false;
    char line[512];
    bool found = false;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, name)) { found = true; break; }
    }
    fclose(f);
    return found;
}

void* init_thread(void*) {
    LOG("init_thread start");
    for (int i = 0; i < 300; ++i) {
        if (module_loaded("libil2cpp.so")) break;
        usleep(100 * 1000);
    }
    if (!module_loaded("libil2cpp.so")) {
        ERR("libil2cpp.so never appeared; aborting init");
        return nullptr;
    }
    // Give il2cpp_init time to finish metadata parse.
    sleep(3);

    if (!il2cpp::init()) {
        ERR("il2cpp api resolve failed");
        return nullptr;
    }

    il2cpp::attach_current_thread();

    input::init();

    registry::register_core();
    registry::load_config();

    if (!hook::install_tick()) {
        ERR("tick hook install failed");
        return nullptr;
    }

    LOG("iistupid initialized (%zu mods)", registry::all().size());
    return nullptr;
}

} // namespace

extern "C" __attribute__((visibility("default")))
jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    LOG("JNI_OnLoad");
    pthread_t t;
    pthread_create(&t, nullptr, init_thread, nullptr);
    pthread_detach(t);
    return JNI_VERSION_1_6;
}

// Fallback entry for plain dlopen-style loaders.
extern "C" __attribute__((visibility("default"), constructor))
void iistupid_ctor() {
    LOG("ctor (dlopen path)");
    pthread_t t;
    pthread_create(&t, nullptr, init_thread, nullptr);
    pthread_detach(t);
}
