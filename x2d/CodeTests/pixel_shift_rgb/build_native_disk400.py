"""四亿原厂写盘候选：离线构建，不访问设备，不修改已发布安装包。"""
from pathlib import Path
import hashlib, json, os, re, subprocess, sys, tarfile

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
OUT=ROOT/'x2d/outputs/4.2.0/pixel-shift-rgb/native-disk400-r1'
BASE=ROOT/'x2d/outputs/4.2.0/raw-stack/universal-installer-r5-8-44/windows-installer/routes/stock/payload'
ZIG=ROOT/'.research-cache/x1d-1.25.0/toolchain/zig-windows-x86_64-0.13.0/zig.exe'

def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def run(*args,**kw):
    subprocess.run([str(a) for a in args],check=True,timeout=300,env=ENV,**kw)
def write(p,text): p.write_text(text,encoding='utf-8',newline='\n')

def main():
    global ENV
    assert Path.cwd().resolve()==ROOT
    OUT.mkdir(exist_ok=True)
    ENV=dict(os.environ,TEMP=str(OUT),TMP=str(OUT),PYTHONIOENCODING='utf-8',
        ZIG_LOCAL_CACHE_DIR=str(OUT/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(OUT/'zig-global'))
    tools=OUT/'payload';tools.mkdir(exist_ok=True)
    algorithm=json.loads((HERE/'native_disk400_algorithm.json').read_text(encoding='utf-8'))
    assert sha(HERE/algorithm['local_file'])==algorithm['sha256'],'四亿核心须与已核对的远端源码一致'
    build_sources={
      'inputs':('native_album_inputs.c',['-DFACTORY_NATIVE_DISK_INPUTS=1','-pthread']),
      'merge6':('onboard_four.c',['-DFACTORY_NATIVE_DOMAIN=1','-DFACTORY_SIX=1',
          '-DFACTORY_PARALLEL_READ=1','-DFACTORY_READER_BATCH_ROWS=32','-DWRITER_ROWS=8','-DFACTORY_STDIO_ROWS=8','-pthread']),
      'pack-raw':('package_native_raw.c',[]),
      'jpeg-grid':('jpeg_grid_assemble.c',[]),
      'worker':('pixel_overclock_worker.c',['-DPO_RAW_STACK=1','-DPO_FULL_PREPARE=1',
          '-DPO_MULTI_RESOLUTION=1','-DPO_NATIVE_DISK400=1'])}
    for name,(source,defines) in build_sources.items():
        run(ZIG,'cc','-target','aarch64-linux-musl','-static','-O2','-Wall','-Wextra','-Werror',
            '-Wno-unused-function','-s',*defines,HERE/source,'-o',tools/name)
    run(sys.executable,'-B',HERE/'build_factory_observer.py','native-domain-repeat-candidate',
        '--native-preview','--six-phase-roi','--runtime-job','--inprocess-route','--capture-preview',
        '--packed-dng','--memory-bands','--native-disk400','--reference-sharpen',
        '--output-job','native-disk400-render-r1')
    render=OUT.parent/'native-disk400-render-r1'
    m=json.loads((render/'build-manifest.json').read_text(encoding='utf-8'))
    assert sha(render/m['binary'])==m['sha256']
    (tools/'render').write_bytes((render/m['binary']).read_bytes())
    for stem in ('run','launch','process','convert'):
        (tools/(stem+'.sh')).write_bytes((HERE/('native_disk400_'+stem+'.sh')).read_bytes())
    baseline=json.loads((BASE/'install-manifest.json').read_text(encoding='utf-8-sig'))
    with tarfile.open(BASE/'install-payload.tar.gz') as archive:
        def original(target):
            name=next(n for n,t in baseline['targets'].items() if t==target)
            data=archive.extractfile(name).read()
            assert hashlib.sha256(data).hexdigest()==baseline['files'][name]
            return data.decode('utf-8')
        publish=original('/system/bin/x2d-memory-manual-r17/publish.sh')
        old='test -f "$s/memory.ready"; test -f "$s/memory.inputs.ready"'
        assert publish.count(old)==1
        publish=publish.replace(old,'test "$(cat "$s/native.disk.ready")" = NATIVE_DISK_RAW_JPEG_DURABLE')
        write(tools/'publish.sh',publish)
        controller=original('/system/etc/x2d-pixel-overclock/PixelOverclockController.qml')
        controller=controller.replace('value === 6 || value === 16 || value === 25','value === 6 || value === 25')
        controller=controller.replace('value.frames===6 || value.frames===16 || value.frames===25','value.frames===6 || value.frames===25')
        controller=controller.replace('value.frames === 16 ? value.work_total === 55 : ','')
        controller=controller.replace('(value.frames === 16 ? 25 : 9)','9').replace('value.jpeg_tiles<=25','value.jpeg_tiles<=9')
        controller=controller.replace('value.frames===16 && value.stage==="jpeg_join" && value.jpeg_join_total===34996',
            'value.frames===6 && value.stage==="jpeg_join" && value.jpeg_join_total===17498')
        controller=controller.replace('value.jpeg_join_rows<=34996','value.jpeg_join_rows<=17498')
        write(tools/'PixelOverclockController.qml',controller)
        settings=original('/system/etc/x2d-pixel-overclock/X2dMultishotSettings.qml')
        assert '[25, 6, 16]' in settings
        settings=settings.replace('[25, 6, 16]','[25, 6]').replace('["25MP", "四亿", "十六亿"]','["25MP", "四亿"]')
        settings=settings.replace('controller.pixelResolution === 16 ? "十六亿" : ','')
        write(tools/'X2dMultishotSettings.qml',settings)
        popover=original('/system/etc/x2d-pixel-overclock/X2dPopoverDriveMode.qml')
        popover=popover.replace('root.multishotController && root.multishotController.pixelResolution === 16 ? "十六亿" : ','')
        write(tools/'X2dPopoverDriveMode.qml',popover)
        page=original('/system/etc/x2d-pixel-overclock/PixelOverclockPage.qml')
        page=re.sub(r'^.*controller.pixelResolution === 16\) return .*\n','',page,flags=re.M)
        mark='            if (!controller.stackMode && controller.pixelResolution === 16) {'
        assert page.count(mark)==1
        start=page.index(mark);end=page.index('\n            }',start)+len('\n            }')
        page=page[:start]+'''            if (!controller.stackMode && controller.pixelResolution === 6) {
                if (controller.stage === "jpeg") return Math.min(controller.jpegTiles + 1, 9) + "/9"
                if (controller.stage === "jpeg_join") return controller.jpegJoinComplete ? "100%" : controller.jpegJoinTotal>0 ? Math.min(99,Math.floor(controller.jpegJoinRows*100/controller.jpegJoinTotal))+"%" : "0%"
            }'''+page[end:]
        page=page.replace('完成 25/25 块编码','完成 9/9 块编码')
        write(tools/'PixelOverclockPage.qml',page)
        launch=original('/system/etc/x2d-multi-resolution/launch.sh')
        launch=launch.replace('25:0|16:[2-9]|16:[1-5][0-9]|16:60','25:0')
        launch='\n'.join(line for line in launch.splitlines() if not any(x in line for x in (
            'fileok /system/bin/x2d-multi-resolution/merge16','fileok /system/bin/x2d-multi-resolution/render16',
            'fileok /system/bin/x2d-multi-resolution/jpeg-grid','fileok /system/bin/x2d-multi-resolution/pack16',
            'fileok /system/bin/x2d-multi-resolution/libx2d_private_capture.so',
            'fileok /system/etc/x2d-multi-resolution/sixteen-run.sh','fileok /system/etc/x2d-multi-resolution/render-pipeline.sh')))+'\n'
        launch=launch[:launch.index('clear_stale_memory_endpoint(){')]+'''(set -C; cat /system/etc/x2d-multi-resolution/quarter-run.sh > "$s/run.sh")
exec sh "$s/run.sh" "$s" 0 86399 0 "$delay" --prepare
'''
        write(tools/'multi-launch.sh',launch)
        host=original('/system/etc/X2dShutterAnimationHost.qml')
        host=host.replace('    property var searchRoot: null','    property bool animationEnabled: false\n    property bool soundEnabled: false\n    property var searchRoot: null')
        host=host.replace('running: !root.drawer','running: root.animationEnabled && !root.drawer')
        host=host.replace('        source: "X2dShutterAnimation.qml"','        active: root.animationEnabled\n        source: "X2dShutterAnimation.qml"')
        write(tools/'X2dShutterAnimationHost.qml',host)
    for p in tools.glob('*.sh'):run('C:/Program Files/Git/bin/bash.exe','-n',p)
    targets={n:'/system/bin/x2d-native-disk400/'+n for n in
        ('inputs','merge6','pack-raw','jpeg-grid','render','run.sh','process.sh','publish.sh','convert.sh')}
    targets.update({'launch.sh':'/system/etc/x2d-pixel-overclock/launch.sh',
      'worker':'/system/bin/x2d-pixel-overclock/worker','multi-launch.sh':'/system/etc/x2d-multi-resolution/launch.sh'})
    targets.update({n:'/system/etc/x2d-pixel-overclock/'+n for n in
        ('PixelOverclockController.qml','X2dMultishotSettings.qml','PixelOverclockPage.qml','X2dPopoverDriveMode.qml')})
    targets['X2dShutterAnimationHost.qml']='/system/etc/X2dShutterAnimationHost.qml'
    record=dict(firmware='4.2.0',frames=6,sixteen_enabled=False,capture_owner='native camera-storage',
      native_sources_retained=True,native_source_removal_enabled=False,shutter_animation_default=False,shutter_sound_default=False,
      capture_references_held=0,full_raw_or_yuv_buffer=False,merge_reader_rows=32,merge_writer_rows=8,merge_stdio_rows=8,
      jpeg_quality=80,jpeg_capacity=64*1024*1024,tile=[11632,8720],tile_halo=8,
      sharpening=dict(amount=300,radius=1.2,threshold=0.7),
      targets=targets,files={n:sha(tools/n) for n in targets},renderer=m,algorithm=algorithm,
      source_hashes={p.name:sha(p) for p in HERE.iterdir() if p.name.startswith('native_disk400_') or p.name in
        ('factory_disk400.inc','build_native_disk400.py','native_album_inputs.c','onboard_four.c','background_writer.h',
         'factory_direct_reprocess.c','factory_runtime_job.h','pixel_overclock_core.h','pixel_overclock_worker.c',
         'six_row_kernel.h','onboard_six_rows.inc','four_raw_reader.h','merge_workers.h')},
      camera_requests=0,installed=False,hardware_validated=False)
    write(OUT/'build-manifest.json',json.dumps(record,ensure_ascii=False,indent=2)+'\n')
    print('NATIVE_DISK400_LOW_MEMORY_BUILT_NOT_INSTALLED')

if __name__=='__main__':main()
