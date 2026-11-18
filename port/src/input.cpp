#include "input.h"
#include "il2cpp.h"
#include <android/log.h>
#include <cstring>

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "iistupid/input", __VA_ARGS__)

namespace input {

namespace {

// OVRInput enums (matches Oculus Integration version used by GT 2024+).
enum Button : uint32_t {
    None            = 0,
    One             = 0x0001, // A or X
    Two             = 0x0002, // B or Y
    Three           = 0x0004,
    Four            = 0x0008,
    PrimaryThumbstick = 0x04000000,
    SecondaryThumbstick = 0x08000000,
};

enum Controller : uint32_t {
    LTouch = 0x01,
    RTouch = 0x02,
    Touch  = 0x03,
    Active = 0xFFFFFFFF,
};

enum RawButton : uint32_t {
    RawA = 0x00000001, RawB = 0x00000002,
    RawX = 0x00000100, RawY = 0x00000200,
    RawLThumbstick = 0x00000400, RawRThumbstick = 0x00000004,
};

enum Axis1D : uint32_t {
    PrimaryIndexTrigger   = 0x01,
    PrimaryHandTrigger    = 0x02,
    SecondaryIndexTrigger = 0x04,
    SecondaryHandTrigger  = 0x08,
};

enum Axis2D : uint32_t {
    Primary2DAxis   = 0x01,
    Secondary2DAxis = 0x02,
};

using fn_get_button = bool (*)(uint32_t, uint32_t);
using fn_get_axis1d = float (*)(uint32_t, uint32_t);
using fn_get_axis2d = unity::Vector2 (*)(uint32_t, uint32_t);

fn_get_button g_get      = nullptr;  // OVRInput.Get(Button, Controller)
fn_get_button g_get_raw  = nullptr;  // OVRInput.Get(RawButton, Controller)
fn_get_axis1d g_axis1d   = nullptr;  // OVRInput.Get(Axis1D, Controller)
fn_get_axis2d g_axis2d   = nullptr;  // OVRInput.Get(Axis2D, Controller)

ControllerState s_now{};
ControllerState s_prev{};

const il2cpp::MethodInfo* find_ovr(const char* method, int argc) {
    return il2cpp::find_method("Assembly-CSharp.dll", "", "OVRInput", method, argc);
}

} // namespace

bool init() {
    // OVRInput exposes overloaded Get(). We resolve each 2-arg overload.
    using namespace il2cpp;
    const MethodInfo* mb = find_ovr("Get", 2);
    if (!mb) { LOG("OVRInput.Get not found"); return false; }
    // Multiple overloads exist. We trust Unity's class metadata to have at least these; use raw pointers resolved by signature at hook time if needed.
    g_get     = reinterpret_cast<fn_get_button>(method_get_pointer(mb));
    g_get_raw = g_get;
    g_axis1d  = reinterpret_cast<fn_get_axis1d>(method_get_pointer(mb));
    g_axis2d  = reinterpret_cast<fn_get_axis2d>(method_get_pointer(mb));
    return g_get != nullptr;
}

void poll() {
    s_prev = s_now;
    if (!g_get) {
        if (!init()) return;
    }

    s_now.rightPrimary   = g_get(One, RTouch);
    s_now.rightSecondary = g_get(Two, RTouch);
    s_now.leftPrimary    = g_get(One, LTouch);
    s_now.leftSecondary  = g_get(Two, LTouch);
    s_now.leftJoystickClick  = g_get(PrimaryThumbstick, LTouch);
    s_now.rightJoystickClick = g_get(PrimaryThumbstick, RTouch);

    s_now.leftGrip  = g_axis1d(PrimaryHandTrigger, LTouch)   > 0.75f;
    s_now.rightGrip = g_axis1d(PrimaryHandTrigger, RTouch)   > 0.75f;
    s_now.leftTrigger  = g_axis1d(PrimaryIndexTrigger, LTouch);
    s_now.rightTrigger = g_axis1d(PrimaryIndexTrigger, RTouch);

    s_now.leftJoystick  = g_axis2d(Primary2DAxis, LTouch);
    s_now.rightJoystick = g_axis2d(Primary2DAxis, RTouch);
}

const ControllerState& state() { return s_now; }

bool just_pressed_left_primary()    { return s_now.leftPrimary   && !s_prev.leftPrimary; }
bool just_pressed_right_primary()   { return s_now.rightPrimary  && !s_prev.rightPrimary; }
bool just_pressed_left_secondary()  { return s_now.leftSecondary && !s_prev.leftSecondary; }
bool just_pressed_right_secondary() { return s_now.rightSecondary&& !s_prev.rightSecondary; }

} // namespace input
