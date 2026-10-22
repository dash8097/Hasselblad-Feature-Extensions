# 结构化诊断与固定只读命令；不在导入时访问设备。
function Protect-PatchText([string]$Text,[int]$Limit=8192) {
 if(!$Text){return ''}
 $value=$Text -replace '(?i)[A-Z]:\\Users\\[^\\\s]+','<用户目录>'
 $value=$value -replace '(?i)(serial(?:number)?|serial_number|device_serial)\s*[:=]\s*[^\s,;]+','$1=<已隐去>'
 $value=$value -replace '(?i)\\\\\?\\usb#[^\s]+','<USB设备路径已隐去>'
 $value=$value -replace '(?:/mnt/media_rw)?/ssd/[^\s"'']+','<照片路径已隐去>'
 $value=$value -replace '(?i)\bB\d{7,}\.(jpg|jpeg|heif|heic|3fr|raw)\b','<照片名已隐去>'
 if($value.Length -gt $Limit){$value=$value.Substring(0,$Limit)+"`n[超过诊断上限，已截断]"}
 return $value
}
function Initialize-PatchDiagnostic([string]$Action) {
 $script:PatchContext=[ordered]@{protocol=1;action=$Action;stage='初始化';route='未识别';command='未执行';target='未取得';expected='未取得';actual='未取得';host_exit='未取得';camera_exit='未取得';reason='';state='未确认';recovery='未确认，不能据此重复安装';readonly='未确认';transport='未连接';camera_log='未读取';time=(Get-Date).ToString('yyyy-MM-dd HH:mm:ss zzz')}
}
function Get-PatchNativeErrorSummary([string]$Reply) {
 # 只保留错误门控与编号；METADATA、UID 等原厂私有内容不进入报告。
 if($Reply.Length -le 16384 -and $Reply.Trim() -match '^[A-Za-z0-9+/=\r\n]+$'){
  try{$Reply=[Text.UTF8Encoding]::new($false,$true).GetString([Convert]::FromBase64String($Reply))}catch{$Reply=''}
 }
 $active='未取得'
 $flags=[regex]::Matches($Reply,'string "active_error"\s+variant\s+boolean (true|false)\s*\)')
 if($flags.Count -eq 1){$active=$flags[0].Groups[1].Value}
 $fields=@{}
 foreach($key in @('CODE','CAT','ACTION')){
  $matches=[regex]::Matches($Reply,('dict entry\(\s*string "'+$key+'"\s+variant\s+(?:int32|uint32) ([0-9]+)\s*\)'))
  $number=0
  if($matches.Count -eq 1 -and [int]::TryParse($matches[0].Groups[1].Value,[ref]$number)){$fields[$key]=[string]$number}else{$fields[$key]='未取得'}
 }
 $text='原厂错误标志='+$active+'；CODE='+$fields.CODE+'；CAT='+$fields.CAT+'；ACTION='+$fields.ACTION
 if($active -eq 'true' -and $fields.CODE -eq '40' -and $fields.CAT -eq '5' -and $fields.ACTION -eq '1'){
  $text+='；请跳过大容量存储。请在相机上操作，保持 USB 连接，再重新检查。'
 }elseif($active -eq 'true'){$text+='；原厂有活动错误，请先核对相机提示；安装器不会自动确认或清除。'}
 if($active -eq '未取得'){$text+='；错误服务回复未能完整解析。'}
 return $text
}
function Test-PatchNativeUsbPrompt([scriptblock]$Read) {
 # 安装操作只自动确认已核实的 USB 提示；Diagnose 不调用此入口。
 $command='/bin/dbus-send --system --print-reply --dest=com.hasselblad.error /error org.freedesktop.DBus.Properties.GetAll string:com.hasselblad.error | /system/xbin/busybox base64'
 Set-PatchContext -Stage 'USB 连接提醒检查' -Command $command -Target '原厂错误服务' -Expected '识别 USB 提示后自动确认一次并回读' -Transport '请求中'
 $replies=@(& $Read ([string[]]@($command)))
 if($replies.Count -eq 1 -and $replies[0] -is [array]){$replies=@($replies[0])}
 if($replies.Count -ne 1){throw 'USB 连接提醒状态未取得，请重新检查。'}
 $summary=Get-PatchNativeErrorSummary ([string]$replies[0])
 Set-PatchContext -Actual $summary -Transport '收到回复；不等于安装成功'
 if($summary.StartsWith('原厂错误标志=true；CODE=40；CAT=5；ACTION=1；')){
  if($script:PatchContext.Contains('usb_prompt_ack_attempted')){throw '本次操作已发送过 USB 提示确认，不自动重发。'}
  $script:PatchContext['usb_prompt_ack_attempted']=$true
  $ack='/bin/dbus-send --system --print-reply --reply-timeout=3000 --dest=com.hasselblad.error /error com.hasselblad.error.ack_error int32:40'
  Set-PatchContext -Stage '自动跳过大容量存储提示' -Command $ack -Target 'USB 连接提醒' -Transport '请求中'
  $result=@(& $Read ([string[]]@($ack)))
  if($result.Count -eq 1 -and $result[0] -is [array]){$result=@($result[0])}
  if($result.Count -ne 1 -or ([string]$result[0]).Trim() -notmatch '\Amethod return[^\r\n]*\z'){throw 'USB 提示确认回执未取得；未重发。'}
  Set-PatchContext -Stage '回读 USB 提示状态' -Command $command -Transport '请求中'
  $result=@(& $Read ([string[]]@($command)))
  if($result.Count -eq 1 -and $result[0] -is [array]){$result=@($result[0])}
  if($result.Count -ne 1){throw 'USB 提示确认后的状态未取得；未重发。'}
  $after=Get-PatchNativeErrorSummary ([string]$result[0])
  Set-PatchContext -Actual $after -Transport '收到确认后的状态回复'
  if(!$after.StartsWith('原厂错误标志=false；')){throw ('USB 提示确认后仍未就绪；未重发。'+$after)}
  Set-PatchContext -Actual '已自动跳过大容量存储提示；不代表安装或拍摄成功'
 }
 # 其他原厂错误继续由既有空闲检查处理，不在这里扩充安装门槛。
}
function Get-PatchWorkerSummary([string]$Reply) {
 try{
  if($Reply.Length -le 8192 -and $Reply.Trim() -match '^[A-Za-z0-9+/=\r\n]+$'){$Reply=[Text.UTF8Encoding]::new($false,$true).GetString([Convert]::FromBase64String($Reply))}
  $worker=$Reply|ConvertFrom-Json
  $flags=@($worker.PSObject.Properties|Where-Object {$_.Name -in @('ready','busy','preparing','prepared') -and $_.Value -is [bool]}|ForEach-Object {$_.Name+'='+$_.Value})
  if(!$flags.Count){return '未取得有效 worker 状态'}
  $summary=$flags -join '; '
  if($worker.stage -is [string] -and $worker.stage -match '^[a-z_]{1,40}$'){$summary+='；阶段='+$worker.stage}
  if($worker.error -is [string] -and $worker.error.Length -le 256){$summary+='；后台错误='+(Protect-PatchText $worker.error)}
  return $summary
 }catch{return '未取得有效 worker 状态'}
}
function Get-PatchTaskLogSummary([string]$Reply) {
 if($Reply.Trim() -eq 'ABSENT' -or !$Reply.Trim()){return '日志不存在或不可读'}
 try{
  if($Reply.Length -gt 8192 -or $Reply.Trim() -notmatch '^[A-Za-z0-9+/=\r\n]+$'){throw 'LOG_TRANSPORT'}
  $text=[Text.UTF8Encoding]::new($false,$true).GetString([Convert]::FromBase64String($Reply))
  return (Protect-PatchText $text)
 }catch{return '任务日志运输格式未确认'}
}
function Write-PatchDiagnosticObject([string]$Prefix,$Object) {
 $json=$Object|ConvertTo-Json -Depth 5 -Compress
 $line=$Prefix+[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($json))
 # 进度旁路不能混入 Reviewed 的返回数组，否则会误判相机回复。
 if($Prefix -eq 'PATCH_CONTEXT='){[Console]::WriteLine($line)}else{Write-Output $line}
}
function Set-PatchContext([string]$Stage,[string]$Route,[string]$Command,[string]$Target,[string]$Expected,[string]$Actual,[string]$Transport) {
 if(!$script:PatchContext){Initialize-PatchDiagnostic 'Unknown'}
 foreach($name in @('Stage','Route','Command','Target','Expected','Actual','Transport')){
  if($PSBoundParameters.ContainsKey($name)){$script:PatchContext[$name.ToLowerInvariant()]=Protect-PatchText $PSBoundParameters[$name]}
 }
 Write-PatchDiagnosticObject 'PATCH_CONTEXT=' $script:PatchContext
}
function Set-PatchCommandContext([string[]]$Commands) {
 # 上传字节不是诊断证据，禁止将其写入报告。
 $display=@($Commands|ForEach-Object {if($_ -match "^printf '%s' '[A-Za-z0-9+/=]+' >> (.+)$"){'传输载荷分块（数据省略） -> '+$Matches[1]}else{$_}}) -join "`n"
 $target='固定状态检查'
 if($display -match '(/(?:system|data|tmp)/[A-Za-z0-9_./-]+)'){$target=$Matches[1]}
 Set-PatchContext -Command $display -Target $target -Expected '未设置独立期望值' -Actual '本次请求尚未回复' -Transport '请求中'
}
function Set-PatchReplyContext([string[]]$Replies) {
 $text=$Replies -join "`n"
 if($script:PatchContext.command -match '(string:current_error|Properties.GetAll string:com.hasselblad.error)'){
  $text=Get-PatchNativeErrorSummary $text
 }elseif($script:PatchContext.command -match 'status.json'){
  $text='运行状态回复（仅保留门控、阶段和错误原因）'
  foreach($reply in $Replies){$text+='；'+(Get-PatchWorkerSummary $reply)}
 }
 Set-PatchContext -Actual $text -Transport '收到回复；不等于操作成功'
}
function Set-PatchUsbFailureContext($ErrorRecord) {
 # UsbFailure 是自定义异常，并不继承 Win32Exception；必须保留其 Win32 字段。
 $inner=$ErrorRecord.Exception;$code='';$native=0
 for($depth=0;$inner -and $depth -lt 16;$depth++){
  if($inner.PSObject.Properties['Code'] -and [string]$inner.Code -match '^[A-Z0-9_]+$'){$code=[string]$inner.Code}
  if($inner.PSObject.Properties['Win32']){$native=[int]$inner.Win32}
  if($inner -is [ComponentModel.Win32Exception]){$native=$inner.NativeErrorCode}
  $inner=$inner.InnerException
 }
 if(!$code -and !$native){return}
 $detail=switch($native){
  121 {'Windows 接收/操作超时'}
  1167 {'Windows 报告设备未连接'}
  995 {'Windows 报告操作已取消'}
  31 {'Windows 报告设备操作失败'}
  5 {'Windows 报告访问被拒绝'}
  0 {'未提供 Windows 错误码'}
  default {'Windows 设备接口错误'}
 }
 $script:PatchContext.reason+='；运输错误='+$code+'；Win32='+$native+'（'+$detail+'）'
 if($code -match 'SHELL_REMOTE_FAILURE$'){
  $script:PatchContext.transport='相机已回复：命令返回失败；不是 USB 无回复'
  $script:PatchContext.camera_exit='非零，协议未提供具体退出码'
 }elseif($native -or $code -match '(SHELL_RECEIVE|USB_READ|USB_MATCH_DEADLINE|USB_OPEN)$'){
  $script:PatchContext.transport='USB 请求已结束但未完成；'+$detail+'；Win32='+$native
  $script:PatchContext.actual='未取得本条命令的完整回复；不能据此判断目标文件缺失或安装损坏。本批后续命令未发送。'
 }
}
function Test-PatchFileStates([string[]]$Paths,[string[]]$Expected,[scriptblock]$Read) {
 # 固定只读文件检查复用一个批次连接；不重发失败请求，不改变安装步骤。
 if(!$Paths.Count -or $Paths.Count -ne $Expected.Count){throw 'FILE_STATE_ARGUMENT_COUNT'}
 $commands=@()
 for($i=0;$i -lt $Paths.Count;$i++){
  if($Paths[$i] -notmatch '^/system/[A-Za-z0-9_./-]+$' -or $Paths[$i].Contains('..') -or $Expected[$i] -notin @('FILE','ABSENT')){throw 'FILE_STATE_ARGUMENT_RANGE'}
  $command='f='+$Paths[$i]+';v=ABSENT;[ -e "$f" ]&&v=UNSUPPORTED_TYPE;[ -f "$f" ]&&v=FILE;[ -L "$f" ]&&v=UNSUPPORTED_LINK;echo "$v"'
  if($command.Length -gt 231){throw 'FILE_STATE_COMMAND_LIMIT'}
  $commands+=$command
 }
 for($offset=0;$offset -lt $commands.Count;$offset+=100){
  $last=[Math]::Min($offset+99,$commands.Count-1)
  $batch=[string[]]@($commands[$offset..$last])
  Set-PatchCommandContext $batch
  try{$reply=@(& $Read $batch)}catch{
   $message=[string]$_.Exception.Message
   if($message -match 'BATCH_STEP_([0-9]+)_'){
    $index=[int]$Matches[1]
    if($index -lt $batch.Count){
     $index+=$offset
     Set-PatchContext -Target $Paths[$index] -Command $commands[$index] -Expected $Expected[$index] -Actual '本条检查未取得完整回复；本批后续命令未发送'
    }
   }
   throw
  }
  if($reply.Count -eq 1 -and $reply[0] -is [array]){$reply=@($reply[0])}
  if($reply.Count -ne $batch.Count){throw 'FILE_STATE_REPLY_COUNT'}
  for($i=0;$i -lt $reply.Count;$i++){
   $index=$offset+$i;$state=([string]$reply[$i]).Trim()
   Set-PatchContext -Target $Paths[$index] -Command $commands[$index] -Expected $Expected[$index] -Actual $state -Transport '收到文件状态回复；不等于安装成功'
   if($state -cne $Expected[$index]){throw ('安装前文件状态不符：'+$Paths[$index]+'；期望='+$Expected[$index]+'；实际='+$state)}
  }
 }
}
function Write-PatchFailure($ErrorRecord,[int]$HostExit=1) {
 $script:PatchContext.host_exit=[string]$HostExit
 $script:PatchContext.reason=Protect-PatchText ([string]$ErrorRecord.Exception.Message)
 Set-PatchUsbFailureContext $ErrorRecord
 Write-PatchDiagnosticObject 'PATCH_DIAGNOSTIC=' $script:PatchContext
}
function Get-PatchTaskSelectionCommand {
 return 'ls -td /mnt/media_rw/ssd/multishot/native-album-ui-* 2>/dev/null|head -n 1|sed ''s|.*/||''; true'
}
function Get-PatchPropertyReply([string]$Reply) {
 # 原厂属性错误也走 ASCII 运输；保留错误正文，不把读取失败当成状态 0。
 if($Reply.Trim() -match '^[A-Za-z0-9+/=\r\n]+$'){
  try{return (Protect-PatchText ([Text.UTF8Encoding]::new($false,$true).GetString([Convert]::FromBase64String($Reply))))}catch{return '属性回复运输格式未确认'}
 }
 return (Protect-PatchText $Reply)
}
function Invoke-PatchCameraDiagnostics([scriptblock]$Read) {
 # 先固定一次任务编号；之后所有日志读取保持同一个目录。
 Set-PatchCommandContext ([string[]]@((Get-PatchTaskSelectionCommand)))
 $selected=@(& $Read ([string[]]@((Get-PatchTaskSelectionCommand))))
 if($selected.Count -eq 1 -and $selected[0] -is [array]){$selected=@($selected[0])}
 if($selected.Count -ne 1){throw '任务选择回复数量不完整'}
 $job=([string]$selected[0]).Trim()
 if(!$job){$job='ABSENT'}
 $items=@(Get-PatchDiagnosticCommands -TaskJob $job)
 Set-PatchContext -Stage '手动读取现场（不安装）' -Target '固定只读诊断批次' -Command ($items.command -join "`n") -Expected '各项诊断回复；属性错误保留原文' -Actual '本批请求尚未完成' -Transport '请求中'
 try{$replies=@(& $Read ([string[]]@($items|ForEach-Object {$_.command})))}catch{
  $errorText=[string]$_.Exception.Message
  if($errorText -match 'BATCH_STEP_([0-9]+)_'){
   $index=[int]$Matches[1]
   if($index -lt $items.Count){
    Set-PatchContext -Stage ('诊断读取：'+$items[$index].name) -Target $items[$index].path -Command $items[$index].command -Actual '本批未取得完整结果；该项失败，后续命令未发送'
   }
  }
  if($errorText -match 'SHELL_REMOTE_FAILURE'){
   Set-PatchContext -Transport '相机已回复：该诊断命令返回失败；不是 USB 无回复'
  }else{Set-PatchContext -Transport '诊断运输未完成，回复证据未取得'}
  throw
 }
 if($replies.Count -eq 1 -and $replies[0] -is [array]){$replies=@($replies[0])}
 Convert-PatchCameraDiagnostics $items ([string[]]$replies)
}
function Get-PatchDiagnosticCommands([string]$TaskJob='ABSENT') {
 if($TaskJob -ne 'ABSENT' -and $TaskJob -cnotmatch '^native-album-ui-[0-9a-f]{8}-[1-9][0-9]{0,9}$'){throw '任务编号格式未确认'}
 $items=[Collections.Generic.List[object]]::new()
 foreach($path in @('/data','/data/local','/data/local/tmp','/data/local/tmp/x2d-rs1','/data/local/tmp/x2d-rs8')){
  $command='d='+$path+'; if test -L "$d"; then echo LINK; elif test -d "$d"; then echo DIRECTORY; elif test -e "$d"; then echo OTHER; else echo ABSENT; fi'
  $items.Add([pscustomobject]@{name='directory'+$path;command=$command;path=$path})
 }
 foreach($kind in @('stock','upgrade')){
  $dir=if($kind -eq 'stock'){'/data/local/tmp/x2d-rs1'}else{'/data/local/tmp/x2d-rs8'}
  foreach($file in @('result','done','install.log')){
   $command='d='+$dir+'; f=$d/'+$file+'; if test ! -L $d && test -d $d && test ! -L $f && test -f $f; then tail -n 20 $f | head -c 4096; else echo ABSENT; fi'
   $items.Add([pscustomobject]@{name=$kind+'/'+$file;command=$command;path=$dir+'/'+$file})
  }
 }
 foreach($entry in @(@('menu','/tmp/x2d-native-menu-preload.status'),@('worker','/tmp/x2d-pixel-overclock/status.json'),@('ui','/tmp/x2d-pixel-overclock/ui.txt'))){
  $path=$entry[1];$dir=$path.Substring(0,$path.LastIndexOf('/'))
  $cmd='d='+$dir+'; f='+$path+'; if test ! -L $d && test ! -L $f && test -f $f; then head -c 4096 $f; else echo ABSENT; fi'
  if($entry[0] -eq 'worker'){$cmd='f='+$path+'; if test ! -L /tmp/x2d-pixel-overclock && test ! -L $f && test -f $f; then head -c 4096 $f | /system/xbin/busybox base64; else echo ABSENT; fi'}
  $items.Add([pscustomobject]@{name=$entry[0];command=$cmd;path=$path})
 }
 $items.Add([pscustomobject]@{name='mount';command='awk ''$2=="/system"{print $4}'' /proc/mounts';path='/proc/mounts'})
 $items.Add([pscustomobject]@{name='lock';command='if test -e /tmp/x2d-multishot-active; then echo PRESENT; else echo ABSENT; fi';path='/tmp/x2d-multishot-active'})
 $items.Add([pscustomobject]@{name='native_error';command='/bin/dbus-send --system --print-reply --dest=com.hasselblad.error /error org.freedesktop.DBus.Properties.GetAll string:com.hasselblad.error | head -c 8192 | /system/xbin/busybox base64';path='/error'})
 foreach($entry in @(@('exposure','camera','exposure_status'),@('storage','storage','storage_status'))){
  $cmd='/bin/dbus-send --system --print-reply --dest=com.hasselblad.'+$entry[1]+' /'+$entry[1]+' org.freedesktop.DBus.Properties.Get string:com.hasselblad.'+$entry[1]+' string:'+$entry[2]
  # 某原厂服务暂时不可用时，返回错误原文而非中断任务日志采集。
  $cmd+=' 2>&1|/system/xbin/busybox base64;true'
  $items.Add([pscustomobject]@{name=$entry[0];command=$cmd;path='/'+$entry[1]})
 }
 foreach($path in @('/mnt/media_rw/ssd','/mnt/media_rw/ssd/multishot')){
  $cmd='d='+$path+'; if test -L "$d"; then echo LINK; elif test -d "$d"; then echo DIRECTORY; test -w "$d" && echo WRITABLE || echo NOT_WRITABLE; elif test -e "$d"; then echo OTHER; else echo ABSENT; fi'
  $items.Add([pscustomobject]@{name='job_root'+$path;command=$cmd;path='任务存储目录状态'})
 }
 $items.Add([pscustomobject]@{name='job_mount';command='awk ''$2=="/mnt/media_rw/ssd"{print $3,$4;n++} END{if(!n)print "ABSENT"}'' /proc/mounts';path='SSD 挂载类型与选项'})
 # 只读最新项目任务的控制日志，不打开 RAW、JPEG、元数据或目录中的其他文件。
 foreach($file in @('task.info','task.result','cleanup.result','failure.result','task.events','task.memory','phase-events','phase-metrics','run.log','capture.log','merge.log','render.log','probe.log','stage','run.rc','capture.rc','restored','memory.failed','finish-restore.log','watchdog.log','finish-metrics')){
  $limit=if($file.EndsWith('.log') -or $file -in @('task.events','phase-events')){4096}elseif($file -in @('finish-metrics','phase-metrics','task.memory','task.info','task.result','cleanup.result','failure.result')){2048}else{128}
  $cmd='j=/mnt/media_rw/ssd/multishot/'+$TaskJob+'; f=$j/'+$file+'; test -d "$j" && test ! -L "$j" && test ! -L "$f" && test -f "$f" && tail -c '+$limit+' "$f"|/system/xbin/busybox base64; true'
  $items.Add([pscustomobject]@{name='task/'+$file;command=$cmd;path='固定任务控制日志/'+$file;task_id=$TaskJob;tail_limit=$limit})
 }
 $items.Add([pscustomobject]@{name='current_memory';command='head -n 5 /proc/meminfo;pidof camera-service camera-storage';path='当前系统内存和服务 PID'})
 foreach($file in @('last-task.txt','last-failed-task.txt','last-cleanup.result')){
  $cmd='d=/tmp/x2d-pixel-overclock; f=$d/'+$file+'; test ! -L "$d" && test ! -L "$f" && test -f "$f" && head -c 6000 "$f"|/system/xbin/busybox base64; true'
  $items.Add([pscustomobject]@{name='archive/'+$file;command=$cmd;path='有界任务摘要/'+$file})
 }
 $items.Add([pscustomobject]@{name='service_failures';command='logcat -d -v threadtime 2>/dev/null|grep -iE "ion_alloc.*failed|VmemAlloc.*failed|Assertion Failed|pre allocated|called exit"|tail -c 4096|/system/xbin/busybox base64; true';path='历史服务异常摘录，不自动归属任务'})
 foreach($item in $items){if([Text.Encoding]::ASCII.GetByteCount($item.command) -gt 231){throw '诊断命令超出 USB 长度限制'}}
 return $items.ToArray()
}
function Convert-PatchCameraDiagnostics($Items,[string[]]$Replies) {
 if($Items.Count -ne $Replies.Count){throw '诊断回复数量不完整'}
 $data=@{}
 for($i=0;$i -lt $Items.Count;$i++){
  $data[$Items[$i].name]=if($Items[$i].name -eq 'native_error'){Get-PatchNativeErrorSummary $Replies[$i]}elseif($Items[$i].name -eq 'worker'){Get-PatchWorkerSummary $Replies[$i]}elseif($Items[$i].name -in @('exposure','storage')){Get-PatchPropertyReply $Replies[$i]}elseif($Items[$i].name -like 'task/*' -or $Items[$i].name -like 'archive/*' -or $Items[$i].name -eq 'service_failures'){Get-PatchTaskLogSummary $Replies[$i]}else{Protect-PatchText $Replies[$i]}
 }
 $summary=[Collections.Generic.List[string]]::new()
 foreach($path in @('/data','/data/local','/data/local/tmp','/data/local/tmp/x2d-rs1','/data/local/tmp/x2d-rs8')){
  $summary.Add('安装目录 '+$path+'：'+$data['directory'+$path].Trim())
 }
 foreach($kind in @('stock','upgrade')){
  $result=$data[$kind+'/result'].Trim();$done=$data[$kind+'/done'].Trim()
  $summary.Add($kind+' 暂存区：result='+$result+'；done='+$done)
  $summary.Add($kind+' 安装日志（最多末尾20行）：'+"`n"+$data[$kind+'/install.log'])
 }
 $taskItem=@($Items|Where-Object {$_.name -eq 'task/task.info'})
 $taskId=if($taskItem.Count -eq 1){$taskItem[0].task_id}else{'未取得'}
 $summary.Insert(0,'固定任务编号：'+$taskId+'。以下文件来自同一任务；当前系统状态及服务历史摘录独立标注，不代表本次安装结果。')
 $insert=1
 foreach($file in @('task.info','task.result','cleanup.result','failure.result','stage','run.rc','capture.rc','restored','memory.failed','task.events','task.memory','phase-events','phase-metrics','run.log','capture.log','merge.log','render.log','probe.log','finish-restore.log','watchdog.log','finish-metrics')){
  $summary.Insert($insert++,('任务 '+$file+"：`n"+$data['task/'+$file]))
  $spec=@($Items|Where-Object {$_.name -eq ('task/'+$file)})
  if($spec.Count -eq 1 -and $spec[0].tail_limit -ge 2048){$summary.Insert($insert++,('读取范围：末尾最多 '+$spec[0].tail_limit+' 字节；更早内容可能未包含。'))}
 }
 $summary.Add('当前系统内存和服务 PID（诊断时读取）：'+"`n"+$data.current_memory)
 foreach($file in @('last-task.txt','last-failed-task.txt','last-cleanup.result')){
  $summary.Add('有界历史摘要 '+$file+'（以摘要内 job_id 关联，不自动归属当前任务）：'+"`n"+$data['archive/'+$file])
 }
 $summary.Add('服务异常摘录（历史日志，未自动绑定上述任务）：'+"`n"+$data.service_failures)
 foreach($key in @('exposure','storage')){$summary.Add('原厂属性读取 '+$key+'（读取失败不代表状态 0）：'+"`n"+$data[$key])}
 $summary.Insert(0,'任务存储目录（仅读取类型和访问状态，不创建目录）：'+"`n"+'SSD 根目录：'+$data['job_root/mnt/media_rw/ssd'].Trim()+"`n"+'MultiShot 父目录：'+$data['job_root/mnt/media_rw/ssd/multishot'].Trim()+"`n"+'SSD 挂载：'+$data.job_mount.Trim()+"`n"+'任务锁当前状态：'+$data.lock.Trim()+'；当前状态不能证明此前是否占用。')
 $workerText=$data.worker
 $script:PatchContext.stage='手动读取现场（不安装）'
 $script:PatchContext.target='固定任务控制日志与原厂状态'
 $script:PatchContext.command='固定只读诊断批次完成'
 $script:PatchContext.expected='各项诊断回复；缺失的原厂状态明确标为未取得'
 $script:PatchContext.actual='已接收全部 '+$Items.Count+' 项诊断回复；不等于各项状态均正常'
 $ui='未取得';if($data.ui -match 'rawStackUI=\d+ popup=\d+'){$ui=$Matches[0]}
 $script:PatchContext.state='任务锁='+$data.lock.Trim()+'；菜单='+$data.menu.Trim()+"`n"+$workerText+"`nUI="+$ui
 $script:PatchContext.state+="`n"+$data.native_error
 foreach($key in @('exposure','storage')){
  $state='未取得'
  if($data[$key] -match 'variant\s+(?:int32|uint32)\s+([0-9]+)\s*$'){$state=$Matches[1]}
  $script:PatchContext.state+='；'+$(if($key -eq 'exposure'){'曝光状态'}else{'存储状态'})+'='+$state
 }
 $script:PatchContext.readonly=if($data.mount.Trim() -match '(^|,)ro(,|$)'){'已读取：只读'}elseif($data.mount.Trim() -match '(^|,)rw(,|$)'){'已读取：可写'}else{'未取得'}
 $script:PatchContext.recovery='未确认。本报告只读状态和安装回执，不重新安装或恢复设置。'
 $script:PatchContext.camera_exit='stock='+$data['stock/result'].Trim()+'；upgrade='+$data['upgrade/result'].Trim()+'（两处历史回执，不能自动归属本次操作）'
 $script:PatchContext.camera_log=Protect-PatchText ($summary -join "`n") -Limit 98304
 $script:PatchContext.reason='只读现场采集完成；历史日志不证明本次安装成功或失败。'
 $script:PatchContext.host_exit='0'
 $script:PatchContext.transport='收到完整诊断回复；不等于拍摄或安装成功'
 Write-PatchDiagnosticObject 'PATCH_DIAGNOSTIC=' $script:PatchContext
}
