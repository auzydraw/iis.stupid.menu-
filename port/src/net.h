#pragma once
#include "il2cpp.h"

namespace net {

// One-shot resolver. Walks every Photon-adjacent assembly looking for
// PhotonNetwork and PhotonView classes. Safe to call before the room joins.
bool init();

bool in_room();
bool is_master();

// Local Photon.Realtime.Player instance (owned by runtime).
void* local_player();

// Resolve a VRRig* to its Photon.Realtime.Player* by walking
// VRRig -> GameObject -> PhotonView -> get_Owner.
void* rig_to_player(void* vrrig);

// Fire PhotonNetwork.SetMasterClient(Player). Returns true on dispatch.
bool set_master(void* player);

} // namespace net
