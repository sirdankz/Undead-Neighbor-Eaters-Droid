# Produces a locally signed Android release APK. The signing key is never bundled.
# Builds only the ARM64 application; no ROM or original game assets are included.
[CmdletBinding()]
param(
  [string]$Keystore = (Join-Path $env:USERPROFILE 'Documents\UndeadNeighborEaters-release.jks'),
  [string]$Alias = 'undead-neighbor-eaters',
  [string]$SdkRoot='',
  [string]$GradleRoot='',
  [string]$JdkRoot='',
  [switch]$CreateKeystore
)
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
function Fail([string]$m){throw "[UNE release] $m"}
function Expose([Security.SecureString]$secure){
  $ptr=[Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure)
  try{return [Runtime.InteropServices.Marshal]::PtrToStringBSTR($ptr)}
  finally{[Runtime.InteropServices.Marshal]::ZeroFreeBSTR($ptr)}
}
if(!$Keystore -or !$Alias){Fail 'Keystore and Alias cannot be empty.'}
$fullKeystore=[IO.Path]::GetFullPath($Keystore)
if(-not (Test-Path -LiteralPath $fullKeystore)){
  if(!$CreateKeystore){Fail "Keystore not found: $fullKeystore. Run with -CreateKeystore once to generate your permanent release key."}
  $jdkChoices=@($JdkRoot,$env:JAVA_HOME)
  $jdkChoices+=@(Get-ChildItem 'C:\Program Files\Microsoft' -Directory -Filter 'jdk*' -ErrorAction SilentlyContinue | ForEach-Object FullName)
  $jdkChoices+=@(Get-ChildItem 'C:\Program Files\Java' -Directory -Filter 'jdk*' -ErrorAction SilentlyContinue | ForEach-Object FullName)
  $keytool=$null
  foreach($jdkCandidate in $jdkChoices){
    if(!$jdkCandidate){continue}
    $candidate=Join-Path $jdkCandidate 'bin\keytool.exe'
    if(Test-Path -LiteralPath $candidate){$keytool=$candidate;break}
  }
  if(!$keytool){$cmd=Get-Command keytool.exe -ErrorAction SilentlyContinue;if($cmd){$keytool=$cmd.Source}}
  if(!$keytool){Fail 'keytool not found. Set -JdkRoot to an installed JDK 17/21 directory.'}
  New-Item -ItemType Directory -Path (Split-Path $fullKeystore -Parent) -Force | Out-Null
  Write-Host '[UNE release] Creating a new permanent signing key. Remember and back up the keystore and passwords.' -ForegroundColor Yellow
  # keytool prompts interactively; do not pass a plaintext password on the command line.
  & $keytool -genkeypair -keystore $fullKeystore -alias $Alias -keyalg RSA -keysize 3072 -validity 10000
  if($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath $fullKeystore)){Fail 'keytool did not create the signing keystore.'}
}
$secureStore=Read-Host 'Android release keystore password' -AsSecureString
$secureKey=Read-Host 'Android release key password (press Enter if same as store password)' -AsSecureString
if($secureKey.Length -eq 0){$secureKey=$secureStore}
$old=@{}
$keys=@('UNE_ANDROID_KEYSTORE','UNE_ANDROID_KEY_ALIAS','UNE_ANDROID_STORE_PASSWORD','UNE_ANDROID_KEY_PASSWORD')
foreach($k in $keys){$old[$k]=[Environment]::GetEnvironmentVariable($k,'Process')}
try{
  $env:UNE_ANDROID_KEYSTORE=$fullKeystore
  $env:UNE_ANDROID_KEY_ALIAS=$Alias
  $env:UNE_ANDROID_STORE_PASSWORD=Expose $secureStore
  $env:UNE_ANDROID_KEY_PASSWORD=Expose $secureKey
  & (Join-Path $root 'BUILD_ANDROID.ps1') -Release -SdkRoot $SdkRoot -GradleRoot $GradleRoot -JdkRoot $JdkRoot
  $built=Join-Path $root 'android_port\app\build\outputs\apk\release\app-release.apk'
  if(!(Test-Path -LiteralPath $built)){Fail 'Release APK not produced.'}
  $output=Join-Path $root 'out\release'
  New-Item -ItemType Directory -Path $output -Force | Out-Null
  $named=Join-Path $output 'UndeadNeighborEaters-R13-arm64.apk'
  Copy-Item -LiteralPath $built -Destination $named -Force
  $hash=(Get-FileHash -LiteralPath $named -Algorithm SHA256).Hash
  Set-Content -LiteralPath (Join-Path $output 'SHA256.txt') -Value "$hash  UndeadNeighborEaters-R13-arm64.apk" -Encoding Ascii
  foreach($f in @('LICENSING.md','CREDITS.md','THIRD_PARTY_NOTICES.md','RELEASE_READINESS.md')){
    Copy-Item -LiteralPath (Join-Path $root $f) -Destination (Join-Path $output $f) -Force
  }
  $licenses=Join-Path $output 'LICENSES'
  New-Item -ItemType Directory -Path $licenses -Force | Out-Null
  Copy-Item -Path (Join-Path $root 'LICENSES\*') -Destination $licenses -Force
  $sdlLicense=Join-Path $root 'android_port\app\jni\SDL\LICENSE.txt'
  if(Test-Path -LiteralPath $sdlLicense){Copy-Item -LiteralPath $sdlLicense -Destination (Join-Path $licenses 'LICENSE-SDL2-zlib.txt') -Force}
  $zip=Join-Path $output 'UndeadNeighborEaters-R13-APK-RELEASE.zip'
  if(Test-Path -LiteralPath $zip){Remove-Item -LiteralPath $zip -Force}
  $toZip=@($named,(Join-Path $output 'SHA256.txt'),(Join-Path $output 'LICENSING.md'),(Join-Path $output 'CREDITS.md'),(Join-Path $output 'THIRD_PARTY_NOTICES.md'),(Join-Path $output 'RELEASE_READINESS.md'),$licenses)
  Compress-Archive -LiteralPath $toZip -DestinationPath $zip -CompressionLevel Optimal
  Write-Host "[UNE release] SIGNED APK: $named" -ForegroundColor Green
  Write-Host "[UNE release] ZIP: $zip" -ForegroundColor Green
  Write-Host "[UNE release] SHA256: $hash"
  Write-Host '[UNE release] Publishing still requires appropriate redistribution rights for upstream-derived code.' -ForegroundColor Yellow
}finally{
  foreach($k in $keys){[Environment]::SetEnvironmentVariable($k,$old[$k],'Process')}
  Remove-Variable secureStore,secureKey -ErrorAction SilentlyContinue
}
