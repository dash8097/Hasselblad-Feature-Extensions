"""准备入口回收已失去监听进程的固定内存端点；不触发拍摄。"""
BODY = r'''
clear_stale_memory_endpoint(){
 ipc=/tmp/x2d-pixel-overclock/memory.sock
 if [ ! -e "$ipc" ] && [ ! -L "$ipc" ]; then return 0; fi
 # 仅回收已失去内核监听者的 socket，不改动正在运行的任务。
 test ! -e /tmp/x2d-multishot-active || return 72
 test ! -L "$ipc" && test -S "$ipc" || return 73
 test -r /proc/net/unix || return 74
 if grep -Fq " $ipc" /proc/net/unix; then return 75; fi
 if pidof render-probe >/dev/null; then return 76; fi
 rm "$ipc"
 echo MEMORY_STALE_ENDPOINT_REMOVED_BEFORE_PREPARATION
}
'''

def patch_launch(text, kind):
    if 'clear_stale_memory_endpoint' in text:
        return text
    marker = 'system_format=' if kind == 'pixel' else 'case "$resolution" in'
    assert text.count(marker) == 1
    call = 'clear_stale_memory_endpoint\n' if kind == 'pixel' else 'if [ "$resolution" = 16 ]; then clear_stale_memory_endpoint; fi\n'
    return text.replace(marker, BODY + call + marker, 1)
