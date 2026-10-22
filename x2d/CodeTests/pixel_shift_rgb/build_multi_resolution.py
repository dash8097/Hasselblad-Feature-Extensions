"""隔离组装单张四分之一和十六张候选；不连接相机、不触发拍摄。"""
from pathlib import Path
import hashlib,json,os,subprocess,tarfile,shutil,sys
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[2]
BASE=ROOT/'x2d/outputs/4.2.0/pixel-shift-rgb'
OUT=BASE/'multi-resolution-r1';OUT.mkdir(parents=True,exist_ok=True)
FROZEN=ROOT/'x2d/outputs/4.2.0/raw-stack/universal-installer-r5-8-25/windows-installer/routes/stock/payload'
ZIG=ROOT/'.research-cache/x1d-1.25.0/toolchain/zig-windows-x86_64-0.13.0/zig.exe'
BIN='/system/bin/x2d-multi-resolution';ETC='/system/etc/x2d-multi-resolution'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def edit(s,a,b):
 assert s.count(a)==1,(a,s.count(a));return s.replace(a,b)
def save(name,s):
 (OUT/name).write_text(s,encoding='utf-8',newline='\n')
 if name.endswith('.sh'):subprocess.run(['C:/Program Files/Git/bin/bash.exe','-n',OUT/name],check=True)
def main():
 assert Path.cwd().resolve()==ROOT
 manifest=json.loads((FROZEN/'install-manifest.json').read_text(encoding='utf-8'))
 with tarfile.open(FROZEN/'install-payload.tar.gz') as archive:
  raw=archive.extractfile('p05-run.sh').read()
 assert hashlib.sha256(raw).hexdigest()==manifest['files']['p05-run.sh']
 baseline=raw.decode('utf-8').replace('\r\n','\n')
 # 注释不参与执行；替换历史乱码注释，保留经验证的命令和恢复流程。
 baseline='\n'.join(line for line in baseline.splitlines() if not line.lstrip().startswith('#'))+'\n'
 env=dict(os.environ,TEMP=str(OUT),TMP=str(OUT),ZIG_LOCAL_CACHE_DIR=str(OUT/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(OUT/'zig-global'))
 common=[str(ZIG),'cc','-O2','-Wall','-Wextra','-Werror','-Wno-unused-function']
 for target,name,static in [('aarch64-linux-musl','quarter-native-raw',True),('x86_64-windows-gnu','quarter-native-raw.exe',False)]:
  subprocess.run([*common,'-target',target,*(['-static','-s'] if static else []),str(HERE/'quarter_native_raw.c'),str(HERE/'quarter_single_core.c'),'-o',str(OUT/name)],env=env,check=True)
 subprocess.run([sys.executable,'-B',str(HERE/'build_factory_observer.py'),'native-domain-repeat-candidate',
  '--native-preview','--six-phase-roi','--runtime-job','--inprocess-route','--capture-preview','--packed-dng',
  '--memory-bands','--raw-stack','--quarter-single','--quarter-memory','--full-memory','--jpeg-single-pass','--output-job','quarter-memory-render-r1'],check=True,env=env)
 rendered=BASE/'quarter-memory-render-r1';rendered_manifest=json.loads((rendered/'build-manifest.json').read_text(encoding='utf-8'))
 (OUT/'render25').write_bytes((rendered/rendered_manifest['binary']).read_bytes())
 native=['raw_stack_capture.c','raw_stack_core.c','raw_stack_parallel.c','raw_stack_async.c','raw_stack_file.c',
  'capture_memory_native.c','factory_private_frame.c','capture_header_export.c','capture_header_native.c',
  'capture_metadata_native.c','factory_capture_identity.c','factory_private_frame_abi.S','quarter_memory_file.c']
 objects=[]
 for name in native:
  obj=OUT/(Path(name).stem+'-quarter.o')
  subprocess.run([*common,'-target','aarch64-linux-gnu','-fPIC','-fno-stack-protector','-mno-outline-atomics',
   '-DX2D_QUARTER_MEMORY=1','-D__errno_location=__errno','-I'+str(BASE),'-c',str(HERE/name),'-o',str(obj)],env=env,check=True);objects.append(str(obj))
 backend=ROOT/'x2d/outputs/4.2.0/raw-stack/backend-r1'
 subprocess.run([str(ZIG),'ld.lld','-m','aarch64elf','-shared','--no-undefined','--allow-shlib-undefined','-z','noexecstack',
  '--strip-all','-soname','libquarter_capture.so',*objects,str(backend/'libc.so'),str(backend/'libdl.so'),'-o',str(OUT/'libquarter_capture.so')],env=env,check=True)
 subprocess.run([*common,'-target','aarch64-linux-musl','-static','-s','-DRAW_STACK_QUARTER_FINAL=1',str(HERE/'raw_stack_finalize.c'),str(HERE/'raw_stack_file.c'),'-o',str(OUT/'finalize25')],env=env,check=True)
 subprocess.run([*common,'-target','aarch64-linux-musl','-static','-s','-DPO_RAW_STACK=1','-DPO_FULL_PREPARE=1','-DPO_MULTI_RESOLUTION=1',str(HERE/'pixel_overclock_worker.c'),'-o',str(OUT/'worker')],env=env,check=True)
 # 快门桥必须接受同一PO4协议；沿用已验证的原厂指纹和AF基础库。
 support=BASE/'raw-stack-shutter-r1';bridge=OUT/'bridge-source';bridge.mkdir(exist_ok=True)
 for n in ('pixel_shutter_bridge.c','pixel_overclock_core.h','pixel_shutter_policy.h','pixel_shutter_readiness.h','pixel_shutter_intent.h','pixel_render_hook.inc','pixel_render_route.h'):
  shutil.copyfile(HERE/n,bridge/n)
 for n in ('shutter_fingerprint.h','render_fingerprint.h','libc.so','libdl.so'):
  shutil.copyfile(support/n,bridge/n)
 with tarfile.open(FROZEN/'install-payload.tar.gz') as archive:data=archive.extractfile('p09-libx2d_af_base0.so').read()
 assert hashlib.sha256(data).hexdigest()==manifest['files']['p09-libx2d_af_base0.so']
 (bridge/'libx2d_af_base0.so').write_bytes(data)
 subprocess.run([str(ZIG),'cc','-target','aarch64-linux-gnu','-O2','-g0','-Wall','-Wextra','-Werror','-fPIC','-shared','-nostdlib','-fno-stack-protector','-I'+str(bridge),
  '-DPO_RENDER_ROUTE=1','-DPO_RAW_STACK=1','-DPO_MULTI_RESOLUTION=1','-Wl,--strip-all','-Wl,--no-undefined','-Wl,-z,noexecstack','-Wl,-soname,libx2d_af_probe.so',str(bridge/'pixel_shutter_bridge.c'),'-Wl,--no-as-needed',
  str(bridge/'libx2d_af_base0.so'),str(bridge/'libc.so'),str(bridge/'libdl.so'),'-o',str(OUT/'libx2d_af_probe.so')],check=True,env=env)
 for n in ('PixelOverclockController.qml','X2dMultishotSettings.qml','PixelOverclockPage.qml','X2dPopoverDriveMode.qml','QuarterCaptureKeyGuard.qml'):(OUT/n).write_bytes((HERE/n).read_bytes())
 # 开发机 Host 另有已安装的准备文字上移事务，沿用其精确基线，避免覆盖现有位置。
 hint=BASE/'preparation-hint-position-r1'
 hint_manifest=json.loads((hint/'deployment.json').read_text(encoding='utf-8'))
 host_bytes=(hint/'payload/p22-PixelOverclockHost.qml').read_bytes()
 assert hashlib.sha256(host_bytes).hexdigest()==hint_manifest['new']['p22-PixelOverclockHost.qml']
 host=host_bytes.decode('utf-8')
 host=edit(host,'root.selectedExtension=controller.stackMode ? "stack" : "multishot";root.open()','root.selectedExtension=controller.stackMode ? "stack" : "multishot";if(controller.stackMode || controller.pixelResolution !== 25) root.open()')
 host=edit(host,'        function onBusyChanged()', '        function onStageChanged(){if(!controller.stackMode && controller.pixelResolution === 25 && controller.stage === "failed") root.open()}\n        function onBusyChanged()')
 host=edit(host,'    PixelOverclockController {', '''    QuarterCaptureKeyGuard {
        parent: root.find(root.searchRoot,"ControlScreen_root") || root.searchRoot
        anchors.fill: parent
        activeCapture: controller.busy && !controller.stackMode && controller.pixelResolution === 25 && controller.stage !== "failed"
        armedCapture: controller.applied && !controller.stackMode && controller.pixelResolution === 25 && !root.showing && !root.preparationPage && !(root.popup && root.popup.active)
    }
    PixelOverclockController {''')
 save('PixelOverclockHost.qml',host)
 quarter=json.loads((BASE/'quarter-single-r1/build-manifest.json').read_text(encoding='utf-8'))
 sixteen=json.loads((BASE/'sixteen-disk-r1/build-manifest.json').read_text(encoding='utf-8'))
 for directory,m,names in [('quarter-single-r1',quarter,('pack25',)),('sixteen-disk-r1',sixteen,('merge16','render16','jpeg-grid','pack16','libx2d_private_capture.so','render-pipeline.sh'))]:
  for n in names:
   p=BASE/directory/n;assert sha(p)==m['files'][n];(OUT/n).write_bytes(p.read_bytes())
 # 单帧复用既有内存拦截器的最大值首帧路径：首次输入直接复制，没有求极值的额外一轮。
 q=baseline
 q=edit(q,'m=/system/bin/x2d-raw-stack/libx2d_raw_stack.so','m='+BIN+'/libquarter_capture.so')
 q=edit(q,'module_hash='+manifest['files']['p00-libx2d_raw_stack.so'],'module_hash='+sha(OUT/'libquarter_capture.so'))
 # Android sh 的函数子壳不能继承调用方的收尾trap；已恢复则不再重启存储。
 q=edit(q,'restore() (\n set -eu','restore() (\n trap - EXIT HUP INT TERM\n set -eu')
 q=edit(q,'live_before=\nexposure_rpc=\n','live_before=\nexposure_rpc=\nrender_pid=\n')
 q=edit(q,' (set -e; restore); restored_rc=$?', ''' if [ -n "$render_pid" ];then
  printf 'ABORT\\n' > "$s/quarter.render.abort"
  wait "$render_pid" || true;render_pid=
 fi
 (set -e; restore); restored_rc=$?''')
 # 非本任务曝光不终止；记录等待并在其自然结束后恢复，避免留下空锁。
 q=edit(q,'test -f "$s/capture.invoked" || { echo RESTORE_DEFERRED_NOT_OWNED_EXPOSURE; return 1; }\n  camera stop_session >/dev/null 2>&1 || true', '''if [ -f "$s/capture.invoked" ];then
   camera stop_session >/dev/null 2>&1 || true
  else echo QUARTER_WAIT_NONOWNED_EXPOSURE_NO_STOP
  fi''')
 q=edit(q,' if [ ! -f "$s/capture.invoked" ]; then\n  stop_owned_storage', ''' if [ ! -f "$s/capture.invoked" ]; then
  foreign_state=$(prop camera exposure_status);foreign_wait=0
  case "$foreign_state" in ''|0) ;; *)
   echo QUARTER_WAIT_NONOWNED_EXPOSURE_BEFORE_STORAGE_RESTORE
   while [ "$foreign_state" != 0 ];do
    foreign_wait=$((foreign_wait+1));test "$foreign_wait" -le 30 || return 1
    sleep 1;foreign_state=$(prop camera exposure_status)
   done;;
  esac
  stop_owned_storage''')
 q=edit(q,' (set -e; restore); restored_rc=$?', ''' if [ -f "$s/restored" ];then
  restored_rc=0
 else
  (set -e; restore) >> "$s/finish-restore.log" 2>&1; restored_rc=$?
 fi
 printf "QUARTER_RESTORE_RESULT rc=%s\\n" "$restored_rc" >> "$s/finish-restore.log"''')
 q=edit(q,'  rmdir /tmp/x2d-multishot-active || restored_rc=90', '''  if [ "$restored_rc" = 0 ];then
   : > "$s/lock.releasable"
   rmdir /tmp/x2d-multishot-active || restored_rc=90
  fi''')
 q=edit(q,' [ "$restored_rc" = 0 ] || exit 90',' [ "$restored_rc" = 0 ] || { printf "90\\n" > "$s/run.rc"; exit 90; }')
 q=edit(q,'stage restore\nrestore\n','stage restore\nrestore\ntrap cleanup EXIT\ntrap \'exit 71\' HUP INT TERM\n')
 # 单张只查当前空闲，不做堆栈的五轮稳定等待；原厂自行验证曝光参数。
 a=q.index('stable=0; i=0\n');b=q.index('stage prepare\n',a)
 q=q[:a]+'''[ "$(prop camera exposure_status)" = 0 ]
[ "$(prop error active_error)" = false ]
[ "$(prop storage storage_status)" = 0 ]
[ "$(prop storage capture_storage_mode)" = 1 ] || exit 19
'''+q[b:]
 q=edit(q,'[ "$(prop camera exposure_status)" = 0 ]; [ "$(prop storage storage_status)" = 0 ]', '[ "$(prop camera exposure_status)" = 0 ] || { echo QUARTER_BEFORE_REQUEST_EXPOSURE_BUSY; exit 26; }\n [ "$(prop storage storage_status)" = 0 ] || { echo QUARTER_BEFORE_REQUEST_STORAGE_BUSY; exit 26; }')
 q=q.replace('[0-6]:[0-2]:[146]:[0-4])','[0-6]:[0-3]:[146]:[0-4]|[0-6]:[0-3]:16:[0-4])')
 q=edit(q,'while [ "$(clock)" -lt "$deadline" ] && [ ! -f "$s/finish.request" ]; do','while [ "$count" -lt 1 ]; do')
 q=edit(q,'wait_token raw.ready RAW_STACK_R1',f'''wait_token quarter.memory.ready QUARTER_MEMORY_R1
(set +e; {BIN}/render25 --quarter-single "$s" > "$s/probe.log" 2>&1
 atomic render.rc "$?") &
render_pid=$!
wait_token raw.ready RAW_STACK_R1''')
 assert q.count('  alive; capture_result; poll_hook')==2
 q=q.replace('  alive; capture_result; poll_hook','''  alive; capture_result; poll_hook
  if [ -n "$render_pid" ] && [ -f "$s/render.rc" ];then test "$(cat "$s/render.rc")" = 0;fi''')
 q=edit(q,'/system/bin/x2d-pixel-overclock/render-probe --raw-stack "$s" > "$s/probe.log" 2>&1', '''atomic quarter.render.allow QUARTER_ALLOW_R1
wait "$render_pid";render_pid=
test "$(cat "$s/render.rc")" = 0''')
 q=edit(q,'/system/bin/x2d-raw-stack/finalize "$s"',BIN+'/finalize25 "$s"')
 a=q.index('/system/bin/x2d-raw-stack/stack-pack ');b=q.index('stage publish',a)
 q=q[:a]+f'''test "$(cat "$s/frame.done")" = 'RS1 1'
{BIN}/pack25 "$s/render.jpg" "$s/preview.jpg" "$s/album.jpg" --direct-staged-output --capture-raw "$s/merged.3FR"
format=0
printf '0\\n' > "$s/format-source"
''' + q[b:]
 q=edit(q,'for name in rawpb.bin preview.jpg album.jpg capture.rc frame.request frame.armed frame.done finish.request; do','for name in render.jpg rawpb.bin source.header raw.memory preview.jpg album.jpg capture.rc frame.request frame.armed frame.done finish.request; do')
 publish=(HERE/'raw_stack_publish.sh').read_text(encoding='utf-8')
 # browse返回目录项列表，不是新文件登记回执。完成移动、刷盘和原厂调用后登记本次路径。
 publish=edit(publish,'grep -q "string \\"$image\\"" "$s/album-browse.log"',
              'test -s "$d/$image.3FR"; test -s "$d/$image.$ext"')
 publish=edit(publish,'> "$s/album-browse.log"',
              '> /dev/null\nprintf "NATIVE_ALBUM_BROWSE_OK\\n" > "$s/album-browse.log"')
 save('quarter-publish.sh',publish)
 q=edit(q,'sh /system/etc/x2d-raw-stack/publish.sh "$s" "$format"',f'sh {ETC}/quarter-publish.sh "$s" "$format"')
 from task_failure_record import instrument
 save('quarter-run.sh',instrument(q))
 # 十六张使用同一份恢复/看门狗前缀，首写工具只向本次任务目录写入。
 s=baseline
 s=edit(s,'m=/system/bin/x2d-raw-stack/libx2d_raw_stack.so','m='+BIN+'/libx2d_private_capture.so')
 s=edit(s,'frames=1\nmode=0','frames=16\nmode=3')
 s=edit(s,'module_hash='+manifest['files']['p00-libx2d_raw_stack.so'],'module_hash='+sixteen['files']['libx2d_private_capture.so'])
 a=s.index('test "$#" = 5;');b=s.index('test ! -e /tmp/x2d-multishot-active;',a)
 s=s[:a]+'''test "$#" = 3; initial_delay=$1; test "$2" = 16; test "$3" = --prepare
case "$initial_delay" in [2-9]|[1-5][0-9]|60) ;; *) exit 2;; esac
available=$(df -k /mnt/media_rw/ssd | awk 'END{print $4}')
case "$available" in ''|*[!0-9]*) exit 2;; esac
test "$available" -ge 7340032 || { echo SIXTEEN_7GIB_SCRATCH_REQUIRED; exit 19; }
''' + s[b:]
 s=s.replace('X2D_RAW_STACK_JOB=$s','X2D_DISK_CAPTURE_JOB=$s')
 s=s.replace('X2D_RAW_STACK_ABI=1 X2D_RAW_STACK_JOB="$s"','X2D_PRIVATE_CAPTURE_ABI=1 X2D_DISK_CAPTURE_JOB="$s"')
 s=edit(s,'(set -C; printf "RS1 %s %s\\n" "$method" "$noise" > "$s/stack.config")','test ! -e "$s/capture.requested"')
 a=s.index('put image_format 0; put drive_mode 0');b=s.index('stage restore',a)
 # 暂停取景之后才启动一次原厂16张，不更改电子/机械快门选择。
 s=s[:a]+'''put image_format 0; put multishot_control_mode 3
put exposures_in_multishot_session 16; put drive_mode 6
(set -C; printf '16\\n' > "$s/capture.requested")
poll_hook; test -f "$s/owner.ready"; test ! -f "$s/failed"
clock(){ awk '{printf "%.0f\\n",$1*1000}' /proc/uptime; }
deadline=$(( $(clock)+initial_delay*1000 ))
stage delay
while [ "$(clock)" -lt "$deadline" ]; do
 kill -0 "$p"; test ! -f "$s/failed"
 printf '%s\\n' "$(((deadline-$(clock)+999)/1000))" > "$s/countdown.tmp"
 mv "$s/countdown.tmp" "$s/countdown"; sleep .1
done
stage capture
test "$(prop camera exposure_status)" = 0; test "$(prop error active_error)" = false
: > "$s/capture.invoked"
(set +e; camera do_exposure_extended2 boolean:true int32:24 int32:180000 > "$s/capture-request.log" 2>&1
 printf '%s\\n' "$?" > "$s/capture.rc.tmp"; mv "$s/capture.rc.tmp" "$s/capture.rc") &
exposure_rpc=$!
deadline=$(( $(clock)+300000 ))
while :; do
 kill -0 "$p"; poll_hook; test ! -f "$s/failed"
 test "$(prop error active_error)" = false
 if [ -f "$s/frames.ready" ] && [ -f "$s/capture.rc" ] && [ "$(prop camera exposure_status)" = 0 ]; then
  test "$(cat "$s/capture.rc")" = 0; break
 fi
 test "$(clock)" -lt "$deadline" || { echo SIXTEEN_CAPTURE_TIMEOUT; exit 26; }
 sleep .1
done
wait "$exposure_rpc"; exposure_rpc=
''' + s[b:]
 a=s.index('/system/bin/x2d-pixel-overclock/render-probe ');b=s.index('stage publish',a)
 s=s[:a]+f'''sh {ETC}/render-pipeline.sh "$s"
test "$(cat "$s/sixteen.output.ready")" = SIXTEEN_RAW_AND_JPEG_DURABLE
(set -C; printf 'RAW_STACK_R1\\n' > "$s/raw.ready")
format=0
printf '0\\n' > "$s/format-source"
''' + s[b:]
 # 十六张设置自身可能是mode3/frames16，恢复范围覆盖这组真实原厂状态。
 s=s.replace('[0-6]:[0-2]:[146]:[0-4])','[0-6]:[0-3]:[146]:[0-4]|[0-6]:[0-3]:16:[0-4])')
 s=s.replace('for name in rawpb.bin preview.jpg album.jpg capture.rc frame.request frame.armed frame.done finish.request; do','for name in render.jpg rawpb.bin preview.jpg album.jpg capture.rc; do')
 # 原厂shell失败路径单独保留收尾回执，避免占用遗留只能看到泛化报错。
 s=edit(s,' (set -e; restore); restored_rc=$?', ' (set -e; restore) >> "$s/finish-restore.log" 2>&1; restored_rc=$?\n printf "SIXTEEN_RESTORE_RESULT rc=%s\\n" "$restored_rc" >> "$s/finish-restore.log"')
 s=edit(s,' rmdir /tmp/x2d-multishot-active || restored_rc=90',' rmdir /tmp/x2d-multishot-active >> "$s/finish-restore.log" 2>&1 || restored_rc=90')
 s=edit(s,' [ "$restored_rc" = 0 ] || exit 90',' [ "$restored_rc" = 0 ] || { printf "90\\n" > "$s/run.rc"; exit 90; }')
 s=edit(s,'stage restore\nrestore\n','stage restore\nrestore\ntrap cleanup EXIT\ntrap \'exit 71\' HUP INT TERM\n')
 s=edit(s,' result=$?; trap - EXIT HUP INT TERM; set +e',' cleanup_entry_rc=$?; result=${1:-$cleanup_entry_rc}; trap - EXIT HUP INT TERM; set +e')
 s=edit(s,f'sh {ETC}/render-pipeline.sh "$s"',f'if sh {ETC}/render-pipeline.sh "$s"; then :; else pipeline_rc=$?; cleanup "$pipeline_rc"; fi')
 s=edit(s,'restore() (\n set -eu','restore() (\n trap - EXIT HUP INT TERM\n set -eu')
 s=edit(s,'live_before=\nexposure_rpc=\n','live_before=\nexposure_rpc=\nrender_pipeline_pid=\n')
 a=s.index('cleanup() {');b=s.index('trap cleanup EXIT',a)
 s=s[:a]+(HERE/'sixteen_finish_cleanup.inc').read_text(encoding='utf-8')+s[b:]
 s=edit(s,f'if sh {ETC}/render-pipeline.sh "$s"; then :; else pipeline_rc=$?; cleanup "$pipeline_rc"; fi',
  f'sh {ETC}/render-pipeline.sh "$s" &\nrender_pipeline_pid=$!\nif wait "$render_pipeline_pid"; then render_pipeline_pid=; else pipeline_rc=$?; render_pipeline_pid=; cleanup "$pipeline_rc"; fi')
 s=edit(s,'sh /system/etc/x2d-raw-stack/publish.sh "$s" "$format"',f'sh {ETC}/quarter-publish.sh "$s" "$format"')
 # 恢复拍摄设置后仍在准备合成；显影阶段由 pipeline 真正开始显影时写入。
 s=edit(s,'stage render\nprepare_render\n','prepare_render\n')
 import counter_idle_policy
 save('sixteen-run.sh',instrument(counter_idle_policy.apply(s)))
 pipeline=(OUT/'render-pipeline.sh').read_text(encoding='utf-8').replace('/system/bin/x2d-sixteen-disk-r1',BIN)
 save('render-pipeline.sh',pipeline)
 launch='''#!/system/bin/sh
set -eu
test "$#" = 4; s=$1; delay=$2; resolution=$3; test "$4" = --prepare
test "${s%/*}" = /mnt/media_rw/ssd/multishot; test -d "$s"; test ! -L "$s"
echo "${s##*/}" | grep -Eq '^native-album-ui-[0-9a-f]{8}-[1-9][0-9]{0,9}$'
test ! -e "$s/run.sh"; test ! -L "$s/run.sh"
case "$resolution:$delay" in 25:0|16:[2-9]|16:[1-5][0-9]|16:60) ;; *) exit 2;; esac
hashok(){ v=$(sha256sum "$1"); test "${v%% *}" = "$2"; }
HASHES
case "$resolution" in
 25) (set -C; cat ETC/quarter-run.sh > "$s/run.sh")
     exec sh "$s/run.sh" "$s" 0 86399 0 "$delay" --prepare;;
 16) (set -C; cat ETC/sixteen-run.sh > "$s/run.sh")
     exec sh "$s/run.sh" "$s" "$delay" 16 --prepare;;
esac
'''.replace('ETC',ETC)
 paths={n:BIN+'/'+n for n in ('libquarter_capture.so','finalize25','render25','pack25','merge16','render16','jpeg-grid','pack16','libx2d_private_capture.so')}
 paths.update({n:ETC+'/'+n for n in ('quarter-run.sh','quarter-publish.sh','sixteen-run.sh','render-pipeline.sh')})
 hashes='\n'.join('hashok '+p+' '+sha(OUT/n) for n,p in paths.items())
 from stale_render_ipc import patch_launch
 save('launch.sh',patch_launch(launch.replace('HASHES',hashes),'multi'))
 paths['launch.sh']=ETC+'/launch.sh'
 paths['worker']='/system/bin/x2d-pixel-overclock/worker'
 paths['libx2d_af_probe.so']='/system/lib64/libx2d_af_probe.so'
 paths.update({n:'/system/etc/x2d-pixel-overclock/'+n for n in ('PixelOverclockController.qml','X2dMultishotSettings.qml','PixelOverclockPage.qml','X2dPopoverDriveMode.qml','PixelOverclockHost.qml','QuarterCaptureKeyGuard.qml')})
 result=dict(installed=False,device_requests=0,offline_integrated=True,firmware='X2D 100C 4.2.0',files={n:sha(OUT/n) for n in paths},targets=paths,
  quarter_capture_frames=1,quarter_effective=[5828,4371],quarter_raw_stored=[5952,4421],quarter_jpeg_quality=80,
  quarter_native_raw_compatibility_verified=False,quarter_private_full_raw_scratch=False,quarter_source_shared_memory=True,
  sixteen_capture_frames=16,sixteen_effective=[46652,34996],sixteen_quality=60,sixteen_capacity=536870912,
  remaining=['native quarter RAW consumer compatibility','device component baseline and install','native shared-memory pool/encoder validation'])
 (OUT/'candidate.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
 print('MULTI_RESOLUTION_PRIVATE_RUNNERS_WORKER_UI_BUILT_NOT_INSTALLED')
if __name__=='__main__':main()
