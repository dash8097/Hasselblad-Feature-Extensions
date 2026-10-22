param([Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-f]{8}$')][string]$Session,[Parameter(Mandatory=$true)][int]$Ack,[int]$ObservedFrames=9)
$ErrorActionPreference='Stop'
$root=(Resolve-Path "$PSScriptRoot/../../..").Path
if((Get-Location).Path -cne $root){throw 'Workspace differs'}
. "$root/x2d/outputs/4.2.0/sensor-exposure-candidates/device-relaunch-20261016-213703-16bit-r2/usb-session.ps1"
$end=[DateTime]::UtcNow.AddSeconds(55)
$last=-1
while([DateTime]::UtcNow -lt $end){
 $r=@(Read-ReviewedBatch @('cat /tmp/x2d-pixel-overclock/status.json'));$s=$r[0]|ConvertFrom-Json
 if($s.session -cne $Session -or $s.ack -ne $Ack -or !$s.busy -or !$s.stack -or $s.stage -notin @('countdown','delay','capture')){throw 'Capture state changed; no UI touch'}
 if($s.stack_frames -ne $last){$last=$s.stack_frames;Write-Output ('FRAMES='+$last)}
 if($s.stack_frames -ge $ObservedFrames){
  if($s.stage -cne 'capture'){throw 'Finish not visible'}
  $r=@(Read-ReviewedBatch @('cat /sys/class/input/event8/device/name'));if($r[0].Trim() -cne 'himax-touchscreen'){throw 'Touch identity'}
  $cs=@('3 47 0','3 57 501','3 53 930','3 54 490','3 48 10','3 58 30','1 330 1','0 0 0')|ForEach-Object {'/system/bin/sendevent /dev/input/event8 '+$_}
  try{$null=Read-ReviewedBatch $cs;Start-Sleep -Milliseconds 80}
  finally{$null=Read-ReviewedBatch @('/system/bin/sendevent /dev/input/event8 3 47 0','/system/bin/sendevent /dev/input/event8 3 57 -1','/system/bin/sendevent /dev/input/event8 1 330 0','/system/bin/sendevent /dev/input/event8 0 0 0')}
  $record=@{time=(Get-Date).ToString('o');action='finish-ui';session=$Session;ack=$Ack;observed_frames=$last;x=930;y=490}|ConvertTo-Json -Compress
  Add-Content "$root/x2d/outputs/4.2.0/raw-stack/continuous-candidate/delivery/c791c85c69dfa12e/device-ui/actions.jsonl" $record
  Write-Output $record;exit
 }
 Start-Sleep -Milliseconds 100
}
throw 'Finish observation timed out; no touch'
