// Low-level Unity GameObject / primitive wrappers for the in-world menu.
#pragma once
#include "unity.h"

namespace ui {

enum class PrimitiveKind {
    Cube   = 3,   // UnityEngine.PrimitiveType.Cube
    Sphere = 0,   // Sphere
};

bool init();

void* create_cube();
void* create_sphere();

void destroy(void* go);
void destroy_collider_of(void* go);

void* transform_of(void* go);
void  set_position(void* go, unity::Vector3 pos);
void  set_rotation(void* go, unity::Quaternion rot);
void  set_local_scale(void* go, unity::Vector3 scale);
void  set_local_position(void* go, unity::Vector3 pos);
void  set_parent(void* child_go, void* parent_tf);
void  set_parent_transform(void* child_tf, void* parent_tf);

unity::Vector3 get_position(void* go);

void set_color(void* go, unity::Color c);

// Try to attach a TextMeshPro label with the given text. If the TMP resolve
// chain fails, this no-ops silently.
void set_label(void* go, const char* text, float font_size, unity::Color color);

// Set the active/inactive state of a GameObject.
void set_active(void* go, bool on);

} // namespace ui
