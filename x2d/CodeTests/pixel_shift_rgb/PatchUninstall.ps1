# 独立卸载流程。仅由统一入口完成包、Phocus、固件及组件检查后调用。
function Invoke-PatchRetainedCleanup([string]$PackageRoot,[scriptblock]$Run) {
 $manifest=Get-Content (Join-Path $PackageRoot 'uninstall/manifest.json') -Raw -Encoding UTF8|ConvertFrom-Json
 $reply=& $Run @('ls -d /system/x2d-un-retained-???????? 2>/dev/null; true')
 if($reply.Count -ne 1){throw '卸载保留目录回复不完整'}
 $directories=@($reply[0].Trim() -split '\r?\n'|Where-Object {$_})
 if($directories.Count -gt 8){throw '卸载保留目录过多，未执行清理'}
 foreach($directory in $directories){
  if($directory -cnotmatch '^/system/x2d-un-retained-([0-9a-f]{8})$'){throw '卸载保留目录格式不符'}
  $stage='/data/local/tmp/x2d-un-'+$Matches[1]
  Set-PatchContext -Stage '重新开机后的卸载保留文件清理' -Target $directory
  $reply=& $Run @("d=$stage; f=`$d/cleanup.sh; if test ! -L `$d && test -d `$d && test ! -L `$f && test -f `$f; then echo FILE; else echo ABSENT; fi")
  if($reply.Count -ne 1 -or $reply[0].Trim() -cne 'FILE'){throw '卸载清理脚本缺失或不可读'}
  $reply=& $Run @("cd $stage && (sh ./cleanup.sh `"`$PWD`" >cleanup.log 2>&1; r=`$?; echo `$r; tail -n 10 cleanup.log)")
  if($reply.Count -ne 1){throw '卸载保留文件的清理回执不完整'}
  $lines=@($reply[0].Trim() -split '\r?\n')
  $script:PatchContext.camera_log=Protect-PatchText $reply[0]
  $script:PatchContext.camera_exit=$lines[0]
  if($lines[0] -cne '0' -or $lines -cnotcontains 'RETAINED_PATCH_FILES_CLEANED'){throw '卸载保留文件未清理完成；请确认已手动关机开机并读取诊断。未重发清理。'}
  Write-Output '重新开机已核对，旧补丁保留文件已清理。'
 }
}
function Invoke-PatchUninstall([string]$PackageRoot,$Route,$Snapshot,[scriptblock]$Run) {
 $directory=Join-Path $PackageRoot 'uninstall'
 $manifest=Get-Content (Join-Path $directory 'manifest.json') -Raw -Encoding UTF8|ConvertFrom-Json
 $proof=Get-Content (Join-Path $directory 'validation.json') -Raw -Encoding UTF8|ConvertFrom-Json
 if(!$proof.offline -or $proof.device_requests -ne 0){throw '卸载事务验证与交付文件不一致'}
 if($Route -eq 'Stock'){Write-Output PATCH_ALREADY_UNINSTALLED;return}
 if($Route -notin @('Upgrade','CurrentStock','CurrentUpgrade','KnownProject')){throw '当前补丁版本不支持完整卸载'}
 foreach($item in $manifest.targets){
  if(!$Snapshot.ContainsKey($item.path) -or !(Test-PatchRemovalState $item $Snapshot[$item.path])){throw ('卸载组件检查不通过：'+$item.path)}
 }
 $digest={param([byte[]]$Bytes)$hash=[Security.Cryptography.SHA256]::Create();try{([BitConverter]::ToString($hash.ComputeHash($Bytes))).Replace('-','').ToLowerInvariant()}finally{$hash.Dispose()}}
 $bytes=[IO.File]::ReadAllBytes((Join-Path $directory 'payload.tar.gz'))
 if((& $digest $bytes) -cne $manifest.archive_sha256){throw '卸载载荷校验失败'}
 $stage='/data/local/tmp/x2d-un-'+[Guid]::NewGuid().ToString('N').Substring(0,8)
 Set-PatchContext -Stage '暂存卸载事务' -Target $stage
 Write-Output ('PATCH_UNINSTALL_STAGE='+$stage)
 $reply=& $Run @("test ! -e $stage && test ! -L $stage && mkdir -m700 $stage && echo UNINSTALL_STAGE_CREATED")
 if($reply[0].Trim() -cne 'UNINSTALL_STAGE_CREATED'){throw '卸载暂存目录创建未确认'}
 $encoded=[Convert]::ToBase64String($bytes)
 $chunk=231-[Text.Encoding]::ASCII.GetByteCount("printf '%s' '' >> $stage/p.b64")
 $chunk=[Math]::Min(170,$chunk)
 for($offset=0;$offset -lt $encoded.Length;$offset+=$chunk){
  $part=$encoded.Substring($offset,[Math]::Min($chunk,$encoded.Length-$offset))
  $reply=& $Run @("printf '%s' '$part' >> $stage/p.b64")
  [Console]::WriteLine('PATCH_PROGRESS=transfer|'+[Math]::Min($offset+$part.Length,$encoded.Length)+'|'+$encoded.Length)
 }
 $reply=& $Run @("cd $stage && /system/xbin/busybox base64 -d p.b64 > p.gz && sha256sum p.gz")
 if($reply[0] -notmatch ('^'+$manifest.archive_sha256+' ')){throw '相机收到的卸载载荷不完整，未开始卸载'}
 $reply=& $Run @("cd $stage && /system/xbin/busybox tar xzf p.gz && test ! -L uninstall.sh && test -r uninstall.sh && sh -n uninstall.sh && echo SCRIPT_READY")
 if($reply[0].Trim() -cne 'SCRIPT_READY'){throw '相机卸载脚本校验失败，未开始卸载'}
 Set-PatchContext -Stage '卸载与原厂文件恢复' -Target $stage
 $script:PatchContext.recovery='等待本次卸载事务回执；不自动重发'
 $reply=& $Run @("cd $stage && test ! -e started && (nohup /system/bin/sh -c 'sh ./uninstall.sh `"`$PWD`" >log 2>&1' >/dev/null 2>&1 </dev/null & echo UNINSTALL_DISPATCHED)")
 if($reply[0].Trim() -cne 'UNINSTALL_DISPATCHED'){throw '卸载启动未确认，请读取现场；没有重新启动事务'}
 $finished=$false
 for($attempt=0;$attempt -lt 150;$attempt++){
  $reply=& $Run @("cd $stage && if test -f result; then cat result; else echo RUNNING; fi")
  $result=$reply[0].Trim()
  if($result -match '^\d+$'){
   $script:PatchContext.camera_exit=$result
   $log=& $Run @("cd $stage && tail -n 20 log")
   Write-Output $log[0]
   $script:PatchContext.camera_log=$log[0]
   if($result -ne '0'){throw '卸载未完成；请查看本次恢复记录，未重发卸载'}
   $done=& $Run @("cd $stage && test ! -L done && cat done")
   if($done[0].Trim() -cne 'PATCH_UNINSTALLED_MANUAL_REBOOT_REQUIRED'){throw '缺少本次卸载完成回执'}
   $finished=$true;break
  }
  if($result -cne 'RUNNING'){throw '卸载状态回复无效'}
  Start-Sleep -Milliseconds 300
 }
 if(!$finished){throw '卸载等待超时；请保留连接读取诊断，不重复启动事务'}
 [Console]::WriteLine('PATCH_PROGRESS=install|5|5')
 Set-PatchContext -Stage '卸载后的原厂文件核对' -Target '/system 原厂组件'
 $verified=0
 foreach($item in $manifest.targets){
  $reply=& $Run @((New-PatchSnapshotCommand $item.path))
  $actual=$reply[0].Trim()
  if(($item.original -eq 'ABSENT' -and $actual -cne 'ABSENT') -or ($item.original -ne 'ABSENT' -and $actual -cne 'FILE')){throw ('卸载后的原厂文件核对失败：'+$item.path)}
  $verified++;[Console]::WriteLine('PATCH_PROGRESS=verify|'+$verified+'|'+$manifest.targets.Count)
 }
 $reply=& $Run @("awk '`$2==`"/system`" && `$4 ~ /(^|,)ro(,|`$)/{ok=1} END{print ok ? `"SYSTEM_READONLY`" : `"UNKNOWN`"}' /proc/mounts")
 if($reply[0].Trim() -cne 'SYSTEM_READONLY'){throw '卸载后系统只读状态未确认'}
 Set-PatchContext -Stage '卸载完成，等待手动重启'
 $script:PatchContext.readonly='已核对只读'
 $script:PatchContext.recovery='原厂文件已还原，电源键所需服务保留；请正常关机后重新开机'
 Write-Output PATCH_UNINSTALLED_MANUAL_REBOOT_REQUIRED
}
