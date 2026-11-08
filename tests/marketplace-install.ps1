param([string]$Archive)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../installer/Bridge.ps1')
. (Join-Path $PSScriptRoot '../installer/Marketplace.ps1')
$fixture=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot ('../.build/marketplace-qa-'+[Guid]::NewGuid().ToString('N'))))
New-Item -ItemType Directory -Force -Path $fixture | Out-Null
try{
    Install-HudMarketplace $fixture $Archive
    $app=Join-Path $fixture 'CustomApps/marketplace'
    if(!(Test-Path -LiteralPath (Join-Path $app 'index.js'))){throw 'Marketplace missing'}
    $original=(Get-FileHash -LiteralPath (Join-Path $app 'extension.js')).Hash
    $extra=Join-Path $app 'user-file.txt';[IO.File]::WriteAllText($extra,'keep')
    Install-HudMarketplace $fixture $Archive
    if([IO.File]::ReadAllText($extra) -ne 'keep'){throw 'Upgrade changed unrelated file'}
    $locked=Join-Path $app 'style.css'
    $handle=[IO.File]::Open($locked,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    try{
        try{Install-HudMarketplace $fixture $Archive;throw 'Expected locked-file failure'}catch{if($_.Exception.Message -eq 'Expected locked-file failure'){throw}}
    }finally{$handle.Dispose()}
    if((Get-FileHash -LiteralPath (Join-Path $app 'extension.js')).Hash -ne $original){throw 'Failed update changed previous files'}
    [IO.File]::Delete((Join-Path $app '.lyrics-hud-marketplace.json'))
    [IO.File]::WriteAllText((Join-Path $app 'index.js'),'independent installation')
    Install-HudMarketplace $fixture $Archive
    if([IO.File]::ReadAllText((Join-Path $app 'index.js')) -ne 'independent installation'){throw 'Overwrote unrelated Marketplace'}
    Write-Host 'PASS Marketplace installation, upgrade, locked-file recovery, unrelated files and independent installation reuse'
}finally{
    $parent=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../.build')).TrimEnd('\')
    if([IO.Path]::GetDirectoryName($fixture) -ne $parent -or [IO.Path]::GetFileName($fixture) -notmatch '^marketplace-qa-[a-f0-9]{32}$'){throw 'Unsafe fixture cleanup'}
    Remove-Item -LiteralPath $fixture -Recurse -Force
}
