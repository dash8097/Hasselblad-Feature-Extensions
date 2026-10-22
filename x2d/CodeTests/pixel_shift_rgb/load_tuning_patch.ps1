param([ValidateSet('Check','Install','ResumeUpdate','Verify','Diagnose','Uninstall')][string]$Action='Check',[string]$PackageRoot=$PSScriptRoot,[string]$PreviousBootFingerprint='')
$ErrorActionPreference='Stop'
[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false)
$script:childDiagnostic=$false
$script:PatchScreenLease=$null
$taskStage='安装包完整性检查'
try {
# 先检查交付文件，随后检查电脑环境，最后才创建 USB 运输层。
$taskRequired=@(
 'HasselbladTuningPatchInstaller.exe','load_tuning_patch.ps1','package-release.json','universal-package.txt',
 'PatchDiagnostics.ps1','PatchRouting.ps1','PatchScreenLease.ps1','PatchSnapshot.ps1','PatchUninstall.ps1','PatchUpdate.ps1','PhocusPreflight.ps1',
 'known-installations/legacy-components.json','known-installations/development-uninstall.json',
 'known-installations/released-r5-6.json','known-installations/released-r5-7.json','known-installations/released-r5-8-2.json','known-installations/released-r5-8-6.json','known-installations/released-r5-8-12.json','known-installations/released-r5-8-13.json','known-installations/released-r5-8-14.json','known-installations/released-r5-8-15.json','known-installations/released-r5-8-16.json',
 'routes/stock/load_raw_stack_backend.ps1','routes/stock/payload/bundle-manifest.json','routes/stock/payload/install-manifest.json',
 'routes/stock/payload/install-payload.tar.gz','routes/stock/payload/install-validation.json','routes/stock/payload/PatchBinaryTransfer.ps1',
 'routes/stock/payload/PatchDiagnostics.ps1','routes/stock/payload/PhocusPreflight.ps1','routes/stock/payload/ShellTransport.inc.cs','routes/stock/payload/WinUsbReadOnly.cs',
 'uninstall/manifest.json','uninstall/payload.tar.gz','uninstall/validation.json'
)
$taskMissing=@(@($taskRequired)+@('package-integrity.json')|Where-Object {![IO.File]::Exists((Join-Path $PackageRoot $_))})
if($taskMissing.Count){throw ('安装必需文件缺失，请完整解压 ZIP：'+($taskMissing -join '，'))}
$taskIntegrity=Get-Content (Join-Path $PackageRoot 'package-integrity.json') -Raw -Encoding UTF8|ConvertFrom-Json
$taskHash=[Security.Cryptography.SHA256]::Create()
try{
 foreach($name in $taskRequired){
  $entry=$taskIntegrity.PSObject.Properties[$name]
  if(!$entry -or $entry.Value -cnotmatch '^[0-9a-f]{64}$'){throw ('完整性清单缺项或指纹无效：'+$name)}
  $bytes=[IO.File]::ReadAllBytes((Join-Path $PackageRoot $name))
  $digest=([BitConverter]::ToString($taskHash.ComputeHash($bytes))).Replace('-','').ToLowerInvariant()
  if($digest -cne $entry.Value){throw ('安装必需文件损坏或版本混用，请重新解压：'+$name)}
 }
}finally{$taskHash.Dispose()}
. (Join-Path $PackageRoot 'PatchDiagnostics.ps1')
Initialize-PatchDiagnostic $Action
. (Join-Path $PackageRoot 'PatchUpdate.ps1')
if($Action -eq 'ResumeUpdate' -and !(Test-PatchBootFingerprint $PreviousBootFingerprint)){throw '更新续接状态无效，未打开 USB。'}
$taskStage='Phocus 与 USB 驱动检查'
Set-PatchContext -Stage $taskStage
. (Join-Path $PackageRoot 'PhocusPreflight.ps1')
Invoke-PhocusPreflight
. (Join-Path $PackageRoot 'PatchRouting.ps1')
. (Join-Path $PackageRoot 'PatchSnapshot.ps1')
Write-Output '安装前要求：不得在其他人的补丁包上叠加安装。可能导致严重故障，无法保证恢复。'
Write-Output (Get-PatchRestoreAdvice)
$stock=Get-Content (Join-Path $PackageRoot 'routes/stock/payload/install-manifest.json') -Raw -Encoding UTF8|ConvertFrom-Json
$upgrade=Get-Content (Join-Path $PackageRoot 'known-installations/legacy-components.json') -Raw -Encoding UTF8|ConvertFrom-Json
$removal=Get-Content (Join-Path $PackageRoot 'uninstall/manifest.json') -Raw -Encoding UTF8|ConvertFrom-Json
$taskStage='USB 运输层初始化'
Set-PatchContext -Stage $taskStage
$source=Get-Content (Join-Path $PackageRoot 'routes/stock/payload/WinUsbReadOnly.cs') -Raw -Encoding UTF8
$methods=Get-Content (Join-Path $PackageRoot 'routes/stock/payload/ShellTransport.inc.cs') -Raw -Encoding UTF8
$source=$source.Replace('namespace HasselbladDebug','namespace TuningPatch').Replace('WinUsbReadOnly','PatchUsb').Replace('uint timeout = 2000;','uint timeout = 45000;')
$methods=$methods.Replace('TemporaryUiUsb','PatchUsb').Replace('output.Add(s.Shell(commands[i],tag+(uint)i));','output.Add(s.Shell(commands[i],tag+(uint)i)); System.Threading.Thread.Sleep(50);')
Add-Type -TypeDefinition $source.Replace('        public void Dispose() {',$methods+'        public void Dispose() {')
if([TuningPatch.PatchUsb]::SelfTestShell() -ne 'SHELL_OFFLINE_PASS'){throw 'USB contract failed'}
. (Join-Path $PackageRoot 'PatchScreenLease.ps1')
$readScreen={param([string[]]$Commands) return ,([TuningPatch.PatchUsb]::RunReviewedBatch($Commands))}
if($Action -in @('Check','Install','ResumeUpdate','Verify','Uninstall')){Test-PatchNativeUsbPrompt $readScreen}
if($Action -in @('Check','Install','ResumeUpdate','Verify')){Test-PatchScreenTimeout $readScreen}
if($Action -eq 'Diagnose'){
 # 参数读取失败也不妨碍收集其他诊断，恢复与卸载不受此设置门槛阻挡。
 try{Test-PatchScreenTimeout $readScreen $false}catch{Write-Output ('息屏设置未确认：'+$_.Exception.Message)}
 Set-PatchContext -Stage '手动读取现场（不安装）' -Transport '请求中'
 Invoke-PatchCameraDiagnostics $readScreen
 exit 0
}
$paths=@($stock.required_guards.PSObject.Properties.Name)+@($stock.targets.PSObject.Properties.Value)+@($upgrade.targets.PSObject.Properties.Value)+@($removal.targets.path)
$paths=@($paths|Sort-Object -Unique)
$taskStage='固件与组件状态读取'
$snapshot=Get-PatchSnapshot $paths {param([string[]]$Commands) return ,([TuningPatch.PatchUsb]::RunReviewedBatch($Commands))}
$taskStage='安装流程识别'
Set-PatchContext -Stage $taskStage -Transport '组件回复已收到'
$previous=@(Get-ChildItem -LiteralPath (Join-Path $PackageRoot 'known-installations') -Filter 'released-*.json' | ForEach-Object {Get-Content -LiteralPath $_.FullName -Raw -Encoding UTF8 | ConvertFrom-Json})
$route=if($Action -eq 'Uninstall'){Get-PatchRemovalRoute $snapshot $removal}else{Get-PatchRoute $snapshot $stock $upgrade $previous $removal}
Set-PatchContext -Route $route
if($route -eq 'KnownProject'){
 Write-Output '已识别本项目的开发版／已知组件组合，可完整卸载；更新将先卸载并等待重启，不覆盖安装。'
 foreach($item in $removal.targets){
  $actual=$snapshot[$item.path]
  if($actual -ceq $item.original){continue}
  $labels=@($item.provenance.$actual)
  Write-Output ('PATCH_KNOWN_COMPONENT='+$item.path+' SHA256='+$actual+' SOURCES='+($labels -join ','))
 }
}
if($Action -in @('Uninstall','Install','ResumeUpdate')){
 . (Join-Path $PackageRoot 'PatchUninstall.ps1')
 $runUninstall={param([string[]]$Commands)
  foreach($command in $Commands){if(!$command -or $command -match '[^\x20-\x7e]' -or [Text.Encoding]::ASCII.GetByteCount($command) -gt 231){throw '卸载命令超过 USB 长度限制'}}
  Set-PatchCommandContext $Commands
  $replies=[TuningPatch.PatchUsb]::RunReviewedBatch($Commands)
  Set-PatchReplyContext $replies
  return ,$replies
 }
 . (Join-Path $PackageRoot 'PatchScreenLease.ps1')
 if($Action -eq 'Uninstall' -or $route -ne 'CurrentStock'){
  Set-PatchContext -Stage '保存息屏设置并临时禁用自动息屏' -Route $route
  Start-PatchScreenLease $runUninstall
 }
 if($Action -eq 'Uninstall'){
  if($route -eq 'Stock'){Invoke-PatchRetainedCleanup $PackageRoot $runUninstall}
  Invoke-PatchUninstall $PackageRoot $route $snapshot $runUninstall
  exit 0
 }
}
$runner={param($kind,$step)
 if($kind -eq 'stock' -and $step -eq 'Check' -and $route -eq 'Stock' -and $Action -in @('Install','ResumeUpdate')){
  Invoke-PatchRetainedCleanup $PackageRoot $runUninstall
 }
 $routeRoot=Join-Path $PackageRoot ('routes/'+$kind)
 Set-PatchContext -Stage $step -Route $kind
 & "$env:WINDIR\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File (Join-Path $routeRoot 'load_raw_stack_backend.ps1') -Action $step -PackageRoot (Join-Path $routeRoot 'payload') | ForEach-Object {
  if($_ -like 'PATCH_DIAGNOSTIC=*'){$script:childDiagnostic=$true}
  Write-Output $_
 }
 if($LASTEXITCODE -ne 0){throw ('安装步骤未完成：'+$step+'。请查看上方具体原因。')}
}
if($Action -in @('Install','ResumeUpdate')){
 $readBoot={
  Set-PatchContext -Stage '更新重启状态核对' -Command 'sha256sum /proc/sys/kernel/random/boot_id' -Target '本次开机状态' -Transport '请求中'
  Get-PatchBootFingerprint {param([string[]]$Commands) return ,([TuningPatch.PatchUsb]::RunReviewedBatch($Commands))}
 }
 $remove={Invoke-PatchUninstall $PackageRoot $route $snapshot $runUninstall}
 Invoke-PatchUpdate $Action $route $runner $readBoot $remove $PreviousBootFingerprint
}else{Invoke-PatchFlow $Action $route $runner}
} catch {
 if(!$script:childDiagnostic){
  if(Get-Command Write-PatchFailure -ErrorAction SilentlyContinue){Write-PatchFailure $_}
  else{
   # 包损坏时不导入未验证的诊断模块；由客户端显示此最小首错。
   $report=@{protocol=1;action=$Action;stage=$taskStage;reason=[string]$_.Exception.Message;host_exit='1';actual=[string]$_.Exception.GetType().Name;transport='未打开 USB';recovery='未打开 USB，未进入安装事务'}
   Write-Output ('PATCH_DIAGNOSTIC='+[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes(($report|ConvertTo-Json -Compress))))
  }
 }
 Write-Error $_ -ErrorAction Continue
 exit 1
} finally {
 if($script:PatchScreenLease){
  try{Stop-PatchScreenLease $runUninstall}catch{
   Set-PatchContext -Stage '恢复原自动息屏设置' -Target '/system screen_idle_timeout'
   $script:PatchContext.recovery=$_.Exception.Message
   Write-PatchFailure $_
   Write-Error $_ -ErrorAction Continue
   exit 1
  }
 }
}
