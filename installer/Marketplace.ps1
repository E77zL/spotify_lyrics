# MIT. Install the bundled Marketplace without replacing an unrelated installation.
function Install-HudMarketplace([string]$UserData,[string]$Archive) {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $target=Join-Path $UserData 'CustomApps/marketplace'
    $marker=Join-Path $target '.lyrics-hud-marketplace.json'
    $owned=$false
    if(Test-Path -LiteralPath $marker){
        try{$owned=(Get-Content -LiteralPath $marker -Raw | ConvertFrom-Json).owner -eq 'SpotifyLyricsBetterHUD'}catch{}
    }
    if((Test-Path -LiteralPath $target) -and !$owned){
        foreach($required in @('manifest.json','index.js','extension.js')){
            if(!(Test-Path -LiteralPath (Join-Path $target $required))){throw 'Existing Marketplace is incomplete; it was not overwritten.'}
        }
        return # Reuse a separately installed Marketplace, retaining its files/settings.
    }
    $expected=@('index.js','extension.js','manifest.json','style.css','README.md','MARKETPLACE-LICENSE.txt','MARKETPLACE-THIRD-PARTY.txt')
    $contents=[IO.Compression.ZipFile]::OpenRead($Archive)
    $changes=@()
    try{
        $names=@($contents.Entries | ForEach-Object {$_.FullName})
        if($names.Count -ne $expected.Count -or @(Compare-Object $expected $names).Count){throw 'Unexpected Marketplace archive contents'}
        foreach($entry in $contents.Entries){
            if($entry.Length -gt 4MB){throw 'Marketplace entry is too large'}
            $inputStream=$entry.Open();$memory=New-Object IO.MemoryStream
            try{$inputStream.CopyTo($memory);$bytes=$memory.ToArray()}finally{$inputStream.Dispose();$memory.Dispose()}
            $changes+=@{file=(Join-Path $target $entry.Name);content=$bytes}
        }
    }finally{$contents.Dispose()}
    $changes+=@{file=$marker;content=[Text.Encoding]::UTF8.GetBytes('{"owner":"SpotifyLyricsBetterHUD","version":"1.0.12"}')}
    New-Item -ItemType Directory -Force -Path $target | Out-Null
    foreach($change in $changes){$change.previous=if(Test-Path -LiteralPath $change.file){[IO.File]::ReadAllBytes($change.file)}else{$null}}
    $applied=0
    try{
        foreach($change in $changes){Set-LyricsBridgeFile $change.file $change.content;$applied++}
    }catch{
        $failure=$_
        for($i=$applied-1;$i -ge 0;$i--){
            $change=$changes[$i]
            if($null -eq $change.previous){[IO.File]::Delete($change.file)}else{Set-LyricsBridgeFile $change.file $change.previous}
        }
        throw $failure
    }
}
