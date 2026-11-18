#include "ui.h"
#include "il2cpp.h"
#include <android/log.h>
#include <cstring>
#include <dlfcn.h>
#include <initializer_list>

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "iistupid/ui", __VA_ARGS__)

namespace ui {

namespace {

struct Api {
    bool ok = false;
    bool tried = false;

    // UnityEngine.GameObject
    void* (*go_create_primitive)(int32_t kind) = nullptr;
    void* (*go_get_transform)(void* go) = nullptr;
    void  (*go_set_active)(void* go, bool on) = nullptr;
    void* (*go_add_component)(void* go, void* type) = nullptr;
    void* (*go_get_component)(void* go, void* type) = nullptr;

    // UnityEngine.Transform
    void (*tf_set_pos)(void* tf, unity::Vector3 p) = nullptr;
    void (*tf_set_rot)(void* tf, unity::Quaternion q) = nullptr;
    void (*tf_set_local_scale)(void* tf, unity::Vector3 s) = nullptr;
    void (*tf_set_local_pos)(void* tf, unity::Vector3 p) = nullptr;
    void (*tf_set_parent)(void* tf, void* parent) = nullptr;
    void (*tf_get_pos)(void* tf, unity::Vector3* out) = nullptr;

    // UnityEngine.Component.get_gameObject
    void* (*cp_get_go)(void* comp) = nullptr;

    // UnityEngine.Object.Destroy
    void (*obj_destroy)(void* obj) = nullptr;

    // UnityEngine.Renderer.get_material -> Material
    void* (*rend_get_material)(void* rend) = nullptr;
    // UnityEngine.Material.set_color
    void (*mat_set_color)(void* mat, unity::Color c) = nullptr;

    // Classes used for GetComponent(Type)
    void* type_renderer_obj  = nullptr;
    void* type_collider_obj  = nullptr;

    // TextMeshPro, optional
    void* type_tmp_obj = nullptr;
    void (*tmp_set_text)(void* t, void* str) = nullptr;
    void (*tmp_set_font_size)(void* t, float v) = nullptr;
    void (*tmp_set_color)(void* t, unity::Color c) = nullptr;
    void (*tmp_set_align)(void* t, int32_t align) = nullptr;
};

Api a;

void* type_from_class(il2cpp::Il2CppClass* k) {
    using namespace il2cpp;
    if (!k) return nullptr;
    static const Il2CppType* (*class_get_type)(Il2CppClass*) = nullptr;
    static void* (*type_get_object)(const Il2CppType*) = nullptr;
    if (!class_get_type) {
        void* lib = dlopen("libil2cpp.so", RTLD_NOW | RTLD_NOLOAD);
        if (lib) {
            class_get_type  = (decltype(class_get_type))dlsym(lib, "il2cpp_class_get_type");
            type_get_object = (decltype(type_get_object))dlsym(lib, "il2cpp_type_get_object");
        }
    }
    if (!class_get_type || !type_get_object) return nullptr;
    return type_get_object(class_get_type(k));
}

} // namespace

bool init() {
    if (a.tried) return a.ok;
    a.tried = true;
    using namespace il2cpp;

    Il2CppImage* core = find_image("UnityEngine.CoreModule.dll");
    if (!core) { LOG("no CoreModule"); return false; }

    if (Il2CppClass* go = class_from_name(core, "UnityEngine", "GameObject")) {
        a.go_create_primitive = (decltype(a.go_create_primitive))method_get_pointer(
            class_get_method_from_name(go, "CreatePrimitive", 1));
        a.go_get_transform = (decltype(a.go_get_transform))method_get_pointer(
            class_get_method_from_name(go, "get_transform", 0));
        a.go_set_active = (decltype(a.go_set_active))method_get_pointer(
            class_get_method_from_name(go, "SetActive", 1));
        a.go_add_component = (decltype(a.go_add_component))method_get_pointer(
            class_get_method_from_name(go, "AddComponent", 1));
        a.go_get_component = (decltype(a.go_get_component))method_get_pointer(
            class_get_method_from_name(go, "GetComponent", 1));
    }
    if (Il2CppClass* tf = class_from_name(core, "UnityEngine", "Transform")) {
        a.tf_set_pos = (decltype(a.tf_set_pos))method_get_pointer(
            class_get_method_from_name(tf, "set_position", 1));
        a.tf_set_rot = (decltype(a.tf_set_rot))method_get_pointer(
            class_get_method_from_name(tf, "set_rotation", 1));
        a.tf_set_local_scale = (decltype(a.tf_set_local_scale))method_get_pointer(
            class_get_method_from_name(tf, "set_localScale", 1));
        a.tf_set_local_pos = (decltype(a.tf_set_local_pos))method_get_pointer(
            class_get_method_from_name(tf, "set_localPosition", 1));
        a.tf_set_parent = (decltype(a.tf_set_parent))method_get_pointer(
            class_get_method_from_name(tf, "SetParent", 1));
        a.tf_get_pos = (decltype(a.tf_get_pos))method_get_pointer(
            class_get_method_from_name(tf, "get_position", 0));
    }
    if (Il2CppClass* cp = class_from_name(core, "UnityEngine", "Component")) {
        a.cp_get_go = (decltype(a.cp_get_go))method_get_pointer(
            class_get_method_from_name(cp, "get_gameObject", 0));
    }
    if (Il2CppClass* obj = class_from_name(core, "UnityEngine", "Object")) {
        a.obj_destroy = (decltype(a.obj_destroy))method_get_pointer(
            class_get_method_from_name(obj, "Destroy", 1));
    }
    if (Il2CppClass* rend = class_from_name(core, "UnityEngine", "Renderer")) {
        a.rend_get_material = (decltype(a.rend_get_material))method_get_pointer(
            class_get_method_from_name(rend, "get_material", 0));
        a.type_renderer_obj = type_from_class(rend);
    }
    if (Il2CppClass* mat = class_from_name(core, "UnityEngine", "Material")) {
        a.mat_set_color = (decltype(a.mat_set_color))method_get_pointer(
            class_get_method_from_name(mat, "set_color", 1));
    }

    Il2CppImage* phys = find_image("UnityEngine.PhysicsModule.dll");
    if (phys) {
        if (Il2CppClass* c = class_from_name(phys, "UnityEngine", "Collider"))
            a.type_collider_obj = type_from_class(c);
    }

    // TextMeshPro optional.
    for (const char* img : {"Unity.TextMeshPro.dll", "TextMeshPro.dll"}) {
        Il2CppImage* ti = find_image(img);
        if (!ti) continue;
        Il2CppClass* tmp = class_from_name(ti, "TMPro", "TextMeshPro");
        if (!tmp) tmp = class_from_name(ti, "TMPro", "TMP_Text");
        if (!tmp) continue;
        a.type_tmp_obj = type_from_class(tmp);
        a.tmp_set_text = (decltype(a.tmp_set_text))method_get_pointer(
            class_get_method_from_name(tmp, "set_text", 1));
        a.tmp_set_font_size = (decltype(a.tmp_set_font_size))method_get_pointer(
            class_get_method_from_name(tmp, "set_fontSize", 1));
        a.tmp_set_color = (decltype(a.tmp_set_color))method_get_pointer(
            class_get_method_from_name(tmp, "set_color", 1));
        a.tmp_set_align = (decltype(a.tmp_set_align))method_get_pointer(
            class_get_method_from_name(tmp, "set_alignment", 1));
        break;
    }

    a.ok = a.go_create_primitive && a.go_get_transform && a.tf_set_pos && a.cp_get_go && a.obj_destroy;
    LOG("ui init ok=%d tmp=%d", a.ok, a.type_tmp_obj != nullptr);
    return a.ok;
}

void* create_cube()   { init(); return a.go_create_primitive ? a.go_create_primitive(3) : nullptr; }
void* create_sphere() { init(); return a.go_create_primitive ? a.go_create_primitive(0) : nullptr; }

void destroy(void* go) {
    if (go && a.obj_destroy) a.obj_destroy(go);
}

void destroy_collider_of(void* go) {
    if (!go || !a.go_get_component || !a.type_collider_obj || !a.obj_destroy) return;
    void* col = a.go_get_component(go, a.type_collider_obj);
    if (col) a.obj_destroy(col);
}

void* transform_of(void* go) {
    return (go && a.go_get_transform) ? a.go_get_transform(go) : nullptr;
}

void set_position(void* go, unity::Vector3 pos) {
    void* tf = transform_of(go);
    if (tf && a.tf_set_pos) a.tf_set_pos(tf, pos);
}

void set_rotation(void* go, unity::Quaternion rot) {
    void* tf = transform_of(go);
    if (tf && a.tf_set_rot) a.tf_set_rot(tf, rot);
}

void set_local_scale(void* go, unity::Vector3 scale) {
    void* tf = transform_of(go);
    if (tf && a.tf_set_local_scale) a.tf_set_local_scale(tf, scale);
}

void set_local_position(void* go, unity::Vector3 pos) {
    void* tf = transform_of(go);
    if (tf && a.tf_set_local_pos) a.tf_set_local_pos(tf, pos);
}

void set_parent(void* child_go, void* parent_tf) {
    void* tf = transform_of(child_go);
    if (tf && a.tf_set_parent) a.tf_set_parent(tf, parent_tf);
}

void set_parent_transform(void* child_tf, void* parent_tf) {
    if (child_tf && a.tf_set_parent) a.tf_set_parent(child_tf, parent_tf);
}

unity::Vector3 get_position(void* go) {
    unity::Vector3 v{};
    void* tf = transform_of(go);
    if (tf && a.tf_get_pos) a.tf_get_pos(tf, &v);
    return v;
}

void set_color(void* go, unity::Color c) {
    if (!go || !a.go_get_component || !a.type_renderer_obj || !a.rend_get_material || !a.mat_set_color) return;
    void* rend = a.go_get_component(go, a.type_renderer_obj);
    if (!rend) return;
    void* mat = a.rend_get_material(rend);
    if (!mat) return;
    a.mat_set_color(mat, c);
}

void set_label(void* go, const char* text, float font_size, unity::Color color) {
    if (!go || !a.type_tmp_obj || !a.go_add_component || !a.tmp_set_text) return;
    void* tmp = a.go_add_component(go, a.type_tmp_obj);
    if (!tmp) return;
    void* str = il2cpp::string_new(text ? text : "");
    if (str) a.tmp_set_text(tmp, str);
    if (a.tmp_set_font_size) a.tmp_set_font_size(tmp, font_size);
    if (a.tmp_set_color) a.tmp_set_color(tmp, color);
    if (a.tmp_set_align) a.tmp_set_align(tmp, 514 /* Center */);
}

void set_active(void* go, bool on) {
    if (go && a.go_set_active) a.go_set_active(go, on);
}

} // namespace ui
