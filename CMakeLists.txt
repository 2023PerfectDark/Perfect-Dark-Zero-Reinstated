# perfectdarkzerorecomp - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(perfectdarkzerorecomp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# Fix git symlinks on Windows (libmspack in SDK uses symlinks)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/fix_symlinks.cmake)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/build_optimizations.cmake)

# Build ReXGlue SDK from source (git submodule)
set(REXSDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/rexglue-sdk" CACHE PATH "Path to rexglue-sdk source tree" FORCE)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/fix-fpscr-exceptions.cmake)

# Enable both graphics backends
if(WIN32)
    set(REXGLUE_USE_D3D12 ON CACHE BOOL "Enable D3D12 graphics backend")
endif()
set(REXGLUE_USE_VULKAN ON CACHE BOOL "Enable Vulkan graphics backend")


include(generated/rexglue.cmake)

set(PERFECTDARKZERORECOMP_SOURCES
    src/main.cpp
    src/game_patches.cpp
    src/physics_timing.cpp
    src/cutscene_timing.cpp
    src/blood_effects.cpp
    src/campaign_gore.cpp
    src/model_render_guard.cpp
    src/blood_decals.cpp
    src/visible_holsters.cpp
    src/local_profiles.cpp
    src/local_multiplayer.cpp
    src/surface_runtime.cpp
    src/game_cvars.cpp
    src/mousehook.cpp
    src/native_pointer.cpp
    src/campaign_rules.cpp
    src/weapon_rules.cpp
    src/weapon_challenge.cpp
    src/mission_weapons.cpp
    src/klobb_weapon.cpp
    src/klobb_ui.cpp
    src/coop_ally.cpp
    src/campaign_portraits.cpp
    src/native_campaign.cpp
    src/mp_characters.cpp
    src/update_service.cpp
    src/dub_language.cpp
    src/render/gpu_wait_hook.cpp
    src/mod/third_person.cpp
)

if(WIN32)
    list(APPEND PERFECTDARKZERORECOMP_SOURCES
        src/perfectdarkzerorecomp.rc
    )
endif()

if(WIN32)
    add_executable(perfectdarkzerorecomp WIN32 ${PERFECTDARKZERORECOMP_SOURCES})
else()
    add_executable(perfectdarkzerorecomp ${PERFECTDARKZERORECOMP_SOURCES})
endif()

rexglue_setup_target(perfectdarkzerorecomp GPU_PLUGINS xenos)

# Link against rexui for UI support (imgui, etc.)
target_link_libraries(perfectdarkzerorecomp PRIVATE rex::ui)

set(pdz_cutscene_input "${CMAKE_CURRENT_SOURCE_DIR}/generated/default/perfectdarkzerorecomp_recomp.162.cpp")
set(pdz_cutscene_output "${CMAKE_CURRENT_BINARY_DIR}/pdz_cutscene_recomp.cpp")
add_custom_command(OUTPUT "${pdz_cutscene_output}"
    COMMAND ${CMAKE_COMMAND}
      -DINPUT_FILE=${pdz_cutscene_input}
      -DOUTPUT_FILE=${pdz_cutscene_output}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/fix-cutscene-timing.cmake
    DEPENDS "${pdz_cutscene_input}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/fix-cutscene-timing.cmake" perfectdarkzerorecomp_codegen
    COMMENT "Applying bounded TU0 cutscene timing correction")
get_target_property(pdz_recomp_sources perfectdarkzerorecomp_recomp SOURCES)
list(REMOVE_ITEM pdz_recomp_sources "${pdz_cutscene_input}")
set_property(TARGET perfectdarkzerorecomp_recomp PROPERTY SOURCES "${pdz_recomp_sources}")
target_sources(perfectdarkzerorecomp_recomp PRIVATE "${pdz_cutscene_output}")

# Each pair is a TU0 translation-unit index and its audited capacity loads.
# Include pickup eligibility, reload clamps, getters and native HUD consumers.
foreach(pdz_weapon_pair "101,1" "133,2" "15,1" "161,3" "162,1"
    "164,1" "184,1" "19,1" "24,1" "28,2" "43,2" "70,1" "82,2" "112,0" "99,0")
  string(REPLACE "," ";" pdz_weapon_fields "${pdz_weapon_pair}")
  list(GET pdz_weapon_fields 0 pdz_weapon_index)
  list(GET pdz_weapon_fields 1 pdz_weapon_count)
  set(pdz_weapon_input "${CMAKE_CURRENT_SOURCE_DIR}/generated/default/perfectdarkzerorecomp_recomp.${pdz_weapon_index}.cpp")
  if(pdz_weapon_index EQUAL 162)
    set(pdz_weapon_input "${pdz_cutscene_output}")
  endif()
  set(pdz_weapon_spread OFF)
  if(pdz_weapon_index EQUAL 112)
    set(pdz_weapon_spread ON)
  endif()
  set(pdz_weapon_output "${CMAKE_CURRENT_BINARY_DIR}/pdz_weapon_${pdz_weapon_index}.cpp")
  add_custom_command(OUTPUT "${pdz_weapon_output}"
      COMMAND ${CMAKE_COMMAND} -DINPUT_FILE=${pdz_weapon_input}
        -DOUTPUT_FILE=${pdz_weapon_output} -DEXPECTED_CAPS=${pdz_weapon_count}
        -DPATCH_SPREAD=${pdz_weapon_spread}
        -DTU_INDEX=${pdz_weapon_index}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/fix-weapon-rules.cmake
      DEPENDS "${pdz_weapon_input}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/fix-weapon-rules.cmake" perfectdarkzerorecomp_codegen
      COMMENT "Applying audited TU0 weapon rules (${pdz_weapon_index})")
  get_target_property(pdz_recomp_sources perfectdarkzerorecomp_recomp SOURCES)
  list(REMOVE_ITEM pdz_recomp_sources "${pdz_weapon_input}")
  set_property(TARGET perfectdarkzerorecomp_recomp PROPERTY SOURCES "${pdz_recomp_sources}")
  target_sources(perfectdarkzerorecomp_recomp PRIVATE "${pdz_weapon_output}")
endforeach()

# Keep a local address map so release crash offsets can be traced to guest hooks.
if(WIN32)
    target_link_options(perfectdarkzerorecomp PRIVATE
        "LINKER:/MAP:${CMAKE_CURRENT_BINARY_DIR}/perfectdarkzerorecomp.map")
endif()


# SDL looks for this file (name set by settings/mapping.toml's
# hid_mappings_file) next to the exe at startup to fill in gamepads it
# doesn't already recognize. Sourced from
# https://github.com/mdqinc/SDL_GameControllerDB, see settings/README.md.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/settings/gamecontrollerdb.txt")
    add_custom_command(TARGET perfectdarkzerorecomp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/settings/gamecontrollerdb.txt"
                "$<TARGET_FILE_DIR:perfectdarkzerorecomp>/gamecontrollerdb.txt"
        COMMENT "Copying gamecontrollerdb.txt next to perfectdarkzerorecomp.exe")
endif()


