# 固定目标指纹读取：小批串行，不重试；拒绝不完整回复，保留具体失败目标。
function Get-PatchSnapshot([string[]]$Paths,[scriptblock]$Run) {
 if(!$Paths -or $Paths.Count -eq 0){throw '组件目标清单为空'}
 # 先验证所有命令，再发第一批；不在半途中接受未审查的目标。
 $commands=@($Paths|ForEach-Object {New-PatchSnapshotCommand $_})
 $snapshot=@{}
 $batchSize=8
 for($first=0;$first -lt $Paths.Count;$first+=$batchSize){
  $last=[Math]::Min($first+$batchSize,$Paths.Count)-1
  $batch=[string[]]$commands[$first..$last]
  Set-PatchContext -Stage '固件与组件状态读取' -Command ($batch -join "`n") -Target ('组件 '+($first+1)+'–'+($last+1)+'/'+$Paths.Count) -Expected '每条返回 ABSENT 或FILE 文件状态' -Actual ('前 '+$first+' 条已核对；本批等待回复') -Transport 'USB 请求中，等待本批回执'
  try{$replies=& $Run $batch}catch{
   $detail=$_.Exception
   while($detail.InnerException){$detail=$detail.InnerException}
   $code=[string]$detail.Message
   $win32=if($detail.PSObject.Properties['Win32']){[int]$detail.Win32}else{0}
   $target='本批组件 '+($first+1)+'–'+($last+1)
   $command=$batch -join "`n"
   $completed=$first
   $stage='固件与组件状态读取'
   if($code -cmatch '\AHANDSHAKE_(FIRMWARE|STATE)_(.+)\z'){
    $phase=$Matches[1]
    $stage=if($phase -ceq 'FIRMWARE'){'本批固件握手'}else{'本批运行状态握手'}
    $target=if($phase -ceq 'FIRMWARE'){'固件版本（ReadParameter 27）'}else{'运行状态（ReadParameter 28）'}
    $command=if($phase -ceq 'FIRMWARE'){'固定版本读取：ReadParameter 27'}else{'固定状态读取：ReadParameter 28'}
   }
   if($code -cmatch '\ABATCH_STEP_(\d+)_(.+)\z'){
    $local=[int]$Matches[1]
    if($local -lt $batch.Count){
     $index=$first+$local;$target=$Paths[$index];$command=$commands[$index];$completed=$index
    }
   }
   $transport=if($code -like '*SHELL_RECEIVE*'){'USB 读取中断，未取得该条完整回复'}else{'USB 组件读取失败：'+$code}
   if($win32){$transport+='；Win32='+$win32}
   $actual=if($stage -ne '固件与组件状态读取'){'前 '+$completed+' 条组件已核对；本批握手未通过，尚未发送本批文件读取命令'}else{'前 '+$completed+' 条已收到有效运输回复；本批未提交完整指纹结果'}
   Set-PatchContext -Stage $stage -Command $command -Target $target -Actual $actual -Transport $transport
   throw
  }
  if($replies.Count -ne $batch.Count){
   Set-PatchContext -Actual ('本批期望 '+$batch.Count+' 条，实际 '+$replies.Count+' 条') -Transport '收到回复，组件状态数量不完整'
   throw '组件状态回复数量不完整，未选择安装流程'
  }
  for($local=0;$local -lt $batch.Count;$local++){
   $index=$first+$local;$path=$Paths[$index];$reply=$replies[$local].Trim()
   $valid=($reply -ceq 'FILE')
   if($reply -ceq 'ABSENT'){$snapshot[$path]='ABSENT'}
   elseif($valid){$snapshot[$path]='FILE'}
   else{
    Set-PatchContext -Command $commands[$index] -Target $path -Actual $reply -Transport '收到回复，但该条文件状态无效'
    throw ('组件状态读取失败或文件类型不支持：'+$path)
   }
  }
  [Console]::WriteLine('PATCH_PROGRESS=snapshot|'+($last+1)+'|'+$Paths.Count)
 }
 Set-PatchContext -Command '固定组件文件检查完成' -Target ('全部 '+$Paths.Count+' 个组件') -Actual ('全部 '+$Paths.Count+' 条文件状态已核对') -Transport 'USB 组件回复已收到并核对'
 return $snapshot
}
