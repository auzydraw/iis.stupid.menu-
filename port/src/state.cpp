#include "state.h"
#include "il2cpp.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <android/log.h>

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "iistupid/state", __VA_ARGS__)

namespace state {

namespace {
Globals g_state;
bool g_admin = false;
std::string g_discord;

// Admin allowlist. Expand as needed.
constexpr const char* kAdminList[] = {
    "rick.bannedonmain",
    nullptr,
};

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

void load_identity() {
    FILE* f = fopen("/sdcard/iiMenu/discord_user.txt", "r");
    if (!f) { LOG("no discord_user.txt; admin locked"); return; }
    char buf[128];
    if (fgets(buf, sizeof(buf), f)) {
        std::string s(buf);
        s.erase(std::remove_if(s.begin(), s.end(),
            [](unsigned char ch){ return ch=='\r' || ch=='\n' || ch==' ' || ch=='\t'; }),
            s.end());
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char ch){ return (char)std::tolower(ch); });
        g_discord = s;
        for (int i = 0; kAdminList[i]; ++i) {
            if (g_discord == kAdminList[i]) {
                g_admin = true;
                LOG("admin unlocked for '%s'", g_discord.c_str());
                break;
            }
        }
        if (!g_admin) LOG("discord '%s' not on admin list", g_discord.c_str());
    }
    fclose(f);
}

bool is_admin() { return g_admin; }
const char* discord_username() { return g_discord.c_str(); }

} // namespace state
