#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
clang -D__ANDROID__ -std=gnu11 -O1 -fsanitize=address,undefined \
  -I/tmp/zamn-r8-sdl/smw-android/app/jni/SDL2/include -Isrc \
  tests/android_r9_touch/test.c android_port/app/src/main/cpp/zamn_android_touch.c -lm \
  -ffunction-sections -fdata-sections -Wl,--gc-sections -o /tmp/zamn_r9_touch_test
/tmp/zamn_r9_touch_test
