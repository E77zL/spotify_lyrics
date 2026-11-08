param([string]$CacheDir = (Join-Path $PSScriptRoot '.cache'), [switch]$Offline)
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$version=(Get-Content -LiteralPath (Join-Path $root 'package.json') -Raw | ConvertFrom-Json).version
$build=Join-Path $root '.build'
$dist=Join-Path $root 'dist'
$CacheDir=[IO.Path]::GetFullPath($CacheDir)
& node (Join-Path $root 'tests/version.cjs')
if($LASTEXITCODE -ne 0){throw 'Version mismatch.'}
& node (Join-Path $root 'scripts/Download.cjs') $CacheDir
if($LASTEXITCODE -ne 0){throw 'Dependency preparation failed.'}
$payload=Join-Path $build 'payload'
New-Item -ItemType Directory -Path $payload,$dist,(Join-Path $build '7zip'),(Join-Path $build 'ws') -Force | Out-Null
& tar -xf (Join-Path $CacheDir '7zip-bin-5.2.0.tgz') -C (Join-Path $build '7zip')
if($LASTEXITCODE -ne 0){throw '7-Zip extraction failed.'}
& tar -xf (Join-Path $CacheDir 'ws-8.22.0.tgz') -C (Join-Path $build 'ws')
if($LASTEXITCODE -ne 0){throw 'Test dependency extraction failed.'}
$env:NODE_PATH=Join-Path $build 'ws/package/..'
# npm archive is named package, while Node expects node_modules/ws.
$modules=Join-Path $build 'node_modules'
New-Item -ItemType Directory -Path $modules -Force | Out-Null
if(!(Test-Path (Join-Path $modules 'ws'))){Copy-Item -LiteralPath (Join-Path $build 'ws/package') -Destination (Join-Path $modules 'ws') -Recurse}
$env:NODE_PATH=$modules
$archiver=Join-Path $build '7zip/package/win/x64/7za.exe'
& (Join-Path $root 'scripts/Bundle-Marketplace.ps1') -CacheDir $CacheDir -Archiver $archiver
& node (Join-Path $root 'scripts/Bundle-Native.cjs')
if($LASTEXITCODE -ne 0){throw 'Provider bundle failed.'}
foreach($test in @('cache','matching','providers','native-renderer','seek-sync')){
    & node (Join-Path $root ('tests/'+$test+'.cjs'))
    if($LASTEXITCODE -ne 0){throw ('Regression failed: '+$test)}
}
& (Join-Path $root 'scripts/Build-Native.ps1') -OutputDir (Join-Path $build 'native-qa') -TestMode
& node (Join-Path $root 'tests/native-hud.cjs')
if($LASTEXITCODE -ne 0){throw 'Native host tests failed.'}
& (Join-Path $root 'scripts/Build-Native.ps1') -OutputDir $payload
& (Join-Path $root 'tests/bridge-install.ps1') -Payload (Join-Path $payload 'version.dll')
& (Join-Path $root 'tests/installer-dependency.ps1')
& (Join-Path $root 'tests/marketplace-install.ps1') -Archive (Join-Path $payload 'marketplace.zip')
& node (Join-Path $root 'tests/marketplace.cjs')
if($LASTEXITCODE -ne 0){throw 'Marketplace startup regression failed.'}
& node (Join-Path $root 'scripts/Archive-Source.cjs') (Join-Path $payload 'HUD-INTEGRATION-SOURCE.zip') $archiver
if($LASTEXITCODE -ne 0){throw 'Source archive failed.'}
foreach($name in @('ConsoleActions.ps1','Bridge.ps1','Dependency.ps1','Marketplace.ps1','marketplace-color.ini','spicetify-release.json')){Copy-Item -LiteralPath (Join-Path $root ('installer/'+$name)) -Destination $payload -Force}
foreach($name in @('HUD-LICENSE.txt','SPICETIFY-LICENSE','JSON-LICENSE.txt','MARKETPLACE-LICENSE.txt')){Copy-Item -LiteralPath (Join-Path $root ('licenses/'+$name)) -Destination $payload -Force}
foreach($name in @('README.md','NOTICE.txt','LICENSE')){Copy-Item -LiteralPath (Join-Path $root $name) -Destination $payload -Force}
foreach($name in @('spicetify-windows-x64.zip','spicetify-source-v2.46.4.tar.gz')){Copy-Item -LiteralPath (Join-Path $CacheDir $name) -Destination $payload -Force}
foreach($name in @('ConsoleActions.ps1','Bridge.ps1','Dependency.ps1')){ $file=Join-Path $payload $name;[IO.File]::WriteAllText($file,[IO.File]::ReadAllText($file),(New-Object Text.UTF8Encoding($true))) }
& (Join-Path $root 'scripts/Pack-Installer.ps1') -Offline:$Offline
if ($Offline) {
    # Run reflection/EXE checks in a separate host so assemblies do not lock later builds.
    & (Get-Process -Id $PID).Path -NoProfile -ExecutionPolicy Bypass -File (Join-Path $root 'tests/installer-package.ps1') -Standard (Join-Path $dist "Spotify-Lyrics-Setup-v$version.exe") -Offline (Join-Path $dist "Spotify-Lyrics-Setup-v$version-Offline.exe")
    if ($LASTEXITCODE -ne 0) { throw 'Packaged installer regression failed.' }
}
Write-Host ("Native $version build complete: "+$dist) -ForegroundColor Green
