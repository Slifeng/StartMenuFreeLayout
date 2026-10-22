param([string]$Payload=$PSScriptRoot,[string]$Output=(Join-Path $PSScriptRoot 'StartMenuFreeLayout-v3.4.0-x64.msi'),[switch]$Test)
$ErrorActionPreference='Stop'
$Payload=[IO.Path]::GetFullPath($Payload);$Output=[IO.Path]::GetFullPath($Output)
$product='StartMenuFreeLayout';$code='{CB6777EC-54F7-4BD5-846A-A9645B219AD2}'
if($Test){$product+='.LifecycleTest';$code='{D6446F23-B73B-4355-A5C9-3B737225E03C}'}
$stage=Join-Path (Split-Path $Output -Parent) ('msi-build-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage|Out-Null
$files=@('StartMenuFreeLayout.exe','FreeLayout.dll','Uninstall.exe','README.md')
$ddf=@('.OPTION EXPLICIT','.Set CabinetNameTemplate=payload.cab',('.Set DiskDirectoryTemplate="'+$stage+'"'),'.Set CompressionType=MSZIP','.Set Cabinet=on','.Set Compress=on',('.Set InfFileName="'+(Join-Path $stage 'setup.inf')+'"'),('.Set RptFileName="'+(Join-Path $stage 'setup.rpt')+'"'))
for($i=0;$i -lt $files.Count;$i++){$ddf+=('"'+(Join-Path $Payload $files[$i])+'" f'+$i)}
[IO.File]::WriteAllLines((Join-Path $stage 'payload.ddf'),$ddf,[Text.Encoding]::Default)
& "$env:WINDIR\System32\makecab.exe" /F (Join-Path $stage 'payload.ddf') | Out-Null
if($LASTEXITCODE -ne 0){throw 'CAB build failed'}
if(Test-Path -LiteralPath $Output){Remove-Item -LiteralPath $Output}
$installer=New-Object -ComObject WindowsInstaller.Installer
$db=$installer.OpenDatabase($Output,3)
function Sql([string]$query){try{$v=$db.OpenView($query);try{$v.Execute()}finally{$v.Close()}}catch{throw ($query+" : "+$_.Exception.Message)}}
function Q([string]$s){return "'"+$s.Replace("'","''")+"'"}
$columnCache=@{}
function Row([string]$table,[object[]]$values){
 if(-not $columnCache.ContainsKey($table)){
  $view=$db.OpenView(('SELECT `Name` FROM `_Columns` WHERE `Table` = '+(Q $table)+' ORDER BY `Number`'));$view.Execute();$columns=@()
  while($record=$view.Fetch()){$columns+=('`'+$record.StringData(1)+'`')};$view.Close();$columnCache[$table]=$columns -join ','
 }
 $v=@($values|ForEach-Object {if($null -eq $_){'NULL'}elseif($_ -is [int]){[string]$_}else{Q ([string]$_)}})
 Sql ('INSERT INTO `'+$table+'` ('+$columnCache[$table]+') VALUES ('+($v -join ',')+')')
}
Sql 'CREATE TABLE `Icon` (`Name` CHAR(72) NOT NULL, `Data` OBJECT NOT NULL PRIMARY KEY `Name`)'
$iconPath=Join-Path $PSScriptRoot 'assets\AppIcon.ico'
$iconView=$db.OpenView('INSERT INTO `Icon` (`Name`,`Data`) VALUES (?,?)')
$iconRecord=$installer.CreateRecord(2);$iconRecord.StringData(1)='AppIcon.ico';$iconRecord.SetStream(2,$iconPath)
$iconView.Execute($iconRecord);$iconView.Close()
Sql 'CREATE TABLE `Property` (`Property` CHAR(72) NOT NULL, `Value` CHAR(0) LOCALIZABLE PRIMARY KEY `Property`)'
Sql 'CREATE TABLE `Directory` (`Directory` CHAR(72) NOT NULL, `Directory_Parent` CHAR(72), `DefaultDir` CHAR(255) NOT NULL LOCALIZABLE PRIMARY KEY `Directory`)'
Sql 'CREATE TABLE `Component` (`Component` CHAR(72) NOT NULL, `ComponentId` CHAR(38), `Directory_` CHAR(72) NOT NULL, `Attributes` SHORT NOT NULL, `Condition` CHAR(255), `KeyPath` CHAR(72) PRIMARY KEY `Component`)'
Sql 'CREATE TABLE `Feature` (`Feature` CHAR(38) NOT NULL, `Feature_Parent` CHAR(38), `Title` CHAR(64) LOCALIZABLE, `Description` CHAR(255) LOCALIZABLE, `Display` SHORT, `Level` SHORT NOT NULL, `Directory_` CHAR(72), `Attributes` SHORT NOT NULL PRIMARY KEY `Feature`)'
Sql 'CREATE TABLE `FeatureComponents` (`Feature_` CHAR(38) NOT NULL, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Feature_`, `Component_`)'
Sql 'CREATE TABLE `File` (`File` CHAR(72) NOT NULL, `Component_` CHAR(72) NOT NULL, `FileName` CHAR(255) NOT NULL LOCALIZABLE, `FileSize` LONG NOT NULL, `Version` CHAR(72), `Language` CHAR(20), `Attributes` SHORT, `Sequence` SHORT NOT NULL PRIMARY KEY `File`)'
Sql 'CREATE TABLE `Media` (`DiskId` SHORT NOT NULL, `LastSequence` SHORT NOT NULL, `DiskPrompt` CHAR(64) LOCALIZABLE, `Cabinet` CHAR(255), `VolumeLabel` CHAR(32), `Source` CHAR(72) PRIMARY KEY `DiskId`)'
Sql 'CREATE TABLE `CustomAction` (`Action` CHAR(72) NOT NULL, `Type` SHORT NOT NULL, `Source` CHAR(72), `Target` CHAR(255) PRIMARY KEY `Action`)'
Sql 'CREATE TABLE `InstallExecuteSequence` (`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` SHORT PRIMARY KEY `Action`)'
Sql 'CREATE TABLE `RemoveFile` (`FileKey` CHAR(72) NOT NULL, `Component_` CHAR(72) NOT NULL, `FileName` CHAR(255) LOCALIZABLE, `DirProperty` CHAR(72) NOT NULL, `InstallMode` SHORT NOT NULL PRIMARY KEY `FileKey`)'
Sql 'CREATE TABLE `Shortcut` (`Shortcut` CHAR(72) NOT NULL, `Directory_` CHAR(72) NOT NULL, `Name` CHAR(128) NOT NULL LOCALIZABLE, `Component_` CHAR(72) NOT NULL, `Target` CHAR(255), `Arguments` CHAR(255), `Description` CHAR(255) LOCALIZABLE, `Hotkey` SHORT, `Icon_` CHAR(72), `IconIndex` SHORT, `ShowCmd` SHORT, `WkDir` CHAR(72) PRIMARY KEY `Shortcut`)'
Sql 'CREATE TABLE `Registry` (`Registry` CHAR(72) NOT NULL, `Root` SHORT NOT NULL, `Key` CHAR(255) NOT NULL LOCALIZABLE, `Name` CHAR(255) LOCALIZABLE, `Value` CHAR(0) LOCALIZABLE, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Registry`)'
foreach($pair in @(@('ProductCode',$code),@('UpgradeCode','{23DC55D9-79B1-43BE-8ED2-5426AD99ABEC}'),@('ProductName',$product),@('ProductVersion','3.4.0'),@('ProductLanguage','1033'),@('Manufacturer','StartMenuFreeLayout'),@('INSTALLLEVEL','1'),@('ARPHELPLINK','https://github.com/12365adwd/StartMenuFreeLayout'),@('ARPPRODUCTICON','AppIcon.ico'),@('ARPNOMODIFY','1'),@('ARPNOREPAIR','1'),@('REBOOT','ReallySuppress'))){Row Property $pair}
Row Directory @('TARGETDIR',$null,'SourceDir');Row Directory @('LocalAppDataFolder','TARGETDIR','.');Row Directory @('INSTALLROOT','LocalAppDataFolder',$product)
Row Directory @('APPROOT','INSTALLROOT','App');Row Directory @('APPDIR','APPROOT','3.4.0');Row Directory @('ProgramMenuFolder','TARGETDIR','.')
Row Feature @('Main',$null,'StartMenuFreeLayout',$null,1,1,'INSTALLROOT',0)
for($i=0;$i -lt $files.Count;$i++) {
 $component='c'+$i;$file='f'+$i
 $sha=[Security.Cryptography.SHA256]::Create();try{$hash=$sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($product+'/'+$files[$i]))}finally{$sha.Dispose()}
 $idBytes=New-Object byte[] 16;[Array]::Copy($hash,$idBytes,16);$componentId=New-Object Guid -ArgumentList (,$idBytes)
 Row Component @($component,('{'+$componentId.ToString().ToUpperInvariant()+'}'),'APPDIR',256,$null,$file)
 Row FeatureComponents @('Main',$component)
 Row File @($file,$component,('F'+$i+'|'+$files[$i]),[int](Get-Item -LiteralPath (Join-Path $Payload $files[$i])).Length,$null,$null,16384,($i+1))
}
Row Media @(1,4,$null,'#payload.cab',$null,$null)
# The installer owns these keys. The * entry removes the complete private key,
# including values created at runtime, when its component is uninstalled.
Row Registry @('OwnProductKey',1,('Software\'+$product),'*',$null,'c0')
Row Registry @('InstallRoot',1,('Software\'+$product),'InstallRoot','[INSTALLROOT]','c0')
Row Registry @('CurrentExe',1,('Software\'+$product),'CurrentExe','[#f0]','c0')
Row Registry @('Version',1,('Software\'+$product),'Version','3.4.0','c0')
# The controller preserves optional startup on upgrade, then removes exactly its
# own value in PrepareUninstall. Never author the shared Run key.
Row CustomAction @('RegisterInstall',18,'f0','/register-install "[IMPORTCONFIG]" "[UserSID]" [CLIENTPROCESSID]')
Row CustomAction @('PrepareUninstall',18,'f0','/prepare-uninstall "[UserSID]" [CLIENTPROCESSID]')
Row CustomAction @('LaunchControl',18,'f0','/launch-control "[UserSID]" [CLIENTPROCESSID]')
Row Shortcut @('ControlShortcut','ProgramMenuFolder',($product+'.lnk'),'c0','[#f0]',$null,'StartMenuFreeLayout',$null,'AppIcon.ico',0,1,'APPDIR')
Row RemoveFile @('RemoveVersion','c0',$null,'APPDIR',2);Row RemoveFile @('RemoveAppRoot','c0',$null,'APPROOT',2);Row RemoveFile @('RemoveProductRoot','c0',$null,'INSTALLROOT',2)
$sequence=@(@('CostInitialize',800),@('FileCost',900),@('CostFinalize',1000),@('InstallValidate',1400),@('InstallInitialize',1500),@('ProcessComponents',1600),@('UnpublishFeatures',1800),@('RemoveShortcuts',3200),@('RemoveRegistryValues',3300),@('RemoveFiles',3500),@('RemoveFolders',3600),@('InstallFiles',4000),@('CreateShortcuts',4500),@('WriteRegistryValues',5000),@('RegisterUser',6000),@('RegisterProduct',6100),@('PublishFeatures',6300),@('PublishProduct',6400),@('InstallFinalize',6600))
foreach($s in $sequence){Row InstallExecuteSequence @($s[0],$null,[int]$s[1])}
Row InstallExecuteSequence @('PrepareUninstall','Installed AND REMOVE="ALL"',1450)
Row InstallExecuteSequence @('RegisterInstall','NOT Installed',6700)
Row InstallExecuteSequence @('LaunchControl','NOT Installed AND LAUNCHAPP="1"',6900)
if($Test){
 Row CustomAction @('LifecycleSelfTest',18,'f0','/lifecycle-selftest "[UserSID]" [CLIENTPROCESSID]')
 Row InstallExecuteSequence @('LifecycleSelfTest','NOT Installed',6800)
}
$v=$db.OpenView('INSERT INTO `_Streams` (`Name`,`Data`) VALUES (?,?)');$r=$installer.CreateRecord(2);$r.StringData(1)='payload.cab';$r.SetStream(2,(Join-Path $stage 'payload.cab'));$v.Execute($r);$v.Close()
$db.Commit()
$summary=$db.SummaryInformation(20)
$summary.Property(2)='StartMenuFreeLayout installation';$summary.Property(3)='Native Start menu grid layout';$summary.Property(4)='StartMenuFreeLayout';$summary.Property(7)='x64;1033';$summary.Property(9)=('{'+[guid]::NewGuid().ToString().ToUpperInvariant()+'}');$summary.Property(14)=500;$summary.Property(15)=10;$summary.Persist();$db.Commit()
[Runtime.InteropServices.Marshal]::FinalReleaseComObject($summary)|Out-Null
[Runtime.InteropServices.Marshal]::FinalReleaseComObject($db)|Out-Null
[Runtime.InteropServices.Marshal]::FinalReleaseComObject($installer)|Out-Null
$summary=$null;$db=$null;$installer=$null;$v=$null;$r=$null
[GC]::Collect();[GC]::WaitForPendingFinalizers();[GC]::Collect();[GC]::WaitForPendingFinalizers()
if(-not ([IO.Path]::GetFullPath($stage).StartsWith((Split-Path $Output -Parent)+'\'))){throw 'Unexpected staging directory'}
Remove-Item -LiteralPath $stage -Recurse -Force
Write-Output $Output
