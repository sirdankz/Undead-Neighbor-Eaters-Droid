# Compatibility alias for the R13 Windows build entry point.
& (Join-Path $PSScriptRoot 'BUILD_ANDROID.ps1') @args
if (!$?) { exit 1 }
