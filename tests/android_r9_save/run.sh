#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
SDL=/tmp/zamn-r8-sdl/smw-android/app/jni/SDL2/include
clang -D__ANDROID__ -std=gnu11 -O1 -ffunction-sections -fdata-sections -g -fsanitize=address,undefined \
  -I"$SDL" -Isrc -I/usr/lib/jvm/java-21-openjdk-amd64/include -I/usr/lib/jvm/java-21-openjdk-amd64/include/linux \
  tests/android_r9_save/test.c android_port/app/src/main/cpp/zamn_android_pause.c \
  -Wl,--gc-sections -lm -o /tmp/zamn_r9_pause_test
/tmp/zamn_r9_pause_test
