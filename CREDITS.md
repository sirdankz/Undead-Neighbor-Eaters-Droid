# Credits — Undead Neighbor Eaters (Android)

## Original game

*Zombies Ate My Neighbors* was developed by LucasArts and published by Konami. All original game copyrights, assets, trademarks, and artwork belong to the respective rightsholders.

## Upstream translated runtime

Runtime architecture and game-derived translated/native logic are based on [Moshferatu's Zombies Ate My Neighbors PC Port](https://github.com/moshferatu/zombies-ate-my-neighbors-pc-port). Attribution is not a substitute for verifying upstream redistribution permission.

## Original Xbox cross-play implementation

The Original Xbox port supplies shared netplay and native runtime components used by Android. The Original Xbox project source is [sirdankz/ZAMN-Original-Xbox](https://github.com/sirdankz/ZAMN-Original-Xbox). Compatible netplay protocol identifiers are preserved for cross-play.

## LakeSnes

SNES compatibility components: [angelo-wf/LakeSnes](https://github.com/angelo-wf/LakeSnes) and [dinkc64/LakeSnes](https://github.com/dinkc64/LakeSnes). Copyright (c) 2021–2023 angelo_wf and contributors. See retained MIT license notices.

## SDL2

[SDL2 by the SDL development community](https://github.com/libsdl-org/SDL) provides the cross-platform game, input, audio and rendering substrate. The Android build uses SDL2 `release-2.32.10`.

## Android tooling

Android Gradle Plugin, Android SDK/NDK, Gradle, OpenJDK and CMake are external build tools and dependencies.
