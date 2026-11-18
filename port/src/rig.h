#pragma once
#include "unity.h"
#include "il2cpp.h"

namespace rig {

// Call from the GTPlayer.LateUpdate hook with the player 'self' pointer.
// Resolves and caches field offsets on first call.
void capture_local_rig(void* gtplayer_self);

bool ready();

// Position/rotation of the player body (world).
unity::Vector3 body_position();
void set_body_position(unity::Vector3 p);

// Head collider transform.
unity::Vector3 head_position();
unity::Vector3 head_forward();
unity::Vector3 head_right();
unity::Vector3 head_up();

// Hand colliders.
unity::Vector3 right_hand_position();
unity::Vector3 right_hand_forward();
unity::Vector3 left_hand_position();
unity::Vector3 left_hand_forward();

// Rigidbody (zero velocity, add force).
void zero_velocity();
void add_force(unity::Vector3 f);

// Move the player by a delta.
void translate(unity::Vector3 delta);

// Current GTPlayer scale (for scaleWithPlayer support).
float scale();

// Expose the raw 'self' pointer for mods that need it.
void* gtplayer();
void* gorilla_tagger();

// --- Extended resolvers (added for Safety/Fun ports) ------------------

// VRRig.LocalRig static instance, cached.
void* vrrig_local();

// VRRig.LocalRig.head.trackingRotationOffset setters.
void set_head_tracking_rotation(unity::Vector3 v);
void add_head_tracking_rotation(unity::Vector3 v);

// Enable / disable the local rig (controls remote visibility of self).
void set_rig_enabled(bool enabled);

// GTPlayer.inOverlay field.
void set_in_overlay(bool on);

// GTPlayer.GetControllerTransform(isRight) -> Transform.
void* controller_transform(bool is_right);

// Hand transform setters.
void set_controller_position(bool is_right, unity::Vector3 p);

// ControllerInputPoller zero-out (NoFinger).
void poller_no_finger();

// GorillaTagger.handTapVolume.
void set_hand_tap_volume(float v);

// System.GC.Collect() shim.
void gc_collect();

} // namespace rig
