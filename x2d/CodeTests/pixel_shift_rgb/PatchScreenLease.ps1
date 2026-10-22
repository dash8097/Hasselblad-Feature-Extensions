# 第一代 X2D 4.2.0：安装/卸载期间临时禁用自动息屏，结束后恢复原值。
# Check/Verify 只读核对；Diagnose 只报告不阻止日志；不改取景开关或唤醒设备。
function Get-PatchScreenTimeout([scriptblock]$Run) {
 $r=& $Run @('/bin/dbus-send --system --print-reply --dest=com.hasselblad.system /system org.freedesktop.DBus.Properties.Get string:com.hasselblad.system string:screen_idle_timeout')
 if(@($r).Count -ne 1 -or $r[0] -notmatch 'variant\s+int32\s+(-?[0-9]+)\s*$'){throw 'SCREEN_TIMEOUT_STATE_UNCONFIRMED：未取得完整息屏设置。'}
 $value=0
 if(![int]::TryParse($Matches[1],[ref]$value) -or $value -lt -1){throw 'SCREEN_TIMEOUT_RANGE：息屏设置格式不符。'}
 return $value
}
function Test-PatchScreenTimeout([scriptblock]$Run,[bool]$RequireAllowed=$true) {
 $command='/bin/dbus-send --system --print-reply --dest=com.hasselblad.system /system org.freedesktop.DBus.Properties.Get string:com.hasselblad.system string:screen_idle_timeout'
 Set-PatchContext -Stage '相机息屏时间检查' -Target '/system screen_idle_timeout' -Command $command -Expected '5分钟（300秒）、0或-1' -Transport '请求中'
 $value=Get-PatchScreenTimeout $Run
 $allowed=$value -in @(300,0,-1)
 $actual='息屏时间='+$value+'秒'+$(if($value -eq 300){'（5分钟）'}else{''})
 Set-PatchContext -Actual $actual -Transport '收到息屏参数完整回复'
 Write-Output ($actual+'；要求：5分钟（300秒）、0或-1。')
 if(!$allowed){
  $message='息屏时间不符合要求：请在相机电源设置中改为5分钟或关闭（参数0或-1），再重新检查。当前='+$value+'秒；本次检查未修改设置。'
  if($RequireAllowed){throw $message}else{Write-Output $message}
 }
}
function Set-PatchScreenTimeout([int]$Value,[scriptblock]$Run) {
 if($Value -lt -1){throw 'SCREEN_TIMEOUT_RANGE'}
 $r=& $Run @('/bin/dbus-send --system --print-reply --dest=com.hasselblad.system /system org.freedesktop.DBus.Properties.Set string:com.hasselblad.system string:screen_idle_timeout variant:int32:'+$Value)
 if(@($r).Count -ne 1 -or $r[0] -notmatch '^method return[^\r\n]*(?:\r?\n\s*)?$'){throw 'SCREEN_TIMEOUT_SET_UNCONFIRMED：设置回复未确认。'}
}
function Start-PatchScreenLease([scriptblock]$Run) {
 if($script:PatchScreenLease){throw 'SCREEN_TIMEOUT_LEASE_ALREADY_ACTIVE'}
 $original=Get-PatchScreenTimeout $Run
 $script:PatchScreenLease=[pscustomobject]@{original=$original;attempted=$false}
 if($original -in @(0,-1)){Write-Output ('息屏设置='+$original+'，保持原值。');return}
 # 先记原值；Set 的运输失败不证明设置没有生效，finally 仍须尝试核对恢复。
 $script:PatchScreenLease.attempted=$true
 Set-PatchScreenTimeout 0 $Run
 if((Get-PatchScreenTimeout $Run) -ne 0){throw 'SCREEN_TIMEOUT_DISABLE_UNCONFIRMED：自动息屏关闭未确认，未开始安装。'}
 Write-Output ('安装期间已临时关闭自动息屏；原设置='+$original+'，结束后恢复。')
}
function Stop-PatchScreenLease([scriptblock]$Run) {
 if(!$script:PatchScreenLease){return}
 $lease=$script:PatchScreenLease
 if(!$lease.attempted){$script:PatchScreenLease=$null;return}
 try{
  $current=Get-PatchScreenTimeout $Run
  if($current -eq 0){
   Set-PatchScreenTimeout $lease.original $Run
   $current=Get-PatchScreenTimeout $Run
   if($current -ne $lease.original){throw 'SCREEN_TIMEOUT_RESTORE_VALUE_DIFFERS'}
  }
  if($current -ne $lease.original){
   # 用户自行改了设置时不覆盖；仍明确报告当前值。
   Write-Output ('息屏设置已变化，保留当前值='+$current+'；安装前='+$lease.original+'。')
  }else{Write-Output ('已恢复原自动息屏设置='+$lease.original+'。')}
  $script:PatchScreenLease=$null
 }catch{throw ('SCREEN_TIMEOUT_RESTORE_UNCONFIRMED：原息屏设置='+$lease.original+'；未能确认恢复，请在相机电源设置中核对。不会自动重连或重发安装。')}
}
