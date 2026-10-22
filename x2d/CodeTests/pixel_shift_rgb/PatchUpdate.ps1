# 更新编排与重新开机验证；运输与卸载由调用者注入，模块不创建 USB 会话。
function Test-PatchBootFingerprint([string]$Fingerprint) {
 return $Fingerprint -cmatch '\A[a-f0-9]{64}\z'
}
function Get-PatchBootFingerprint([scriptblock]$Run) {
 $command='sha256sum /proc/sys/kernel/random/boot_id'
 $replies=& $Run ([string[]]@($command))
 if($replies.Count -ne 1){throw '开机状态回复数量不完整，更新未继续。'}
 $match=[regex]::Match($replies[0].Trim(),'\A([a-f0-9]{64})\s+/proc/sys/kernel/random/boot_id\z')
 if(!$match.Success){
  throw '无法核对本次开机状态，更新未继续。请保持连接并读取诊断。'
 }
 return $match.Groups[1].Value
}
function Invoke-PatchUpdate([string]$Action,[string]$Route,[scriptblock]$RunStep,
 [scriptblock]$ReadBoot,[scriptblock]$Uninstall,[string]$PreviousBootFingerprint='') {
 if($Action -eq 'ResumeUpdate'){
  if(!(Test-PatchBootFingerprint $PreviousBootFingerprint)){throw '更新续接状态无效，未开始安装。'}
  if($Route -ne 'Stock'){throw '自动卸载后的原厂文件状态不匹配，更新未继续。请读取诊断。'}
  $boot=& $ReadBoot
  if(!(Test-PatchBootFingerprint $boot)){throw '开机状态读取无效，更新未继续。'}
  if($boot -ceq $PreviousBootFingerprint){
   Write-Output PATCH_UPDATE_RESTART_STILL_PENDING
   Write-Output '旧版文件已还原。请先手动关机开机，再点击“检测并安装”。本次没有重新安装。'
   return
  }
  Write-Output PATCH_UPDATE_RESTART_VERIFIED
  Invoke-PatchFlow 'Install' 'Stock' $RunStep $true
 }elseif($Action -eq 'Install'){
  if($Route -eq 'Stock'){Invoke-PatchFlow 'Install' $Route $RunStep $true}
  elseif($Route -eq 'CurrentStock'){
   Invoke-PatchFlow 'Verify' $Route $RunStep
   Write-Output PATCH_ALREADY_CURRENT
  }elseif($Route -in @('Upgrade','CurrentUpgrade','KnownProject')){
   $boot=& $ReadBoot
   if(!(Test-PatchBootFingerprint $boot)){throw '无法核对更新前开机状态，未卸载补丁。'}
   Write-Output ('PATCH_DETECTED_ROUTE='+$Route)
   Write-Output PATCH_UPDATE_AUTOMATIC_UNINSTALL
   & $Uninstall
   # 只有卸载完成、原厂文件和只读挂载核对通过后才允许客户端记住续接状态。
   Write-Output ('PATCH_UPDATE_RESTART_PENDING='+$boot)
   Write-Output '旧补丁已自动卸载。请保持本客户端打开，手动重启相机，然后点击“检测并安装”。'
  }else{throw 'Unknown patch route'}
 }else{throw 'Unknown update action'}
}
