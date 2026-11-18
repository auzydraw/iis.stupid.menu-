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
    dobby
    GIT_REPOSITORY https://github.com/jmpews/Dobby.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)
set(DOBBY_GENERATE_SHARED OFF CACHE BOOL "" FORCE)
set(DOBBY_DEBUG OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(dobby)

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
    src/mods/fun.cpp
    src/mods/visuals.cpp
)

add_library(iistupid SHARED ${SRC})
target_include_directories(iistupid PRIVATE src ${dobby_SOURCE_DIR}/include)
target_link_libraries(iistupid PRIVATE dobby_static log android)
set_target_properties(iistupid PROPERTIES OUTPUT_NAME "iistupid")
