#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
clang -std=gnu11 -O1 -g -ffunction-sections -fdata-sections -fsanitize=address,undefined \
    -I/tmp/zamn-r8-sdl/smw-android/app/jni/SDL2/include -Isrc \
    tests/android_r8_touch/test.c android_port/app/src/main/cpp/zamn_android_touch.c \
    -Wl,--gc-sections -lm -o /tmp/zamn_r8_touch_test
/tmp/zamn_r8_touch_test
