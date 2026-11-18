#pragma once
#include <atomic>

namespace state {

struct Globals {
    std::atomic<float> fly_speed{8.0f};
    std::atomic<float> playspace_speed{6.0f};
    std::atomic<float> speed_boost{1.3f};
    std::atomic<float> arm_length{1.25f};
    std::atomic<float> jump_power{7.5f};
    std::atomic<float> jump_multi{1.1f};
    std::atomic<bool>  scale_with_player{true};
    std::atomic<bool>  noclip_active{false};
    std::atomic<float> delta_time{0.011f};
    std::atomic<double> frame_time{0.0};
};

Globals& g();

// Called from the tick each frame; updates delta_time via Time.deltaTime.
void refresh_time();

// --- Admin identity -------------------------------------------------------

// One-shot: reads /sdcard/iiMenu/discord_user.txt and sets admin flag if the
// (trimmed, lowercased) username matches the admin allowlist.
void load_identity();

bool is_admin();
const char* discord_username();

} // namespace state
