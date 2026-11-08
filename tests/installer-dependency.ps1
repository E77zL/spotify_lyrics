$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../installer/Dependency.ps1')
$fixture = Join-Path $PSScriptRoot ('../.build/dependency-qa-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
$archive = Join-Path $fixture 'spicetify-windows-x64.zip'
$metadata = Join-Path $fixture 'spicetify-release.json'
$content = [Text.Encoding]::UTF8.GetBytes('verified dependency fixture')
$script:downloadCount = 0
$script:downloadFailure = $false
function Invoke-WebRequest {
    param($Uri, $OutFile, $TimeoutSec, $Proxy, [switch]$UseBasicParsing)
    $script:downloadCount++
    [IO.File]::WriteAllBytes($OutFile, $content)
    if ($script:downloadFailure) { throw 'Simulated connection reset' }
}
try {
    [IO.File]::WriteAllBytes($archive, $content)
    $release = @{ version='v2.46.4'; name='spicetify-2.46.4-windows-x64.zip';
        url='https://github.com/spicetify/cli/releases/download/v2.46.4/spicetify-2.46.4-windows-x64.zip';
        sha256=(Get-FileHash -LiteralPath $archive).Hash.ToLowerInvariant() }
    $release | ConvertTo-Json | Set-Content -LiteralPath $metadata
    Get-SpicetifyArchive $fixture | Out-Null
    if ($downloadCount -ne 0) { throw 'Bundled dependency should not download.' }
    Remove-Item -LiteralPath $archive
    Get-SpicetifyArchive $fixture | Out-Null
    if ($downloadCount -ne 1) { throw 'Missing dependency should download once.' }
    [IO.File]::WriteAllText($archive, 'corrupt')
    try { Get-SpicetifyArchive $fixture; throw 'Expected checksum rejection' }
    catch { if ($_.Exception.Message -notmatch 'SHA256') { throw } }
    Remove-Item -LiteralPath $archive
    $release.sha256 = '0' * 64
    $release | ConvertTo-Json | Set-Content -LiteralPath $metadata
    try { Get-SpicetifyArchive $fixture; throw 'Expected download checksum rejection' }
    catch { if ($_.Exception.Message -notmatch 'SHA256') { throw } }
    if (Test-Path -LiteralPath $archive) { throw 'Invalid download was published.' }
    $script:downloadFailure = $true
    try { Get-SpicetifyArchive $fixture; throw 'Expected download failure' }
    catch { if ($_.Exception.Message -notmatch 'Offline') { throw } }
    if (@(Get-ChildItem -LiteralPath $fixture -Filter '*.download').Count) { throw 'Partial download leaked.' }
    $release.url = 'http://example.com/invalid.zip'
    $release | ConvertTo-Json | Set-Content -LiteralPath $metadata
    try { Get-SpicetifyArchive $fixture; throw 'Expected URL rejection' }
    catch { if ($_.Exception.Message -notmatch 'Invalid pinned') { throw } }
    Write-Host 'PASS offline reuse, pinned first-install download, hash rejection, partial cleanup and URL validation'
} finally {
    $resolved = [IO.Path]::GetFullPath($fixture)
    $expectedParent = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../.build')).TrimEnd('\')
    if ([IO.Path]::GetDirectoryName($resolved) -ne $expectedParent -or
        [IO.Path]::GetFileName($resolved) -notmatch '^dependency-qa-[a-f0-9]{32}$') { throw 'Unsafe fixture cleanup path.' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
