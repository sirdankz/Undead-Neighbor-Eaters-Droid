# Release readiness — Android

## Technical packaging

- [x] Source package excludes commercial game ROMs, original manuals and executable artifacts.
- [x] Android application display name is `Undead Neighbor Eaters` while its package ID is retained for compatibility.
- [x] GitHub-facing README, credits, component licensing, SDK build instructions and Git ignore rules are provided.
- [ ] Build a fresh APK using a documented Windows SDK installation.
- [ ] Test offline mode, save/load restrictions, HUD editing, physical gamepads, Android hosting, Xbox cross-play, profile login, and online shared pause.
- [x] Document a repeatable release-signing build script with a keystore kept outside Git.
- [ ] Execute the script with the project signing key and verify the resulting APK on hardware.
- [ ] Confirm release upgrade compatibility, package signatures and ROM picker behavior on real hardware.
- [ ] Review public matchmaking availability, account token transport and player privacy.

## Licensing and distribution — unresolved prerequisite

- [ ] Establish appropriate redistribution permission for code derived from the Moshferatu PC port and any original-game-derived translated code.
- [ ] Confirm that licensing declarations are authorized by the respective contributors/rightsholders.
- [ ] Retain LakeSnes, SDL2, and other applicable notices in distributed forms.
- [ ] Review original game name/logo/artwork/screenshots used on public repository or store listings.

The absence of a game ROM in the repository or APK does not establish licensing clearance for the executable code. See `LICENSING.md`.
