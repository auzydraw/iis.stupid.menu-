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

} // namespace rig
