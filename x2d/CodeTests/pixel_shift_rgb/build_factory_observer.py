import sys,os,subprocess,hashlib,json,re
from pathlib import Path
root=Path.cwd();o=root/'x2d/outputs/4.2.0/pixel-shift-rgb';sys.path.insert(0,str(root/'x2d/tools'))
if '--output-job' in sys.argv:
 index=sys.argv.index('--output-job')
 if index+1>=len(sys.argv) or not re.fullmatch('[a-z0-9-]{1,48}',sys.argv[index+1]):raise ValueError('Invalid output job')
 o=o/sys.argv[index+1]
 o.mkdir(exist_ok=True)
(o/'host-temp').mkdir(exist_ok=True)
from firmware_image import system_file,FirmwareElf
lib=o/'libdbus.so';lib.write_bytes(system_file('/lib64/libdbus.so'))
env=os.environ.copy();env.update(TEMP=str(o/'host-temp'),TMP=str(o/'host-temp'),ZIG_LOCAL_CACHE_DIR=str(o/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(o/'zig-global'))
native_repeat=len(sys.argv)>1 and sys.argv[1]=='native-domain-repeat-candidate'
native_domain=native_repeat or (len(sys.argv)>1 and sys.argv[1]=='native-domain-tiled-candidate')
native_level=native_domain or len(sys.argv)>1 and sys.argv[1]=='native-level-tiled-candidate'
tiled=native_level or (len(sys.argv)>1 and sys.argv[1]=='tiled-four-candidate')
four=tiled or (len(sys.argv)>1 and sys.argv[1]=='four-offline-candidate')
fresh=four or (len(sys.argv)>1 and sys.argv[1]=='fresh')
rebuild=fresh or (len(sys.argv)>1 and sys.argv[1]=='rebuild')
direct=rebuild or (len(sys.argv)>1 and sys.argv[1]=='direct')
stem='factory_direct_reprocess' if direct else 'factory_request_observer'
t=o/(('factory_native_domain_repeat_408_candidate' if native_repeat else 'factory_native_domain_408_candidate' if native_domain else 'factory_native_level_408_candidate' if native_level else 'factory_tiled_408_candidate' if tiled else 'factory_408_reprocess_candidate' if four else 'factory_fresh_reprocess' if fresh else 'factory_rebuilt_reprocess' if rebuild else stem)+'_arm')
defines=['-DFACTORY_REBUILD_REQUEST=1','-DFACTORY_JPEG_OUTPUT="/mnt/media_rw/ssd/x2d-factory-rebuilt-100mp-01.jpg"'] if rebuild else []
if fresh:defines=['-DFACTORY_REBUILD_REQUEST=1','-DFACTORY_NEW_BUFFERS=1','-DFACTORY_JPEG_OUTPUT="/mnt/media_rw/ssd/x2d-factory-fresh-100mp-01.jpg"']
if four:defines=['-DFACTORY_REBUILD_REQUEST=1','-DFACTORY_NEW_BUFFERS=1','-DFACTORY_TARGET_408=1','-DFACTORY_JPEG_OUTPUT="/mnt/media_rw/ssd/x2d-factory-408-candidate-01.jpg"']
if tiled:defines=['-DFACTORY_REBUILD_REQUEST=1','-DFACTORY_NEW_BUFFERS=1','-DFACTORY_TARGET_408=1','-DFACTORY_TILED_408=1','-DFACTORY_JPEG_CAPACITY=268435456u','-DFACTORY_JPEG_OUTPUT="/mnt/media_rw/ssd/x2d-factory-tiled-408-01.jpg"']
if native_level:defines=[d.replace('x2d-factory-tiled-408-01.jpg','x2d-factory-native-level-408-01.jpg') for d in defines]
if native_domain:defines=[d.replace('x2d-factory-native-level-408-01.jpg','x2d-factory-native-domain-408-01.jpg') for d in defines]
if native_repeat:defines=[d.replace('x2d-factory-native-domain-408-01.jpg','x2d-factory-native-domain-408-02.jpg') for d in defines]
if '--light-sharpen' in sys.argv[2:]:
 if not tiled:raise ValueError('Light sharpening requires the tiled JPEG path')
 t=t.with_name(t.name.replace('_arm','_light_sharpen_arm'))
 defines=[d.replace('.jpg"','-light-sharpen.jpg"') if d.startswith('-DFACTORY_JPEG_OUTPUT=') else d for d in defines]
 defines.append('-DFACTORY_JPEG_LIGHT_SHARPEN=1')
if '--reference-sharpen' in sys.argv[2:]:
 if not any(s in sys.argv for s in ('--full-memory','--native-disk400')) or '--light-sharpen' in sys.argv:
  raise ValueError('Reference sharpening requires full memory and excludes the old light candidate')
 t=t.with_name(t.name.replace('_arm','_reference_sharpen_arm'))
 defines.append('-DFACTORY_JPEG_REFERENCE_SHARPEN=1')
if '--native-preview' in sys.argv[2:]:
 if not tiled:raise ValueError('Native preview requires the tiled JPEG path')
 t=t.with_name(t.name.replace('_arm','_with_preview_arm'))
 defines=[d.replace('.jpg"','-with-preview.jpg"') if d.startswith('-DFACTORY_JPEG_OUTPUT=') else d for d in defines]
 defines.append('-DFACTORY_NATIVE_PREVIEW_OUTPUT="/mnt/media_rw/ssd/multishot/package-check-01/generated-preview.jpg"')
if '--six-phase-roi' in sys.argv[2:]:
 if not tiled:raise ValueError('Six-shot phase adaptation requires the tiled JPEG path')
 t=t.with_name(t.name.replace('_arm','_six_phase_roi_arm'))
 defines=[d.replace('.jpg"','-six-phase-roi.jpg"') if d.startswith(('-DFACTORY_JPEG_OUTPUT=','-DFACTORY_NATIVE_PREVIEW_OUTPUT=')) else d for d in defines]
 defines.append('-DFACTORY_SIX_PHASE_ROI=1')
if '--job-dir' in sys.argv[2:]:
 index=sys.argv.index('--job-dir')
 if index+1>=len(sys.argv):raise ValueError('Missing private job directory')
 job=sys.argv[index+1]
 if not tiled or not re.fullmatch('/mnt/media_rw/ssd/multishot/[a-z0-9-]{1,48}',job):raise ValueError('Invalid private job directory')
 t=t.with_name(t.name.replace('_arm','_private_job_arm'))
 defines=[f'-DFACTORY_JPEG_OUTPUT="{job}/render.jpg"' if d.startswith('-DFACTORY_JPEG_OUTPUT=') else
          f'-DFACTORY_NATIVE_PREVIEW_OUTPUT="{job}/preview.jpg"' if d.startswith('-DFACTORY_NATIVE_PREVIEW_OUTPUT=') else d for d in defines]
if '--runtime-job' in sys.argv[2:]:
 if not native_repeat or '--job-dir' in sys.argv or '--native-preview' not in sys.argv:raise ValueError('Runtime jobs require native repeat plus preview and no fixed directory')
 defines.append('-DFACTORY_RUNTIME_JOB=1')
 t=t.with_name(t.name.replace('_arm','_runtime_job_arm'))
libs=[str(lib)]
if '--inprocess-route' in sys.argv[2:]:
 if not native_repeat or '--runtime-job' not in sys.argv:raise ValueError('Task route requires native repeat runtime jobs')
 defines.append('-DFACTORY_INPROCESS_ROUTE=1')
 t=t.with_name(t.name.replace('_arm','_task_route_arm'))
if '--capture-preview' in sys.argv[2:]:
 if '--inprocess-route' not in sys.argv or '--native-preview' not in sys.argv:raise ValueError('Capture preview requires the reviewed runtime task route')
 defines.append('-DFACTORY_CAPTURE_PREVIEW=1')
 t=t.with_name(t.name.replace('_arm','_capture_preview_arm'))
if '--packed-dng' in sys.argv[2:]:
    if '--capture-preview' not in sys.argv or '--six-phase-roi' not in sys.argv:raise ValueError('Packed input requires the validated six-phase task route and capture preview')
    defines.append('-DFACTORY_PACKED_DNG_INPUT=1')
    t=t.with_name(t.name.replace('_arm','_packed_dng_arm'))
if '--six-edge-tiles' in sys.argv[2:]:
    if '--packed-dng' not in sys.argv:raise ValueError('Six tiles require direct packed six-phase input')
    defines.append('-DFACTORY_SIX_EDGE_TILES=1')
    t=t.with_name(t.name.replace('_arm','_six_edge_tiles_arm'))
if '--memory-bands' in sys.argv[2:]:
    if '--packed-dng' not in sys.argv or '--six-edge-tiles' in sys.argv:
        raise ValueError('Memory bands require the protected runtime six-phase route')
    defines.append('-DFACTORY_MEMORY_BANDS=1')
    t=t.with_name(t.name.replace('_arm','_memory_bands_arm'))
if '--native-disk400' in sys.argv:
    if not all(s in sys.argv for s in ('--memory-bands','--reference-sharpen','--inprocess-route')) or any(s in sys.argv for s in ('--full-memory','--raw-stack','--sixteen-disk','--quarter-single')):
        raise ValueError('Native disk400 is a separate six-frame disk consumer, not a capture hook')
    defines.extend(('-DFACTORY_NATIVE_DISK400=1','-DFACTORY_JPEG_DISABLE_LARGER_RETRY=1'))
    defines=[('-DFACTORY_JPEG_CAPACITY=67108864u' if d.startswith('-DFACTORY_JPEG_CAPACITY=') else d) for d in defines]
    t=t.with_name(t.name.replace('_arm','_native_disk400_arm'))
if '--full-memory' in sys.argv[2:]:
    if '--memory-bands' not in sys.argv:raise ValueError('Full result requires the protected memory route')
    defines.append('-DFACTORY_MEMORY_FULL_RESULT=1')
if '--raw-stack' in sys.argv[2:]:
    if '--memory-bands' not in sys.argv:raise ValueError('RAW stack requires protected native route')
    defines.append('-DFACTORY_RAW_STACK=1')
if '--sixteen-disk' in sys.argv[2:]:
    if '--raw-stack' not in sys.argv or '--jpeg-restart-bands' in sys.argv:
        raise ValueError('Sixteen disk requires native RAW request helpers and its own tile encoder')
    defines.extend(('-DFACTORY_SIXTEEN_DISK=1','-DFACTORY_JPEG_NATIVE_QUALITY=60u'))
    if '--reference-sharpen' in sys.argv:
        defines.extend(('-DFS_AMOUNT=400u','-DFS_RADIUS_TENTHS=15u','-DFS_THRESHOLD_TENTHS=3u'))
elif '--reference-sharpen' in sys.argv:
    defines.extend(('-DFS_AMOUNT=300u','-DFS_RADIUS_TENTHS=12u','-DFS_THRESHOLD_TENTHS=7u'))
if '--quarter-single' in sys.argv[2:]:
    if '--raw-stack' not in sys.argv or any(s in sys.argv for s in ('--sixteen-disk','--reference-sharpen','--jpeg-restart-bands')):
        raise ValueError('Quarter single requires the native single render route without sixteen or extra sharpening')
    defines.append('-DFACTORY_QUARTER_SINGLE=1')
if '--quarter-memory' in sys.argv:
    if '--quarter-single' not in sys.argv:raise ValueError('Quarter shared memory requires quarter single rendering')
    defines.append('-DFACTORY_QUARTER_MEMORY=1')
if '--jpeg-capacity-mib' in sys.argv:
    index=sys.argv.index('--jpeg-capacity-mib')
    if index+1>=len(sys.argv) or sys.argv[index+1] not in ('256','386','512') or '--full-memory' not in sys.argv:
        raise ValueError('Explicit JPEG capacity requires the full memory route and a reviewed size')
    capacity=int(sys.argv[index+1])*1048576
    defines=[f'-DFACTORY_JPEG_CAPACITY={capacity}u' if d.startswith('-DFACTORY_JPEG_CAPACITY=') else d for d in defines]
if '--jpeg-restart-bands' in sys.argv:
    if '--full-memory' not in sys.argv or '--reference-sharpen' not in sys.argv:
        raise ValueError('JPEG restart bands require the complete native YUV and sharpening path')
    defines.append('-DFACTORY_JPEG_RESTART_BANDS=1')
if '--jpeg-single-pass' in sys.argv:
    if '--full-memory' not in sys.argv or '--jpeg-restart-bands' in sys.argv:
        raise ValueError('Single JPEG request requires full memory and excludes restart bands')
    defines.append('-DFACTORY_JPEG_DISABLE_LARGER_RETRY=1')
if direct:
 h=o/'libduml_hal.so';h.write_bytes(system_file('/lib64/libduml_hal.so'));libs.append(str(h));v=o/'libduml_vcodec.so';v.write_bytes(system_file('/lib64/libduml_vcodec.so'));libs.append(str(v))
compiler=str(root/'.research-cache/x1d-1.25.0/toolchain/zig-windows-x86_64-0.13.0/zig.exe')
source=root/('x2d/CodeTests/pixel_shift_rgb/'+stem+'.c')
flags=[compiler,'cc','-target','aarch64-linux-gnu','-O2','-Wall','-Wextra','-Werror','-ffreestanding','-fno-stack-protector']
if '--memory-bands' in sys.argv:
    # 编译阶段保留系统头；链接阶段使用官方 Bionic，不加入 GNU runtime。
    c=o/'libc.so';c.write_bytes(system_file('/lib64/libc.so'));libs.append(str(c))
    obj=o/(t.stem+'.o')
    subprocess.run([*flags,'-fPIE',*defines,'-c',str(source),'-o',str(obj)],env=env,check=True)
    link_input=[str(obj)]
    subprocess.run([compiler,'ld.lld','-m','aarch64elf','-pie','--no-undefined','--allow-shlib-undefined','-z','noexecstack','-e','_start','--no-gc-sections','--strip-all','--dynamic-linker=/system/bin/linker64',*link_input,*libs,'-o',str(t)],env=env,check=True)
    assert [s.data() for s in FirmwareElf(t.read_bytes()).elf.iter_segments() if s['p_type']=='PT_INTERP']==[b'/system/bin/linker64\0']
else:
    link_input=[*defines,str(source)]
    subprocess.run([*flags,'-nostdlib','-fPIE','-pie','-Wl,--no-undefined','-Wl,--allow-shlib-undefined','-Wl,-z,noexecstack','-Wl,-e,_start','-Wl,--no-gc-sections','-Wl,--strip-all','-Wl,--dynamic-linker,/system/bin/linker64',*link_input,*libs,'-o',str(t)],env=env,check=True)
b=FirmwareElf(t.read_bytes());imports=[s.name for s in b.elf.get_section_by_name('.dynsym').iter_symbols() if s.name and s['st_shndx']=='SHN_UNDEF'];assert all(n.startswith(('dbus_','duss_hal_')) or ('--memory-bands' in sys.argv and n in ('memcpy','memset','memcmp','__libc_init')) or ('--native-disk400' in sys.argv and n=='memmove') or (any(s in sys.argv for s in ('--reference-sharpen','--quarter-single')) and n in ('pthread_create','pthread_join')) or ('--raw-stack' in sys.argv and n in ('strlen','strcpy','strncmp')) for n in imports),imports
if '--raw-stack' in sys.argv or '--native-disk400' in sys.argv:
    sources=('factory_direct_reprocess.c','factory_raw_stack.inc','factory_raw_stack_diagnostics.h','factory_reference_sharpen.h','factory_memory_tiles.inc','factory_cpu_result_pool.h','factory_direct_codec.inc','factory_jpeg_bands.inc','factory_jpeg_band_header.h')
    if '--sixteen-disk' in sys.argv:sources+=('factory_sixteen_disk.inc','sixteen_disk_policy.h','factory_raw_tile.h')
    if '--native-disk400' in sys.argv:sources+=('factory_disk400.inc','factory_raw_tile.h','factory_runtime_job.h')
    if '--quarter-single' in sys.argv:sources+=('factory_quarter_single.inc','quarter_single_core.c','quarter_single_core.h')
    if '--quarter-memory' in sys.argv:sources+=('factory_quarter_memory.inc','quarter_memory_wire.h')
    (o/'build-manifest.json').write_text(json.dumps(dict(binary=t.name,sha256=hashlib.sha256(t.read_bytes()).hexdigest(),
       defines=defines,source_hashes={n:hashlib.sha256((source.parent/n).read_bytes()).hexdigest() for n in sources},
       sharpening=(dict(amount=400 if '--sixteen-disk' in sys.argv else 300,
                       radius=1.5 if '--sixteen-disk' in sys.argv else 1.2,
                       threshold=0.3 if '--sixteen-disk' in sys.argv else 0.7,halo=0,
                       luma_only=True,threads=4,capture_one_equivalent=False)
                   if '--reference-sharpen' in sys.argv else None),
       camera_access=False,installed=False),indent=2)+'\n',encoding='utf-8')
print(json.dumps({'sha256':hashlib.sha256(t.read_bytes()).hexdigest(),'bytes':t.stat().st_size}))
