param([ValidateSet('Snapshot','Tap','Swipe','Status')][string]$Action,[ValidateRange(0,1023)][int]$X,[ValidateRange(0,767)][int]$Y,[ValidateRange(0,1023)][int]$ToX,[ValidateRange(0,767)][int]$ToY,[ValidatePattern('^[0-9]{3,4}$')][string]$Id,[switch]$DiagnosticUi,[switch]$Timing)
$ErrorActionPreference='Stop'
$root=(Resolve-Path "$PSScriptRoot/../../..").Path
if((Get-Location).Path -cne $root){throw 'Workspace differs'}
. "$root/x2d/outputs/4.2.0/sensor-exposure-candidates/device-relaunch-20261016-213703-16bit-r2/usb-session.ps1"
if($DiagnosticUi){
 # 仅本 UI 工具：错误页返回及截图不依赖拍摄引擎 running=true。
 # 固件和协议校验保留；不提供快门、DBus 设置或任意命令入口。
 $src=(Get-Content "$root/native/WinUsbReadOnly.cs" -Raw).Replace('namespace HasselbladDebug','namespace UiDiagnostic')
 $inc=(Get-Content "$root/x2d/CodeTests/temporary_wifi_button/ShellTransport.inc.cs" -Raw).Replace('TemporaryUiUsb','WinUsbReadOnly').Replace('if(rs!="i1")throw new UsbFailure("CAMERA_NOT_RUNNING");','if(rs!="i1"&&rs!="i0")throw new UsbFailure("CAMERA_STATE_UNKNOWN");')
 Add-Type -TypeDefinition $src.Replace('        public void Dispose() {',$inc+'        public void Dispose() {')
 function Read-ReviewedBatch([string[]]$Commands){[UiDiagnostic.WinUsbReadOnly]::RunReviewedBatch($Commands)}
}
$out=Join-Path $root 'x2d/outputs/4.2.0/raw-stack/continuous-candidate/delivery/c791c85c69dfa12e/device-ui'
$remote='/data/local/tmp/x2d-ui-test-c791c85c'
function Run([string[]]$cs){return ,@(Read-ReviewedBatch $cs)}
if($Action -eq 'Status'){
 $r=Run @('/system/xbin/busybox base64 /tmp/x2d-pixel-overclock/status.json')
 $s=[Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($r[0]))|ConvertFrom-Json
 $s.PSObject.Properties.Remove('album')
 $record=@{time=(Get-Date).ToString('o');id=$Id;status=$s}|ConvertTo-Json -Depth 5 -Compress
 Add-Content "$out/status.jsonl" $record
 Write-Output $record;exit
}
if($Action -in @('Tap','Swipe')){
 $r=Run @('cat /sys/class/input/event8/device/name')
 if($r[0].Trim() -cne 'himax-touchscreen'){throw 'Input identity'}
 $cs=@('3 47 0','3 57 501',('3 53 '+$X),('3 54 '+$Y),'3 48 10','3 58 30','1 330 1','0 0 0')|ForEach-Object {'/system/bin/sendevent /dev/input/event8 '+$_}
 try{$null=Run $cs;Start-Sleep -Milliseconds 80
  if($Action -eq 'Swipe'){$moves=@();for($i=1;$i -le 12;$i++){$px=[int]($X+($ToX-$X)*$i/12);$py=[int]($Y+($ToY-$Y)*$i/12);$moves+=('/system/bin/sendevent /dev/input/event8 3 53 '+$px);$moves+=('/system/bin/sendevent /dev/input/event8 3 54 '+$py);$moves+='/system/bin/sendevent /dev/input/event8 0 0 0'};$null=Run $moves}
 }
 finally{$null=Run @('/system/bin/sendevent /dev/input/event8 3 47 0','/system/bin/sendevent /dev/input/event8 3 57 -1','/system/bin/sendevent /dev/input/event8 1 330 0','/system/bin/sendevent /dev/input/event8 0 0 0')}
 Add-Content "$out/actions.jsonl" (@{time=(Get-Date).ToString('o');action=$Action;x=$X;y=$Y;to_x=$ToX;to_y=$ToY}|ConvertTo-Json -Compress)
 if($Timing){
  for($sample=0;$sample -lt 8;$sample++){
   $timingReport=Run @('cat /tmp/x2d-pixel-overclock/ui.txt')
   Add-Content "$out/timing.jsonl" (@{time=(Get-Date).ToString('o');id=$Id;action=$Action;x=$X;y=$Y;sample=$sample;report=$timingReport[0]}|ConvertTo-Json -Compress)
   Start-Sleep -Milliseconds 125
  }
 }else{Start-Sleep -Milliseconds 900}
 Write-Output "UI_TOUCH_DELIVERED $X,$Y";exit
}
if(!$Id){throw 'Screenshot id required'}
$name='screen-'+$Id
if(Test-Path "$out/$name.png"){throw 'Local screenshot exists'}
$r=Run @("test ! -e $remote/$name.raw && test ! -L $remote/$name.raw && echo FRESH")
if($r[0].Trim() -cne 'FRESH'){throw 'Screenshot exists'}
$r=Run @(('/system/bin/odin-output -o "Back/EVF" -s '+$remote+'/'+$name+'.raw 2>&1'),"cd $remote && /system/xbin/busybox gzip -c $name.raw > $name.gz && wc -c < $name.gz && sha256sum $name.gz")
if($r[0] -notmatch 'screenshot_done' -or $r[0] -notmatch '1024x768'){throw 'Screen geometry changed'}
if($r[1] -notmatch '\A\s*(\d+)\s+([0-9a-f]{64})\s'){throw 'Screen metadata'}
$length=[int]$Matches[1];$hash=$Matches[2]
if($length -lt 1 -or $length -gt 4194304){throw 'Screen size'}
$r=Run @("if grep -q ':7661 .* 0A ' /proc/net/tcp; then echo BUSY; else echo FREE; fi")
if($r[0].Trim() -cne 'FREE'){throw 'Transfer port busy'}
$null=Run @("nohup /bin/toybox timeout -k 1 15 /bin/toybox netcat -l -s 192.168.42.2 -p 30305 <$remote/$name.gz >/dev/null 2>&1 &")
$client=[Net.Sockets.TcpClient]::new([Net.IPEndPoint]::new([Net.IPAddress]::Parse('192.168.42.1'),0))
try{
 $pending=$client.BeginConnect('192.168.42.2',30305,$null,$null)
 try{if(!$pending.AsyncWaitHandle.WaitOne(3000)){throw 'Screen connection timeout'};$client.EndConnect($pending)}finally{$pending.AsyncWaitHandle.Close()}
 $client.ReceiveTimeout=10000;$stream=$client.GetStream();$bytes=[byte[]]::new($length);$offset=0
 while($offset -lt $length){$n=$stream.Read($bytes,$offset,$length-$offset);if($n -le 0){throw 'Truncated screen'};$offset+=$n}
 [IO.File]::WriteAllBytes("$out/$name.gz",$bytes)
}finally{$client.Dispose()}
if((Get-FileHash "$out/$name.gz").Hash.ToLowerInvariant() -cne $hash){throw 'Screen digest'}
Add-Type -AssemblyName System.Drawing
$f=[IO.File]::OpenRead("$out/$name.gz");$gz=[IO.Compression.GZipStream]::new($f,[IO.Compression.CompressionMode]::Decompress);$ms=[IO.MemoryStream]::new()
try{$gz.CopyTo($ms);$raw=$ms.ToArray()}finally{$gz.Dispose();$f.Dispose();$ms.Dispose()}
if($raw.Length -ne 3145728){throw 'Raw screen size'}
$bitmap=[Drawing.Bitmap]::new(1024,768,[Drawing.Imaging.PixelFormat]::Format32bppRgb)
try{$locked=$bitmap.LockBits([Drawing.Rectangle]::new(0,0,1024,768),[Drawing.Imaging.ImageLockMode]::WriteOnly,[Drawing.Imaging.PixelFormat]::Format32bppRgb);[Runtime.InteropServices.Marshal]::Copy($raw,0,$locked.Scan0,$raw.Length);$bitmap.UnlockBits($locked);$bitmap.Save("$out/$name.png",[Drawing.Imaging.ImageFormat]::Png)}finally{$bitmap.Dispose()}
Add-Content "$out/actions.jsonl" (@{time=(Get-Date).ToString('o');action='screenshot';id=$Id;transport_sha256=$hash}|ConvertTo-Json -Compress)
$null=Run @("rm -f $remote/$name.raw $remote/$name.gz")
Write-Output "$out/$name.png"


