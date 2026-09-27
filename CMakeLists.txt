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
    src/legacy_config/legacy_config.cpp
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
        src/legacy_config/legacy_config.cpp
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

    # The canonical config conversion's differential test: the dev build
    # a18847d's reader and startup code (the oracle, compiled from byte copies
    # in tests/config_differential/oracle) against the frozen reader in
    # src/legacy_config/. The oracle is its own library so its includes resolve
    # to its copies and never to src/.
    set(TWHT_DIFFERENTIAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests/config_differential)
    set(TWHT_ORACLE_GENERATED_DIR "${CMAKE_BINARY_DIR}/oracle-generated")
    file(MAKE_DIRECTORY "${TWHT_ORACLE_GENERATED_DIR}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                 "${TWHT_DIFFERENTIAL_DIR}/data/headtracking-a18847d.ini")
    file(READ "${TWHT_DIFFERENTIAL_DIR}/data/headtracking-a18847d.ini" SHIPPED_INI_TEXT)
    configure_file("${TWHT_DIFFERENTIAL_DIR}/oracle/cmake/default_config.h.in"
                   "${TWHT_ORACLE_GENERATED_DIR}/default_config.h" @ONLY)
    add_library(twht_oracle STATIC ${TWHT_DIFFERENTIAL_DIR}/oracle/oracle_reader.cpp)
    target_include_directories(twht_oracle PRIVATE
        ${TWHT_DIFFERENTIAL_DIR}/oracle/src
        ${TWHT_ORACLE_GENERATED_DIR}
    )
    target_link_libraries(twht_oracle PRIVATE cameraunlock)

    add_executable(twht_config_differential
        ${TWHT_DIFFERENTIAL_DIR}/differential_tests.cpp
        src/legacy_config/legacy_config.cpp
    )
    target_include_directories(twht_config_differential PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${TWHT_DIFFERENTIAL_DIR}
    )
    target_compile_definitions(twht_config_differential PRIVATE
        TWHT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(twht_config_differential PRIVATE twht_oracle cameraunlock)
    if(MSVC)
        target_compile_options(twht_oracle PRIVATE /W4 /WX /MP /wd4324 /EHa)
        target_compile_options(twht_config_differential PRIVATE /W4 /WX /MP /wd4324 /EHa /bigobj)
    endif()
    add_test(NAME config_differential COMMAND twht_config_differential)

    # What the differential test compiles is pinned by hash, line endings aside:
    # the oracle's byte copies must stay a18847d's (git show a18847d:<path>), the
    # core sources both readers compile must stay the ones a18847d was built with
    # (core c480d8a), and the frozen reader is never edited.
    function(twht_require_hash path expected)
        file(READ "${path}" text)
        string(REPLACE "\r" "" text "${text}")
        string(SHA256 actual "${text}")
        if(NOT actual STREQUAL expected)
            message(FATAL_ERROR "${path} has changed (SHA-256 of its text ${actual}, pinned "
                                "${expected}). The config differential test compares against "
                                "exactly this file; see CMakeLists.txt.")
        endif()
    endfunction()
    set(TWHT_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cameraunlock-core/cpp)
    foreach(pinned
            "${TWHT_DIFFERENTIAL_DIR}/oracle/src/core/config.cpp|f7b1b9f4ef49389ba2b82357496bebde4cdc427465d9bda6486d97fced24110a"
            "${TWHT_DIFFERENTIAL_DIR}/oracle/src/core/config.h|d908976938d2269eb08f2eb3267d9c97a37328dc6aaaa2a3710ac53dd71d3847"
            "${TWHT_DIFFERENTIAL_DIR}/oracle/src/core/constants.h|76eb71f2862422c59535ffca62b82b8443106c6b84545878bce48a940c92c735"
            "${TWHT_DIFFERENTIAL_DIR}/oracle/src/pch.h|0602ee283b91d6d1d57f5e15f7ebee567929ec74d80dceb54be057a4c6c261ba"
            "${TWHT_DIFFERENTIAL_DIR}/oracle/cmake/default_config.h.in|ed0913f5257f38602fa2a5197e2979fc518ab3462a1e608da600d4f16085318c"
            "${TWHT_CORE_DIR}/include/cameraunlock/config/ini_reader.h|a7ffb44210ff59672fa97e8e5feaa2cb3e81938fcc0334a384c68bc371b3857a"
            "${TWHT_CORE_DIR}/src/config/ini_reader.cpp|e01515c2656aaf533bae4350dc45b702c3e3d4043935743dcc9bd5ea581fbe1c"
            "${TWHT_CORE_DIR}/include/cameraunlock/config/value_guards.h|6d3e3512bdfc9ac2a54bee750c5425191bdb4c24f3ae2ea3b9925612e01c78e5"
            "${TWHT_CORE_DIR}/src/config/value_guards.cpp|a833ff7f2721f7974ce039a3597349a0974e6695ce3e75e598ce5bf4985a14b1"
            "${TWHT_CORE_DIR}/include/cameraunlock/math/finite_utils.h|c59772d698d54ade3374ee1221b74f5563a86d76f0eb34a989ef7efab389c0ad"
            "${TWHT_CORE_DIR}/include/cameraunlock/protocol/port_utils.h|91bff564d5e279b66527ec5553afcf4d591812dab0e78f71a48ef412e4db7a44"
            "${TWHT_CORE_DIR}/include/cameraunlock/math/smoothing_utils.h|fc2146f8c585e5f610c7234e302f59de4945679cfa28ff479ca47477ec073f22"
            "${TWHT_CORE_DIR}/include/cameraunlock/math/angle_utils.h|d7a905270933e3cb0c4c361d29d3fd701655498cbcd1875ea79d180468bdbe6a"
            "${TWHT_CORE_DIR}/include/cameraunlock/data/position_settings.h|b24dceb8e25475aebc5a468a5c7362a4a4e64204d183d1408525345f32f547f5"
            "${TWHT_CORE_DIR}/include/cameraunlock/camera/lean_clamp.h|a53cf2d4c0060643d2cec5308f0f979bf5868c4f892a62c9bbe4572c427c2a8b"
            "${TWHT_CORE_DIR}/include/cameraunlock/math/vec3.h|021c45902da44d12911a7cacd9b76fa28775b382583dca2e5e9df3888e242683"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/legacy_config/legacy_config.h|ddd605b648c954208674180709a4b5e6c5dd483c8e52a073c7faa1941033792f"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/legacy_config/legacy_config.cpp|93f36cf88d2ffb053e92f1dc134e9a7379bfa42207a5344265e89ff058662da4")
        string(REPLACE "|" ";" pair "${pinned}")
        list(GET pair 0 pinned_path)
        list(GET pair 1 pinned_hash)
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${pinned_path}")
        twht_require_hash("${pinned_path}" "${pinned_hash}")
    endforeach()
endif()
