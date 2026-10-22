# 大文件走原厂系统自带 netcat；不调用有提前完成缺陷的 DataTransfer 测试号。
function Test-PatchUsbNetwork {
 $address=@(Get-NetIPAddress -AddressFamily IPv4 -IPAddress '192.168.42.1' -ErrorAction SilentlyContinue)
 if($address.Count -ne 1 -or (Get-NetAdapter -InterfaceIndex $address[0].InterfaceIndex).Status -ne 'Up'){
  throw 'USB_NETWORK_REQUIRED: 请启用相机 USB 网络连接；不会回退到慢速小命令上传'
 }
}
function New-PatchBinaryConnection {
 $tcp=[Net.Sockets.TcpClient]::new([Net.IPEndPoint]::new([Net.IPAddress]::Parse('192.168.42.1'),0))
 try{
  $pending=$tcp.BeginConnect('192.168.42.2',30305,$null,$null)
  try{if(!$pending.AsyncWaitHandle.WaitOne(3000)){throw 'BINARY_CONNECT_TIMEOUT'};$tcp.EndConnect($pending)}finally{$pending.AsyncWaitHandle.Close()}
  $tcp.NoDelay=$true;$tcp.SendTimeout=10000
  return $tcp
 }catch{$tcp.Close();throw}
}
function Get-PatchDataSpaceCommand {
 # df 的设备名可能独占一行；按挂载点和末尾列定位 Available，不依赖行号。
 return 'v=$(df -k /data 2>/dev/null); r=$?; printf ''%s\n'' "$v" | awk -v r="$r" ''$NF=="/data"{n++;v=$(NF-2)} END{if(r==0&&n==1&&v~/^[0-9]+$/)print "DATA_FREE_KIB="v;else print "DATA_FREE_UNREADABLE"}'''
}
function Assert-PatchDataSpace([string]$Reply,[string]$Command) {
 $free=0L;$match=[regex]::Match($Reply.Trim(),'\ADATA_FREE_KIB=([0-9]+)\z')
 $valid=$match.Success -and [long]::TryParse($match.Groups[1].Value,[ref]$free)
 if(Get-Command Set-PatchContext -ErrorAction SilentlyContinue){
  Set-PatchContext -Stage '传输前读取相机数据分区空间' -Target '/data 可用空间' -Command $Command -Expected '可用空间大于32768 KiB（32 MiB）' -Actual $(if($valid){'可用空间='+$free+' KiB'}else{'空间读取结果='+$Reply.Trim()})
 }
 if(!$valid){throw 'BINARY_SPACE_UNREADABLE：未读到 /data 的有效剩余空间，不代表空间不足；大文件传输尚未开始。'}
 if($free -le 32768){throw ('BINARY_SPACE_INSUFFICIENT：/data 剩余 '+$free+' KiB，需要大于32768 KiB（32 MiB）；大文件传输尚未开始。')}
 Write-Host ('STACK_DATA_FREE_KIB='+$free)
}
function Invoke-PatchBinaryTransfer([string]$Path,[string]$ExpectedHash,[string]$Destination,[scriptblock]$Run){
 if($ExpectedHash -notmatch '^[0-9a-f]{64}$' -or $Destination -notmatch '^/data/local/tmp/(?:x2d-rs[18]/p-[0-9a-f]{8}-bin-[0-7]\.gz|x2d-bv-[0-9a-f]{8}/archive\.gz)$'){
  throw 'BINARY_ARGUMENT_RANGE'
 }
 $data=[IO.File]::ReadAllBytes($Path)
 $digest=[Security.Cryptography.SHA256]::Create()
 try{$actual=[BitConverter]::ToString($digest.ComputeHash($data)).Replace('-','').ToLowerInvariant()}finally{$digest.Dispose()}
 if($data.Length -lt 1 -or $data.Length -gt 16777216 -or $actual -cne $ExpectedHash){[Array]::Clear($data,0,$data.Length);throw 'BINARY_LOCAL_ARCHIVE_INVALID'}
 $directory='/data/local/tmp/x2d-bt-'+[Guid]::NewGuid().ToString('N').Substring(0,8)
 $tcp=$null
 $review={param([string[]]$Commands)
  foreach($c in $Commands){if(!$c -or $c -match '[^\x20-\x7e]' -or $c.Length -gt 231){throw 'BINARY_COMMAND_RANGE'}}
  $reply=& $Run $Commands
  if(@($reply).Count -ne $Commands.Count){throw 'BINARY_REPLY_COUNT'}
  return ,@($reply)
 }
 try{
  Test-PatchUsbNetwork
  $r=& $review @('test -x /system/bin/toybox && echo FILE',
   '/bin/dbus-send --system --print-reply --dest=com.hasselblad.error /error org.freedesktop.DBus.Properties.Get string:com.hasselblad.error string:active_error',
   '/bin/dbus-send --system --print-reply --dest=com.hasselblad.camera /camera org.freedesktop.DBus.Properties.Get string:com.hasselblad.camera string:exposure_status',
   '/bin/dbus-send --system --print-reply --dest=com.hasselblad.storage /storage org.freedesktop.DBus.Properties.Get string:com.hasselblad.storage string:storage_status')
  if($r[0].Trim() -cne 'FILE'){throw 'BINARY_TOYBOX_MISSING'}
  if($r[1] -match 'variant\s+boolean\s+true\s*$'){
   $native='具体原厂错误未取得。'
   try{
    $details=& $review @('/bin/dbus-send --system --print-reply --dest=com.hasselblad.error /error org.freedesktop.DBus.Properties.GetAll string:com.hasselblad.error | head -c 8192 | /system/xbin/busybox base64')
    $native=Get-PatchNativeErrorSummary $details[0]
   }catch{$native='具体原厂错误未取得（只读查询未完成）。'}
   if(Get-Command Set-PatchContext -ErrorAction SilentlyContinue){
    Set-PatchContext -Stage '传输前原厂错误检查' -Target '/error' -Expected '无活动错误，曝光和存储空闲' -Actual ('前置读取 active_error=true；'+$native)
    $script:PatchContext.recovery='本次大文件传输未开始；未解包或切换正式入口。未清除相机错误。'
   }
   throw ('BINARY_CAMERA_ACTIVE_ERROR：原厂错误标志阻止传输。'+$native+' 本次大文件传输未开始；确认原因后重新检查。')
  }
  if($r[1] -notmatch 'variant\s+boolean\s+false\s*$' -or $r[2] -notmatch 'variant\s+(?:int32|uint32)\s+[0-9]+\s*$' -or $r[3] -notmatch 'variant\s+(?:int32|uint32)\s+[0-9]+\s*$'){
   throw 'BINARY_CAMERA_STATE_UNCONFIRMED：原厂状态回复不完整，停止传输；请读取诊断。'
  }
  if($r[2] -notmatch 'variant\s+(?:int32|uint32)\s+0\s*$' -or $r[3] -notmatch 'variant\s+(?:int32|uint32)\s+0\s*$'){
   throw 'BINARY_CAMERA_NOT_IDLE：曝光或存储正在运行，请结束任务后重新检查。'
  }
  $spaceCommand=Get-PatchDataSpaceCommand
  $r=& $review @('test ! -L /data/local/tmp && test ! -e /tmp/x2d-multishot-active && echo BINARY_PARENT_IDLE',
   $spaceCommand,
   "test -r /proc/net/tcp && if grep -q ':7661 .* 0A ' /proc/net/tcp; then echo PORT_BUSY; else echo PORT_FREE; fi")
  if($r[0].Trim() -ne 'BINARY_PARENT_IDLE'){throw 'BINARY_PARENT_OR_TASK'}
  Assert-PatchDataSpace $r[1] $spaceCommand
  if($r[2].Trim() -ne 'PORT_FREE'){throw 'BINARY_PORT_BUSY'}
  $r=& $review @("d=$directory; test ! -e `$d && test ! -L `$d && mkdir -m700 `$d && echo BINARY_FRESH")
  if($r[0].Trim() -ne 'BINARY_FRESH'){throw 'BINARY_FRESH_FAILED'}
  # 单连接，仅绑定 USB 地址；已有工具接收裸字节，无接收后执行命令。最多存活 61 秒。
  $r=& $review @("d=$directory; cd `$d; nohup /bin/sh -c '/bin/toybox timeout -k 1 60 /bin/toybox netcat -l -s 192.168.42.2 -p 30305 -W 15 < /dev/null >p.bin 2>log; echo `$? >result' >/dev/null 2>&1 & echo `$! >pid")
  $ready=$false
  for($i=0;$i -lt 12;$i++){
   $r=& $review @("if grep -q '022AA8C0:7661 .* 0A ' /proc/net/tcp; then echo BINARY_LISTENING; else echo NOT_READY; fi")
   if($r[0].Trim() -eq 'BINARY_LISTENING'){$ready=$true;break}
   Start-Sleep -Milliseconds 50
  }
  if(!$ready){throw 'BINARY_LISTENER_NOT_READY'}
  $tcp=New-PatchBinaryConnection
  $timer=[Diagnostics.Stopwatch]::StartNew();$stream=$tcp.GetStream()
  for($offset=0;$offset -lt $data.Length;$offset+=65536){$stream.Write($data,$offset,[Math]::Min(65536,$data.Length-$offset))}
  $drained=$false
  for($i=0;$i -lt 100;$i++){
   $r=& $review @("d=$directory; test ! -L `$d/p.bin && wc -c < `$d/p.bin")
   $length=0L
   if(![long]::TryParse($r[0].Trim(),[ref]$length) -or $length -gt $data.Length -or $length -lt 0){throw 'BINARY_SIZE_INVALID'}
   if($length -eq $data.Length){$drained=$true;break}
   Start-Sleep -Milliseconds 50
  }
  if(!$drained){throw 'BINARY_SIZE_TIMEOUT'}
  # 收齐后才送 EOF。WinUSB 回复或 TcpClient.Write 完成均不表示文件接收完成。
  $tcp.Client.Shutdown([Net.Sockets.SocketShutdown]::Send)
  $completed=$false
  for($i=0;$i -lt 30;$i++){
   $r=& $review @("d=$directory; if test -f `$d/result && test ! -L `$d/result; then cat `$d/result; wc -c < `$d/p.bin; sha256sum `$d/p.bin; else echo RECEIVING; fi")
   if($r[0].Trim() -eq 'RECEIVING'){Start-Sleep -Milliseconds 50;continue}
   if($r[0] -notmatch ('\A\s*0\s+'+$data.Length+'\s+'+$ExpectedHash+'\s+'+[Regex]::Escape($directory+'/p.bin')+'\s*\z')){throw 'BINARY_COMPLETE_HASH_INVALID'}
   $completed=$true;break
  }
  if(!$completed){throw 'BINARY_COMPLETE_TIMEOUT'}
  $tcp.Close();$tcp=$null;$timer.Stop()
  # /data 内硬链接原子拒绝覆盖；不额外复制包，接收工具退出且哈希正确后才交付暂存。
  $parent=$Destination.Substring(0,$Destination.LastIndexOf('/'))
  $r=& $review @("test ! -L $parent && test -d $parent && echo BINARY_DESTINATION_OK")
  if($r[0].Trim() -ne 'BINARY_DESTINATION_OK'){throw 'BINARY_DESTINATION_INVALID'}
  $r=& $review @("d=$directory; f=$Destination; test ! -e /tmp/x2d-multishot-active && test ! -e `$f && test ! -L `$f && ln `$d/p.bin `$f && echo BINARY_PUBLISHED")
  if($r[0].Trim() -ne 'BINARY_PUBLISHED'){throw 'BINARY_PUBLISH_FAILED'}
  $r=& $review @("d=$directory; for n in p.bin log pid result; do test ! -L `$d/`$n || exit 1; done; rm `$d/p.bin `$d/log `$d/pid `$d/result && rmdir `$d && echo BINARY_CLEANED")
  if($r[0].Trim() -ne 'BINARY_CLEANED'){throw 'BINARY_CLEANUP_UNCONFIRMED'}
  Write-Host ('STACK_BINARY_TRANSFER_VERIFIED bytes='+$data.Length+' chunk=65536 send_to_hash_ms='+$timer.ElapsedMilliseconds)
 }catch{
  Write-Host ('STACK_BINARY_DIAGNOSTIC_DIRECTORY='+$directory)
  # 失败不解包、不激活、不盲重发；临时接收器有固定超时，证据留在独占目录。
  throw
 }finally{if($tcp){$tcp.Close()};[Array]::Clear($data,0,$data.Length)}
}
