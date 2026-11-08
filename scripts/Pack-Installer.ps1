param([switch]$Offline)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$version=(Get-Content -LiteralPath (Join-Path $root 'package.json') -Raw | ConvertFrom-Json).version
$payload=Join-Path $root '.build/payload'
$dist=Join-Path $root 'dist'
$packed=Join-Path $root '.build/packed'
$files=@('ConsoleActions.ps1','Bridge.ps1','Dependency.ps1','Marketplace.ps1','marketplace.zip','marketplace-color.ini','MARKETPLACE-LICENSE.txt','spotify-lyrics-hud.js','version.dll','HUD-LICENSE.txt','JSON-LICENSE.txt','spicetify-windows-x64.zip','spicetify-release.json','SPICETIFY-LICENSE','LICENSE','README.md','spicetify-source-v2.46.4.tar.gz','NOTICE.txt')
$hashes=foreach($name in $files){(Get-FileHash -LiteralPath (Join-Path $payload $name)).Hash.ToLowerInvariant()+'  '+$name}
[IO.File]::WriteAllLines((Join-Path $payload 'manifest.sha256'),$hashes,(New-Object Text.UTF8Encoding($false)))
New-Item -ItemType Directory -Path $packed,$dist -Force | Out-Null
# Use the OS decoder and bounded stream copies, without shipping an archiver/runtime.
foreach($name in $files+@('manifest.sha256')){
    if(!$Offline -and $name -in @('spicetify-windows-x64.zip','spicetify-source-v2.46.4.tar.gz')){continue}
    $source=[IO.File]::OpenRead((Join-Path $payload $name))
    $destination=[IO.File]::Create((Join-Path $packed $name))
    try {
        $compress=New-Object IO.Compression.DeflateStream($destination,[IO.Compression.CompressionLevel]::Optimal,$true)
        try{$source.CopyTo($compress)}finally{$compress.Dispose()}
    }finally{$source.Dispose();$destination.Dispose()}
}
$compiler=Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
$variants=@('');if($Offline){$variants+='-Offline'}
foreach($suffix in $variants){
    $output=Join-Path $dist "Spotify-Lyrics-Setup-v$version$suffix.exe"
    $compilerArgs=@('/nologo','/target:winexe','/platform:x64','/optimize+','/debug-','/codepage:65001','/utf8output',('/win32manifest:'+(Join-Path $root 'installer/installer.manifest')),('/out:'+$output))
    $framework=Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319'
    foreach($assembly in @('WPF/PresentationFramework.dll','WPF/PresentationCore.dll','WPF/WindowsBase.dll','System.Xaml.dll')){
        $compilerArgs+='/reference:'+(Join-Path $framework $assembly)
    }
    $compilerArgs+='/resource:'+(Join-Path $root 'installer/SetupWindow.xaml')+',setup.window.xaml'
    $compilerArgs+='/resource:'+(Join-Path $root 'installer/SetupStrings.xml')+',setup.strings.xml'
    foreach($name in $files+@('manifest.sha256')){
        if(!$suffix -and $name -in @('spicetify-windows-x64.zip','spicetify-source-v2.46.4.tar.gz')){continue}
        $compilerArgs+='/resource:'+(Join-Path $packed $name)+',payload.'+$name
    }
    $compilerArgs+=Join-Path $root 'installer/Installer.cs'
    $compilerArgs+=Join-Path $root 'installer/SetupWindow.cs'
    $compilerArgs+=Join-Path $root 'installer/SetupText.cs'
    & $compiler @compilerArgs
    if($LASTEXITCODE -ne 0){throw 'Installer compilation failed.'}
    # Wait explicitly for the GUI-subsystem executable, including its CLI checks.
    foreach($check in @('--self-test','--ui-self-test')){
        $test=Start-Process -FilePath $output -ArgumentList $check -WindowStyle Hidden -Wait -PassThru
        if($test.ExitCode -ne 0){throw ('Installer check failed: '+$check)}
    }
    Write-Host ("Packed "+[IO.Path]::GetFileName($output)+": "+(Get-Item -LiteralPath $output).Length+' bytes')
}
Copy-Item -LiteralPath (Join-Path $payload 'HUD-INTEGRATION-SOURCE.zip') -Destination (Join-Path $dist "Spotify-Lyrics-Source-v$version.zip") -Force
Copy-Item -LiteralPath (Join-Path $payload 'spicetify-source-v2.46.4.tar.gz') -Destination $dist -Force
$releaseFiles=@("Spotify-Lyrics-Setup-v$version.exe","Spotify-Lyrics-Setup-v$version-Offline.exe","Spotify-Lyrics-Source-v$version.zip",'spicetify-source-v2.46.4.tar.gz') | Where-Object {Test-Path -LiteralPath (Join-Path $dist $_)}
$releaseHashes=foreach($name in $releaseFiles){(Get-FileHash -LiteralPath (Join-Path $dist $name)).Hash.ToLowerInvariant()+'  '+$name}
[IO.File]::WriteAllLines((Join-Path $dist 'SHA256SUMS.txt'),$releaseHashes,(New-Object Text.UTF8Encoding($false)))
