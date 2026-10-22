# 只读检查电脑上的软件、进程和 Windows 驱动元数据；不打开相机接口。
function Test-PhocusDriverInf([string]$Text) {
 return ($Text -match '(?im)^\s*[^;\[\]\r\n]+=[^\r\n,]+,\s*USB\\VID_2756&PID_0009&MI_03\s*$' -and
         $Text -match '(?im)^\s*AddService\s*=\s*WinUSB\s*,' -and
         $Text -match '(?im)^\s*HKR\s*,[^\r\n]*DeviceInterfaceGUIDs[^\r\n]*\{CC135687-5267-4313-B0C7-C344609D6EF0\}')
}
function Get-PhocusEnvironmentSnapshot {
 $locations=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
 foreach($base in @($env:ProgramFiles,${env:ProgramFiles(x86)})) {
  if($base){[void]$locations.Add((Join-Path $base 'Hasselblad'))}
 }
 $keys=@('HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*',
         'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*',
         'HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*')
 foreach($entry in @(Get-ItemProperty $keys -ErrorAction SilentlyContinue | Where-Object {$_.DisplayName -match '^Phocus(?:\s|$)'})) {
  if($entry.InstallLocation){[void]$locations.Add($entry.InstallLocation)}
 }
 $executables=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
 foreach($location in $locations) {
  if(!(Test-Path -LiteralPath $location -PathType Container)){continue}
  $folders=@($location)+@(Get-ChildItem -LiteralPath $location -Directory -ErrorAction Stop |
                         Where-Object {$_.Name -match '^Phocus(?:\s|$)'} | Select-Object -ExpandProperty FullName)
  foreach($folder in $folders){foreach($name in @('Phocus.exe','Phocus64.exe')){[void]$executables.Add((Join-Path $folder $name))}}
 }
 foreach($hive in @('HKLM','HKCU')){foreach($name in @('Phocus.exe','Phocus64.exe')){
  $entry=Get-ItemProperty "$($hive):\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\$name" -ErrorAction SilentlyContinue
  if($entry -and $entry.'(default)'){[void]$executables.Add($entry.'(default)')}
 }}
 $versions=@()
 foreach($exe in $executables) {
  if(!(Test-Path -LiteralPath $exe -PathType Leaf)){continue}
  $info=(Get-Item -LiteralPath $exe).VersionInfo
  if($info.ProductName -eq 'Phocus' -and $info.CompanyName -match 'Hasselblad' -and
     $info.ProductVersion -match '^\d+(?:\.\d+){1,3}$'){$versions+=$info.ProductVersion}
 }
 $running=@(Get-Process -ErrorAction Stop | Where-Object {$_.ProcessName -match '^(Phocus|Phocus64|PhocusLoader|PhocusRelaunch)$'}).Count -gt 0
 $drivers=@()
 foreach($inf in @(Get-ChildItem -LiteralPath (Join-Path $env:WINDIR 'INF') -Filter 'oem*.inf' -File -ErrorAction Stop)) {
  $reader=[IO.StreamReader]::new($inf.FullName,$true)
  try{$text=$reader.ReadToEnd()}finally{$reader.Dispose()}
  if(Test-PhocusDriverInf $text){$drivers+=$inf.Name}
 }
 # 只使用实例标识在内存中过滤；不输出或保存可能包含序列号的路径。
 $bindingIssues=@(Get-CimInstance Win32_PnPEntity -ErrorAction Stop | Where-Object {
  $_.Present -eq $true -and $_.PNPDeviceID -like 'USB\VID_2756&PID_0009&MI_03\*' -and
  ($_.Service -ne 'WinUSB' -or $_.ConfigManagerErrorCode -ne 0)
 }).Count
 return [pscustomobject]@{Versions=@($versions|Sort-Object -Unique);Running=$running;
  DriverPackages=$drivers;BindingIssues=$bindingIssues;
  WinUsbAvailable=(Test-Path -LiteralPath (Join-Path $env:WINDIR 'System32\drivers\WinUSB.sys') -PathType Leaf)}
}
function Get-PhocusBlockReason($Snapshot) {
 if($Snapshot.Running){return '请先关闭 Phocus（包括 Phocus64、启动器），再重新检查或安装。'}
 if(@($Snapshot.Versions).Count -eq 0){return '未找到已安装的 Phocus 主程序和有效版本。请先安装官方 Phocus，再重新检查。'}
 if(@($Snapshot.DriverPackages).Count -eq 0 -or !$Snapshot.WinUsbAvailable){return '未找到支持 X2D 控制接口的已安装 Phocus USB 驱动。请通过官方 Phocus 安装程序安装或修复驱动。'}
 if($Snapshot.BindingIssues -gt 0){return '当前 X2D 控制接口的驱动绑定或设备状态异常。请修复官方 USB 驱动后重新检查。'}
 return ''
}
function Invoke-PhocusPreflight {
 $snapshot=Get-PhocusEnvironmentSnapshot
 Write-Output ('PHOCUS_INSTALLED_VERSION='+($snapshot.Versions -join ','))
 Write-Output ('PHOCUS_RUNNING='+$snapshot.Running+'; DRIVER_PACKAGES='+@($snapshot.DriverPackages).Count+'; WINUSB='+$snapshot.WinUsbAvailable+'; BINDING_ISSUES='+$snapshot.BindingIssues)
 $reason=Get-PhocusBlockReason $snapshot
 if($reason){throw $reason}
 Write-Output 'PHOCUS_NOT_RUNNING_AND_X2D_WINUSB_DRIVER_PACKAGE_VERIFIED'
}
