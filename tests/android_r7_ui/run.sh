#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
javac --release 8 -d "$TMP" $(find "$ROOT/tests/android_r7_ui/android" -name '*.java') \
  "$ROOT/android_port/app/src/main/java/com/zamn/android/XboxMenuView.java" \
  "$ROOT/android_port/app/src/main/java/com/zamn/android/LeaderboardTime.java" \
  "$ROOT/tests/android_r7_ui/com/zamn/android/TestR7.java"
java -cp "$TMP" com.zamn.android.TestR7
