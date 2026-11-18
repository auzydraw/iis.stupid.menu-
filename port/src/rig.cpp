#include "rig.h"
#include "il2cpp.h"
#include <android/log.h>
#include <cstring>
#include <initializer_list>

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
    int32_t gt_off_left_hand_transform  = -1;
    int32_t gt_off_right_hand_transform = -1;
    int32_t gt_off_hand_tap_volume      = -1;

    // GTPlayer extras.
    int32_t off_in_overlay = -1;
    void* (*gtp_get_controller_tf)(void* self, bool is_right) = nullptr;

    // VRRig.LocalRig chain.
    void* (*vrrig_get_local)() = nullptr;
    int32_t vr_off_enabled      = -1;
    int32_t vr_off_head         = -1;
    int32_t head_off_tracking_rot = -1;

    // ControllerInputPoller.instance + fields.
    void* (*cip_get_instance)() = nullptr;
    int32_t cip_off_left_grip  = -1;
    int32_t cip_off_right_grip = -1;
    int32_t cip_off_left_idx   = -1;
    int32_t cip_off_right_idx  = -1;
    int32_t cip_off_left_a     = -1;
    int32_t cip_off_right_a    = -1;
    int32_t cip_off_left_b     = -1;
    int32_t cip_off_right_b    = -1;

    // System.GC.Collect()
    void (*gc_collect_fn)() = nullptr;

    bool resolved{};
    bool ext_resolved{};
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
        c.off_in_overlay    = off("inOverlay");

        c.gtp_get_controller_tf = (decltype(c.gtp_get_controller_tf))method_get_pointer(
            class_get_method_from_name(gtp, "GetControllerTransform", 1));
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
        c.gt_off_left_hand_transform  = off("leftHandTransform");
        c.gt_off_right_hand_transform = off("rightHandTransform");
        c.gt_off_hand_tap_volume      = off("handTapVolume");
    }

    // VRRig.LocalRig + VRMap head chain
    if (Il2CppClass* vrrig = class_from_name(game, "", "VRRig")) {
        c.vrrig_get_local = (decltype(c.vrrig_get_local))method_get_pointer(
            class_get_method_from_name(vrrig, "get_LocalRig", 0));
        auto off = [&](const char* n) {
            FieldInfo* f = class_get_field_from_name(vrrig, n);
            return f ? field_get_offset(f) : -1;
        };
        c.vr_off_enabled = off("enabled");
        c.vr_off_head    = off("head");
    }
    if (Il2CppClass* vrmap = class_from_name(game, "", "VRMap")) {
        FieldInfo* f = class_get_field_from_name(vrmap, "trackingRotationOffset");
        c.head_off_tracking_rot = f ? field_get_offset(f) : -1;
    }

    // ControllerInputPoller.instance
    if (Il2CppClass* cip = class_from_name(game, "", "ControllerInputPoller")) {
        c.cip_get_instance = (decltype(c.cip_get_instance))method_get_pointer(
            class_get_method_from_name(cip, "get_instance", 0));
        auto off = [&](const char* n) {
            FieldInfo* f = class_get_field_from_name(cip, n);
            return f ? field_get_offset(f) : -1;
        };
        c.cip_off_left_grip  = off("leftControllerGripFloat");
        c.cip_off_right_grip = off("rightControllerGripFloat");
        c.cip_off_left_idx   = off("leftControllerIndexFloat");
        c.cip_off_right_idx  = off("rightControllerIndexFloat");
        c.cip_off_left_a     = off("leftControllerPrimaryButton");
        c.cip_off_right_a    = off("rightControllerPrimaryButton");
        c.cip_off_left_b     = off("leftControllerSecondaryButton");
        c.cip_off_right_b    = off("rightControllerSecondaryButton");
    }

    // System.GC.Collect
    for (const char* lib : {"mscorlib.dll", "System.dll", "netstandard.dll"}) {
        Il2CppImage* im = find_image(lib);
        if (!im) continue;
        if (Il2CppClass* gc = class_from_name(im, "System", "GC")) {
            const MethodInfo* m = class_get_method_from_name(gc, "Collect", 0);
            if (m) {
                c.gc_collect_fn = (decltype(c.gc_collect_fn))method_get_pointer(m);
                break;
            }
        }
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

// --- Extended resolvers ------------------------------------------------

void* vrrig_local() {
    return c.vrrig_get_local ? c.vrrig_get_local() : nullptr;
}

void set_head_tracking_rotation(unity::Vector3 v) {
    void* rig = vrrig_local();
    if (!rig || c.vr_off_head < 0 || c.head_off_tracking_rot < 0) return;
    void* head = field<void*>(rig, c.vr_off_head);
    if (!head) return;
    *reinterpret_cast<unity::Vector3*>(reinterpret_cast<uint8_t*>(head) + c.head_off_tracking_rot) = v;
}

void add_head_tracking_rotation(unity::Vector3 v) {
    void* rig = vrrig_local();
    if (!rig || c.vr_off_head < 0 || c.head_off_tracking_rot < 0) return;
    void* head = field<void*>(rig, c.vr_off_head);
    if (!head) return;
    auto* p = reinterpret_cast<unity::Vector3*>(reinterpret_cast<uint8_t*>(head) + c.head_off_tracking_rot);
    p->x += v.x; p->y += v.y; p->z += v.z;
}

void set_rig_enabled(bool enabled) {
    void* rig = vrrig_local();
    if (!rig || c.vr_off_enabled < 0) return;
    *reinterpret_cast<bool*>(reinterpret_cast<uint8_t*>(rig) + c.vr_off_enabled) = enabled;
}

void set_in_overlay(bool on) {
    if (!c.gtplayer || c.off_in_overlay < 0) return;
    *reinterpret_cast<bool*>(reinterpret_cast<uint8_t*>(c.gtplayer) + c.off_in_overlay) = on;
}

void* controller_transform(bool is_right) {
    if (!c.gtplayer || !c.gtp_get_controller_tf) return nullptr;
    return c.gtp_get_controller_tf(c.gtplayer, is_right);
}

void set_controller_position(bool is_right, unity::Vector3 p) {
    void* tf = controller_transform(is_right);
    if (tf && c.transform_set_position) c.transform_set_position(tf, p);
}

void poller_no_finger() {
    if (!c.cip_get_instance) return;
    void* inst = c.cip_get_instance();
    if (!inst) return;
    auto zero_f = [&](int32_t off) {
        if (off < 0) return;
        *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(inst) + off) = 0.f;
    };
    auto zero_b = [&](int32_t off) {
        if (off < 0) return;
        *reinterpret_cast<bool*>(reinterpret_cast<uint8_t*>(inst) + off) = false;
    };
    zero_f(c.cip_off_left_grip);
    zero_f(c.cip_off_right_grip);
    zero_f(c.cip_off_left_idx);
    zero_f(c.cip_off_right_idx);
    zero_b(c.cip_off_left_a);
    zero_b(c.cip_off_right_a);
    zero_b(c.cip_off_left_b);
    zero_b(c.cip_off_right_b);
}

void set_hand_tap_volume(float v) {
    if (!c.gorilla_tagger || c.gt_off_hand_tap_volume < 0) return;
    *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(c.gorilla_tagger) + c.gt_off_hand_tap_volume) = v;
}

void gc_collect() {
    if (c.gc_collect_fn) c.gc_collect_fn();
}

} // namespace rig
