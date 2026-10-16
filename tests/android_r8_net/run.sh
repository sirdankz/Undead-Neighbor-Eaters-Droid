#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
clang -std=gnu11 -O1 -g -fsanitize=address,undefined -Itests/android_r3_smoke -Isrc tests/android_r8_net/host_test.c src/netplay/netplay_core.c -o /tmp/zamn_r8_host_test
/tmp/zamn_r8_host_test xbox
/tmp/zamn_r8_host_test mobile
/tmp/zamn_r8_host_test xbox auto
/tmp/zamn_r8_host_test mobile auto
