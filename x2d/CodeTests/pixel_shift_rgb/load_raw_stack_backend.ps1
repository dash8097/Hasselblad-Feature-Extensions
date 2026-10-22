param([ValidateSet('Inspect','Check','Stage','Activate','Verify','VerifyCurrent','PollInstall','UpdateScript','Drain','Diagnose')][string]$Action='Inspect',[string]$PackageRoot='')
$ErrorActionPreference='Stop'
[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false)
if(!$PackageRoot){$taskRoot=(Resolve-Path "$PSScriptRoot/../../..").Path;$PackageRoot=Join-Path $taskRoot 'x2d/outputs/4.2.0/raw-stack/backend-r1'}
. (Join-Path $PackageRoot 'PatchDiagnostics.ps1')
Initialize-PatchDiagnostic $Action
try {
Set-PatchContext -Stage 'Phocus 与 USB 驱动检查'
. (Join-Path $PackageRoot 'PhocusPreflight.ps1')
Invoke-PhocusPreflight
$taskSource=Get-Content -LiteralPath (Join-Path $PackageRoot 'WinUsbReadOnly.cs') -Raw -Encoding UTF8
$taskMethods=Get-Content -LiteralPath (Join-Path $PackageRoot 'ShellTransport.inc.cs') -Raw -Encoding UTF8
$taskSource=$taskSource.Replace('namespace HasselbladDebug','namespace StackPublic').Replace('WinUsbReadOnly','StackUsb').Replace('uint timeout = 2000;','uint timeout = 45000;')
if($Action -eq 'Drain'){$taskSource=$taskSource.Replace('uint timeout = 45000;','uint timeout = 1000;')}
# Shell 仅在收到匹配的最终回复后返回；缩短主机额外停顿，保留串行回复校验。
$taskMethods=$taskMethods.Replace('TemporaryUiUsb','StackUsb').Replace('output.Add(s.Shell(commands[i],tag+(uint)i));','output.Add(s.Shell(commands[i],tag+(uint)i)); System.Threading.Thread.Sleep(50);')
Set-PatchContext -Stage 'USB 运输层初始化'
Add-Type -TypeDefinition $taskSource.Replace('        public void Dispose() {',$taskMethods+'        public void Dispose() {')
if([StackPublic.StackUsb]::SelfTestShell() -ne 'SHELL_OFFLINE_PASS'){throw 'USB contract failed'}
function Reviewed([string[]]$commands){
 Set-PatchCommandContext $commands
 foreach($command in $commands){
  $size=[Text.Encoding]::UTF8.GetByteCount($command)
  if(!$command -or $command -match '[^\x20-\x7e]' -or $size -gt 231){
   Set-PatchContext -Expected 'ASCII 命令长度 1–231 字节' -Actual ($size.ToString()+' 字节')
   throw ('USB command exceeds the reviewed transport contract: '+$size+' bytes')
  }
 }
 try{$reply=[StackPublic.StackUsb]::RunReviewedBatch($commands)}catch{
  $errorChain=$_.Exception;$errorText=[string]$errorChain.Message
  while($errorChain.InnerException){$errorChain=$errorChain.InnerException;$errorText+=' '+$errorChain.Message}
  if($errorText -match 'BATCH_STEP_([0-9]+)_SHELL_REMOTE_FAILURE'){
   $index=[int]$Matches[1]
   if($index -lt $commands.Count){Set-PatchCommandContext @($commands[$index])}
   Set-PatchContext -Transport '相机已回复：命令返回失败；不是 USB 无回复' -Actual '本条远端命令未成功；本批后续命令未发送'
   $script:PatchContext.camera_exit='非零，协议未提供具体退出码'
  }
  throw
 }
 Set-PatchReplyContext $reply
 return ,$reply
}
function FileStateCommand([string]$path){
 if($path -notmatch '^(/system/[A-Za-z0-9_./-]+|[A-Za-z0-9_.-]+)$' -or $path.Contains('..')){throw 'Invalid reviewed file path'}
 # 完整路径只出现一次；拒绝链接与非普通文件，保留 SHA-256 校验。
 return ('f='+$path+'; test ! -L "$f" && if test -f "$f"; then sha256sum "$f"; elif test -e "$f"; then echo UNSUPPORTED_TYPE; else echo ABSENT; fi')
}
function Digest([byte[]]$bytes){$taskHash=[Security.Cryptography.SHA256]::Create();try{return ([BitConverter]::ToString($taskHash.ComputeHash($bytes))).Replace('-','').ToLowerInvariant()}finally{$taskHash.Dispose()}}
# PATCH_STAGE_DIRECTORIES_BEGIN
function Initialize-PatchStageDirectories {
 if($taskStage -notmatch '^/data/local/tmp/x2d-rs[1-8]$'){throw 'Invalid installation staging path'}
 foreach($directory in @('/data','/data/local','/data/local/tmp',$taskStage)){
  Set-PatchContext -Stage '准备安装暂存目录' -Target $directory
  $command='d='+$directory+'; if test -L "$d"; then echo LINK; elif test -d "$d"; then echo DIRECTORY; elif test -e "$d"; then echo OTHER; else echo ABSENT; fi'
  $r=Reviewed @($command)
  $state=$r[0].Trim()
  if($state -eq 'DIRECTORY'){continue}
  if($state -ne 'ABSENT' -or $directory -eq '/data'){
   throw ('安装暂存路径不可用：'+$directory+'；状态='+$state+'。已有文件和链接保持不变。')
  }
  $r=Reviewed @('d='+$directory+'; test ! -e "$d" && test ! -L "$d" && mkdir -m700 "$d" && echo STACK_DIRECTORY_CREATED')
  if($r[0].Trim() -ne 'STACK_DIRECTORY_CREATED'){throw ('安装暂存目录创建未确认：'+$directory)}
 }
 Set-PatchContext -Stage $Action -Target $taskStage
}
# PATCH_STAGE_DIRECTORIES_END
function StageState {
 $r=Reviewed @('cd '+$taskStage+' && (for f in started done result backup; do if test -e "$f" || test -L "$f"; then echo TRANSACTION; exit; fi; done; echo STACK_STAGE_IDLE)')
 return $r[0].Trim()
}
function PreserveTerminalStage {
 # 官方还原后的系统可能是原厂，而 /data 仍保留已完成事务和原版备份。
 $r=Reviewed @("cd $taskStage && (test ! -L done && if test -f done; then cat done; else test ! -e done && echo ABSENT; fi)","cd $taskStage && test ! -L result && test -f result && cat result")
 $completed=$r[0].Trim() -match '^RAW_STACK_(BACKEND_INSTALLED_NO_CAPTURE|STOCK_FILES_INSTALLED_MANUAL_REBOOT_REQUIRED)$' -and $r[1].Trim() -eq '0'
 $failed=$r[0].Trim() -eq 'ABSENT' -and $r[1].Trim() -eq '1'
 if(!$completed -and !$failed){throw 'Previous installation is not confirmed complete or a supported terminal failure; staged data and backups preserved'}
 if($failed){
  $r=Reviewed @("cd $taskStage && test ! -L started && test -f started && test ! -L install.sh && sha256sum install.sh")
  if($r[0].Trim() -notmatch '^([0-9a-f]{64})\s+install\.sh$' -or $install.recoverable_failed_install_scripts -notcontains $Matches[1]){throw 'Failed installation script is not a known recoverable revision; staged data and backups preserved'}
 }
 foreach($prop in $install.targets.PSObject.Properties){
  $r=Reviewed @((FileStateCommand $prop.Value));$old=$install.original.($prop.Name)
  if(($old -and $r[0] -notmatch ('^'+$old+' ')) -or (!$old -and $r[0].Trim() -ne 'ABSENT')){throw ('Previous installation cannot be archived: original baseline differs: '+$prop.Name)}
  if($failed){
   $r=Reviewed @('f='+$prop.Value+'; test ! -e "$f.rs-next" && test ! -L "$f.rs-next" && test ! -e "$f.rs-rollback" && test ! -L "$f.rs-rollback" && echo CLEAN')
   if($r[0].Trim() -ne 'CLEAN'){throw ('Failed installation temporary files remain: '+$prop.Name)}
  }
 }
 $r=Reviewed @('test ! -e /tmp/x2d-multishot-active && echo STACK_LOCK_RELEASED')
 if($r[0].Trim() -ne 'STACK_LOCK_RELEASED'){throw 'Camera task is active; previous installation preserved'}
 if($failed){
  $r=Reviewed @('pidof camera-service >/dev/null && pidof camera-gui >/dev/null && echo SERVICES_RUNNING','awk ''$2=="/system"{print $4}'' /proc/mounts')
  if($r[0].Trim() -ne 'SERVICES_RUNNING' -or $r[1].Trim() -notmatch '(^|,)ro(,|$)'){throw 'Failed installation recovery is not confirmed: services or read-only mount'}
  foreach($check in @(@('camera','exposure_status','0'),@('storage','storage_status','0'),@('error','active_error','false'),@('system','system_state','1'))){
   $command='s='+$check[0]+';p='+$check[1]+';/bin/dbus-send --system --print-reply --reply-timeout=3000 --dest=com.hasselblad.$s /$s org.freedesktop.DBus.Properties.Get string:com.hasselblad.$s string:$p | awk ''/variant/{print $NF}'''
   $r=Reviewed @($command)
   if($r[0].Trim() -ne $check[2]){throw ('Failed installation recovery is not confirmed: '+$check[0]+'.'+$check[1])}
  }
 }
 $suffix=[Guid]::NewGuid().ToString('N').Substring(0,8)
 Write-Output ('STACK_PREVIOUS_STAGE_PRESERVE='+$taskStage+'.saved-'+$suffix)
 $expectedResult=if($failed){'1'}else{'0'}
 $command='d='+$taskStage+'; q=$d.saved-'+$suffix+'; test ! -L $d && test ! -e $q && test ! -L $q && test ! -e /tmp/x2d-multishot-active && test "$(cat $d/result)" = '+$expectedResult+' && mv $d $q && echo STAGE_SAVED'
 $r=Reviewed @($command)
 if($r[0].Trim() -ne 'STAGE_SAVED'){throw 'Previous stage preservation not confirmed; inspect before continuing'}
 $r=Reviewed @("mkdir -m700 $taskStage && echo STAGE_CREATED")
 if($r[0].Trim() -ne 'STAGE_CREATED'){throw 'Fresh staging directory creation not confirmed'}
 if($failed){Write-Output 'STACK_PREVIOUS_FAILED_STAGE_RECOVERY_VERIFIED_AND_PRESERVED'}else{Write-Output 'STACK_PREVIOUS_COMPLETED_STAGE_PRESERVED'}
}
$taskStage='/data/local/tmp/x2d-rs1'
$taskManifest=Get-Content -LiteralPath (Join-Path $PackageRoot 'bundle-manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
Set-PatchContext -Stage $Action -Target $taskStage
if($Action -eq 'Diagnose'){
 Invoke-PatchCameraDiagnostics {param([string[]]$Commands) return ,([StackPublic.StackUsb]::RunReviewedBatch($Commands))}
 exit 0
}
if($Action -eq 'Drain'){[StackPublic.StackUsb]::DrainReplies();exit}
if($Action -eq 'Inspect'){
 $taskSnapshot=@{}
 $taskPaths=@($taskManifest.install_paths.PSObject.Properties.Value)+@('/system/etc/x2d-pixel-overclock/launch.sh','/system/etc/x2d-pixel-overclock/run.sh','/system/lib64/libx2d_native_menu.so')
 foreach($taskPath in ($taskPaths | Select-Object -Unique)){
  $r=Reviewed @((FileStateCommand $taskPath))
  $v=$r[0].Trim();if($v -eq 'ABSENT'){$taskSnapshot[$taskPath]=$null}elseif($v -match '^([0-9a-f]{64}) '){$taskSnapshot[$taskPath]=$Matches[1]}else{throw 'Invalid file state'}
 }
 $taskSnapshot | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $PackageRoot 'device-baseline.json') -Encoding UTF8
 Reviewed @('df -k /system /data','test ! -e /tmp/x2d-multishot-active && echo STACK_NO_ACTIVE_TASK','cat /tmp/x2d-pixel-render-route.status','cat /tmp/x2d-native-menu-preload.status')
 exit
}
# PATCH_INSTALL_BODY_BEGIN
if($Action -in @('Check','Stage','Activate','Verify','VerifyCurrent')){Test-PatchNativeUsbPrompt {param([string[]]$Commands) return ,(Reviewed $Commands)}}
$install=Get-Content -LiteralPath (Join-Path $PackageRoot 'install-manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
Set-PatchContext -Route $install.package_type -Stage $Action
if($Action -eq 'Check'){
 $guards=@{'/system/bin/camera-storage'='8c086e9c2a340c2a388e92ae3de39d6d0e5ce19ac3d582626112a24fd3b97aef';'/system/bin/camera-service'='fbcf828f73bca13f0c8b95e7dd0b95ac483ae36954ec06179098c8a1a65f9f82'}
 if($install.package_type -eq 'stock'){$guards=@{};foreach($p in $install.required_guards.PSObject.Properties){$guards[$p.Name]=$p.Value}}
 if($install.face_eye_included){$guards['/system/lib64/libaaa.so']='feef8a8dc3a27395e47232c2b25a5da7a7ab335922fbb527e637c439e35bcec7';$guards['/system/lib64/librcam.so']='72ebc8deebce4a29047c475e77ab4edbf2860abb1f572fea45260fe17ad0bda5'}
 foreach($path in $guards.Keys){$r=Reviewed @("test ! -L $path && sha256sum $path");Set-PatchContext -Expected $guards[$path] -Actual $r[0];if($r[0] -notmatch ('^'+$guards[$path]+' ')){throw ('Firmware or required component does not match: '+$path)}}
 foreach($prop in $install.targets.PSObject.Properties){
  $path=$prop.Value;$r=Reviewed @((FileStateCommand $path))
  $actual=$r[0].Trim();$old=$install.original.($prop.Name);$new=$install.files.($prop.Name)
  Set-PatchContext -Expected ('原版='+$old+'；候选='+$new) -Actual $actual
  if($actual -ne 'ABSENT' -or $old){if($actual -notmatch ('^'+$new+' ') -and (!$old -or $actual -notmatch ('^'+$old+' '))){throw ('Component mismatch: '+$prop.Name)}}
 }
 if($install.package_type -ne 'stock'){
  $r=Reviewed @('cat /tmp/x2d-pixel-overclock/status.json');$state=$r[0]|ConvertFrom-Json
  if($state.busy -or !$state.ready){throw 'Camera worker is not idle and ready'}
 }
 Write-Output STACK_CONNECTION_AND_BASELINE_VERIFIED_NO_IDENTITY_WHITELIST
 exit
}elseif($Action -eq 'UpdateScript'){
 $validation=Get-Content -LiteralPath (Join-Path $PackageRoot 'install-validation.json') -Raw -Encoding UTF8 | ConvertFrom-Json
 if($validation.script_sha256 -ne $install.install_sha256){throw 'Offline transaction validation differs'}
 $old=Get-Content -LiteralPath (Join-Path $PackageRoot 'upload-v1/install-manifest.json') -Raw -Encoding UTF8|ConvertFrom-Json
 $r=Reviewed @("cd $taskStage && test ! -e started && test ! -L install.sh && sha256sum install.sh")
 if($r[0] -notmatch ('^'+$old.install_sha256+' ')){throw 'Staged original script differs'}
 $patchFiles=@($install.files.PSObject.Properties | Where-Object { $old.files.($_.Name) -ne $_.Value })
 $patchFiles+= [pscustomobject]@{Name='install.sh';Value=$install.install_sha256}
 $index=0
 foreach($patch in $patchFiles){
  $name=$patch.Name;$hash=$patch.Value;$previous=$old.files.($name);if($name -eq 'install.sh'){$previous=$old.install_sha256}
  $r=Reviewed @("cd $taskStage && ("+(FileStateCommand $name)+")")
  if($r[0] -match ('^'+$hash+' ')){$index++;continue}
  if($previous){if($r[0] -notmatch ('^'+$previous+' ')){throw 'Staged patch baseline differs'}}elseif($r[0].Trim() -ne 'ABSENT'){throw 'New staged patch file already exists'}
  $encoded=[Convert]::ToBase64String([IO.File]::ReadAllBytes((Join-Path $PackageRoot $name)));$token="patch-$index";$index++
  $state=Reviewed @("cd $taskStage && if test -e $token.b64; then test ! -L $token.b64 && wc -c < $token.b64 && sha256sum $token.b64; else echo ABSENT; fi")
  $offset=0
  if($state[0].Trim() -ne 'ABSENT'){
   $lines=$state[0].Trim() -split '\r?\n';if($lines.Count -ne 2 -or $lines[0].Trim() -notmatch '^[0-9]+$'){throw 'Patch prefix state invalid'}
   $offset=[int]$lines[0].Trim();if($offset -gt $encoded.Length -or (Digest ([Text.Encoding]::ASCII.GetBytes($encoded.Substring(0,$offset)))) -ne ($lines[1].Trim() -split '\s+')[0]){throw 'Patch prefix differs'}
  }else{Reviewed @("cd $taskStage && (set -C; : > $token.b64)")|Out-Null}
  $batch=[Collections.Generic.List[string]]::new()
  for($i=$offset;$i -lt $encoded.Length;$i+=170){$part=$encoded.Substring($i,[Math]::Min(170,$encoded.Length-$i));$batch.Add("printf '%s' '$part' >> $taskStage/$token.b64");if($batch.Count -eq 24){Reviewed $batch.ToArray()|Out-Null;$batch.Clear();Write-Output "STACK_PATCH_TRANSFER=$name"}}
  if($batch.Count){Reviewed $batch.ToArray()|Out-Null}
  Reviewed @("cd $taskStage && if test ! -e $token.new; then test ! -L $token.new && (set -C; /system/xbin/busybox base64 -d $token.b64 > $token.new); fi")|Out-Null
  $r=Reviewed @("sha256sum $taskStage/$token.new");if($r[0] -notmatch ('^'+$hash+' ')){throw 'Corrected file transfer differs'}
  Reviewed @("cd $taskStage && test ! -e started && /system/xbin/busybox mv $token.new $name")|Out-Null
 }
 Write-Output STACK_CORRECTED_SCRIPT_STAGED_HASH_VERIFIED
 exit
}elseif($Action -eq 'Stage'){
 $taskTar=Join-Path $PackageRoot 'install-payload.tar.gz'
 if((Digest ([IO.File]::ReadAllBytes($taskTar))) -ne $install.archive_sha256){throw 'Package archive changed'}
 Initialize-PatchStageDirectories
 $stageState=StageState
 if($stageState -eq 'TRANSACTION'){PreserveTerminalStage | ForEach-Object {Write-Output $_}}
 elseif($stageState -ne 'STACK_STAGE_IDLE'){throw 'Staging directory state unavailable'}
 $complete=Reviewed @("cd $taskStage && if test -f received; then cat received; else echo ABSENT; fi")
 if($complete[0].Trim() -eq $install.archive_sha256){Write-Output STACK_ARCHIVE_ALREADY_RECEIVED;exit}
 # 原厂自带接收工具走二进制 TCP；旧小命令暂存保留，失败不回退慢速上传。
 . (Join-Path $PackageRoot 'PatchBinaryTransfer.ps1')
 $selected=$false;$archiveReady=$false
 for($slot=0;$slot -lt 8;$slot++){
  $token='p-'+$install.archive_sha256.Substring(0,8)+'-bin-'+$slot
  $gz=Reviewed @("cd $taskStage && ("+(FileStateCommand ($token+'.gz'))+")")
  if($gz[0] -match ('^'+$install.archive_sha256+' ')){$archiveReady=$true;$selected=$true;break}
  if($gz[0].Trim() -eq 'ABSENT'){$selected=$true;break}
  if($gz[0] -notmatch '^[0-9a-f]{64} '){throw 'Invalid staged archive state'}
  Write-Output "STACK_INCOMPLETE_ARCHIVE_PRESERVED=$token"
 }
 if(!$selected){throw 'All binary transfer slots differ; staged data preserved'}
 if(!$archiveReady){
  Invoke-PatchBinaryTransfer $taskTar $install.archive_sha256 "$taskStage/$token.gz" {param([string[]]$Commands) Reviewed $Commands}
 }
 $h=Reviewed @("sha256sum $taskStage/$token.gz");if($h[0] -notmatch ('^'+$install.archive_sha256+' ')){throw 'Transferred archive differs'}
 if((StageState) -ne 'STACK_STAGE_IDLE'){throw 'Staging transaction changed during transfer'}
 Reviewed @("cd $taskStage && /system/xbin/busybox tar xzf $token.gz && printf '%s' '$($install.archive_sha256)' > received") | Out-Null
 Write-Output STACK_FILES_RECEIVED_HASH_VERIFIED
}elseif($Action -eq 'Activate'){
 if(!(Test-Path -LiteralPath (Join-Path $PackageRoot 'install-validation.json'))){throw 'Offline install checks missing'}
 $validation=Get-Content -LiteralPath (Join-Path $PackageRoot 'install-validation.json') -Raw -Encoding UTF8|ConvertFrom-Json
 if($validation.script_sha256 -ne $install.install_sha256){throw 'Offline validation script differs'}
 $r=Reviewed @("cd $taskStage && if test -e done; then cat done; else echo ABSENT; fi")
 if($r[0].Trim() -match '^RAW_STACK_(BACKEND_INSTALLED_NO_CAPTURE|STOCK_FILES_INSTALLED_MANUAL_REBOOT_REQUIRED)$'){Write-Output STACK_INSTALL_ALREADY_DONE;exit}
 $h=Reviewed @("sha256sum $taskStage/install.sh");if($h[0] -notmatch ('^'+$install.install_sha256+' ')){throw 'Installation script changed'}
 Reviewed @("cd $taskStage && test ! -e started && (set -C; : > started) && nohup sh install.sh > install.log 2>&1 < /dev/null &")
 Write-Output STACK_INSTALL_DISPATCHED_CHECK_VERIFY
}else{
 if($Action -ne 'VerifyCurrent'){
 for($attempt=0;$attempt -lt 90;$attempt++){
  $r=Reviewed @("cd $taskStage && if test -e done; then cat done; elif test -e result; then cat result && tail -n 15 install.log; else tail -n 3 install.log; fi")
  if($r[0] -match 'RAW_STACK_(BACKEND_INSTALLED_NO_CAPTURE|STOCK_FILES_INSTALLED_MANUAL_REBOOT_REQUIRED)'){break}
  if($r[0] -match '^\s*([0-9]+)\s*\r?\n'){$script:PatchContext.camera_exit=$Matches[1];$script:PatchContext.camera_log=Protect-PatchText $r[0];throw ('Installation ended: '+$r[0])}
  Start-Sleep -Milliseconds 1000
 }
 $r
 if($r[0] -notmatch 'RAW_STACK_(BACKEND_INSTALLED_NO_CAPTURE|STOCK_FILES_INSTALLED_MANUAL_REBOOT_REQUIRED)'){throw 'Installation has not confirmed completion'}
 }
 foreach($prop in $install.targets.PSObject.Properties){
  $h=Reviewed @("test ! -L $($prop.Value) && sha256sum $($prop.Value)")
  Set-PatchContext -Expected $install.files.($prop.Name) -Actual $h[0]
  if($h[0] -notmatch ('^'+$install.files.($prop.Name)+' ')){throw ('Installed file differs: '+$prop.Value)}
 }
 if($Action -eq 'PollInstall'){Write-Output STACK_FILES_VERIFIED_MANUAL_REBOOT_REQUIRED;exit}
 Set-PatchContext -Stage '已安装文件通过；核对后台运行状态'
 Write-Output '安装文件已完整核对，接下来只核对运行状态；不会重新安装。'
 # 临时状态缺失或不可读也是运行证据；正常回传标记，不让 cat 中断整个只读批次。
 $menuRead='f=/tmp/x2d-native-menu-preload.status; if test -L $f; then echo LINK; elif test ! -e $f; then echo ABSENT; elif test ! -r $f; then echo UNREADABLE; else head -c 256 $f 2>/dev/null || echo READ_FAILED; fi'
 $uiRead='f=/tmp/x2d-pixel-overclock/ui.txt; if test -L $f; then echo LINK; elif test ! -e $f; then echo ABSENT; elif test ! -r $f; then echo UNREADABLE; else head -c 1024 $f 2>/dev/null || echo READ_FAILED; fi'
 $r=Reviewed @('f=/tmp/x2d-multishot-active; if test -L "$f"; then echo STACK_LOCK_UNSUPPORTED; elif test -d "$f"; then echo STACK_LOCK_HELD; elif test -e "$f"; then echo STACK_LOCK_UNSUPPORTED; else echo STACK_LOCK_RELEASED; fi',$menuRead,'f=/tmp/x2d-pixel-overclock/status.json; if test ! -L "$f" && test -f "$f"; then head -c 4096 "$f" | /system/xbin/busybox base64; else echo ABSENT; fi','awk ''$2=="/system"{print $4}'' /proc/mounts',$uiRead)
 $menuState=$r[1].Trim();$uiState=$r[4].Trim()
 $script:PatchContext.state=Protect-PatchText ('任务锁='+$r[0].Trim()+'；菜单='+$menuState+'；'+(Get-PatchWorkerSummary $r[2])+'；UI='+$uiState)
 $script:PatchContext.readonly=if($r[3].Trim() -match '(^|,)ro(,|$)'){'已读取：只读'}else{'未确认只读；挂载回复='+$r[3].Trim()}
 Set-PatchContext -Expected '菜单、后台、界面就绪；系统只读' -Actual $script:PatchContext.state
 if($menuState -ne 'MENU_AND_AFC_POINTERS_READY'){
  Set-PatchContext -Target '/tmp/x2d-native-menu-preload.status' -Command $menuRead -Expected 'MENU_AND_AFC_POINTERS_READY' -Actual $menuState
  $menuReason=switch($menuState){
   'ABSENT' {'菜单启动状态文件缺失'}
   'UNREADABLE' {'菜单启动状态文件不可读'}
   'LINK' {'菜单启动状态文件是链接，未读取'}
   'READ_FAILED' {'菜单启动状态文件读取失败'}
   default {'菜单尚未确认正常挂接：'+$menuState}
  }
  throw ($menuReason+'；安装文件已完整核对，运行状态未确认。请读取诊断，无需重复安装。')
 }
 try{$state=[Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($r[2]))|ConvertFrom-Json}catch{throw '后台状态格式不完整；安装文件已核对，不需要重复安装。'}
 foreach($key in @('busy','ready','prepared','preparing')){if($state.$key -isnot [bool]){throw '后台状态门控字段缺失或类型不符；安装文件已核对，不需要重复安装。'}}
 if($state.protocol -ne 3){throw '后台状态协议不符；安装文件已核对，不需要重复安装。'}
 if($state.busy -or $state.preparing){throw '安装文件已核对；相机正在准备、拍摄或处理，请等任务结束后再核对，无需重新安装。'}
 $preparedReady=$state.ready -and $state.prepared -and $r[0].Trim() -eq 'STACK_LOCK_HELD'
 $idleReady=$state.ready -and !$state.prepared -and $r[0].Trim() -eq 'STACK_LOCK_RELEASED'
 if(!$preparedReady -and !$idleReady){throw '后台状态与任务锁不一致或未就绪；安装文件已核对，请读取诊断，不要重复安装。'}
 if($preparedReady){Write-Output '后台已完成模式提前准备，任务锁由准备流程持有；运行核对通过。'}
 if($r[3].Trim() -notmatch '(^|,)ro(,|$)' -or $r[4] -notmatch 'rawStackUI=3 popup=1'){throw 'Read-only mount or UI version not confirmed'}
 $r=Reviewed @('/bin/dbus-send --system --print-reply --dest=com.hasselblad.error /error org.freedesktop.DBus.Properties.Get string:com.hasselblad.error string:active_error | awk ''/variant/{print $NF}''','/bin/dbus-send --system --print-reply --dest=com.hasselblad.camera /camera org.freedesktop.DBus.Properties.Get string:com.hasselblad.camera string:live_view_state | awk ''/variant/{print $NF}''')
 if($r[0].Trim() -ne 'false'){throw 'Camera reports an active error'}
 if($r[1].Trim() -notmatch '^[01]$'){throw 'Live view state unavailable'}
 if($install.face_eye_included){
  Set-PatchContext -Stage '人脸与人眼后端核对' -Target '/tmp/x2d-face-lock-backend.json'
  $faceReply=Reviewed @('pidof camera-service; cat /tmp/x2d-face-lock-backend.json')
  $faceLines=$faceReply[0].Trim() -split '\r?\n',2
  if($faceLines.Count -ne 2 -or $faceLines[0] -notmatch '^\d+$'){throw 'Face backend process evidence unavailable'}
  $face=$faceLines[1]|ConvertFrom-Json
  if($face.protocol -ne 1 -or !$face.ready -or $face.pid -ne [int]$faceLines[0] -or $face.eyeProtocol -ne 1 -or $face.ageFrames -ne 10 -or $face.ageError -ne 0){throw 'Face/eye backend has not confirmed readiness for current camera-service'}
  Set-PatchContext -Stage '八镜头对焦增强后端核对' -Target '/tmp/x2d-af-probe.json'
  $afReply=Reviewed @('pidof camera-service; head -c 600 /tmp/x2d-af-probe.json | sed ''s/,"events":.*$/}/''')
  $afLines=$afReply[0].Trim() -split '\r?\n',2
  if($afLines.Count -ne 2 -or $afLines[0] -notmatch '^\d+$'){throw 'AF backend process evidence unavailable'}
  $af=$afLines[1]|ConvertFrom-Json
  if($af.protocol -ne 1 -or !$af.ready -or !$af.sender -or $af.pid -ne [int]$afLines[0]){throw 'AF enhancement has not confirmed readiness for current camera-service'}
 }
 @{installed=$true;physical_capture_verified=$false;identity_whitelist=$false;files=$install.files}|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $PackageRoot 'device-install-validation.json') -Encoding UTF8
 Write-Output STACK_INSTALL_FINAL_HASHES_VERIFIED
}
# PATCH_INSTALL_BODY_END
} catch {
 Write-PatchFailure $_
 Write-Error $_ -ErrorAction Continue
 exit 1
}
