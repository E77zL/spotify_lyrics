# MIT. Fetch only missing, pinned dependencies; never check for updates.
function Get-SpicetifyArchive([string]$Directory, [switch]$Source) {
    $release = Get-Content -LiteralPath (Join-Path $Directory 'spicetify-release.json') -Raw | ConvertFrom-Json
    if ($release.version -notmatch '^v\d+\.\d+\.\d+$') { throw 'Invalid pinned Spicetify release.' }
    $name = 'spicetify-windows-x64.zip'
    $url = $release.url
    $hash = $release.sha256
    $expectedUrl = "https://github.com/spicetify/cli/releases/download/$($release.version)/$($release.name)"
    if ($Source) {
        $name = "spicetify-source-$($release.version).tar.gz"
        $url = $release.sourceUrl
        $hash = $release.sourceSha256
        $expectedUrl = "https://codeload.github.com/spicetify/cli/legacy.tar.gz/refs/tags/$($release.version)"
    } elseif ($release.name -notmatch '^spicetify-[\d.]+-windows-x64\.zip$') {
        throw 'Invalid pinned Spicetify release.'
    }
    if ($url -cne $expectedUrl -or $hash -notmatch '^[a-f0-9]{64}$') { throw 'Invalid pinned Spicetify release.' }
    $archive = Join-Path $Directory $name
    if (!(Test-Path -LiteralPath $archive)) {
        Write-Host "  Downloading pinned Spicetify $($release.version): $name (SHA256 verified)." -ForegroundColor DarkGray
        $temporary = $archive + '.' + [Guid]::NewGuid().ToString('N') + '.download'
        $previousTls = [Net.ServicePointManager]::SecurityProtocol
        try {
            [Net.ServicePointManager]::SecurityProtocol = $previousTls -bor [Net.SecurityProtocolType]::Tls12
            $download = @{ UseBasicParsing=$true; Uri=$url; OutFile=$temporary; TimeoutSec=120 }
            # Windows PowerShell does not read HTTPS_PROXY automatically.
            $proxy = $env:HTTPS_PROXY
            if (!$proxy) { $proxy = $env:HTTP_PROXY }
            if ($proxy) { $download.Proxy = $proxy }
            Invoke-WebRequest @download
            if ((Get-FileHash -LiteralPath $temporary -Algorithm SHA256).Hash -ne $hash) {
                throw 'Spicetify SHA256 mismatch; download was not installed.'
            }
            Move-Item -LiteralPath $temporary -Destination $archive
        } catch {
            throw ("Spicetify download failed. Check the network or use the Offline installer. " + $_.Exception.Message)
        } finally {
            [Net.ServicePointManager]::SecurityProtocol = $previousTls
            if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Force }
        }
    }
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $hash) { throw 'Spicetify SHA256 mismatch.' }
    return $archive
}
