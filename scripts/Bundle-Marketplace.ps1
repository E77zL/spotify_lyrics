param([string]$CacheDir,[string]$Archiver)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$dependencies=Get-Content -LiteralPath (Join-Path $root 'dependencies.lock.json') -Raw | ConvertFrom-Json
$matches=@($dependencies | Where-Object {$_.name -eq 'marketplace-v1.0.12.zip'})
if($matches.Count -ne 1){throw 'Marketplace dependency must have exactly one lock entry'}
$entry=$matches[0]
$archive=Join-Path $CacheDir $entry.name
if((Get-FileHash -LiteralPath $archive).Hash -ne $entry.sha256){throw 'Marketplace release SHA256 mismatch'}
$stage=Join-Path $root '.build/marketplace-bundle'
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Expand-Archive -LiteralPath $archive -DestinationPath $stage -Force
$app=Join-Path $stage 'marketplace-dist'
& node (Join-Path $PSScriptRoot 'Patch-Marketplace.cjs') (Join-Path $app 'extension.js')
if($LASTEXITCODE -ne 0){throw 'Marketplace patch failed'}
Copy-Item -LiteralPath (Join-Path $root 'licenses/MARKETPLACE-LICENSE.txt') -Destination $app -Force
Copy-Item -LiteralPath (Join-Path $root 'licenses/MARKETPLACE-THIRD-PARTY.txt') -Destination $app -Force
$output=Join-Path $root '.build/payload/marketplace.zip'
if(Test-Path -LiteralPath $output){Remove-Item -LiteralPath $output -Force}
Push-Location $app
try{
 & $Archiver a -tzip -mx=9 -bso0 $output index.js extension.js manifest.json style.css README.md MARKETPLACE-LICENSE.txt MARKETPLACE-THIRD-PARTY.txt
 if($LASTEXITCODE -ne 0){throw 'Marketplace packaging failed'}
}finally{Pop-Location}
