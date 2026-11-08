$ErrorActionPreference = 'Stop'

function Test-LyricsBridgeOwned([string]$File, [string]$InstallRoot) {
    if (!(Test-Path -LiteralPath $File)) { return $false }
    try {
        $state = Get-Content -LiteralPath (Join-Path $InstallRoot 'bridge-install.json') -Raw | ConvertFrom-Json
        if ($state.module -eq [IO.Path]::GetFullPath($File) -and $state.sha256 -eq (Get-FileHash -LiteralPath $File -Algorithm SHA256).Hash) { return $true }
    } catch { }
    $info = (Get-Item -LiteralPath $File).VersionInfo
    return $info.InternalName -eq 'SpotifyLyricsBridge' -and $info.ProductName -eq 'Spotify Lyrics Better HUD'
}

function Set-LyricsBridgeFile([string]$File, [byte[]]$Content) {
    $temporary = $File + '.' + [Guid]::NewGuid().ToString('N') + '.tmp'
    try {
        [IO.File]::WriteAllBytes($temporary,$Content)
        if (Test-Path -LiteralPath $File) { [IO.File]::Replace($temporary,$File,[NullString]::Value) }
        else { [IO.File]::Move($temporary,$File) }
    } finally {
        if (Test-Path -LiteralPath $temporary) { [IO.File]::Delete($temporary) }
    }
}

function Install-LyricsBridge([string]$SpotifyRoot, [string]$InstallRoot, [string]$Payload) {
    $module = Join-Path $SpotifyRoot 'version.dll'
    if ((Test-Path -LiteralPath $module) -and !(Test-LyricsBridgeOwned $module $InstallRoot)) {
        throw 'Spotify 目录内已有其它程序的 version.dll，未覆盖。请先移除冲突的插件再安装。'
    }
    $settings = Join-Path $SpotifyRoot 'LyricsHUD'
    $config = Join-Path $settings 'bridge.ini'
    $info = (Get-Item -LiteralPath $Payload).VersionInfo
    if ($info.InternalName -ne 'SpotifyLyricsBridge' -or $info.ProductName -ne 'Spotify Lyrics Better HUD') { throw 'Invalid Spotify process bridge payload.' }
    New-Item -ItemType Directory -Path $settings,$InstallRoot -Force | Out-Null
    $state = @{ version = $info.ProductVersion; module = [IO.Path]::GetFullPath($module); sha256 = (Get-FileHash -LiteralPath $Payload -Algorithm SHA256).Hash }
    $changes = @(
        @{ file = $module; content = [IO.File]::ReadAllBytes($Payload) },
        # Win32 INI APIs require a UTF-16 BOM for arbitrary Windows user paths.
        @{ file = $config; content = [Text.Encoding]::Unicode.GetPreamble() + [Text.Encoding]::Unicode.GetBytes("[LyricsHUD]`r`nEnabled=1`r`nInstallRoot=$InstallRoot`r`n") },
        @{ file = (Join-Path $InstallRoot 'bridge-install.json'); content = [Text.Encoding]::UTF8.GetBytes(($state | ConvertTo-Json -Compress)) }
    )
    foreach ($change in $changes) {
        $change.previous = if (Test-Path -LiteralPath $change.file) { [IO.File]::ReadAllBytes($change.file) } else { $null }
    }
    $applied = 0
    try {
        foreach ($change in $changes) { Set-LyricsBridgeFile $change.file $change.content; $applied++ }
    } catch {
        $failure = $_
        $recovery = $null
        for ($i = $applied - 1; $i -ge 0; $i--) {
            $change = $changes[$i]
            try {
                if ($null -eq $change.previous) { [IO.File]::Delete($change.file) }
                else { Set-LyricsBridgeFile $change.file $change.previous }
            } catch {
                # Preserve the old bytes if another process prevents rollback.
                if (!$recovery) { $recovery = Join-Path $InstallRoot ('bridge-recovery-' + [Guid]::NewGuid().ToString('N')); [IO.Directory]::CreateDirectory($recovery) | Out-Null }
                if ($null -ne $change.previous) { [IO.File]::WriteAllBytes((Join-Path $recovery ([IO.Path]::GetFileName($change.file))),$change.previous) }
                [IO.File]::AppendAllText((Join-Path $recovery 'restore.txt'),($change.file + "`r`n"))
            }
        }
        if ($recovery) { throw ($failure.Exception.Message + ' Rollback backup: ' + $recovery) }
        throw $failure
    }
}

function Remove-LyricsBridge([string]$SpotifyRoot, [string]$InstallRoot) {
    $module = Join-Path $SpotifyRoot 'version.dll'
    if (Test-Path -LiteralPath $module) {
        if (!(Test-LyricsBridgeOwned $module $InstallRoot)) { throw 'version.dll 已被其它程序更改，未删除。' }
        Remove-Item -LiteralPath $module -Force
    }
    $config = Join-Path $SpotifyRoot 'LyricsHUD\bridge.ini'
    if (Test-Path -LiteralPath $config) { Remove-Item -LiteralPath $config -Force }
    $state = Join-Path $InstallRoot 'bridge-install.json'
    if (Test-Path -LiteralPath $state) { Remove-Item -LiteralPath $state -Force }
    # Deliberately retain per-track lyric-offsets.json and the user's HUD settings.
}
