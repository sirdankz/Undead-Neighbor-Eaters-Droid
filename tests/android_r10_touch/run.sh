#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
SDL_INC=${SDL_INC:-/tmp/zamn-r8-sdl/smw-android/app/jni/SDL2/include}
clang -std=gnu11 -O1 -g -fsanitize=address,undefined -I"$SDL_INC" -Isrc \
  tests/android_r10_touch/test.c android_port/app/src/main/cpp/zamn_android_touch.c \
  -lm -ffunction-sections -fdata-sections -Wl,--gc-sections -o /tmp/zamn_r10_touch_test
/tmp/zamn_r10_touch_test
