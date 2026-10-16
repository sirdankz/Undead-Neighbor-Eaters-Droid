# Android build documentation

## Tool versions

| Tool | Version |
| --- | --- |
| JDK | 17 or 21 |
| Gradle | 8.9 |
| Android Gradle Plugin | 8.5.2 |
| Android SDK | Platform 34, Build Tools 34.0.0 |
| NDK | 27.3.13750724 |
| CMake | 3.22.1 |
| SDL2 | release-2.32.10, fetched from GitHub |
| Target CPU | arm64-v8a |

The Android frontend and shared native runtime are built with the Gradle project in `android_port/`. No Original Xbox SDK or RXDK installation is required.

The Windows helper accepts `-SdkRoot`, `-GradleRoot`, `-JdkRoot`, and `-InstallApk`. Installed Android SDK, JDK and Gradle environment variables may also be used.

```powershell
.\BUILD_ANDROID.ps1 -SdkRoot 'C:\Android\sdk' -GradleRoot 'C:\Android\gradle-8.9'
```

The script clones SDL2 once to `android_port/app/jni/SDL` when absent; that checkout and all generated files are Git-ignored. The build writes an Android `local.properties` file outside version control.

Run a second time to rebuild using the existing checkout and tool installations. Output: `android_port/app/build/outputs/apk/debug/app-debug.apk`.

### Manual Gradle build

With SDL2 checked out under `android_port/app/jni/SDL`, execute from `android_port/`:

```powershell
gradle --no-daemon :app:assembleDebug
```

The SDK and CMake paths must be configured through standard Android environment variables or the local `local.properties` file.

### Release APKs

`BUILD_ANDROID_RELEASE.ps1` creates a locally signed ARM64 release APK using a persistent signing key held outside the repository. Run it with `-CreateKeystore` once, then without that switch for subsequent releases. See [RELEASE_ANDROID.md](RELEASE_ANDROID.md) for commands, secure key storage, signing verification, the release ZIP and debug-to-release upgrade limitations. Source and compiled APK distribution rights must still be established separately.

### Privacy and online services

Public-room matchmaking and account functions rely on a separate service. Account tokens and chat traffic require a privacy and transport-security review before large-scale public distribution. Repository publication does not change server security.
