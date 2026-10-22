"""十六亿准备期：取消必须等待计数同步子进程完成回滚，再释放任务锁。"""
CALL='sh /system/etc/x2d-pixel-overclock/counter-sync.sh "$s" before'
GUARDED='''# 计数同步可能等待空闲；取消时不能留下仍会写存储状态的孤儿。
counter_pid=
counter_restore_failed=0
cancel_counter_sync(){
 trap '' HUP INT TERM
 counter_cancel_pid=${counter_pid:-${!:-}}
 if [ -n "$counter_cancel_pid" ] && [ "$counter_cancel_pid" != 0 ]; then
  kill -TERM "$counter_cancel_pid" 2>/dev/null || true
  if wait "$counter_cancel_pid" 2>/dev/null; then :; else
   counter_cancel_rc=$?
   if [ "$counter_cancel_rc" = 90 ]; then counter_restore_failed=1; fi
  fi
 fi
 exit 71
}
trap cancel_counter_sync HUP INT TERM
X2D_COUNTER_WAIT_IDLE=1 sh /system/etc/x2d-pixel-overclock/counter-sync.sh "$s" before &
counter_pid=$!
if wait "$counter_pid"; then counter_pid=0; else counter_rc=$?; counter_pid=0; if [ "$counter_rc" = 90 ]; then counter_restore_failed=1; fi; exit "$counter_rc"; fi
trap - HUP INT TERM'''
def apply(text):
    assert text.count(CALL)==1
    release='rmdir /tmp/x2d-multishot-active || exit 90; fi; }'
    assert text.count(release)==1
    return text.replace(CALL,GUARDED).replace(release,
        'if [ "${counter_restore_failed:-0}" = 1 ] || [ -e "$s/counter-restore.failed" ]; then echo COUNTER_RECOVERY_REQUIRES_CHECK; exit 90; fi; '+release)
