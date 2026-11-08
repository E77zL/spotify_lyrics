param([ValidateSet('Install','Uninstall','Diagnose','Verify','ExportSources')][string]$Action = 'Diagnose')
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
[Console]::OutputEncoding = New-Object Text.UTF8Encoding($false)
$installRoot = Join-Path $env:LOCALAPPDATA 'SpotifyLyricsCompanion'
$runKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
. (Join-Path $PSScriptRoot 'Bridge.ps1')
. (Join-Path $PSScriptRoot 'Dependency.ps1')
. (Join-Path $PSScriptRoot 'Marketplace.ps1')

function Write-Step([string]$Number, [string]$Message) { Write-Host "  [$Number] $Message" -ForegroundColor Green }
function Find-Spicetify {
    $command = Get-Command spicetify -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    foreach ($p in @("$env:LOCALAPPDATA\spicetify\spicetify.exe", "$env:APPDATA\spicetify\spicetify.exe", "$env:USERPROFILE\spicetify-cli\spicetify.exe")) { if (Test-Path -LiteralPath $p) { return $p } }
}
function Find-Spotify {
    if ($env:LYRICS_SETUP_GUI -eq '1' -and $env:LYRICS_SETUP_SPOTIFY_PATH) {
        $selected = $env:LYRICS_SETUP_SPOTIFY_PATH
        if (!(Test-Path -LiteralPath (Join-Path $selected 'Spotify.exe'))) { throw 'The selected folder does not contain Spotify.exe.' }
        return $selected
    }
    try {
        $saved = (Get-Content -LiteralPath (Join-Path $installRoot 'connection.json') -Raw | ConvertFrom-Json).spotifyPath
        if ($saved -and (Test-Path -LiteralPath (Join-Path $saved 'Spotify.exe'))) { return $saved }
    } catch { }
    foreach ($p in @("$env:APPDATA\Spotify", "$env:LOCALAPPDATA\Spotify", "$env:ProgramFiles\Spotify")) { if (Test-Path -LiteralPath (Join-Path $p 'Spotify.exe')) { return $p } }
}
function Test-IsElevated {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}
function Invoke-Spicetify {
    $cliArgs = @('--no-restart'); if ($script:IsElevated) { $cliArgs += '--bypass-admin' }; $cliArgs += $args
    & $script:Spicetify @cliArgs
    if ($LASTEXITCODE -ne 0) { throw "Spicetify failed ($LASTEXITCODE): $args" }
}
function Invoke-BackupApply {
    try {
        Invoke-Spicetify backup apply
    } catch {
        # Spicetify refuses `backup apply` when an earlier backup belongs to
        # an already modified client. Its documented recovery is exactly this
        # chain: restore the existing backup, create fresh preprocessed data,
        # then apply the configured extensions.
        Write-Host '  检测到 Spicetify 已有旧备份，自动执行 restore backup apply 修复。' -ForegroundColor Yellow
        Invoke-Spicetify restore backup apply
    }
}
function Resolve-SpicetifyConfigPath([object[]]$OutputLines) {
    $paths = @(foreach ($raw in $OutputLines) { foreach ($line in ([string]$raw -split "`r?`n")) {
        $clean = ([regex]::Replace($line, '\x1B\[[0-?]*[ -/]*[@-~]', '')).Trim().Trim('"')
        if ([IO.Path]::IsPathRooted($clean) -and [IO.Path]::GetFileName($clean) -eq 'config-xpui.ini') { [IO.Path]::GetFullPath($clean) }
    } })
    if (!$paths.Count) { throw 'Spicetify did not return a valid config path.' }; $paths[-1]
}
function Get-SpicetifyConfig([switch]$InitializeIfMissing) {
    $path = Resolve-SpicetifyConfigPath @(Invoke-Spicetify -c)
    if ($InitializeIfMissing -and !(Test-Path -LiteralPath $path)) { Invoke-Spicetify config spotify_path | Out-Host }
    if ($InitializeIfMissing -and !(Test-Path -LiteralPath $path)) { throw 'Spicetify failed to initialize its config.' }; $path
}
function Wait-SpotifyClosed {
    while (Get-Process Spotify -ErrorAction SilentlyContinue | Where-Object { $_.SessionId -eq [Diagnostics.Process]::GetCurrentProcess().SessionId }) {
        if ($env:LYRICS_SETUP_GUI -eq '1') { throw 'Close Spotify, including its system tray icon, then retry.' }
        Write-Host '  请退出 Spotify（包括托盘），按回车继续，或输入 Q 取消。' -ForegroundColor Yellow
        if ((Read-Host) -match '^[Qq]$') { throw '已取消。' }
    }
}
function Stop-Companion {
    $launcher = Join-Path $installRoot 'SpotifyLyricsLauncher.exe'
    if (Test-Path -LiteralPath $launcher) {
        $stopProcess = Start-Process -FilePath $launcher -ArgumentList '--stop' -WindowStyle Hidden -PassThru
        $stopProcess.WaitForExit(5000) | Out-Null
        for ($i = 0; $i -lt 8; $i++) {
            $running = @(Get-Process SpotifyLyricsLauncher -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $launcher })
            if (!$running.Count) { break }; Start-Sleep -Milliseconds 500
        }
        if ($running.Count) { throw '请退出当前 歌词联动程序后重试。' }
    }
}
function Disable-OldOverlay {
    Remove-ItemProperty -Path $runKey -Name 'NetEaseLyricsOverlay' -ErrorAction SilentlyContinue
    Get-Process NetEaseLyricsOverlay -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
}
function Ensure-Spicetify {
    $script:Spicetify = Find-Spicetify
    if ($script:Spicetify) { return }
    if ($env:PROCESSOR_ARCHITECTURE -ne 'AMD64') { throw '此包仅支持 Windows x64。' }
    $archive = Get-SpicetifyArchive $PSScriptRoot
    $correspondingSource = Get-SpicetifyArchive $PSScriptRoot -Source
    $stage = Join-Path $PSScriptRoot 'spicetify-unpacked'; Expand-Archive -LiteralPath $archive -DestinationPath $stage
    $binary = @(Get-ChildItem -LiteralPath $stage -Filter spicetify.exe -Recurse)
    if ($binary.Count -ne 1) { throw 'Spicetify archive structure is invalid.' }
    $dest = Join-Path $env:LOCALAPPDATA 'spicetify'; New-Item -ItemType Directory -Path $dest -Force | Out-Null
    Copy-Item -Path (Join-Path $binary[0].Directory.FullName '*') -Destination $dest -Recurse -Force
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'SPICETIFY-LICENSE'), $correspondingSource -Destination $dest -Force
    $script:Spicetify = Join-Path $dest 'spicetify.exe'
}

try {
    if ($Action -eq 'ExportSources') {
        $destination = $env:LYRICS_SETUP_EXPORT_PATH
        if (!$destination -or ![IO.Path]::IsPathRooted($destination)) { throw 'An absolute export directory is required.' }
        $names = @('spicetify-source-v2.46.4.tar.gz','HUD-LICENSE.txt','SPICETIFY-LICENSE','MARKETPLACE-LICENSE.txt','LICENSE','NOTICE.txt')
        foreach ($name in $names) {
            if (Test-Path -LiteralPath (Join-Path $destination $name)) { throw "File already exists: $name" }
        }
        $source = Get-SpicetifyArchive $PSScriptRoot -Source
        New-Item -ItemType Directory -Path $destination -Force | Out-Null
        foreach ($name in $names) { Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $destination -ErrorAction Stop }
        Write-Host 'Exported third-party corresponding source and licenses. Project source is a separate Release ZIP.'
        exit 0
    }
    if ($Action -eq 'Verify') {
        $tokens = $null; $errors = $null
        [System.Management.Automation.Language.Parser]::ParseFile($PSCommandPath, [ref]$tokens, [ref]$errors) | Out-Null
        if ($errors.Count) { throw ($errors | Out-String) }
        foreach ($name in @('version.dll','Bridge.ps1','Marketplace.ps1','marketplace.zip','marketplace-color.ini','spotify-lyrics-hud.js','HUD-LICENSE.txt','JSON-LICENSE.txt','MARKETPLACE-LICENSE.txt')) { if (!(Test-Path -LiteralPath (Join-Path $PSScriptRoot $name))) { throw "Missing payload $name" } }
        $bridgeInfo = (Get-Item -LiteralPath (Join-Path $PSScriptRoot 'version.dll')).VersionInfo
        if ($bridgeInfo.InternalName -ne 'SpotifyLyricsBridge') { throw 'Invalid Spotify process bridge.' }
        $release = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'spicetify-release.json') -Raw | ConvertFrom-Json
        $archive = Join-Path $PSScriptRoot 'spicetify-windows-x64.zip'
        if (Test-Path -LiteralPath $archive) { Get-SpicetifyArchive $PSScriptRoot | Out-Null }
        elseif ($release.sha256 -notmatch '^[a-f0-9]{64}$') { throw 'Invalid dependency hash.' }
        Write-Step 'OK' 'PowerShell syntax / native Spotify DLL / pinned Spicetify dependency'; exit 0
    }
    if ($Action -eq 'Diagnose') {
        Write-Step 'INFO' ('Spotify: ' + (Find-Spotify)); Write-Step 'INFO' ('Spicetify: ' + (Find-Spicetify))
        $spotify = Find-Spotify
        Write-Step 'INFO' ('Spotify process integration: ' + [bool]($spotify -and (Test-LyricsBridgeOwned (Join-Path $spotify 'version.dll') $installRoot)))
        Write-Host '  此检查不会修改文件或安装组件。'; exit 0
    }
    $script:IsElevated = Test-IsElevated
    if ($script:IsElevated) { Write-Host '  管理员窗口已支持，Spicetify 将自动使用 --bypass-admin。' }
    if ($Action -eq 'Uninstall') {
        Wait-SpotifyClosed
        Stop-Companion
        Remove-ItemProperty -Path $runKey -Name 'SpotifyLyricsCompanion' -ErrorAction SilentlyContinue
        $spotify = Find-Spotify
        if ($spotify) { Remove-LyricsBridge $spotify $installRoot }
        $script:Spicetify = Find-Spicetify
        if ($script:Spicetify) { Wait-SpotifyClosed; Invoke-Spicetify config extensions 'spotify-lyrics-hud.js-'; Invoke-Spicetify apply }
        Write-Host '  已停用 歌词联动和按钮。歌词设置、组件文件及其他扩展保留。' -ForegroundColor Green; exit 0
    }
    Write-Step '1/6' '检测 Spotify 官网桌面版'
    $spotify = Find-Spotify
    if (!$spotify) {
        if ($env:LYRICS_SETUP_GUI -eq '1') { throw 'Spotify was not found. Select Spotify.exe with the Spotify folder button, then retry.' }
        $spotify = (Read-Host '  输入包含 Spotify.exe 的目录').Trim().Trim('"')
    }
    if (!(Test-Path -LiteralPath (Join-Path $spotify 'Spotify.exe'))) { throw '未找到 Spotify.exe。' }
    $existingBridge = Join-Path $spotify 'version.dll'
    if ((Test-Path -LiteralPath $existingBridge) -and !(Test-LyricsBridgeOwned $existingBridge $installRoot)) {
        throw 'Spotify 目录内已有其它程序的 version.dll，未覆盖。请先移除冲突的插件再安装。'
    }
    Wait-SpotifyClosed
    Write-Step '2/6' '准备 Spicetify 并备份配置'
    Ensure-Spicetify
    $configPath = Get-SpicetifyConfig -InitializeIfMissing
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
    Copy-Item -LiteralPath $configPath -Destination ($configPath + '.hud-backup-' + $stamp)
    Write-Step '3/6' '停用旧歌词组件，安装 Spotify Lyrics 2.1.0'
    Stop-Companion; Disable-OldOverlay
    $originalHud = @(Get-Process 'Spotify悬浮歌词' -ErrorAction SilentlyContinue | Where-Object { $_.SessionId -eq [Diagnostics.Process]::GetCurrentProcess().SessionId })
    foreach ($process in $originalHud) {
        if (!$process.HasExited) {
            Stop-Process -Id $process.Id -Force
            if (!$process.WaitForExit(5000)) { throw '无法退出原歌词程序，请退出后重试。' }
        }
    }
    if (Test-Path $runKey) {
        $runValues = Get-ItemProperty -Path $runKey
        foreach ($property in $runValues.PSObject.Properties) {
            if ($property.Name -notmatch '^PS' -and [string]$property.Value -match 'Spotify悬浮歌词\.exe') { Remove-ItemProperty -Path $runKey -Name $property.Name }
        }
    }
    New-Item -ItemType Directory -Path $installRoot -Force | Out-Null
    # Retain a recoverable old runtime, but never launch it in the native version.
    $hudDir = Join-Path $installRoot 'HUD'
    if (Test-Path -LiteralPath $hudDir) { Move-Item -LiteralPath $hudDir -Destination (Join-Path $installRoot ('HUD.backup-' + $stamp)) }
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'HUD-LICENSE.txt'),(Join-Path $PSScriptRoot 'JSON-LICENSE.txt'),(Join-Path $PSScriptRoot 'NOTICE.txt') -Destination $installRoot -Force
    $connectionPath = Join-Path $installRoot 'connection.json'
    $connection = $null
    if (Test-Path -LiteralPath $connectionPath) { try { $connection = Get-Content -LiteralPath $connectionPath -Raw | ConvertFrom-Json } catch { } }
    if (!$connection -or [string]$connection.token -notmatch '^[a-f0-9]{64}$') {
        $bytes = New-Object byte[] 32; $random = [Security.Cryptography.RandomNumberGenerator]::Create()
        try { $random.GetBytes($bytes) } finally { $random.Dispose() }
        $connection = @{ port = 27946; token = ([BitConverter]::ToString($bytes).Replace('-','').ToLowerInvariant()) }
    }
    $connection = @{ port = 27946; token = [string]$connection.token; spotifyPath = $spotify }
    [IO.File]::WriteAllText($connectionPath, ($connection | ConvertTo-Json -Compress), (New-Object Text.UTF8Encoding($false)))
    Write-Step '4/6' '安装麦克风左侧的 UI 开关'
    $extensionsDir = Join-Path (Split-Path -Parent $configPath) 'Extensions'
    New-Item -ItemType Directory -Path $extensionsDir -Force | Out-Null
    $target = Join-Path $extensionsDir 'spotify-lyrics-hud.js'
    if (Test-Path -LiteralPath $target) { Copy-Item -LiteralPath $target -Destination ($target + '.backup-' + $stamp) }
    $extension = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'spotify-lyrics-hud.js')).Replace('__HUD_CONNECTION_TOKEN__', $connection.token)
    [IO.File]::WriteAllText($target, $extension, (New-Object Text.UTF8Encoding($false)))
    Invoke-Spicetify config spotify_path $spotify
    Invoke-Spicetify config extensions 'netease-lyrics.js-'
    Invoke-Spicetify config extensions 'spotify-lyrics-hud.js'
    Write-Host '  安装 Spotify 内部 Marketplace：主题、扩展和 CSS 小组件。' -ForegroundColor DarkGray
    $userData=Split-Path -Parent $configPath
    Install-HudMarketplace $userData (Join-Path $PSScriptRoot 'marketplace.zip')
    $themeRoot=Join-Path $userData 'Themes/marketplace'
    New-Item -ItemType Directory -Force -Path $themeRoot | Out-Null
    if(!(Test-Path -LiteralPath (Join-Path $themeRoot 'color.ini'))){Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'marketplace-color.ini') -Destination (Join-Path $themeRoot 'color.ini')}
    $theme=@(Invoke-Spicetify config current_theme | ForEach-Object {([regex]::Replace([string]$_,'\x1B\[[0-?]*[ -/]*[@-~]','')).Trim()} | Where-Object {$_})
    if(!$theme.Count){Invoke-Spicetify config current_theme marketplace}
    Invoke-Spicetify config custom_apps marketplace
    Write-Step '5/6' '应用 Spotify 按钮'
    Invoke-BackupApply
    Write-Step '6/6' '安装 Spotify 进程内歌词联动模块'
    Install-LyricsBridge $spotify $installRoot (Join-Path $PSScriptRoot 'version.dll')
    Remove-ItemProperty -Path $runKey -Name 'SpotifyLyricsCompanion' -ErrorAction SilentlyContinue
    $legacy = Join-Path $installRoot 'SpotifyLyricsLauncher.exe'
    if (Test-Path -LiteralPath $legacy) { Remove-Item -LiteralPath $legacy -Force }
    Write-Host '  安装完成。打开 Spotify，点击麦克风左侧 A 按钮显示 / 隐藏 UI。' -ForegroundColor Green
    Write-Host '  Spotify 内打开 Marketplace 或头像菜单中的 Customization，选择主题、扩展和小组件。';
    Write-Host '  歌词窗口与 Spotify 使用同一进程。悬停歌词，通过校时数字、Aa、透明度和颜色入口调节；拖动边缘缩放。'; exit 0
} catch { Write-Host ('  [ERROR] ' + $_.Exception.Message) -ForegroundColor Red; exit 1 }
