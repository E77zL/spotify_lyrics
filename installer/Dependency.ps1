# MIT. Only a first installation without an existing CLI needs this download.
function Get-SpicetifyArchive([string]$Directory) {
    $release = Get-Content -LiteralPath (Join-Path $Directory 'spicetify-release.json') -Raw | ConvertFrom-Json
    $archive = Join-Path $Directory 'spicetify-windows-x64.zip'
    if (!(Test-Path -LiteralPath $archive)) {
        $expectedUrl = "https://github.com/spicetify/cli/releases/download/$($release.version)/$($release.name)"
        if ($release.url -cne $expectedUrl -or $release.version -notmatch '^v\d+\.\d+\.\d+$' -or
            $release.name -notmatch '^spicetify-[\d.]+-windows-x64\.zip$' -or $release.sha256 -notmatch '^[a-f0-9]{64}$') {
            throw 'Invalid pinned Spicetify release.'
        }
        Write-Host "  首次安装：下载固定版本 Spicetify $($release.version)，完成后校验 SHA256。" -ForegroundColor DarkGray
        $temporary = $archive + '.' + [Guid]::NewGuid().ToString('N') + '.download'
        $previousTls = [Net.ServicePointManager]::SecurityProtocol
        try {
            [Net.ServicePointManager]::SecurityProtocol = $previousTls -bor [Net.SecurityProtocolType]::Tls12
            $download = @{ UseBasicParsing=$true; Uri=$release.url; OutFile=$temporary; TimeoutSec=120 }
            # Windows PowerShell does not read HTTPS_PROXY automatically.
            $proxy = $env:HTTPS_PROXY
            if (!$proxy) { $proxy = $env:HTTP_PROXY }
            if ($proxy) { $download.Proxy = $proxy }
            Invoke-WebRequest @download
            if ((Get-FileHash -LiteralPath $temporary -Algorithm SHA256).Hash -ne $release.sha256) {
                throw 'Spicetify SHA256 校验失败，未安装下载文件。'
            }
            Move-Item -LiteralPath $temporary -Destination $archive
        } catch {
            throw ("Spicetify 下载失败，可检查网络或使用 Offline 离线安装包。" + $_.Exception.Message)
        } finally {
            [Net.ServicePointManager]::SecurityProtocol = $previousTls
            if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Force }
        }
    }
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $release.sha256) {
        throw 'Spicetify SHA256 校验失败。'
    }
    return $archive
}
