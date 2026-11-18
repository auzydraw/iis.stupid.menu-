#include "input.h"
#include "il2cpp.h"
#include <android/log.h>
#include <cstring>

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "iistupid/input", __VA_ARGS__)

namespace input {

namespace {

enum Button : uint32_t {
    One             = 0x0001, // A or X
    Two             = 0x0002, // B or Y
    PrimaryThumbstick = 0x04000000,
};

enum Controller : uint32_t {
    LTouch = 0x01,
    RTouch = 0x02,
};

enum Axis1D : uint32_t {
    PrimaryIndexTrigger = 0x01,
    PrimaryHandTrigger  = 0x02,
};

enum Axis2D : uint32_t {
    Primary2DAxis = 0x01,
};

using fn_get_button = bool (*)(uint32_t, uint32_t);
using fn_get_axis1d = float (*)(uint32_t, uint32_t);
using fn_get_axis2d = unity::Vector2 (*)(uint32_t, uint32_t);

fn_get_button g_get_btn  = nullptr;
fn_get_axis1d g_get_ax1  = nullptr;
fn_get_axis2d g_get_ax2  = nullptr;

ControllerState s_now{};
ControllerState s_prev{};

} // namespace

bool init() {
    using namespace il2cpp;
    Il2CppImage* img = find_image("Oculus.VR.dll");
    if (!img) img = find_image("Assembly-CSharp.dll");
    if (!img) { LOG("no image for OVRInput"); return false; }

    Il2CppClass* k = class_from_name(img, "", "OVRInput");
    if (!k) { LOG("OVRInput class not found"); return false; }

    const MethodInfo* m_btn = find_method_by_param_suffix(k, "Get", 2, 0, "OVRInput/Button");
    const MethodInfo* m_ax1 = find_method_by_param_suffix(k, "Get", 2, 0, "OVRInput/Axis1D");
    const MethodInfo* m_ax2 = find_method_by_param_suffix(k, "Get", 2, 0, "OVRInput/Axis2D");

    // Fallback: some IL2CPP builds encode the nested enum name without the slash.
    if (!m_btn) m_btn = find_method_by_param_suffix(k, "Get", 2, 0, "Button");
    if (!m_ax1) m_ax1 = find_method_by_param_suffix(k, "Get", 2, 0, "Axis1D");
    if (!m_ax2) m_ax2 = find_method_by_param_suffix(k, "Get", 2, 0, "Axis2D");

    g_get_btn = (fn_get_button)method_get_pointer(m_btn);
    g_get_ax1 = (fn_get_axis1d)method_get_pointer(m_ax1);
    g_get_ax2 = (fn_get_axis2d)method_get_pointer(m_ax2);

    LOG("OVRInput resolve: btn=%p ax1=%p ax2=%p", g_get_btn, g_get_ax1, g_get_ax2);
    return g_get_btn && g_get_ax1 && g_get_ax2;
}

void poll() {
    s_prev = s_now;
    if (!g_get_btn && !init()) return;

    s_now.rightPrimary   = g_get_btn(One, RTouch);
    s_now.rightSecondary = g_get_btn(Two, RTouch);
    s_now.leftPrimary    = g_get_btn(One, LTouch);
    s_now.leftSecondary  = g_get_btn(Two, LTouch);
    s_now.leftJoystickClick  = g_get_btn(PrimaryThumbstick, LTouch);
    s_now.rightJoystickClick = g_get_btn(PrimaryThumbstick, RTouch);

    s_now.leftGrip  = g_get_ax1(PrimaryHandTrigger, LTouch) > 0.75f;
    s_now.rightGrip = g_get_ax1(PrimaryHandTrigger, RTouch) > 0.75f;
    s_now.leftTrigger  = g_get_ax1(PrimaryIndexTrigger, LTouch);
    s_now.rightTrigger = g_get_ax1(PrimaryIndexTrigger, RTouch);

    s_now.leftJoystick  = g_get_ax2(Primary2DAxis, LTouch);
    s_now.rightJoystick = g_get_ax2(Primary2DAxis, RTouch);
}

const ControllerState& state() { return s_now; }

bool just_pressed_left_primary()    { return s_now.leftPrimary   && !s_prev.leftPrimary; }
bool just_pressed_right_primary()   { return s_now.rightPrimary  && !s_prev.rightPrimary; }
bool just_pressed_left_secondary()  { return s_now.leftSecondary && !s_prev.leftSecondary; }
bool just_pressed_right_secondary() { return s_now.rightSecondary&& !s_prev.rightSecondary; }

} // namespace input
