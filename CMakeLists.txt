cmake_minimum_required(VERSION 3.20)
project(TheWitnessHeadTracking VERSION 0.0.0 LANGUAGES CXX)

# C++20 for designated initializers in the build-offset registry. That registry
# grows by hand every time a patch moves the RVAs, and it is 22 consecutive
# uint32_t fields: named initialisation is what makes the compiler check the
# next profile rather than letting a transposed pair through to the users of
# exactly the build it was added for.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# MinHook is not fetched here. cameraunlock-core vendors it and supplies the
# target from cameraunlock-core/vendor/minhook, which is what keeps a clean
# configure - CI on every push, the release job, a contributor's first build -
# from needing github.com reachable. Core's own guard is `if(NOT TARGET
# minhook)`, so declaring it above this line would silently take the vendored
# copy back out of the build.
set(CAMERAUNLOCK_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CAMERAUNLOCK_BUILD_HOOKS ON CACHE BOOL "" FORCE)
set(CAMERAUNLOCK_BUILD_DISCOVERY ON CACHE BOOL "" FORCE)
add_subdirectory(cameraunlock-core/cpp ${CMAKE_BINARY_DIR}/cameraunlock-core)

# The shipped HeadTracking.ini, compiled into the DLL as a string so the mod can
# lay it down when it is missing. There is one copy of the defaults - this file -
# and it carries its own comments, so a user who ends up without an ini gets the
# documented one rather than a second, generated file that spells its booleans
# 1 and 0 and explains nothing. Regenerated whenever the ini changes.
set(GENERATED_CONFIG_DIR "${CMAKE_BINARY_DIR}/generated")
set(GENERATED_CONFIG_HEADER "${GENERATED_CONFIG_DIR}/default_config.h")
file(MAKE_DIRECTORY "${GENERATED_CONFIG_DIR}")
# file(READ) registers no dependency of its own, so without this the header
# keeps the ini's old text through every rebuild and the DLL seeds a stale
# config. configure_file only tracks its .in template.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${CMAKE_CURRENT_SOURCE_DIR}/HeadTracking.ini")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/HeadTracking.ini" SHIPPED_INI_TEXT)
if(SHIPPED_INI_TEXT MATCHES "TWHT_INI")
    message(FATAL_ERROR "HeadTracking.ini contains the raw-string delimiter TWHT_INI")
endif()
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/default_config.h.in"
    "${GENERATED_CONFIG_HEADER}"
    @ONLY
)

add_library(TheWitnessHeadTracking SHARED
    src/pch.cpp
    src/dllmain.cpp
    src/openvr_proxy.cpp
    src/core/mod.cpp
    src/core/config.cpp
    src/core/debug_log.cpp
    src/core/early_diag.cpp
    src/core/window_centering.cpp
    src/input/hotkey_handler.cpp
    src/camera/camera_hook.cpp
    src/camera/pose_conversion.cpp
    src/camera/reticle_projection.cpp
    src/game/build_profile.cpp
    src/game/steam_offsets.cpp
    src/game/engine.cpp
    src/game/world_trace.cpp
)

target_precompile_headers(TheWitnessHeadTracking PRIVATE src/pch.h)

# Ship as openvr_api.dll - The Witness loads it natively on startup.
# Export forwarding for the original VR API is handled at runtime via
# LoadLibrary("openvr_api.dll.backup") + GetProcAddress in openvr_proxy.cpp.
set_target_properties(TheWitnessHeadTracking PROPERTIES
    OUTPUT_NAME "openvr_api"
    PREFIX ""
    SUFFIX ".dll"
    RUNTIME_OUTPUT_DIRECTORY         "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_SOURCE_DIR}/bin/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin/Release"
)

target_link_libraries(TheWitnessHeadTracking PRIVATE
    cameraunlock
    cameraunlock_hooks
    minhook
    ws2_32
    Psapi
)

target_include_directories(TheWitnessHeadTracking PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${GENERATED_CONFIG_DIR}
)

if(MSVC)
    target_compile_options(TheWitnessHeadTracking PRIVATE /W4 /WX /MP /wd4324 /EHa)
endif()

# The boundary code - the tracker-to-engine signs, the horizon-locked lean
# basis, the zoom factor, the reticle solve and the ini parse - built again
# without the hooks so it can be exercised off a running game. Those signs and
# that composition order were each measured against the drawn reticle once; the
# tests are what stops them being "tidied" back into a coin flip. The config
# half is here for the same reason in reverse: what a user can type is not
# discoverable by reading the code, so every hazard has a case.
option(THEWITNESS_BUILD_TESTS "Build the pure-maths characterization tests" ON)

if(THEWITNESS_BUILD_TESTS)
    enable_testing()
    add_executable(the_witness_tests
        tests/test_main.cpp
        tests/pose_conversion_tests.cpp
        tests/reticle_projection_tests.cpp
        tests/config_tests.cpp
        src/camera/pose_conversion.cpp
        src/camera/reticle_projection.cpp
        src/core/config.cpp
    )
    target_include_directories(the_witness_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${GENERATED_CONFIG_DIR}
    )
    # The ini the packager ships is the only source of the defaults, so it is
    # read directly: a key the guards would correct is a file that warns on
    # every user's launch, and a value that drifted from the built-in default
    # would be a setting the file claims to offer and does not.
    target_compile_definitions(the_witness_tests PRIVATE
        THEWITNESS_SHIPPED_INI="${CMAKE_CURRENT_SOURCE_DIR}/HeadTracking.ini"
        THEWITNESS_README="${CMAKE_CURRENT_SOURCE_DIR}/README.md"
    )
    target_link_libraries(the_witness_tests PRIVATE cameraunlock)
    if(MSVC)
        target_compile_options(the_witness_tests PRIVATE /W4 /WX /MP /wd4324 /EHa)
    endif()
    add_test(NAME the_witness_tests COMMAND the_witness_tests)
endif()
