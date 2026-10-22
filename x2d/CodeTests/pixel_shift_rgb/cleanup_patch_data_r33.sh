#!/system/bin/sh
# 后台执行、结果落盘，减少引用扫描中的子进程。
set -eu
export PATH=/system/bin:/system/xbin:/sbin
mode=$1
self=$2
case "$mode" in before|installed|uninstalled) ;; *) exit 2;; esac
base=/data/local/tmp
case "$self" in "$base"/x2d-tidy-????????) ;; *) exit 2;; esac
suffix=${self##*/x2d-tidy-}
case "$suffix" in *[!a-f0-9]*) exit 2;; esac
for p in /data /data/local "$base" "$self"; do test ! -L "$p"; test -d "$p"; done
lock=/tmp/x2d-data-cleanup-lock
owned=0
finish(){ rc=$?; trap - EXIT HUP INT TERM; set +e; if test "$owned" = 1; then rmdir "$lock" || rc=91; fi; printf '%s\n' "$rc" > "$self/result.next"; mv "$self/result.next" "$self/result"; exit "$rc"; }
trap finish EXIT
trap 'exit 143' HUP INT TERM
progress(){ printf '%s\n' "$1" > "$self/progress.next"; mv "$self/progress.next" "$self/progress"; }
progress PHASE=starting
test ! -e /tmp/x2d-multishot-active || { echo DATA_CLEANUP_DEFERRED_CAPTURE; exit 0; }
mkdir "$lock" || { echo DATA_CLEANUP_DEFERRED_OTHER_CLEANUP; exit 0; }
owned=1
refs=$self/refs
: > "$refs"
record(){ awk -v b="$base/" 'index($0,b)==1{v=substr($0,length(b)+1);sub(/\/.*/,"",v);if(v~/^x2d-[A-Za-z0-9_.-]+$/)print b v}' >> "$refs"; }
progress PHASE=references
# 一次枚举链接，一次读取映射，一次读取参数；不再逐个fd启动readlink/awk。
ls -l /proc/[0-9]*/cwd /proc/[0-9]*/exe /proc/[0-9]*/fd/* 2>/dev/null | awk -F ' -> ' 'NF>1{print $NF}' | record
awk '{print $NF}' /proc/[0-9]*/maps 2>/dev/null | record
cat /proc/[0-9]*/cmdline 2>/dev/null | tr '\000' '\n' | record
awk '{print $2}' /proc/mounts | record
referenced=' '
while IFS= read -r d; do referenced="$referenced$d "; done < "$refs"
used(){ case "$referenced" in *" $1 "*) return 0;; *) return 1;; esac; }
# 当前失败安装保留；最近一份归档失败或清理失败保留。
progress PHASE=diagnostics
latest=''; stamp=0
for d in "$base"/x2d-rs1.saved-* "$base"/x2d-un-* "$base"/x2d-tidy-*; do
 test "$d" != "$self" || continue
 test ! -L "$d" && test -d "$d" || continue
 marker=''
 for f in exit result; do
  test ! -L "$d/$f" && test -f "$d/$f" || continue
  marker=$d/$f; break
 done
 if test -n "$marker"; then
  result=$(cat "$marker")
  test "$result" != 0 || continue
 elif test ! -L "$d/started" && test -f "$d/started"; then marker=$d/started
 else continue; fi
 t=$(stat -c %Y "$marker")
 case "$t" in ''|*[!0-9]*) exit 3;; esac
 if test "$t" -gt "$stamp"; then latest=$d; stamp=$t; fi
done
before=$(df -k /data | awk '$NF=="/data"{print $(NF-2)}')
total=0
for d in "$base"/x2d-*; do test ! -L "$d" && test -d "$d" && total=$((total+1)) || true; done
count=0; kept=0; scanned=0
for d in "$base"/x2d-*; do
 test ! -L "$d" && test -d "$d" || continue
 scanned=$((scanned+1)); progress "PHASE=directories scanned=$scanned total=$total removed=$count"
 name=${d##*/}
 case "$name" in *[!A-Za-z0-9_.-]*) continue;; esac
 test "$d" != "$self" || continue
 test "$d" != "$latest" || { kept=$((kept+1)); continue; }
 if used "$d"; then kept=$((kept+1)); continue; fi
 if test "$name" = x2d-rs1; then
  test "$mode" != before || continue
  test ! -L "$d/result" && test -f "$d/result" || continue
  test "$(cat "$d/result")" = 0 || continue
  test ! -L "$d/done" && test -f "$d/done" || continue
  case "$(cat "$d/done")" in RAW_STACK_BACKEND_INSTALLED_NO_CAPTURE|RAW_STACK_STOCK_FILES_INSTALLED_MANUAL_REBOOT_REQUIRED) ;; *) continue;; esac
 fi
 case "$name" in x2d-un-????????)
  id=${name#x2d-un-}
  if test -e "/system/x2d-un-retained-$id" || test -L "/system/x2d-un-retained-$id"; then kept=$((kept+1)); continue; fi;;
 esac
 rm -rf "$d"
 test ! -e "$d" && test ! -L "$d"
 count=$((count+1))
done
after=$(df -k /data | awk '$NF=="/data"{print $(NF-2)}')
progress "PHASE=complete scanned=$scanned total=$total removed=$count"
printf 'DATA_CLEANUP_COMPLETE removed=%s retained=%s free_before_kib=%s free_after_kib=%s\n' "$count" "$kept" "$before" "$after"
