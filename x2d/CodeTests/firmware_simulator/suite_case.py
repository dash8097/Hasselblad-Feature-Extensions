"""离线场景适配器。源码计算/Qt/ARM执行与显式设备替身分别记录。"""
from pathlib import Path
import hashlib
import importlib
import json
import os
import re
import runpy
import shutil
import struct
import subprocess
import sys
import traceback

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
PIX = ROOT / 'x2d/CodeTests/pixel_shift_rgb'
ZIG = ROOT / '.research-cache/x1d-1.25.0/toolchain/zig-windows-x86_64-0.13.0/zig.exe'
OUT = Path(sys.argv[2]).resolve()
CASE = sys.argv[1]
sys.dont_write_bytecode = True
sys.path[:0] = [str(PIX), str(ROOT / 'x2d/tools'), str(ROOT / '.research-cache/x1d-1.25.0/python')]
sys.argv = [__file__]
INPUTS, COMMANDS = {}, []
NATIVE_MODULES = set()
AUDITING = False


def windows_job():
    """父编排超时时连同本 stage 的编译器/QEMU 一起回收；不按进程名杀其它任务。"""
    if os.name != 'nt':
        return None
    import ctypes as c
    from ctypes import wintypes as w
    class Basic(c.Structure):
        _fields_ = [('process_time', c.c_int64), ('job_time', c.c_int64), ('flags', w.DWORD),
                    ('min_ws', c.c_size_t), ('max_ws', c.c_size_t), ('process_limit', w.DWORD),
                    ('affinity', c.c_size_t), ('priority', w.DWORD), ('schedule', w.DWORD)]
    class IO(c.Structure):
        _fields_ = [(n, c.c_uint64) for n in ('r', 'w', 'o', 'rb', 'wb', 'ob')]
    class Extended(c.Structure):
        _fields_ = [('basic', Basic), ('io', IO), ('process_memory', c.c_size_t),
                    ('job_memory', c.c_size_t), ('peak_process', c.c_size_t), ('peak_job', c.c_size_t)]
    k = c.WinDLL('kernel32', use_last_error=True)
    k.CreateJobObjectW.argtypes = [c.c_void_p, w.LPCWSTR]; k.CreateJobObjectW.restype = w.HANDLE
    k.GetCurrentProcess.restype = w.HANDLE
    k.SetInformationJobObject.argtypes = [w.HANDLE, c.c_int, c.c_void_p, w.DWORD]
    k.AssignProcessToJobObject.argtypes = [w.HANDLE, w.HANDLE]
    handle = k.CreateJobObjectW(None, None); info = Extended(); info.basic.flags = 0x2000
    if not handle or not k.SetInformationJobObject(handle, 9, c.byref(info), c.sizeof(info)) or not k.AssignProcessToJobObject(handle, k.GetCurrentProcess()):
        raise OSError(c.get_last_error(), 'Cannot establish stage job lifetime')
    return handle


def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda: f.read(1024*1024), b''):
            h.update(b)
    return h.hexdigest()


def native_modules():
    """只读取本Python进程已加载模块；不枚举其它进程、设备或服务。"""
    if os.name != 'nt':
        return
    import ctypes as c
    from ctypes import wintypes as w
    k = c.WinDLL('kernel32', use_last_error=True)
    k.GetCurrentProcess.restype = w.HANDLE
    k.K32EnumProcessModules.argtypes = [w.HANDLE, c.POINTER(w.HMODULE), w.DWORD, c.POINTER(w.DWORD)]
    k.K32EnumProcessModules.restype = w.BOOL
    k.GetModuleFileNameW.argtypes = [w.HMODULE, w.LPWSTR, w.DWORD]
    k.GetModuleFileNameW.restype = w.DWORD
    handles = (w.HMODULE * 2048)(); needed = w.DWORD()
    if not k.K32EnumProcessModules(k.GetCurrentProcess(), handles, c.sizeof(handles), c.byref(needed)) or needed.value > c.sizeof(handles):
        raise OSError(c.get_last_error(), 'Cannot bind native runtime inputs')
    interpreter = Path(sys.executable).resolve().parent
    for handle in handles[:needed.value // c.sizeof(w.HMODULE)]:
        name = c.create_unicode_buffer(32768)
        length = k.GetModuleFileNameW(handle, name, len(name))
        if not length or length == len(name):
            raise OSError(c.get_last_error(), 'Cannot resolve native runtime input')
        path = Path(name.value).resolve()
        if path.is_relative_to(ROOT) or path.is_relative_to(interpreter):
            if not path.is_relative_to(OUT):
                bind(path); NATIVE_MODULES.add(path)


def audit(event, args):
    global AUDITING
    if AUDITING:
        return
    if event in ('socket.connect', 'socket.getaddrinfo'):
        raise RuntimeError('Offline adapter forbids network: ' + event)
    if event == 'import' and len(args) > 1 and isinstance(args[1], str):
        module = Path(args[1]).resolve()
        if module.suffix.lower() in ('.pyd', '.dll'):
            bind(module)
            # 每次原生扩展加载前保留之前已加载的依赖，结束后再补最后一批。
            native_modules()
    if event == 'open' and isinstance(args[0], (str, bytes)):
        p = Path(os.fsdecode(args[0])).resolve()
        if str(p).startswith('\\\\.\\'):
            raise RuntimeError('Device path forbidden')
        if p.is_relative_to(ROOT) and not p.is_relative_to(OUT):
            mode, flags = args[1], args[2]
            writing = (isinstance(mode, str) and any(v in mode for v in 'wax+')) or flags & (os.O_WRONLY | os.O_RDWR | os.O_CREAT | os.O_TRUNC)
            if writing:
                raise RuntimeError('Adapter write outside this attempt: ' + str(p))
            bind(p)
    if event == 'subprocess.Popen':
        COMMANDS.append([str(v) for v in args[1]] if isinstance(args[1], (list, tuple)) else str(args[1]))
        if isinstance(args[1], (list, tuple)):
            argv = [str(v) for v in args[1]]
            for i, value in enumerate(argv):
                p = Path(value).resolve()
                if p.is_file() and p.is_relative_to(ROOT) and not p.is_relative_to(OUT):
                    bind(p)
                include = argv[i+1] if value == '-I' and i+1 < len(argv) else value[2:] if value.startswith('-I') else None
                if include:
                    directory = Path(include).resolve()
                    if directory.is_relative_to(ROOT) and not directory.is_relative_to(OUT):
                        for header in directory.rglob('*'):
                            if header.is_file() and header.suffix in ('.h', '.hpp', '.inc'):
                                bind(header)


def bind(path):
    global AUDITING
    if path in INPUTS or not path.is_file():
        return
    AUDITING = True
    try:
        INPUTS[path] = sha(path)
    finally:
        AUDITING = False


def call(args, marker=None, timeout=180, ok=True):
    p = subprocess.run([str(a) for a in args], capture_output=True, text=True,
                       encoding='utf-8', errors='replace', timeout=timeout,
                       creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
    print(p.stdout, end=''); print(p.stderr, end='')
    if (p.returncode == 0) != ok or (marker and marker not in p.stdout):
        raise RuntimeError('Offline child rejected: ' + str(args[0]) + '\n' + p.stdout + p.stderr)
    return p.stdout


def compile_test(name, sources, flags=(), args=(), marker=None, directory=PIX):
    target = OUT / (name + '.exe')
    call([ZIG, 'cc', '-O2', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function',
          '-I', directory, *flags, *[directory / n for n in sources], '-o', target])
    return call([target, *args], marker)


def runtime():
    import run as sim
    sim.OUT = OUT / 'core'
    sim.execute()
    import check_harness
    check_harness.main()
    return dict(real='QEMU + 原厂4.2.0加载器/Bionic + 生产计算核心',
                evidence=str(sim.OUT / 'latest-attempt.json'), negative_reference=True)


def memory():
    import run as sim
    golden = OUT / 'analytic.3fr'
    golden.write_bytes(bytes(4096) + sim.reference_400(0, 2059, 67, 2059))
    output = OUT / 'merged.bin'
    compile_test('session', ['check_capture_memory_session.c', 'capture_memory_session.c',
        'capture_memory_merge.c', 'capture_merge_workers.c', 'capture_output_workers.c', 'private_capture_group.c'],
        flags=['-DX2D_CAPTURE_SESSION_HOST_CHECK=1'], args=[golden, output], marker='SESSION_14_CASES')
    assert output.read_bytes() == golden.read_bytes()[4096:]
    # 已有绑定只作为输入；逐入口对固定原厂 ELF 重验，不信任旧通过记录。
    from firmware_image import system_elf
    storage = system_elf('/bin/camera-storage', '8c086e9c2a340c2a388e92ae3de39d6d0e5ce19ac3d582626112a24fd3b97aef')
    binding = (ROOT / 'x2d/outputs/4.2.0/pixel-shift-rgb/storage_private_bindings.inc').read_text('utf-8')
    fingerprints = re.findall(r'\{(0x[0-9a-fA-F]+)(?:UL)?,\s*\{([^}]+)\}\}', binding.split('private_bindings[]', 1)[1])
    assert fingerprints
    for address, encoded in fingerprints:
        expected = bytes(int(x.strip(), 0) for x in encoded.split(',') if x.strip())
        assert storage.read(int(address, 16), len(expected)) == expected
    (OUT / 'storage_private_bindings.inc').write_text(binding, encoding='utf-8')
    compile_test('native-borrow', ['check_capture_memory_native.c'], flags=['-I', OUT], marker='OWNER_THREAD_COMPLETION')
    compile_test('input', ['check_capture_memory_input.c', 'private_capture_group.c'], marker='PASSED')
    compile_test('fanout', ['check_pixel_chunk_fanout.c'], marker='PASSED')
    return dict(real='实际CsSession、四工人合成、两个输出线程和原厂引用适配代码',
                fixture='67x2059源图，独立解析参考；全幅分配但仅图样行有内容',
                cases=28, mocked='Qt对象/曝光回调/ISP/发布函数；不是完整服务',
                output_pixels_sha256=sha(output), source_pixels_sha256=sha(golden))


def sixteen():
    import check_sixteen_disk_merge as test
    test.OUT = OUT / 'merge'; test.main()
    compile_test('render', ['check_sixteen_render.c'], marker='SIXTEEN_128')
    compile_test('join-progress', ['check_sixteen_join_progress.c'], marker='PASSED')
    result = json.loads((test.OUT / 'validation.json').read_text('utf-8'))
    assert len(result['cases']) == 7
    return dict(real='生产16帧4线程合成、元数据、25块显影控制', merge_cases=result['cases'],
                mocked='17x21缩小源图；显影/编码RPC及ION为替身', jpeg_quality=60)


def quarter():
    # Pillow/numpy位于应用随附的离线运行库；不下载依赖。
    runtime = Path.home() / '.cache/codex-runtimes/codex-primary-runtime/dependencies/python/Lib/site-packages'
    sys.path.insert(0, str(runtime))
    import check_quarter_upstream as test
    test.build.OUT = OUT; test.main()
    return json.loads((OUT / 'upstream-validation.json').read_text('utf-8'))


def stack():
    compile_test('core', ['check_raw_stack_core.c', 'check_raw_stack_segments.c', 'raw_stack_core.c',
                          'raw_stack_parallel.c', 'raw_stack_session.c'],
                 flags=['-DRS_SEGMENT_TEST_NO_MAIN=1'], marker='RAW_STACK_CORE_OK')
    compile_test('async', ['check_raw_stack_async.c', 'raw_stack_async.c', 'raw_stack_core.c'],
                 flags=['-DRS_ASYNC_HOST_CHECK=1'], marker='RAW_STACK_ASYNC_PERSISTENT')
    for n in ('pool', 'protocol', 'iso'):
        compile_test(n, ['check_raw_stack_' + n + '.c'], marker='PASS')
    capture = (PIX / 'raw_stack_capture.c').read_text('utf-8')
    for name, marker, begin, end, extra in (
        ('rejection', 'PRODUCTION_LOG_FIRST_CLAIM', 'static void log_first_claim_rejection(', 'static int receive(', []),
        ('dispatch', 'PRODUCTION_DISPATCH_FRAME', 'static int frame_slot_available(void){', 'static void pump(void)', []),
        ('deferred', 'PRODUCTION_POOL_FUNCTIONS', 'static int pool_prepare(', 'static int destination(', [PIX / 'raw_stack_core.c']),
        ('iso_capture', 'PRODUCTION_CLAIM_WORK', 'static int claim_work(', 'static int local_notification(', [])):
        template = (PIX / ('check_raw_stack_' + name + '.c')).read_text('utf-8')
        token = '/* ' + marker + ' */'
        assert template.count(token) == 1
        extracted = capture[capture.index(begin):capture.index(end)]
        source = OUT / (name + '-check.c')
        source.write_text(template.replace(token, extracted), encoding='utf-8')
        compile_test(name, [source, *extra], marker='PASS')
    return dict(real='生产累计核心、常驻工人、归还/计算交接、池生命周期',
                mocked='小像素/模拟原厂引用、元数据和曝光；无真实5fps/DMA吞吐验收')


def precapture():
    directory = ROOT / 'x2d/CodeTests/liveview_precapture'
    sys.path.insert(0, str(directory))
    import inspect_firmware as firmware
    firmware.OUT = OUT / 'firmware-r1'; firmware.main()
    compile_test('ring', ['precapture_ring.c', 'check_ring.c'], directory=directory, marker='PRECAPTURE_RING_PASS')
    compile_test('race', ['precapture_ring.c', 'check_race.c'], flags=['-DPC_TEST_HOOK'], directory=directory, marker='PASS')
    compile_test('frames', ['precapture_ring.c', 'precapture_frame420.c', 'check_frame420.c'], directory=directory,
                 args=[firmware.OUT / f'nv12-{s}-{r}.bin' for s,r in [(16,2),(128,16),(256,64)]], marker='PASS')
    import check_candidate as test
    test.b.OUT = OUT / 'candidate'; test.b.JBUILD = test.b.OUT / 'jpeg-musl'; test.main()
    return json.loads((test.b.OUT / 'runtime-validation.json').read_text('utf-8'))


def af():
    directory = ROOT / 'x2d/CodeTests/afc_observer'
    runpy.run_path(str(directory / 'check_afc_cdaf_speed_types.py'), run_name='__main__')
    # 当前一帧候选也重新构建执行，包含AF-S/MF不生效和持续变化抑制。
    sys.path.insert(0, str(directory)); sys.argv = [str(directory), '--one-frame']
    runpy.run_path(str(directory / 'build_afc_settle.py'), run_name='__main__')
    runpy.run_path(str(directory / 'check_afc_settle.py'), run_name='__main__')
    return dict(real='4.2.0原厂反差选速/距离分类ARM及一帧候选',
                mocked='模式/统计/镜头和系统函数替身', historical_camera_records_used=False)


def face():
    directory = ROOT / 'x2d/CodeTests/face_edge_fix'
    # 历史成套QML输入必须显式绑定；不把它称为当前安装器的状态。
    package = ROOT / 'x2d/outputs/4.2.0/face-edge-fix/package-fallback'
    os.environ['X2D_SIM_FACE_PACKAGE'] = str(package)
    sys.argv = [str(directory / 'check.py'), '--fallback']
    runpy.run_path(str(directory / 'check.py'), run_name='__main__')
    return dict(real='Qt人脸/眼框与模式交互、合焦着色和旧回执拒绝',
                input_package=str(package.relative_to(ROOT)), mocked='人脸识别/原厂设置/真实合焦')


def eye():
    runpy.run_path(str(ROOT / 'x2d/CodeTests/face_edge_fix/check_eye_preference.py'), run_name='__main__')
    return dict(real='当前EyePreferencePolicy.js在Qt JS引擎执行', mocked='自造人眼坐标/时钟')


def power():
    directory = ROOT / 'x2d/CodeTests/display_controls'; sys.path.insert(0, str(directory))
    import prepare_liveview
    prepare_liveview.OUT = OUT
    runpy.run_path(str(directory / 'check_liveview.py'), run_name='__main__')
    return json.loads((OUT / 'qt-validation.json').read_text('utf-8'))


def capture_ui():
    runpy.run_path(str(PIX / 'check_pixel_overclock_ui.py'), run_name='__main__')
    return json.loads((OUT / 'qt-check.json').read_text('utf-8'))


def installer():
    directory = ROOT / 'x2d/CodeTests/installer_hardening/camera_native'
    sys.path.insert(0, str(directory))
    frozen = os.environ.get('X2D_SIM_INSTALLER_REVISION')
    if frozen:
        origin = Path(frozen).resolve()
        assert origin.is_relative_to(ROOT / 'x2d/outputs') and re.fullmatch('[0-9a-f]{20}', origin.name)
        proof = json.loads((origin / 'build-proof.json').read_text('utf-8'))
        rev = OUT / 'native-flow/revisions' / origin.name
        (rev / 'private-build/source').mkdir(parents=True)
        (rev / 'artifacts').mkdir()
        for name, value in proof['source_sha256'].items():
            assert Path(name).name == name and sha(origin / 'private-build/source' / name) == value
            shutil.copyfile(origin / 'private-build/source' / name, rev / 'private-build/source' / name)
        assert sha(origin / 'artifacts/x2d-native-flow') == proof['artifact']['sha256']
        shutil.copyfile(origin / 'artifacts/x2d-native-flow', rev / 'artifacts/x2d-native-flow')
        shutil.copyfile(origin / 'build-proof.json', rev / 'build-proof.json')
    else:
        import build_flow as build
        sys.argv = [str(directory), '--output', str(OUT / 'native-flow')]; build.main()
        rev, = (OUT / 'native-flow/revisions').iterdir()
    proof = json.loads((rev / 'build-proof.json').read_text('utf-8')); digest = proof['artifact']['sha256']
    import checker_uninstall_linux as uninstall
    uninstall.REV = rev; uninstall.SHA = digest; uninstall.OUT = OUT / 'uninstall'; uninstall.main()
    import checker_install_linux as install
    install.R7 = OUT
    # 原厂probe也是输入，保留既有固定版本但禁止把旧验证当成本次执行。
    probe = ROOT / 'x2d/outputs/4.2.0/installer-hardening-r7/install-probe/revisions/092516980d4ea967eb63/artifacts/install-probe'
    dest = OUT / 'install-probe/revisions/092516980d4ea967eb63/artifacts/install-probe'
    dest.parent.mkdir(parents=True); dest.write_bytes(probe.read_bytes())
    for flags in ([], ['--model'], ['--model', '--mapping'], ['--model', '--restart-failure']):
        sys.argv = [str(directory), '--revision', rev.name, '--sha', digest, *flags]; install.main()
    return dict(real='原生事务在ARM64 Linux重新执行卸载、精确入口及安装/映射/重启失败模型',
                build_mode='explicit_frozen_revision' if frozen else 'fresh_current_source_build',
                revision=rev.name, frozen_origin=str(origin.relative_to(ROOT)) if frozen else None,
                source_sha256=proof['source_sha256'], latest_installer_qualified=False,
                artifact_sha256=digest, mocked='相机进程和D-Bus、安装测试模型替换3项硬件相关载荷',
                system_partition_verified=False, public_installer_qualified=False)


def mobile():
    from mobile_case import check
    return check(OUT, call, compile_test)


def main():
    if not OUT.is_relative_to(ROOT / 'x2d/outputs/4.2.0/firmware-simulator-r2') or Path.cwd().resolve() != ROOT or not __debug__:
        raise RuntimeError('Invalid offline workspace')
    lifetime = windows_job()
    sys.addaudithook(audit)
    report = dict(case=CASE, passed=False, real_camera=False, device_operations=0)
    try:
        native_modules()
        # 定位固定固件依赖，导入只读取本地镜像。
        import firmware_image
        report['details'] = globals()[CASE]()
        report['passed'] = True
    except BaseException as error:
        report['error'] = str(error); traceback.print_exc()
    finally:
        # 捕获Python读取的输入及编译器/原生模块依赖，编排结束时会再次校验。
        try:
            native_modules()
        except Exception as error:
            report['passed'] = False; report['native_runtime_error'] = str(error)
        initial = dict(INPUTS)
        report['input_sha256'] = {str(p.relative_to(ROOT)): h for p,h in sorted(initial.items()) if p.is_relative_to(ROOT)}
        report['external_runtime_sha256'] = {str(p): h for p,h in sorted(initial.items()) if not p.is_relative_to(ROOT)}
        report['native_module_sha256'] = {str(p): initial[p] for p in sorted(NATIVE_MODULES)}
        report['inputs_unchanged'] = all(p.is_file() and sha(p)==h for p,h in initial.items())
        report['passed'] = report['passed'] and report['inputs_unchanged']
        report['commands'] = COMMANDS
        # 绑定本轮全部生成文件，包括无扩展名程序、guest镜像和RAW/JPEG样本。
        # 顶层result由父进程绑定，不能包含自身摘要；编译缓存在case目录之外。
        report['artifact_sha256'] = {str(p.relative_to(OUT)): sha(p) for p in OUT.rglob('*')
            if p.is_file() and p != OUT / 'result.json'}
        report['process_tree_job'] = bool(lifetime)
        (OUT / 'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    if not report['passed']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
