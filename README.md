# Undead Neighbor Eaters — Android Native Port

**Undead Neighbor Eaters** is an unofficial Android port of *Zombies Ate My Neighbors*. It requires a separately supplied compatible game ROM; no commercial ROM or original game manual is included.

**Licensing and distribution status:** The repository contains upstream-derived translated game logic for which project-wide redistribution permission has not been established. Android-specific files and shared netplay files have component-specific licensing notices; those grants do **not** cover all upstream-derived code or confer permission to distribute complete APKs. See [LICENSING.md](LICENSING.md) and [RELEASE_READINESS.md](RELEASE_READINESS.md).

## Features

- Native Android ARM64 gameplay with SDL2 and the shared native runtime
- ROM import through Android's system document picker
- Landscape Xbox-inspired menus that use the entire phone display; gameplay retains its original aspect ratio
- Editable touchscreen HUD, eight-way disc, floating stick, adjustable deadzone, per-button size/position, on-screen MAP, previous and next weapon
- Remappable physical gamepads, including PlayStation-style controllers
- Offline pause, ruleset-separated local saves, and save protection for No Save modes
- Direct Connect and Public Room multiplayer with the compatible Original Xbox port
- Android hosting and joining; automatic/manual delay and 1–3 frame rollback (maximum 1 frame for Xbox peers)
- Synchronized online Start+Back pause and save/load barriers; leaderboard, profile and chat UI

## Requirements

- Android 6.0 (API 23) or newer; **ARM64** device
- A legally obtained compatible USA version SNES ROM (provided by the player)
- Network connection for multiplayer, public rooms, profiles, chat and leaderboards
- Matching compatible Xbox/Android netplay protocol builds for cross-platform play

## Running

Install a locally built or appropriately licensed and signed APK, open the application and import the compatible game ROM using the system file picker. Select **Offline** for local play, or **Online** for matchmaking. A ROM is not provided by this repository.

The application ID remains `com.zamn.android` for compatibility with existing saved preferences and installed testing builds; the displayed application name is **Undead Neighbor Eaters**.

## Build from source on Windows

The project builds through Gradle 8.9, Android Gradle Plugin 8.5.2, JDK 17/21, Android SDK Platform 34, Android NDK `27.3.13750724`, CMake `3.22.1` and SDL2 `release-2.32.10`. Git is required on the first build to obtain SDL2. The build downloads tooling dependencies but **does not download or package game ROM data**.

A short source checkout and Android SDK path is recommended on Windows because the NDK/Ninja toolchain can encounter Windows path-length limits.

From PowerShell in the repository root:

```powershell
.\BUILD_ANDROID.ps1 -SdkRoot 'C:\SHSDK' -GradleRoot 'C:\Android\gradle-8.9'
```

If `ANDROID_HOME` / `ANDROID_SDK_ROOT`, `JAVA_HOME`, and `GRADLE_HOME` are configured, explicit arguments may be omitted:

```powershell
.\BUILD_ANDROID.ps1
```

Gradle's test APK is written to:

```text
android_port/app/build/outputs/apk/debug/app-debug.apk
```

For device installation, add `-InstallApk` with Android Debug Bridge and USB debugging enabled. See [BUILDING.md](BUILDING.md) for troubleshooting and build configuration.

**Release APKs:** The optional `BUILD_ANDROID_RELEASE.ps1` script builds and verifies a signed APK using a locally created signing key. See [RELEASE_ANDROID.md](RELEASE_ANDROID.md). Signing does not by itself establish permission for public redistribution of the game-derived code.

## Repository layout

| Directory | Description |
| --- | --- |
| `android_port/` | Android Gradle project, Java menu/UI, native Android frontend, SDL2 checkout location |
| `src/` | Shared translated runtime and netplay code, with Xbox-origin compatibility files |
| `third_party/lakesnes/` | SNES compatibility implementation; MIT notices preserved |
| `tests/` | Android-specific regression test fixtures |
| `LICENSES/` | Component license texts |

## Multiplayer and compatibility

The companion Original Xbox port is maintained at [Undead Neighbor Eaters — Original Xbox](https://github.com/sirdankz/Undead-Neighbor-Eaters-Xbox).

Android and Original Xbox builds share the ZNP1 network protocol and compatible game state. Public-room features connect to the project's remote matchmaking service. Android-hosted mobile-only sessions support rollback up to three frames; Xbox sessions are limited to one. The online synchronized pause is **Start + Back**, while Android's local control editor is available offline.

No matchmaking server binaries or private administration keys are included. Multiplayer connections and account handling should undergo security/privacy review before broad distribution. A public IP address configured in the app is not an authentication secret.

## Credits and legal notices

See [CREDITS.md](CREDITS.md), [LICENSING.md](LICENSING.md), and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

This is an unofficial community project and is not sponsored, endorsed, or affiliated with the original game's rightsholders.
