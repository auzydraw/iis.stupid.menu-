cmake_minimum_required(VERSION 3.22)
project(iistupid LANGUAGES C CXX ASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

add_compile_options(
    -Wall -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter
    -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti
    -fvisibility=hidden -fvisibility-inlines-hidden
)
add_link_options(
    -Wl,--gc-sections -Wl,--exclude-libs,ALL -Wl,--build-id=none
    -Wl,-z,max-page-size=16384
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O3 -flto)
    add_link_options(-flto -s)
endif()

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
FetchContent_Declare(
    shadowhook
    GIT_REPOSITORY https://github.com/bytedance/android-inline-hook.git
    GIT_TAG v1.1.1
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(shadowhook)
if(NOT shadowhook_POPULATED)
    FetchContent_Populate(shadowhook)
endif()

set(SH_ROOT ${shadowhook_SOURCE_DIR}/shadowhook/src/main/cpp)
if(${ANDROID_ABI} STREQUAL "arm64-v8a")
    set(SH_ARCH arm64)
elseif(${ANDROID_ABI} STREQUAL "armeabi-v7a")
    set(SH_ARCH arm)
endif()
file(GLOB SH_SRC
    ${SH_ROOT}/*.c
    ${SH_ROOT}/arch/${SH_ARCH}/*.c
    ${SH_ROOT}/common/*.c
    ${SH_ROOT}/third_party/*/*.c)
# Exclude the JNI shim; we don't expose shadowhook through Java.
list(FILTER SH_SRC EXCLUDE REGEX "sh_jni\\.c$")
add_library(shadowhook_static STATIC ${SH_SRC})
target_include_directories(shadowhook_static PUBLIC
    ${SH_ROOT} ${SH_ROOT}/include ${SH_ROOT}/arch/${SH_ARCH}
    ${SH_ROOT}/common ${SH_ROOT}/third_party/xdl
    ${SH_ROOT}/third_party/bsd ${SH_ROOT}/third_party/lss)
target_compile_options(shadowhook_static PRIVATE
    -std=c17 -O2 -ffunction-sections -fdata-sections
    -Wno-everything)
target_link_libraries(shadowhook_static PUBLIC log)

set(SRC
    src/main.cpp
    src/il2cpp.cpp
    src/hook.cpp
    src/input.cpp
    src/rig.cpp
    src/state.cpp
    src/registry.cpp
    src/menu.cpp
    src/mods/movement.cpp
    src/mods/advantages.cpp
    src/mods/visuals.cpp
    src/mods/fun.cpp
    src/mods/overpowered.cpp
    src/mods/safety.cpp
)

add_library(iistupid SHARED ${SRC})
target_include_directories(iistupid PRIVATE src)
target_link_libraries(iistupid PRIVATE shadowhook_static log android)
set_target_properties(iistupid PROPERTIES OUTPUT_NAME "iistupid")
