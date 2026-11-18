#include "rig.h"
#include "il2cpp.h"
#include <android/log.h>
#include <cstring>

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "iistupid/rig", __VA_ARGS__)

namespace rig {

namespace {

struct Cached {
    void* gtplayer{};
    void* gorilla_tagger{};

    // Field offsets on GTPlayer.
    int32_t off_head_collider = -1;
    int32_t off_body_collider = -1;
    int32_t off_right_hand    = -1;
    int32_t off_left_hand     = -1;
    int32_t off_rigidbody     = -1;
    int32_t off_scale         = -1;

    // Unity Transform method pointers.
    void (*transform_get_position)(void* t, unity::Vector3* out) = nullptr;
    void (*transform_set_position)(void* t, unity::Vector3 p)    = nullptr;
    void (*transform_get_forward)(void* t, unity::Vector3* out)  = nullptr;
    void (*transform_get_right)(void* t, unity::Vector3* out)    = nullptr;
    void (*transform_get_up)(void* t, unity::Vector3* out)       = nullptr;
    void* (*component_get_transform)(void* comp)                 = nullptr;

    // Rigidbody.
    void (*rb_set_velocity)(void* rb, unity::Vector3 v) = nullptr;
    void (*rb_add_force)(void* rb, unity::Vector3 f, int mode) = nullptr;

    // GorillaTagger.Instance static getter.
    void* (*gt_get_instance)() = nullptr;
    int32_t gt_off_rigidbody = -1;
    int32_t gt_off_head_collider = -1;

    bool resolved{};
};

Cached c{};

template <typename T>
inline T field(void* obj, int32_t off) {
    return *reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(obj) + off);
}

inline unity::Vector3 transform_pos(void* t) {
    unity::Vector3 v{};
    if (c.transform_get_position && t) c.transform_get_position(t, &v);
    return v;
}
inline unity::Vector3 transform_fwd(void* t) {
    unity::Vector3 v{};
    if (c.transform_get_forward && t) c.transform_get_forward(t, &v);
    return v;
}
inline unity::Vector3 transform_right(void* t) {
    unity::Vector3 v{};
    if (c.transform_get_right && t) c.transform_get_right(t, &v);
    return v;
}
inline unity::Vector3 transform_up(void* t) {
    unity::Vector3 v{};
    if (c.transform_get_up && t) c.transform_get_up(t, &v);
    return v;
}

inline void* collider_transform(void* collider) {
    return c.component_get_transform ? c.component_get_transform(collider) : nullptr;
}

void resolve_once() {
    if (c.resolved) return;
    using namespace il2cpp;

    Il2CppImage* core = find_image("UnityEngine.CoreModule.dll");
    Il2CppImage* phys = find_image("UnityEngine.PhysicsModule.dll");
    Il2CppImage* game = find_image("Assembly-CSharp.dll");
    if (!core || !phys || !game) { LOG("missing core images"); return; }

    Il2CppClass* tf = class_from_name(core, "UnityEngine", "Transform");
    Il2CppClass* cp = class_from_name(core, "UnityEngine", "Component");
    Il2CppClass* rb = class_from_name(phys, "UnityEngine", "Rigidbody");

    c.transform_get_position = (decltype(c.transform_get_position))method_get_pointer(
        class_get_method_from_name(tf, "get_position", 0));
    c.transform_set_position = (decltype(c.transform_set_position))method_get_pointer(
        class_get_method_from_name(tf, "set_position", 1));
    c.transform_get_forward  = (decltype(c.transform_get_forward))method_get_pointer(
        class_get_method_from_name(tf, "get_forward", 0));
    c.transform_get_right    = (decltype(c.transform_get_right))method_get_pointer(
        class_get_method_from_name(tf, "get_right", 0));
    c.transform_get_up       = (decltype(c.transform_get_up))method_get_pointer(
        class_get_method_from_name(tf, "get_up", 0));
    c.component_get_transform= (decltype(c.component_get_transform))method_get_pointer(
        class_get_method_from_name(cp, "get_transform", 0));

    // Unity 2022.3.x on Quest uses set_velocity; the rename to set_linearVelocity
    // happens in 2022.3.17+/Unity 6. Try the legacy name first for GT's current build.
    c.rb_set_velocity = (decltype(c.rb_set_velocity))method_get_pointer(
        class_get_method_from_name(rb, "set_velocity", 1));
    if (!c.rb_set_velocity) c.rb_set_velocity = (decltype(c.rb_set_velocity))method_get_pointer(
        class_get_method_from_name(rb, "set_linearVelocity", 1));
    c.rb_add_force = (decltype(c.rb_add_force))method_get_pointer(
        class_get_method_from_name(rb, "AddForce", 2));

    Il2CppClass* gtp = class_from_name(game, "GorillaLocomotion", "GTPlayer");
    if (gtp) {
        auto off = [&](const char* n) {
            FieldInfo* f = class_get_field_from_name(gtp, n);
            return f ? field_get_offset(f) : -1;
        };
        c.off_head_collider = off("headCollider");
        c.off_body_collider = off("bodyCollider");
        c.off_right_hand    = off("rightHandTransform");
        c.off_left_hand     = off("leftHandTransform");
        c.off_rigidbody     = off("playerRigidBody");
        if (c.off_rigidbody < 0) c.off_rigidbody = off("rigidBody");
        c.off_scale         = off("scale");
    }

    Il2CppClass* gt = class_from_name(game, "", "GorillaTagger");
    if (gt) {
        const MethodInfo* gi = class_get_method_from_name(gt, "get_Instance", 0);
        c.gt_get_instance = (decltype(c.gt_get_instance))method_get_pointer(gi);
        auto off = [&](const char* n) {
            FieldInfo* f = class_get_field_from_name(gt, n);
            return f ? field_get_offset(f) : -1;
        };
        c.gt_off_rigidbody     = off("rigidbody");
        c.gt_off_head_collider = off("headCollider");
    }

    c.resolved = true;
    LOG("rig resolved head=%d body=%d rh=%d lh=%d rb=%d sc=%d",
        c.off_head_collider, c.off_body_collider, c.off_right_hand,
        c.off_left_hand, c.off_rigidbody, c.off_scale);
}

} // namespace

void capture_local_rig(void* gtplayer_self) {
    resolve_once();
    c.gtplayer = gtplayer_self;
    if (c.gt_get_instance) c.gorilla_tagger = c.gt_get_instance();
}

bool ready() { return c.resolved && c.gtplayer != nullptr; }

void* gtplayer() { return c.gtplayer; }
void* gorilla_tagger() { return c.gorilla_tagger; }

float scale() {
    if (!ready() || c.off_scale < 0) return 1.f;
    return field<float>(c.gtplayer, c.off_scale);
}

unity::Vector3 body_position() {
    if (!ready() || c.off_body_collider < 0) return {};
    void* body = field<void*>(c.gtplayer, c.off_body_collider);
    return transform_pos(collider_transform(body));
}

void set_body_position(unity::Vector3 p) {
    if (!ready() || c.off_body_collider < 0) return;
    void* body = field<void*>(c.gtplayer, c.off_body_collider);
    void* tf = collider_transform(body);
    if (tf && c.transform_set_position) c.transform_set_position(tf, p);
}

unity::Vector3 head_position() {
    if (!ready() || c.off_head_collider < 0) return {};
    void* h = field<void*>(c.gtplayer, c.off_head_collider);
    return transform_pos(collider_transform(h));
}
unity::Vector3 head_forward() {
    if (!ready() || c.off_head_collider < 0) return {};
    void* h = field<void*>(c.gtplayer, c.off_head_collider);
    return transform_fwd(collider_transform(h));
}
unity::Vector3 head_right() {
    if (!ready() || c.off_head_collider < 0) return {};
    void* h = field<void*>(c.gtplayer, c.off_head_collider);
    return transform_right(collider_transform(h));
}
unity::Vector3 head_up() {
    if (!ready() || c.off_head_collider < 0) return {};
    void* h = field<void*>(c.gtplayer, c.off_head_collider);
    return transform_up(collider_transform(h));
}

unity::Vector3 right_hand_position() {
    if (!ready() || c.off_right_hand < 0) return {};
    void* t = field<void*>(c.gtplayer, c.off_right_hand);
    return transform_pos(t);
}
unity::Vector3 right_hand_forward() {
    if (!ready() || c.off_right_hand < 0) return {};
    void* t = field<void*>(c.gtplayer, c.off_right_hand);
    return transform_fwd(t);
}
unity::Vector3 left_hand_position() {
    if (!ready() || c.off_left_hand < 0) return {};
    void* t = field<void*>(c.gtplayer, c.off_left_hand);
    return transform_pos(t);
}
unity::Vector3 left_hand_forward() {
    if (!ready() || c.off_left_hand < 0) return {};
    void* t = field<void*>(c.gtplayer, c.off_left_hand);
    return transform_fwd(t);
}

void zero_velocity() {
    if (!ready() || c.off_rigidbody < 0 || !c.rb_set_velocity) return;
    void* rb = field<void*>(c.gtplayer, c.off_rigidbody);
    if (rb) c.rb_set_velocity(rb, {0, 0, 0});
}

void add_force(unity::Vector3 f) {
    if (!ready() || c.off_rigidbody < 0 || !c.rb_add_force) return;
    void* rb = field<void*>(c.gtplayer, c.off_rigidbody);
    if (rb) c.rb_add_force(rb, f, /*ForceMode.Acceleration*/ 5);
}

void translate(unity::Vector3 delta) {
    if (delta.x == 0 && delta.y == 0 && delta.z == 0) return;
    set_body_position(body_position() + delta);
}

} // namespace rig
