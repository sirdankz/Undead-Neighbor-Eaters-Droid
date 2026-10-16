# Android release APK build — Undead Neighbor Eaters R13

The repository provides both a developer debug APK build and a **locally signed, ARM64 release APK** workflow. It does not contain an Android signing key, commercial game ROM, or original manual scans.

## Create and protect a signing identity

Use a dedicated signing key under a local path outside the repository. On the first release build, PowerShell can run the JDK's `keytool` interactively to create one:

```powershell
.\BUILD_ANDROID_RELEASE.ps1 -CreateKeystore
```

The default key path is `%USERPROFILE%\Documents\UndeadNeighborEaters-release.jks`, with alias `undead-neighbor-eaters`. `keytool` prompts for the new keystore's password and certificate metadata. After creating the key, the release script prompts again for the passwords needed to sign the APK.

**Back up the keystore and passwords securely.** Future APK updates must be signed with the **same key**, or Android will refuse in-place installation. Do not commit the signing keystore or passwords to GitHub. The scripts pass signing credentials temporarily using process-local environment variables and clear them when the build completes. They are not written to project files.

For subsequent release builds:

```powershell
.\BUILD_ANDROID_RELEASE.ps1
```

If the JDK, SDK and Gradle installations are not in standard locations, pass `-JdkRoot`, `-SdkRoot`, and `-GradleRoot`. For example:

```powershell
.\BUILD_ANDROID_RELEASE.ps1 -SdkRoot 'C:\SHSDK' -GradleRoot "$env:LOCALAPPDATA\SilentHillAndroidBuild\gradle-8.9"
```

## Release output

After building, `BUILD_ANDROID.ps1` verifies the APK signature using Android `apksigner`. The release helper then creates:

- `out/release/UndeadNeighborEaters-R13-arm64.apk`
- `out/release/UndeadNeighborEaters-R13-APK-RELEASE.zip`
- `out/release/SHA256.txt`

The ZIP includes the APK, copyright and third-party notices, and a checksum. No game ROM or original manual is packaged. The Android app still requires the player to import their compatible ROM.

## Update and compatibility notes

The application ID remains `com.zamn.android` to preserve Android's data identity. Existing **debug-signed** builds generally cannot be updated in-place by a **release-signed** build due to their different signing certificates. Back up user data first, and test a fresh release install on a secondary device if possible. Retain the release keystore for future versions, and increase `versionCode` for subsequent public APK updates.

The source and APK may contain game-derived material for which permission to distribute is not established. Signing an APK does not confer redistribution rights. Before public uploading, resolve the conditions documented in `LICENSING.md` and `RELEASE_READINESS.md`, and review account-token and multiplayer transport security.
