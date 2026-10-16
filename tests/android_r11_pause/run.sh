#!/usr/bin/env bash
set -eu
cd "$(dirname "$0")/../.."
SDL=/tmp/zamn-r8-sdl/smw-android/app/jni/SDL2/include
clang -std=gnu11 -O0 -g -fsanitize=address,undefined -I. -Isrc -I"$SDL" \
  tests/android_r11_pause/test.c android_port/app/src/main/cpp/zamn_android_online_pause.c \
  -lm -o /tmp/zamn_r11_pause_test
/tmp/zamn_r11_pause_test
