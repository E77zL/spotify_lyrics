param([string]$Payload, [string]$LegacyPayload)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../installer/Bridge.ps1')
$fixture = Join-Path (Split-Path $Payload) ('install-qa-' + [Guid]::NewGuid().ToString('N'))
$spotify = Join-Path $fixture 'Spotify'
$runtime = Join-Path $fixture 'Runtime'
New-Item -ItemType Directory -Path $spotify,$runtime,(Join-Path $spotify 'LyricsHUD') -Force | Out-Null
$module = Join-Path $spotify 'version.dll'
$offsets = Join-Path $spotify 'LyricsHUD/lyric-offsets.json'
[IO.File]::WriteAllText($offsets,'{"keep":-500}')
if ($LegacyPayload) {
    Copy-Item -LiteralPath $LegacyPayload -Destination $module
    if (!(Test-LyricsBridgeOwned $module $runtime)) { throw 'Legacy bridge metadata was not recognized.' }
}
Install-LyricsBridge $spotify $runtime $Payload
if (!(Test-LyricsBridgeOwned $module $runtime)) { throw 'Fresh install ownership missing' }
$hash = (Get-FileHash -LiteralPath $module).Hash
$config = [IO.File]::ReadAllBytes((Join-Path $spotify 'LyricsHUD/bridge.ini'))
$stateFile = Join-Path $runtime 'bridge-install.json'
$state = [IO.File]::ReadAllBytes($stateFile)
Install-LyricsBridge $spotify $runtime $Payload
if ((Get-FileHash -LiteralPath $module).Hash -ne $hash) { throw 'Upgrade changed module' }
try { Install-LyricsBridge $spotify $runtime (Join-Path $fixture 'missing.dll'); throw 'Expected failure' }
catch { if ($_.Exception.Message -eq 'Expected failure') { throw } }
if ((Get-FileHash -LiteralPath $module).Hash -ne $hash) { throw 'Rollback lost previous module' }
if ([Convert]::ToBase64String([IO.File]::ReadAllBytes((Join-Path $spotify 'LyricsHUD/bridge.ini'))) -ne [Convert]::ToBase64String($config)) { throw 'Rollback changed config' }
$changed = Join-Path $fixture 'changed.dll'
[IO.File]::WriteAllBytes($changed,([IO.File]::ReadAllBytes($Payload) + [byte[]](1,2,3,4)))
$lock = [IO.File]::Open($stateFile,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
try {
    try { Install-LyricsBridge $spotify $runtime $changed; throw 'Expected metadata write failure' }
    catch { if ($_.Exception.Message -eq 'Expected metadata write failure') { throw } }
} finally { $lock.Dispose() }
if ((Get-FileHash -LiteralPath $module).Hash -ne $hash) { throw 'Late failure did not roll back module' }
if ([Convert]::ToBase64String([IO.File]::ReadAllBytes($stateFile)) -ne [Convert]::ToBase64String($state)) { throw 'Late failure corrupted ownership metadata' }
if ([Convert]::ToBase64String([IO.File]::ReadAllBytes((Join-Path $spotify 'LyricsHUD/bridge.ini'))) -ne [Convert]::ToBase64String($config)) { throw 'Late failure changed config' }
if (@(Get-ChildItem -LiteralPath $spotify,$runtime -Recurse -Filter '*.tmp').Count) { throw 'Temporary installation files leaked' }
Remove-LyricsBridge $spotify $runtime
if (Test-Path -LiteralPath $module) { throw 'Uninstall left module behind' }
if ([IO.File]::ReadAllText($offsets) -ne '{"keep":-500}') { throw 'Uninstall changed user offsets' }
[IO.File]::WriteAllText($module,'another plugin')
try { Install-LyricsBridge $spotify $runtime $Payload; throw 'Expected conflict' }
catch { if ($_.Exception.Message -eq 'Expected conflict') { throw } }
try { Remove-LyricsBridge $spotify $runtime; throw 'Expected conflict' }
catch { if ($_.Exception.Message -eq 'Expected conflict') { throw } }
if ([IO.File]::ReadAllText($module) -ne 'another plugin') { throw 'Conflict overwrote third-party module' }
Write-Host 'PASS bridge install, upgrade, late locked-file rollback, unchanged metadata, temporary cleanup, owned-only uninstall, retained offsets and third-party conflict'
