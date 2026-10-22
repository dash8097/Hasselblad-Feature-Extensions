"""在恢复前保存原始失败；写日志不改变原有退出码或恢复动作。"""
import re

FAILURE_RECORD = r'''
 if [ "$result" != 0 ]; then
  failed_stage=$(cat "$s/stage" 2>/dev/null)
  case "$failed_stage" in prepare|delay|capture|merge|render|jpeg|jpeg_join|package|raw|convert|publish|restore) ;; *) failed_stage=unknown;; esac
  failed_phase=${phase:-unknown}
  case "$failed_phase" in ''|*[!A-Za-z0-9_-]*) failed_phase=unknown;; esac
  if [ "${#failed_phase}" -gt 64 ]; then failed_phase=unknown; fi
  if [ ! -L "$s/failure.result" ]; then
   (set -C; printf 'exit=%s\nstage=%s\nphase=%s\n' "$result" "$failed_stage" "$failed_phase" > "$s/failure.result") 2>/dev/null || true
  fi
 fi'''

def instrument(runner):
    if 'failed_phase=${phase:-unknown}' in runner:
        return runner
    # 多分辨率允许 cleanup 显式传入最初错误码，不能改成当前 $?。
    pattern = r'(?m)^( (?:result=\$\?|cleanup_entry_rc=\$\?; result=\$\{1:-\$cleanup_entry_rc\}); trap - EXIT HUP INT TERM; set \+e)'
    text, count = re.subn(pattern,lambda m:m[0]+FAILURE_RECORD,runner)
    if count != 1:
        raise ValueError('任务 cleanup 原始退出码入口未唯一匹配')
    return text
