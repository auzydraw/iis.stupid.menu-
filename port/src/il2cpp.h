// IL2CPP native API wrappers.
// Resolves symbols from libil2cpp.so once loaded, exposes typed helpers.
#pragma once
#include <cstdint>
#include <cstddef>

namespace il2cpp {

using Il2CppDomain = void;
using Il2CppImage = void;
using Il2CppAssembly = void;
using Il2CppClass = void;
using Il2CppObject = void;
using Il2CppString = void;
using Il2CppArray = void;
using Il2CppType = void;
using MethodInfo = void;
using FieldInfo = void;

// Must be called once libil2cpp.so is loaded (il2cpp_init has run or been hooked).
// Returns true on success; false means libil2cpp.so is not loaded yet.
bool init();
bool ready();

void attach_current_thread();
void detach_current_thread();

Il2CppDomain* domain_get();
Il2CppAssembly* domain_assembly_open(Il2CppDomain* d, const char* name);
const Il2CppImage* assembly_get_image(const Il2CppAssembly* a);
Il2CppAssembly** domain_get_assemblies(const Il2CppDomain* d, size_t* size);

Il2CppClass* class_from_name(const Il2CppImage* img, const char* ns, const char* name);
const MethodInfo* class_get_method_from_name(Il2CppClass* k, const char* name, int argc);
FieldInfo* class_get_field_from_name(Il2CppClass* k, const char* name);
Il2CppClass* class_from_system_type(const Il2CppType* t);
const char* class_get_name(Il2CppClass* k);

// Iterate every method on a class. Pass nullptr in to begin, reuse the returned iter.
const MethodInfo* class_get_methods(Il2CppClass* k, void** iter);

// Method reflection.
const char* method_get_name(const MethodInfo* m);
int32_t     method_get_param_count(const MethodInfo* m);
const Il2CppType* method_get_param(const MethodInfo* m, uint32_t index);
const char* type_get_name(const Il2CppType* t); // caller-owned; free()

// Walk every 2-arg overload of `method_name` on class and pick the one whose
// Nth parameter type name ends with `suffix` (e.g. "OVRInput/Button").
const MethodInfo* find_method_by_param_suffix(
    Il2CppClass* k, const char* method_name, int argc, int param_index, const char* suffix);

void field_static_get_value(FieldInfo* f, void* out);
void field_set_value(Il2CppObject* obj, FieldInfo* f, void* value);
void field_get_value(Il2CppObject* obj, FieldInfo* f, void* out);
int32_t field_get_offset(FieldInfo* f);

Il2CppObject* runtime_invoke(const MethodInfo* m, void* instance, void** args, Il2CppObject** exc);
void* method_get_pointer(const MethodInfo* m);

Il2CppString* string_new(const char* s);
const char* string_chars_utf8(Il2CppString* s); // allocates; free with free()

Il2CppImage* find_image(const char* name); // e.g. "Assembly-CSharp.dll"

// Convenience: full lookup by (image, namespace, class, method, argc).
const MethodInfo* find_method(const char* image, const char* ns, const char* cls, const char* method, int argc);

// Get a raw function pointer to a managed method via runtime_invoke pointer.
template <typename F>
inline F method_as(const MethodInfo* m) {
    return reinterpret_cast<F>(method_get_pointer(m));
}

} // namespace il2cpp
