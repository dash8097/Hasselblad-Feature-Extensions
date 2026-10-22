cleanup() {
 cleanup_entry_rc=$?; result=${1:-$cleanup_entry_rc}; trap - EXIT HUP INT TERM; set +e
 # 原厂曝光和后台显影工人不再读取本任务后，才可恢复取景与交还模式。
 if [ -n "$exposure_rpc" ]; then
  wait "$exposure_rpc"; exposure_wait_rc=$?; exposure_rpc=
  if [ "$result" = 0 ] && [ "$exposure_wait_rc" != 0 ]; then result=$exposure_wait_rc; fi
 fi
 if [ -n "$render_pipeline_pid" ]; then
  wait "$render_pipeline_pid"; render_wait_rc=$?; render_pipeline_pid=
  if [ "$result" = 0 ] && [ "$render_wait_rc" != 0 ]; then result=$render_wait_rc; fi
 fi
 restored_rc=0
 if [ ! -f "$s/restored" ]; then
  (set -e; restore) >> "$s/finish-restore.log" 2>&1; restored_rc=$?
 fi
 printf 'SIXTEEN_RESTORE_RESULT rc=%s\n' "$restored_rc" >> "$s/finish-restore.log"
 if [ "$restored_rc" = 0 ]; then
  if [ "$live_before" = 1 ]; then live true || restored_rc=90; fi
  if [ "$restored_rc" = 0 ]; then
   : > "$s/lock.releasable"
   rmdir /tmp/x2d-multishot-active >> "$s/finish-restore.log" 2>&1 || restored_rc=90
  fi
 fi
 [ "$restored_rc" = 0 ] || { printf '90\n' > "$s/run.rc"; exit 90; }
 (set -C; printf '%s\n' "$result" > "$s/run.rc")
 exit "$result"
}
