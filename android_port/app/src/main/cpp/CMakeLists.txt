cmake_minimum_required(VERSION 3.22)
set(CMAKE_SUPPRESS_REGENERATION ON)
project(ZamnAndroid C CXX)
# Private Android R1 adaptation of the user's OG Xbox source.
# XBOX_PORT is the legacy *runtime optimization* switch in shared C sources;
# we do NOT build the XBE frontend or define XBOX / _XBOX.
get_filename_component(ZAMN_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL2_DISABLE_SDL2MAIN ON CACHE BOOL "" FORCE)
add_subdirectory("${ZAMN_ROOT}/android_port/app/jni/SDL" SDL2)
# SDL2 Android HID library includes C++ and requires the NDK STL.
set_target_properties(SDL2 PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(SDL2 PRIVATE c++_shared)

set(ZAMN_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/zamn_android_main.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/zamn_android_touch.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/zamn_android_pause.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/zamn_android_online_pause.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/zamn_android_net.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/zamn_android_lobby.c"
    "${ZAMN_ROOT}/src/native/runtime.c"
    "${ZAMN_ROOT}/third_party/lakesnes/snes/apu.c"
    "${ZAMN_ROOT}/third_party/lakesnes/snes/cart.c"
    "${ZAMN_ROOT}/third_party/lakesnes/snes/cpu.c"
    "${ZAMN_ROOT}/third_party/lakesnes/snes/dma.c"
    "${ZAMN_ROOT}/third_party/lakesnes/snes/dsp.c"
    "${ZAMN_ROOT}/third_party/lakesnes/snes/input.c"
    "${ZAMN_ROOT}/third_party/lakesnes/snes/ppu.c"
    "${ZAMN_ROOT}/third_party/lakesnes/snes/snes.c"
    "${ZAMN_ROOT}/third_party/lakesnes/snes/snes_other.c"
    "${ZAMN_ROOT}/third_party/lakesnes/snes/spc.c"
    "${ZAMN_ROOT}/third_party/lakesnes/snes/statehandler.c"
    "${ZAMN_ROOT}/src/analysis/w65816.c"
    "${ZAMN_ROOT}/src/analysis/cdl.c"
    "${ZAMN_ROOT}/src/analysis/movie.c"
    "${ZAMN_ROOT}/src/assets/lzss.c"
    "${ZAMN_ROOT}/src/assets/gfx.c"
    "${ZAMN_ROOT}/src/assets/rom.c"
    "${ZAMN_ROOT}/src/assets/level.c"
    "${ZAMN_ROOT}/src/assets/actor.c"
    "${ZAMN_ROOT}/src/assets/sprite.c"
    "${ZAMN_ROOT}/src/assets/music.c"
    "${ZAMN_ROOT}/src/assets/password.c"
    "${ZAMN_ROOT}/src/port/coverage.c"
    "${ZAMN_ROOT}/src/port/apu.c"
    "${ZAMN_ROOT}/src/port/bodies.c"
    "${ZAMN_ROOT}/src/port/boss.c"
    "${ZAMN_ROOT}/src/port/bossbg.c"
    "${ZAMN_ROOT}/src/port/collide.c"
    "${ZAMN_ROOT}/src/port/fade.c"
    "${ZAMN_ROOT}/src/port/floor.c"
    "${ZAMN_ROOT}/src/port/hud.c"
    "${ZAMN_ROOT}/src/port/camera.c"
    "${ZAMN_ROOT}/src/port/levelmap.c"
    "${ZAMN_ROOT}/src/port/lzss.c"
    "${ZAMN_ROOT}/src/port/monster.c"
    "${ZAMN_ROOT}/src/port/oam.c"
    "${ZAMN_ROOT}/src/port/player.c"
    "${ZAMN_ROOT}/src/port/player_resume.c"
    "${ZAMN_ROOT}/src/port/hotpaths.c"
    "${ZAMN_ROOT}/src/port/rng.c"
    "${ZAMN_ROOT}/src/port/sched.c"
    "${ZAMN_ROOT}/src/port/score.c"
    "${ZAMN_ROOT}/src/port/sprite_cache.c"
    "${ZAMN_ROOT}/src/port/step.c"
    "${ZAMN_ROOT}/src/port/terrain.c"
    "${ZAMN_ROOT}/src/port/thread.c"
    "${ZAMN_ROOT}/src/port/trig.c"
    "${ZAMN_ROOT}/src/port/vblank.c"
    "${ZAMN_ROOT}/src/port/walk.c"
    "${ZAMN_ROOT}/src/port/chase.c"
    "${ZAMN_ROOT}/src/cosim/cosim.c"
    "${ZAMN_ROOT}/src/cosim/routines.c"
    "${ZAMN_ROOT}/src/native/audio.c"
    "${ZAMN_ROOT}/src/native/video.c"
    "${ZAMN_ROOT}/src/netplay/netplay_core.c"
    "${ZAMN_ROOT}/src/platform/xbox/profile_xbox.c"
 )
add_library(main SHARED ${ZAMN_SOURCES})
target_include_directories(main PRIVATE "${ZAMN_ROOT}/src" "${ZAMN_ROOT}/third_party/lakesnes/snes")
target_compile_definitions(main PRIVATE
    XBOX_PORT
    COSIM_FAST_RUNTIME=1
    ZAMN_R24_RELEASE_FAST=1
    ZAMN_R28_NATIVE_BULK_BURN=1
    ZAMN_R32_PPU_HOISTS=1
    ZAMN_R33_TRUE_NATIVE_CUTOVER=1
    ZAMN_R33_RELEASE_LEAN=1
    ZAMN_R34_TRUE_NATIVE_CUTOVER=1
    ZAMN_R35_NATIVE_OBJECT_PIPELINE=1
    ZAMN_R36_NATIVE_LEAN=1
    ZAMN_R37_READONLY_GUARDS=1
    ZAMN_R38_READONLY_WALK_GUARD=1
    ZAMN_RELEASE_NO_DIAGNOSTICS=1
    ZAMN_R40_FAST_OAM_PREFLIGHT=1
    ZAMN_R41_TRANSITION_WAIT_CUTOVER=1
    ZAMN_R41_NATIVE_RENDER_OWNER=1
    ZAMN_R41_NATIVE_AUDIO=1
    ZAMN_R42_LEVEL_INTRO_WAIT_CUTOVER=1
    ZAMN_R43_NATIVE_WAIT_CUTOVER=1
    ZAMN_R44_NATIVE_BURN_ATTRIB=1
    ZAMN_R45_APU_SET_NATIVE_TRANSFER=1
    ZAMN_R46_APU_COMPACT_TRACE=1
    ZAMN_R46_STRICT_NATIVE_SHARE=1
    ZAMN_R48_ROLLBACK=1
    ZAMN_R50_NATIVE_HOTPATHS=1
    ZAMN_R51_NATIVE_HOTPATHS=1
    ZAMN_R52_FAST_COLLISION_GUARDS=1
    ZAMN_R53_NATIVE_MOVEMENT=1
    ZAMN_R54_NATIVE_D9_CLUSTER=1
    ZAMN_R55_NATIVE_ACTOR_FRAGMENTS=1
    ZAMN_R56_SAFE_OAM_CUTOVER=1
    ZAMN_R57_OAM_COLLISION_PROOF=1
    ZAMN_R58_COLLISION_THREAD_FASTPATH=1
    ZAMN_R59_NATIVE_COLLISION_FRAGMENTS=1
    ZAMN_R60_CONNECTED_NATIVE=1
    ZAMN_R61_THREAD_ACTOR_NATIVE=1
    ZAMN_R62_ACTOR_CONTROL_NATIVE=1
    ZAMN_R63_POSTCALL_NATIVE=1
    ZAMN_R64_ACTOR_RECORD_NATIVE=1
    ZAMN_R65_HOT_NATIVE_DISPATCH=1
    ZAMN_R66_TOTAL_HANDLER_FAST_GUARDS=1
    ZAMN_R68_INPUT_READONLY_GUARDS=1
    ZAMN_R70_HOT_THREAD_9A6D=1
    ZAMN_R74_COLLISION_OAM_INPUT_PROOF=1
    ZAMN_R71_CONTROL_MAPPING=1
    ZAMN_R76_LT_FALLBACK_FIX=1
    ZAMN_R421_ENHANCED_PRESETS=1
    ZAMN_R41_AUDIO_DEFAULT=1
    ZAMN_R421_AUDIO_PROFILE=2
 )
target_compile_options(main PRIVATE -Wno-error=format-security)
target_link_libraries(main PRIVATE SDL2::SDL2 log android m)
