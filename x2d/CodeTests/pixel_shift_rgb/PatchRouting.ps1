# 组件只读状态按固定路径/文件类型判断，不读取组件指纹。
function New-PatchSnapshotCommand([string]$Path) {
 if($Path -notmatch '^/system/[A-Za-z0-9_./-]+$' -or $Path -match '/\.\.?(/|$)'){throw '安装目标路径无效'}
 $command='f='+$Path+';v=ABSENT;[ -e "$f" ]&&v=UNSUPPORTED_TYPE;[ -f "$f" ]&&v=FILE;[ -L "$f" ]&&v=UNSUPPORTED_LINK;echo "$v"'
 if($command.Length -gt 231){throw 'FILE_STATE_COMMAND_LIMIT'}
 return $command
}
function Get-PatchView($Manifest,[bool]$Original) {
 $view=@{};foreach($entry in $Manifest.targets.PSObject.Properties){$view[$entry.Value]=if($Original -and !$Manifest.original.($entry.Name)){'ABSENT'}else{'FILE'}};return $view
}
function Test-PatchView($Snapshot,$View) {
 foreach($path in $View.Keys){if(!$Snapshot.ContainsKey($path) -or $Snapshot[$path] -cne $View[$path]){return $false}};return $true
}
function Get-PatchRestoreAdvice {return '本包支持第一代 X2D 4.2.0；安装其他来源补丁的相机请先用官方固件还原。'}
function Test-PatchRemovalState($Item,$Actual) {return ($Actual -ceq 'ABSENT' -or $Actual -ceq 'FILE')}
function Get-PatchRemovalRoute($Snapshot,$Removal) {
 $changed=$false
 foreach($item in $Removal.targets){
  if(!$Snapshot.ContainsKey($item.path) -or !(Test-PatchRemovalState $item $Snapshot[$item.path])){throw ('组件文件类型或状态不支持：'+$item.path)}
  if($item.original -eq 'ABSENT' -and $Snapshot[$item.path] -ceq 'FILE'){$changed=$true}
 }
 if($changed){return 'KnownProject'};return 'Stock'
}
function Get-PatchRoute($Snapshot,$Stock,$Upgrade,$Previous=@(),$Removal=$null) {
 foreach($entry in $Stock.required_guards.PSObject.Properties){if($Snapshot[$entry.Name] -cne 'FILE'){throw ('原厂必需文件缺失：'+$entry.Name)}}
 return (Get-PatchRemovalRoute $Snapshot $Removal)
}
function Invoke-PatchFlow([string]$Action,[string]$Route,[scriptblock]$RunStep,[bool]$ManualRebootRequired=$false) {
 if($Route -notin @('Stock','CurrentStock','Upgrade','CurrentUpgrade','KnownProject')){throw 'Unknown patch route'}
 $kind='stock'
 $current=$Route -like 'Current*'
 Write-Output ('PATCH_DETECTED_ROUTE='+$Route)
 if($Action -eq 'Verify'){
  if($Route -ne 'CurrentStock'){throw '相机尚未安装本包完整版本。已有本项目旧补丁请点击“安装／更新补丁”。'}
  & $RunStep $kind 'VerifyCurrent'
  Write-Output PATCH_VERSION_VERIFIED
 }elseif($Action -eq 'Check'){
  if($Route -in @('Upgrade','CurrentUpgrade','KnownProject')){Write-Output PATCH_UPDATE_AVAILABLE}
  else{$step=if($current){'VerifyCurrent'}else{'Check'};& $RunStep $kind $step}
  Write-Output PATCH_AUTO_CHECK_COMPLETE
 }elseif($Action -eq 'Install'){
  if($Route -ne 'Stock'){throw '已有补丁不能覆盖安装，请使用统一客户端的“安装／更新补丁”流程。'}
  foreach($step in @('Check','Stage','Activate')){& $RunStep $kind $step}
  if($kind -eq 'stock' -or $ManualRebootRequired){
   & $RunStep $kind 'PollInstall'
   Write-Output PATCH_MANUAL_REBOOT_REQUIRED
  }else{
   & $RunStep $kind 'Verify'
   Write-Output PATCH_INSTALL_COMPLETE
  }
 }else{throw 'Unknown patch action'}
}
