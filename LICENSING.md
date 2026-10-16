# Licensing — Undead Neighbor Eaters (Android)

This repository contains code from multiple sources under different licensing terms. It does not have a project-wide license covering the entire application or compiled APK. Each license applies only to the material for which its rights holders have granted those rights.

## Android frontend and netplay — MPL 2.0

The original Android frontend and Android-specific netplay implementations developed for this port are designated under the Mozilla Public License 2.0 (MPL-2.0), **only for contributions whose copyright is controlled by their licensors**.

The Android-specific contributions are contained in:

- `android_port/app/src/main/cpp/zamn_android_main.c`
- `android_port/app/src/main/cpp/zamn_android_touch.c` and `.h`
- `android_port/app/src/main/cpp/zamn_android_pause.c` and `.h`
- `android_port/app/src/main/cpp/zamn_android_online_pause.c` and `.h`
- `android_port/app/src/main/cpp/zamn_android_net.c` and `.h`
- `android_port/app/src/main/cpp/zamn_android_lobby.c` and `.h`
- `android_port/app/src/main/java/com/zamn/android/` (original Android-specific contributions)

A copy of the Mozilla Public License 2.0 is provided at:

- `LICENSES/LICENSE-MPL-2.0.txt`

The MPL-2.0 designation applies only to rights the relevant contributors can grant. It does **not** relicense any copied or upstream-derived code in the listed files, and it does not license the commercial game or the shared game runtime as a whole.

## Shared Original Xbox netplay — MPL 2.0

The Android port also uses the original netplay implementation from the Original Xbox port, designated MPL-2.0 for the contributors' own work in:

- `src/netplay/netplay_core.c`
- `src/netplay/netplay_core.h`

The Original Xbox repository additionally designates its Xbox-specific adapter files, `src/platform/xbox/xbox_netplay.cpp` and `.h`. Those files are **not** part of the Android application's compiled frontend.

The MPL-2.0 does not change the licensing status of any other shared source file.

## Upstream ZAMN PC port

Portions of this project are derived from or based on:

**Zombies Ate My Neighbors PC Port** by Moshferatu  
https://github.com/moshferatu/zombies-ate-my-neighbors-pc-port

The upstream project did not include a clear project-wide redistribution license when this port was created. The component-specific MPL license grants in this repository do not grant permission to redistribute upstream-derived code beyond rights held by the respective copyright holders. Required permissions must be established independently.

## LakeSnes — MIT

Code under `third_party/lakesnes/` is derived from LakeSnes and is licensed under the MIT License.

The required MIT notices are preserved in:

- `third_party/lakesnes/LICENSE.txt`
- `LICENSES/LICENSE-LakeSnes-MIT.txt`

Upstream projects:

- https://github.com/angelo-wf/LakeSnes
- https://github.com/dinkc64/LakeSnes

Copyright (c) 2021–2023 angelo_wf and contributors.

## SDL2 and Android toolchain

SDL2 is obtained from https://github.com/libsdl-org/SDL using the pinned `release-2.32.10` branch and is licensed under the zlib license. Its source license and attribution must be retained as applicable when redistributing builds containing SDL2.

The Android SDK, Android NDK, Gradle, Java toolchain and other external build dependencies are not distributed in this source repository. Their own license terms apply.

## Original game

*Zombies Ate My Neighbors* was developed by LucasArts and published by Konami.

No game ROM, commercial game distribution, or original manual scans are included in this repository. Players must supply their own compatible ROM through the application's file picker.

Original game code, graphics, music, trademarks, names, manuals and other intellectual property remain subject to the rights of their respective owners. Renaming the community port **Undead Neighbor Eaters** does not alter the rights to the underlying game or derived code.

## Project

**Undead Neighbor Eaters** is an unofficial fan-made Android port. It is not affiliated with or endorsed by LucasArts/Lucasfilm Games, Konami, Google, SDL contributors, LakeSnes contributors or the upstream PC port.

Distributing the repository or a compiled APK requires authorization for all material included, not just the designated MPL and MIT components. Consult `CREDITS.md` and `RELEASE_READINESS.md` for provenance and release checks.

## Optional game manual

Original manual text, scans, artwork and other protected materials are not included in this source repository. The repository's code licenses do not grant rights to those materials.
