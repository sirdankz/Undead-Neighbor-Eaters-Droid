# Undead Neighbor Eaters Android R13 - developer build helper.
# No ROM or original game assets are downloaded or bundled.
[CmdletBinding()]
param([string]$SdkRoot='', [string]$GradleRoot='', [string]$JdkRoot='', [switch]$InstallApk, [switch]$Release)
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$root=$PSScriptRoot
function Say([string]$s){Write-Host "[Undead Neighbor Eaters R13] $s" -ForegroundColor Cyan}
function Fail([string]$s){throw "[Undead Neighbor Eaters R13] $s"}
Say 'Validating source and Android toolchain...'
if (!(Test-Path -LiteralPath (Join-Path $root 'src\native\runtime.c'))) {Fail 'Expected ZAMN source files beside the script.'}
if (!(Test-Path -LiteralPath (Join-Path $root 'android_port\app\src\main\cpp\CMakeLists.txt'))) {Fail 'Android CMake source missing.'}

$jdkChoices=@($JdkRoot,$env:JAVA_HOME,'C:\Android\jdk-17')
$jdkChoices+=@(Get-ChildItem 'C:\Program Files\Microsoft' -Directory -Filter 'jdk*' -ErrorAction SilentlyContinue | ForEach-Object FullName)
$jdkChoices+=@(Get-ChildItem 'C:\Program Files\Java' -Directory -Filter 'jdk*' -ErrorAction SilentlyContinue | ForEach-Object FullName)
$javacCmd=Get-Command javac.exe -ErrorAction SilentlyContinue
if ($javacCmd) {$jdkChoices+=Split-Path (Split-Path $javacCmd.Source -Parent) -Parent}
$jdk=$null
foreach($candidate in $jdkChoices){
  if([string]::IsNullOrWhiteSpace($candidate)){continue}
  $exe=Join-Path $candidate 'bin\javac.exe'
  if(!(Test-Path -LiteralPath $exe)){continue}
  $v=((& $exe -version 2>&1)|Out-String).Trim()
  if($LASTEXITCODE -eq 0 -and $v -match '^javac\s+(17|21)\b'){$jdk=$candidate;break}
}
if (!$jdk){Fail 'JDK 17/21 missing. Pass -JdkRoot C:\path\to\jdk'}
$env:JAVA_HOME=$jdk
$env:Path="$(Join-Path $jdk 'bin');$env:Path"
Say "Java: $jdk"

# Prefer standard SDK environment variables; short checkout and SDK paths
# help avoid NDK/Ninja path-length errors on Windows.
if (!$SdkRoot) {
  foreach($candidate in @($env:ANDROID_HOME,$env:ANDROID_SDK_ROOT,'C:\SHSDK',(Join-Path $env:LOCALAPPDATA 'Android\Sdk'))){
    if($candidate -and (Test-Path -LiteralPath (Join-Path $candidate 'platform-tools\adb.exe'))){$SdkRoot=$candidate;break}
  }
}
if (!$SdkRoot -or !(Test-Path -LiteralPath (Join-Path $SdkRoot 'platform-tools\adb.exe'))) {Fail 'Android SDK not found. Install SDK Platform 34, NDK 27.3.13750724, CMake 3.22.1; then set ANDROID_HOME or pass -SdkRoot C:\short\sdk.'}
$needed=@('platforms\android-34\android.jar','build-tools\34.0.0\aapt2.exe','ndk\27.3.13750724\source.properties','cmake\3.22.1\bin\cmake.exe')
foreach($relative in $needed){if(!(Test-Path -LiteralPath (Join-Path $SdkRoot $relative))){Fail "Missing $relative in Android SDK; install required packages."}}
$env:ANDROID_HOME=$SdkRoot
$env:ANDROID_SDK_ROOT=$SdkRoot
Say "SDK: $SdkRoot"

if (!$GradleRoot) {
  foreach($candidate in @($env:GRADLE_HOME,'C:\Android\gradle-8.9',(Join-Path $env:LOCALAPPDATA 'SilentHillAndroidBuild\gradle-8.9'),(Join-Path $env:LOCALAPPDATA 'Gradle\gradle-8.9'))){
    if($candidate -and (Test-Path -LiteralPath (Join-Path $candidate 'bin\gradle.bat'))){$GradleRoot=$candidate;break}
  }
}
$gradleExe=$null
if ($GradleRoot -and (Test-Path -LiteralPath (Join-Path $GradleRoot 'bin\gradle.bat'))) { $gradleExe=Join-Path $GradleRoot 'bin\gradle.bat' }
if (!$gradleExe) { $cmd=Get-Command gradle.bat -ErrorAction SilentlyContinue; if($cmd){$gradleExe=$cmd.Source} }
if (!$gradleExe) { Fail 'Gradle 8.9 not found. Set GRADLE_HOME or pass -GradleRoot C:\path\to\gradle-8.9.' }
Say "Gradle: $gradleExe"

$sdl=Join-Path $root 'android_port\app\jni\SDL'
New-Item -Path (Split-Path $sdl -Parent) -ItemType Directory -Force | Out-Null
if(!(Test-Path -LiteralPath (Join-Path $sdl 'CMakeLists.txt'))){
  if(!(Get-Command git.exe -ErrorAction SilentlyContinue)){Fail 'Git is required to download SDL2 source.'}
  if(Test-Path -LiteralPath $sdl){
    if(@(Get-ChildItem -LiteralPath $sdl -Force -ErrorAction SilentlyContinue).Count -gt 0){Fail 'The SDL directory exists but is not a valid SDL2 checkout; review it manually.'}
    Remove-Item -LiteralPath $sdl -Force -Recurse
  }
  Say 'Checking out SDL2 release-2.32.10'
  & git.exe clone --depth 1 --branch release-2.32.10 --single-branch https://github.com/libsdl-org/SDL.git $sdl
  if($LASTEXITCODE -ne 0){Fail 'SDL2 checkout failed.'}
}

$project=Join-Path $root 'android_port'
Set-Content -LiteralPath (Join-Path $project 'local.properties') -Value ('sdk.dir=' + ($SdkRoot -replace '\\','/')) -Encoding Ascii
if($Release){
  foreach($k in @('UNE_ANDROID_KEYSTORE','UNE_ANDROID_KEY_ALIAS','UNE_ANDROID_STORE_PASSWORD','UNE_ANDROID_KEY_PASSWORD')){
    if([string]::IsNullOrWhiteSpace([Environment]::GetEnvironmentVariable($k))){Fail "Missing signing setting $k; run BUILD_ANDROID_RELEASE.ps1 instead."}
  }
  Say 'Assembling signed RELEASE APK (arm64-v8a)...'
  $gradleTask=':app:assembleRelease'
}else{
  Say 'Assembling DEBUG APK (arm64-v8a)...'
  $gradleTask=':app:assembleDebug'
}
Push-Location $project
try{
  & $gradleExe --no-daemon $gradleTask
  $status=$LASTEXITCODE
} finally {Pop-Location}
if($status -ne 0){Fail 'Gradle/NDK compilation failed; send the first FAILED: or error: block.'}
$apk=if($Release){Join-Path $project 'app\build\outputs\apk\release\app-release.apk'}else{Join-Path $project 'app\build\outputs\apk\debug\app-debug.apk'}
if(!(Test-Path -LiteralPath $apk)){Fail 'Gradle returned success but the APK is missing.'}
Say "BUILT: $apk"
Say "SHA256: $((Get-FileHash -LiteralPath $apk -Algorithm SHA256).Hash)"
if($Release){
  $verifyTool=Join-Path $SdkRoot 'build-tools\34.0.0\apksigner.bat'
  if(!(Test-Path -LiteralPath $verifyTool)){Fail 'Android apksigner missing; cannot verify the release APK.'}
  & $verifyTool verify --verbose $apk
  if($LASTEXITCODE -ne 0){Fail 'Release APK signing verification failed.'}
}
Say 'For gameplay, import a compatible independently supplied game ROM in the app.' 
if($InstallApk){
  & (Join-Path $SdkRoot 'platform-tools\adb.exe') install -r $apk
  if($LASTEXITCODE -ne 0){Fail 'ADB install failed; signing may differ from an already installed build.'}
}
