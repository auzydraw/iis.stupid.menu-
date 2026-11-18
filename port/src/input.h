#pragma once
#include "unity.h"

namespace input {

struct ControllerState {
    bool  leftPrimary;    // X on left Touch
    bool  leftSecondary;  // Y
    bool  rightPrimary;   // A
    bool  rightSecondary; // B
    bool  leftGrip;
    bool  rightGrip;
    float leftTrigger;    // 0..1
    float rightTrigger;   // 0..1
    unity::Vector2 leftJoystick;
    unity::Vector2 rightJoystick;
    bool  leftJoystickClick;
    bool  rightJoystickClick;
};

// Resolve the OVRInput / UnityEngine.XR.Input symbols (one-shot).
bool init();

// Poll controller state. Call once per frame on the Unity thread.
void poll();

const ControllerState& state();

// Edge helpers: true only on the frame the condition transitioned.
bool just_pressed_left_primary();
bool just_pressed_right_primary();
bool just_pressed_left_secondary();
bool just_pressed_right_secondary();

} // namespace input
