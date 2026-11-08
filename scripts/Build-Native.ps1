param([string]$OutputDir = (Join-Path $PSScriptRoot '..\.build\payload'), [switch]$TestMode)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$OutputDir=[IO.Path]::GetFullPath($OutputDir)
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'Visual C++ x64 compiler not found.'}
$vcvars=Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$mode=if($TestMode){'/DHUD_BRIDGE_TEST'}else{''}
$batch=@"
@echo off
call "$vcvars" >nul
if %errorlevel% neq 0 exit /b 1
cd /d "$OutputDir"
rc /nologo /fo bridge.res "$root\native\bridge.rc"
if %errorlevel% neq 0 exit /b 1
cl /nologo /LD /MT /O1 /GL /Gy /Gw /W4 /WX /wd4191 /wd4100 /EHsc /utf-8 /std:c++20 /DUNICODE /D_UNICODE $mode "$root\native\SpotifyLyricsBridge.cpp" "$root\native\NativeHud.cpp" "$root\native\NativeNetwork.cpp" "$root\native\NativeFlyout.cpp" "$root\native\NativeGlass.cpp" bridge.res bcrypt.lib crypt32.lib winhttp.lib ws2_32.lib gdiplus.lib user32.lib gdi32.lib dwmapi.lib d2d1.lib dwrite.lib windowscodecs.lib ole32.lib windowsapp.lib CoreMessaging.lib /link /LTCG /OPT:REF /OPT:ICF /DEF:"$root\native\version.def" /OUT:"version.dll" /INCREMENTAL:NO
if %errorlevel% neq 0 exit /b 1
"@
if($TestMode){$batch+=@"

cl /nologo /MT /O2 /W4 /WX /wd4191 /EHsc /utf-8 /std:c++20 /DUNICODE /D_UNICODE "$root\tests\native-fixture.cpp" version.lib user32.lib gdi32.lib psapi.lib dwmapi.lib /link /OUT:"SpotifyBridgeTest.exe" /INCREMENTAL:NO
if %errorlevel% neq 0 exit /b 1
cl /nologo /MT /O2 /W4 /WX /EHsc /utf-8 /std:c++20 /DUNICODE /D_UNICODE "$root\tests\fullscreen-native.cpp" user32.lib /link /OUT:"FullscreenTest.exe" /INCREMENTAL:NO
if %errorlevel% neq 0 exit /b 1
FullscreenTest.exe
if %errorlevel% neq 0 exit /b 1
cl /nologo /MT /O2 /W4 /WX /EHsc /utf-8 /std:c++20 /DUNICODE /D_UNICODE "$root\tests\native-appearance.cpp" "$root\native\NativeFlyout.cpp" "$root\native\NativeGlass.cpp" gdiplus.lib d2d1.lib dwrite.lib windowscodecs.lib ole32.lib windowsapp.lib CoreMessaging.lib user32.lib gdi32.lib dwmapi.lib /link /OUT:"AppearanceTest.exe" /INCREMENTAL:NO
if %errorlevel% neq 0 exit /b 1
AppearanceTest.exe
if %errorlevel% neq 0 exit /b 1
cl /nologo /MT /O2 /W4 /WX /wd4100 /EHsc /utf-8 /std:c++20 /DUNICODE /D_UNICODE "$root\tests\native-corners.cpp" "$root\native\NativeNetwork.cpp" "$root\native\NativeGlass.cpp" bcrypt.lib crypt32.lib winhttp.lib ws2_32.lib gdiplus.lib user32.lib gdi32.lib dwmapi.lib d2d1.lib dwrite.lib windowscodecs.lib ole32.lib windowsapp.lib CoreMessaging.lib /link /OUT:"CornersTest.exe" /INCREMENTAL:NO
if %errorlevel% neq 0 exit /b 1
CornersTest.exe
if %errorlevel% neq 0 exit /b 1
"@}
$batch+="`r`nexit /b 0`r`n"
$file=Join-Path $OutputDir 'compile-native.cmd'
[IO.File]::WriteAllText($file,$batch,(New-Object Text.UTF8Encoding($false)))
& $env:ComSpec /d /c $file
if($LASTEXITCODE -ne 0){throw 'Spotify Lyrics native compilation failed.'}
