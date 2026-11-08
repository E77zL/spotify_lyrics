param([Parameter(Mandatory=$true)][string]$Standard, [Parameter(Mandatory=$true)][string]$Offline, [switch]$DownloadSource)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$fixture = Join-Path $root ('.build/installer-package-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
$before = @(Get-ChildItem -LiteralPath ([IO.Path]::GetTempPath()) -Directory -Filter 'SpotifyNetEaseSetup-*' | ForEach-Object FullName)
function Run-Setup([string]$File, [string[]]$Arguments, [int]$Expected=0) {
    $process = Start-Process -FilePath $File -ArgumentList $Arguments -WindowStyle Hidden -Wait -PassThru `
        -RedirectStandardOutput (Join-Path $fixture 'stdout.txt') -RedirectStandardError (Join-Path $fixture 'stderr.txt')
    if ($process.ExitCode -ne $Expected) {
        throw ('Setup exited ' + $process.ExitCode + ': ' + [IO.File]::ReadAllText((Join-Path $fixture 'stdout.txt')) + [IO.File]::ReadAllText((Join-Path $fixture 'stderr.txt')))
    }
}
try {
    foreach ($file in @($Standard,$Offline)) {
        $file = [IO.Path]::GetFullPath($file)
        $assembly = [Reflection.Assembly]::LoadFile($file)
        $type = $assembly.GetType('Installer', $true)
        $flags = [Reflection.BindingFlags]'NonPublic,Static'
        $type.GetMethod('LoadManifest',$flags).Invoke($null,@()) | Out-Null
        $resources = @($assembly.GetManifestResourceNames())
        if ($resources -contains 'payload.HUD-INTEGRATION-SOURCE.zip') { throw 'Duplicate project source inflates the installer.' }
        foreach ($action in @('Diagnose','Uninstall')) {
            $names = @($type.GetMethod('ActionResources',$flags).Invoke($null,@($action)))
            if ($names.Count -ne 4 -or @($names | Where-Object { $_ -notlike '*.ps1' }).Count) { throw 'Read-only/disable staging contains unrelated payloads.' }
        }
        $sourceEmbedded = $resources -contains 'payload.spicetify-source-v2.46.4.tar.gz'
        if ($sourceEmbedded -ne ($file -eq [IO.Path]::GetFullPath($Offline))) { throw 'Unexpected source packaging variant.' }
        Run-Setup $file @('--diagnose')
        if ($sourceEmbedded -or $DownloadSource) {
            $destination = Join-Path $fixture ([IO.Path]::GetFileNameWithoutExtension($file) + ' source export')
            Run-Setup $file @('--export-source', ('"' + $destination + '"'))
            $source = Join-Path $destination 'spicetify-source-v2.46.4.tar.gz'
            $hash = (Get-FileHash -LiteralPath $source).Hash
            if ($hash -ne '28d0e8bb3438ec0138e4005429a1d5825b355e11f40694584733785714d45d11') { throw 'Exported corresponding source differs from the pinned release.' }
            foreach ($name in @('LICENSE','NOTICE.txt','SPICETIFY-LICENSE','HUD-LICENSE.txt','MARKETPLACE-LICENSE.txt')) {
                if (!(Test-Path -LiteralPath (Join-Path $destination $name))) { throw ('Missing exported license: ' + $name) }
            }
            Run-Setup $file @('--export-source', ('"' + $destination + '"')) 1
            if ((Get-FileHash -LiteralPath $source).Hash -ne $hash) { throw 'Repeated export overwrote existing files.' }
        }
    }
    $leaked = @(Get-ChildItem -LiteralPath ([IO.Path]::GetTempPath()) -Directory -Filter 'SpotifyNetEaseSetup-*' | Where-Object { $_.FullName -notin $before })
    if ($leaked.Count) { throw 'Installer left temporary staging directories.' }
    Write-Host 'PASS real standard/offline diagnostics, minimal action staging, optional source payloads, verified source/license export, overwrite rejection and temp cleanup'
} finally {
    $resolved = [IO.Path]::GetFullPath($fixture)
    if ([IO.Path]::GetDirectoryName($resolved) -ne (Join-Path $root '.build') -or [IO.Path]::GetFileName($resolved) -notmatch '^installer-package-[a-f0-9]{32}$') { throw 'Unsafe package fixture cleanup path.' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
