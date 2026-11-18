#include "il2cpp.h"
#include <dlfcn.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <mutex>
#include <android/log.h>

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "iistupid/il2cpp", __VA_ARGS__)

namespace il2cpp {

namespace {

using fn_init = int (*)(const char*);
using fn_domain_get = Il2CppDomain*(*)();
using fn_domain_assembly_open = Il2CppAssembly*(*)(Il2CppDomain*, const char*);
using fn_domain_get_assemblies = Il2CppAssembly**(*)(const Il2CppDomain*, size_t*);
using fn_assembly_get_image = const Il2CppImage*(*)(const Il2CppAssembly*);
using fn_class_from_name = Il2CppClass*(*)(const Il2CppImage*, const char*, const char*);
using fn_class_get_method_from_name = const MethodInfo*(*)(Il2CppClass*, const char*, int);
using fn_class_get_field_from_name = FieldInfo*(*)(Il2CppClass*, const char*);
using fn_class_from_system_type = Il2CppClass*(*)(const Il2CppType*);
using fn_class_get_name = const char*(*)(Il2CppClass*);
using fn_field_static_get_value = void (*)(FieldInfo*, void*);
using fn_field_set_value = void (*)(Il2CppObject*, FieldInfo*, void*);
using fn_field_get_value = void (*)(Il2CppObject*, FieldInfo*, void*);
using fn_field_get_offset = int32_t (*)(FieldInfo*);
using fn_runtime_invoke = Il2CppObject*(*)(const MethodInfo*, void*, void**, Il2CppObject**);
using fn_method_get_pointer = void*(*)(const MethodInfo*);
using fn_string_new = Il2CppString*(*)(const char*);
using fn_string_chars = const uint16_t*(*)(const Il2CppString*);
using fn_string_length = int32_t (*)(const Il2CppString*);
using fn_image_get_name = const char*(*)(const Il2CppImage*);
using fn_thread_attach = void*(*)(Il2CppDomain*);
using fn_thread_detach = void (*)(void*);

struct Api {
    fn_domain_get domain_get{};
    fn_domain_assembly_open domain_assembly_open{};
    fn_domain_get_assemblies domain_get_assemblies{};
    fn_assembly_get_image assembly_get_image{};
    fn_class_from_name class_from_name{};
    fn_class_get_method_from_name class_get_method_from_name{};
    fn_class_get_field_from_name class_get_field_from_name{};
    fn_class_from_system_type class_from_system_type{};
    fn_class_get_name class_get_name{};
    fn_field_static_get_value field_static_get_value{};
    fn_field_set_value field_set_value{};
    fn_field_get_value field_get_value{};
    fn_field_get_offset field_get_offset{};
    fn_runtime_invoke runtime_invoke{};
    fn_method_get_pointer method_get_pointer{};
    fn_string_new string_new{};
    fn_string_chars string_chars{};
    fn_string_length string_length{};
    fn_image_get_name image_get_name{};
    fn_thread_attach thread_attach{};
    fn_thread_detach thread_detach{};
    void* handle{};
    bool ok{};
};

Api g;
std::once_flag g_once;

template <typename T>
bool resolve(T& out, const char* name) {
    out = reinterpret_cast<T>(dlsym(g.handle, name));
    if (!out) { LOG("missing symbol: %s", name); return false; }
    return true;
}

} // namespace

bool init() {
    std::call_once(g_once, [] {
        g.handle = dlopen("libil2cpp.so", RTLD_NOW | RTLD_GLOBAL);
        if (!g.handle) { LOG("libil2cpp.so not loaded yet"); return; }

        bool ok = true;
        ok &= resolve(g.domain_get,                "il2cpp_domain_get");
        ok &= resolve(g.domain_assembly_open,      "il2cpp_domain_assembly_open");
        ok &= resolve(g.domain_get_assemblies,     "il2cpp_domain_get_assemblies");
        ok &= resolve(g.assembly_get_image,        "il2cpp_assembly_get_image");
        ok &= resolve(g.class_from_name,           "il2cpp_class_from_name");
        ok &= resolve(g.class_get_method_from_name,"il2cpp_class_get_method_from_name");
        ok &= resolve(g.class_get_field_from_name, "il2cpp_class_get_field_from_name");
        ok &= resolve(g.class_from_system_type,    "il2cpp_class_from_system_type");
        ok &= resolve(g.class_get_name,            "il2cpp_class_get_name");
        ok &= resolve(g.field_static_get_value,    "il2cpp_field_static_get_value");
        ok &= resolve(g.field_set_value,           "il2cpp_field_set_value");
        ok &= resolve(g.field_get_value,           "il2cpp_field_get_value");
        ok &= resolve(g.field_get_offset,          "il2cpp_field_get_offset");
        ok &= resolve(g.runtime_invoke,            "il2cpp_runtime_invoke");
        ok &= resolve(g.method_get_pointer,        "il2cpp_method_get_pointer");
        ok &= resolve(g.string_new,                "il2cpp_string_new");
        ok &= resolve(g.string_chars,              "il2cpp_string_chars");
        ok &= resolve(g.string_length,             "il2cpp_string_length");
        ok &= resolve(g.image_get_name,            "il2cpp_image_get_name");
        ok &= resolve(g.thread_attach,             "il2cpp_thread_attach");
        ok &= resolve(g.thread_detach,             "il2cpp_thread_detach");
        g.ok = ok;
        LOG("il2cpp api resolved: %s", ok ? "ok" : "fail");
    });
    return g.ok;
}

bool ready() { return g.ok; }

void attach_current_thread() { if (g.ok) g.thread_attach(g.domain_get()); }
void detach_current_thread() { if (g.ok) g.thread_detach(nullptr); }

Il2CppDomain* domain_get() { return g.domain_get(); }
Il2CppAssembly* domain_assembly_open(Il2CppDomain* d, const char* name) { return g.domain_assembly_open(d, name); }
const Il2CppImage* assembly_get_image(const Il2CppAssembly* a) { return g.assembly_get_image(a); }
Il2CppAssembly** domain_get_assemblies(const Il2CppDomain* d, size_t* size) { return g.domain_get_assemblies(d, size); }
Il2CppClass* class_from_name(const Il2CppImage* img, const char* ns, const char* name) { return g.class_from_name(img, ns, name); }
const MethodInfo* class_get_method_from_name(Il2CppClass* k, const char* name, int argc) { return g.class_get_method_from_name(k, name, argc); }
FieldInfo* class_get_field_from_name(Il2CppClass* k, const char* name) { return g.class_get_field_from_name(k, name); }
Il2CppClass* class_from_system_type(const Il2CppType* t) { return g.class_from_system_type(t); }
const char* class_get_name(Il2CppClass* k) { return g.class_get_name(k); }

void field_static_get_value(FieldInfo* f, void* out) { g.field_static_get_value(f, out); }
void field_set_value(Il2CppObject* obj, FieldInfo* f, void* value) { g.field_set_value(obj, f, value); }
void field_get_value(Il2CppObject* obj, FieldInfo* f, void* out) { g.field_get_value(obj, f, out); }
int32_t field_get_offset(FieldInfo* f) { return g.field_get_offset(f); }

Il2CppObject* runtime_invoke(const MethodInfo* m, void* instance, void** args, Il2CppObject** exc) {
    return g.runtime_invoke(m, instance, args, exc);
}
void* method_get_pointer(const MethodInfo* m) { return g.method_get_pointer(m); }

Il2CppString* string_new(const char* s) { return g.string_new(s); }

const char* string_chars_utf8(Il2CppString* s) {
    if (!s) return nullptr;
    int32_t len = g.string_length(s);
    const uint16_t* w = g.string_chars(s);
    char* out = static_cast<char*>(malloc(len * 4 + 1));
    int o = 0;
    for (int i = 0; i < len; ++i) {
        uint32_t c = w[i];
        if (c < 0x80) out[o++] = (char)c;
        else if (c < 0x800) {
            out[o++] = (char)(0xC0 | (c >> 6));
            out[o++] = (char)(0x80 | (c & 0x3F));
        } else {
            out[o++] = (char)(0xE0 | (c >> 12));
            out[o++] = (char)(0x80 | ((c >> 6) & 0x3F));
            out[o++] = (char)(0x80 | (c & 0x3F));
        }
    }
    out[o] = 0;
    return out;
}

Il2CppImage* find_image(const char* name) {
    size_t n = 0;
    auto** asms = g.domain_get_assemblies(g.domain_get(), &n);
    for (size_t i = 0; i < n; ++i) {
        const Il2CppImage* img = g.assembly_get_image(asms[i]);
        if (!img) continue;
        const char* in = g.image_get_name(img);
        if (in && strcmp(in, name) == 0) return const_cast<Il2CppImage*>(img);
    }
    return nullptr;
}

const MethodInfo* find_method(const char* image, const char* ns, const char* cls, const char* method, int argc) {
    Il2CppImage* img = find_image(image);
    if (!img) return nullptr;
    Il2CppClass* k = g.class_from_name(img, ns, cls);
    if (!k) return nullptr;
    return g.class_get_method_from_name(k, method, argc);
}

} // namespace il2cpp
