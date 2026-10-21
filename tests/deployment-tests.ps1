param([Parameter(Mandatory=$true)][string]$TestBase)
$ErrorActionPreference='Stop'
$TestBase=[IO.Path]::GetFullPath($TestBase)
New-Item -ItemType Directory -Path $TestBase -Force|Out-Null
$source=Split-Path $PSScriptRoot -Parent
$csc=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$sources=@('Launcher.cs','Startup.cs','ModePolicy.cs','Deployment.cs')|ForEach-Object{Join-Path $source $_}
foreach($name in @('StartMenuFreeLayout','Uninstall')) {
 $defines='LIFECYCLE_TEST';if($name -eq 'Uninstall'){$defines+=',UNINSTALLER'}
 & $csc /nologo /target:winexe /platform:x64 "/define:$defines" "/out:$TestBase\$name.exe" /reference:System.Windows.Forms.dll /reference:System.Drawing.dll @sources
 if($LASTEXITCODE -ne 0){throw 'Test build failed'}
}
Copy-Item -LiteralPath (Join-Path $source 'FreeLayout.dll'),(Join-Path $source 'README.md') -Destination $TestBase
$package=Join-Path $TestBase 'Test.msi'
& (Join-Path $source 'package-msi.ps1') -Payload $TestBase -Output $package -Test
$product='StartMenuFreeLayout.LifecycleTest'
$code='{D6446F23-B73B-4355-A5C9-3B737225E03C}'
$root=Join-Path $env:LOCALAPPDATA $product
$productKey="Software\$product"
$runKey='Software\Microsoft\Windows\CurrentVersion\Run'
$realRun=(Get-ItemProperty -LiteralPath "HKCU:\$runKey" -Name StartMenuFreeLayout -ErrorAction SilentlyContinue).StartMenuFreeLayout
function Assert($condition,[string]$message){if(-not $condition){throw $message}}
function Run-Program([string]$file,[string]$arguments) {
 $start=New-Object Diagnostics.ProcessStartInfo($file,$arguments)
 $start.UseShellExecute=$false;$start.CreateNoWindow=$true;$start.WindowStyle='Hidden';$start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
 $process=[Diagnostics.Process]::Start($start)
 if(-not $process.WaitForExit(60000)){throw "Command timeout: $arguments"}
 $output=$process.StandardOutput.ReadToEnd();$errorText=$process.StandardError.ReadToEnd()
 $result=[PSCustomObject]@{Code=$process.ExitCode;Output=$output;Error=$errorText};$process.Dispose();return $result
}
Assert (-not (Test-Path -LiteralPath $root)) 'Test root already exists; inspect rather than deleting it automatically'
$import=Join-Path $TestBase 'layout.ini'
[IO.File]::WriteAllText($import,"[Settings]`r`nMode=enable`r`nEditMode=2`r`nLastMode=free`r`nGridRows=7`r`n[Cells]`r`npreserved=5,6`r`n[Testing]`r`nFolderMove=must-not-replay`r`n",[Text.Encoding]::Unicode)
$msiexec=Join-Path $env:WINDIR 'System32\msiexec.exe'
$result=Run-Program $msiexec ('/i "'+$package+'" /qn /norestart IMPORTCONFIG="'+$import+'" /L*v "'+(Join-Path $TestBase 'install.log')+'"')
Assert ($result.Code -eq 0) "MSI install failed: $($result.Code)"
'PASS: standard per-user MSI installation'
$installed=Join-Path $root 'App\3.4.0\StartMenuFreeLayout.exe'
Assert (Test-Path -LiteralPath $installed) 'Installed app missing'
$config=Join-Path $root 'Data\layout.ini'
$text=[IO.File]::ReadAllText($config)
Assert ($text.Contains('preserved=5,6')) 'Migration lost layout'
Assert (-not $text.Contains('must-not-replay')) 'Migration replayed diagnostic request'
Assert (Test-Path -LiteralPath (Join-Path $root 'Data\selftest.ok')) 'In-context mode/startup self-test failed'
$moved=Join-Path $TestBase 'moved setup.msi'
Move-Item -LiteralPath $package -Destination $moved
[IO.File]::WriteAllText($import,"[Settings]`r`nLastMode=system`r`n")
Assert ([IO.File]::ReadAllText($config).Contains('LastMode=free')) 'Source config affects managed state'
Assert (Test-Path -LiteralPath $installed) 'Moving setup removed installed app'
'PASS: setup can move; managed layout and mode remain independent'
'PASS: in-context self-test switched mode and verified stable startup path'
$outside=Join-Path $TestBase 'outside-preserve';New-Item -ItemType Directory -Path $outside|Out-Null
[IO.File]::WriteAllText((Join-Path $outside 'keep.txt'),'keep')
$junction=Join-Path $root 'do-not-follow'
New-Item -ItemType Junction -Path $junction -Target $outside|Out-Null
$result=Run-Program $msiexec ('/x '+$code+' /qn /norestart /L*v "'+(Join-Path $TestBase 'refuse.log')+'"')
Assert ($result.Code -ne 0) 'Uninstall followed a junction'
Assert (Test-Path -LiteralPath (Join-Path $outside 'keep.txt')) 'Outside file removed'
Assert (Test-Path -LiteralPath $installed) 'Failed uninstall removed app'
Remove-Item -LiteralPath $junction -Force
'PASS: uninstall rejects junction and preserves outside data'
[IO.File]::WriteAllText((Join-Path $root 'Data\cache.tmp'),'cache')
[IO.File]::WriteAllText((Join-Path $root 'Data\extension.log'),'log')
$result=Run-Program $msiexec ('/x '+$code+' /qn /norestart /L*v "'+(Join-Path $TestBase 'uninstall.log')+'"')
Assert ($result.Code -eq 0) "MSI uninstall failed: $($result.Code)"
Assert (-not (Test-Path -LiteralPath $root)) 'Managed files remained after uninstall'
$audit=Join-Path $env:TEMP ($product+'.audit')
Assert ((Get-Content -LiteralPath $audit -Raw).Contains('exists=False startup=0')) 'In-context private registry cleanup failed'
Remove-Item -LiteralPath $audit
Assert (-not (Test-Path -LiteralPath "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\$code")) 'MSI uninstall registry remained'

$realRunAfter=(Get-ItemProperty -LiteralPath "HKCU:\$runKey" -Name StartMenuFreeLayout -ErrorAction SilentlyContinue).StartMenuFreeLayout
Assert ($realRunAfter -eq $realRun) 'Real startup changed during isolated test'
Assert (Test-Path -LiteralPath (Join-Path $outside 'keep.txt')) 'Unrelated data removed'
Assert (Test-Path -LiteralPath $moved) 'User-held setup was removed'
'PASS: complete managed files/data/cache/log/startup/registry cleanup; source files, outside data and real startup preserved.'
