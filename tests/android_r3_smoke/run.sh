#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
clang -std=gnu11 -O1 -g -fsanitize=address,undefined -Itests/android_r3_smoke -Isrc tests/android_r3_smoke/rollback_test.c src/netplay/netplay_core.c -o /tmp/zamn_android_r3_rb_test
/tmp/zamn_android_r3_rb_test
