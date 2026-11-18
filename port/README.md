# ii's Stupid Menu — Native Android Port (libiistupid.so)

A `.so` port of a core slice of the PC mod to Quest / Android IL2CPP
Gorilla Tag. The PC build is a BepInEx (Mono) plugin; this is a native
C++ library built with the Android NDK that hooks IL2CPP methods via
Dobby.

## What's ported

| Category | Mod                 | Notes                                                       |
| -------- | ------------------- | ----------------------------------------------------------- |
| Movement | Fly                 | A to fly forward                                            |
| Movement | Trigger Fly         | Right trigger to fly forward                                |
| Movement | Noclip Fly          | A to fly, marks noclip state                                |
| Movement | Joystick Fly        | Left stick to fly relative to head                          |
| Movement | Hand Fly            | A to fly along right-hand forward                           |
| Movement | Bark Fly            | 6-DOF joystick fly with gravity cancel                      |
| Movement | Speed Boost         | Amplifies left-stick walk                                   |
| Movement | Long Arms           | Overwrites `GTPlayer.armLengthMultiplier`                   |
| Movement | Platforms           | Lifts body above hand position while trigger held           |
| Movement | Zero / Low / High Gravity | AddForce every frame                                  |
| Movement | High Jump           | A edge → big upward kick                                    |
| Fun      | Head Spin           | Scaffold — needs VRRig head offset resolve                  |
| Fun      | Tag Seek            | Flies toward nearest non-local rig                          |
| Fun      | Rapid Jump          | Hold A → continuous upward force                            |
| Fun      | Freeze              | Snaps body to captured position                             |
| Visuals  | Player Dot          | Logs rig pointers / positions to logcat                     |
| Visuals  | Third Person Push   | Pushes body opposite head forward                           |

The PC source ships ~1000 mods across 43k LOC of C#. This port covers
the controller-only, non-networked core. Everything that touches Photon
RPC, Harmony IL patches, Unity UI prefabs, or the resource pack is left
for follow-up work — the registry, il2cpp API wrapper, and rig helpers
are designed to accept it incrementally.

## Build

Requirements:
- Android NDK r25 or newer (set `NDK_HOME` or `ANDROID_NDK_HOME`)
- CMake 3.22+
- Ninja

```bash
cd port
./build.sh                       # Release, arm64-v8a
ABI=arm64-v8a ./build.sh Debug   # Debug variant
```

Output: `port/out/arm64-v8a/libiistupid.so`

CMake fetches Dobby from GitHub on first run.

## Inject

The standard GT Quest injection paths all work:
- **Rename-and-load**: rename `libiistupid.so` to something the APK
  already loads (for a modded APK, place in `lib/arm64-v8a/`).
- **lsplant / lsposed**: load the `.so` into the Gorilla Tag process
  before `libil2cpp.so`.
- **Zygisk module**: wrap in a Zygisk shim that calls `dlopen` on this
  `.so` from the package specializer.

`JNI_OnLoad` spawns a watcher thread; it waits for `libil2cpp.so` to
appear in `/proc/self/maps`, then installs the tick hook.

## Controls

The menu is gesture-driven (no in-world UI — add that later):

| Action                        | Combo                        |
| ----------------------------- | ---------------------------- |
| Open / close menu             | Left Grip + press B          |
| Highlight previous mod        | Left stick up                |
| Highlight next mod            | Left stick down              |
| Toggle highlighted mod        | Press A                      |
| Save enabled state to config  | Press X                      |

Config persists to `/sdcard/iiMenu/config.ini` and reloads on launch.
Logcat tag: `iistupid*`.

## Adding a mod

1. In a `mods/*.cpp`, write a free function:
   ```cpp
   void my_mod_tick() { rig::translate({0, 0.1f, 0}); }
   ```
2. In `register_all()`:
   ```cpp
   registry::add("My Mod", "Movement", my_mod_tick);
   ```
3. Rebuild. The menu will list it automatically.

For state that resets on toggle, pass a `toggled(bool)` callback as the
fourth argument to `registry::add`.

## IL2CPP offset notes

GT's IL2CPP metadata rotates with every update. The mods call into the
runtime by name (`class_from_name`, `class_get_field_from_name`), so no
hard-coded offsets ship in this build. If a field rename happens, update
the string literal in `rig.cpp` or the relevant mod and rebuild.

## License

GPL-3.0, matching the upstream mod. All ported logic retains the
upstream copyright line.
