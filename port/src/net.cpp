#include "net.h"
#include "il2cpp.h"
#include <android/log.h>
#include <cstring>
#include <dlfcn.h>

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "iistupid/net", __VA_ARGS__)

namespace net {

namespace {

struct Api {
    bool resolved = false;

    // PhotonNetwork statics.
    bool  (*get_in_room)()         = nullptr;
    bool  (*get_is_master)()       = nullptr;
    void* (*get_local_player)()    = nullptr;
    void  (*set_master_client)(void*) = nullptr;

    // PhotonView.get_Owner()
    void* (*pv_get_owner)(void*) = nullptr;
    il2cpp::Il2CppClass* photon_view_class = nullptr;

    // UnityEngine helpers for VRRig -> GameObject -> PhotonView.
    void* (*comp_get_go)(void*)                         = nullptr;
    void* (*go_get_component_byname)(void*, void*)      = nullptr;
    void* (*type_from_class)(il2cpp::Il2CppClass*)      = nullptr;

    // Cache the System.Type for PhotonView (reused per call).
    void* photon_view_type = nullptr;
};

Api a;

const char* kPhotonImages[] = {
    "PhotonUnityNetworking.dll",
    "Photon.Pun.dll",
    "Photon3Unity3D.dll",
    "Assembly-CSharp.dll",
    nullptr,
};

il2cpp::Il2CppClass* find_class_in_photon(const char* ns, const char* name) {
    using namespace il2cpp;
    for (int i = 0; kPhotonImages[i]; ++i) {
        Il2CppImage* img = find_image(kPhotonImages[i]);
        if (!img) continue;
        if (Il2CppClass* k = class_from_name(img, ns, name)) return k;
    }
    return nullptr;
}

} // namespace

bool init() {
    if (a.resolved) return a.get_in_room != nullptr;
    a.resolved = true;

    using namespace il2cpp;

    Il2CppClass* pn = find_class_in_photon("Photon.Pun", "PhotonNetwork");
    if (!pn) { LOG("PhotonNetwork class not found"); return false; }

    a.get_in_room      = (bool(*)())method_get_pointer(class_get_method_from_name(pn, "get_InRoom", 0));
    a.get_is_master    = (bool(*)())method_get_pointer(class_get_method_from_name(pn, "get_IsMasterClient", 0));
    a.get_local_player = (void*(*)())method_get_pointer(class_get_method_from_name(pn, "get_LocalPlayer", 0));
    a.set_master_client= (void(*)(void*))method_get_pointer(class_get_method_from_name(pn, "SetMasterClient", 1));

    a.photon_view_class = find_class_in_photon("Photon.Pun", "PhotonView");
    if (a.photon_view_class) {
        a.pv_get_owner = (void*(*)(void*))method_get_pointer(
            class_get_method_from_name(a.photon_view_class, "get_Owner", 0));
    }

    Il2CppImage* core = find_image("UnityEngine.CoreModule.dll");
    if (core) {
        if (Il2CppClass* cp = class_from_name(core, "UnityEngine", "Component"))
            a.comp_get_go = (void*(*)(void*))method_get_pointer(class_get_method_from_name(cp, "get_gameObject", 0));
        if (Il2CppClass* go = class_from_name(core, "UnityEngine", "GameObject"))
            a.go_get_component_byname = (void*(*)(void*, void*))method_get_pointer(
                class_get_method_from_name(go, "GetComponent", 1));
        if (Il2CppClass* tp = class_from_name(core, "UnityEngine", "Type"))
            a.type_from_class = (void*(*)(Il2CppClass*))method_get_pointer(
                class_get_method_from_name(tp, "internal_from_handle", 1));
    }

    LOG("net resolve: InRoom=%p IsMaster=%p Local=%p SetMaster=%p PhotonView=%p GetOwner=%p",
        a.get_in_room, a.get_is_master, a.get_local_player,
        a.set_master_client, a.photon_view_class, a.pv_get_owner);

    return a.get_in_room != nullptr;
}

bool in_room()   { if (!init() || !a.get_in_room)   return false; return a.get_in_room(); }
bool is_master() { if (!init() || !a.get_is_master) return false; return a.get_is_master(); }

void* local_player() {
    if (!init() || !a.get_local_player) return nullptr;
    return a.get_local_player();
}

void* rig_to_player(void* vrrig) {
    if (!vrrig || !init()) return nullptr;
    if (!a.comp_get_go || !a.go_get_component_byname || !a.photon_view_class || !a.pv_get_owner)
        return nullptr;
    void* go = a.comp_get_go(vrrig);
    if (!go) return nullptr;
    // Build a System.Type for PhotonView to pass to GetComponent(Type).
    // il2cpp exposes il2cpp_type_get_object(Il2CppType*) for this.
    using namespace il2cpp;
    static void* (*type_get_object)(const Il2CppType*) = nullptr;
    if (!type_get_object) {
        void* lib = dlopen("libil2cpp.so", RTLD_NOW | RTLD_NOLOAD);
        if (lib) type_get_object = (decltype(type_get_object))dlsym(lib, "il2cpp_type_get_object");
    }
    static void* cached_type = nullptr;
    if (!cached_type && type_get_object) {
        static const Il2CppType* (*class_get_type)(Il2CppClass*) = nullptr;
        if (!class_get_type) {
            void* lib = dlopen("libil2cpp.so", RTLD_NOW | RTLD_NOLOAD);
            if (lib) class_get_type = (decltype(class_get_type))dlsym(lib, "il2cpp_class_get_type");
        }
        if (class_get_type)
            cached_type = type_get_object(class_get_type(a.photon_view_class));
    }
    if (!cached_type) return nullptr;
    void* pv = a.go_get_component_byname(go, cached_type);
    if (!pv) return nullptr;
    return a.pv_get_owner(pv);
}

bool set_master(void* player) {
    if (!player || !init() || !a.set_master_client) return false;
    a.set_master_client(player);
    return true;
}

} // namespace net
