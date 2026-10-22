"""X2D 4.2.0 离线组件模拟器：只用已缓存依赖，不连接设备或启动原厂硬件服务。"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import struct
import subprocess
import sys
import time

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
SOURCE = ROOT / 'x2d/CodeTests/pixel_shift_rgb'
OUT = ROOT / 'x2d/outputs/4.2.0/firmware-simulator-r1'
DEPS = ROOT / 'x2d/outputs/4.2.0/installer-hardening-r5/deps'
ZIG = ROOT / '.research-cache/x1d-1.25.0/toolchain/zig-windows-x86_64-0.13.0/zig.exe'
PINNED = {
    '/bin/linker64': '9dd62f476163da5568fc079d218c1eeb15789be7d69690191d0ff2be732423bd',
    '/lib64/libc.so': '40e9ea6efccfe8757b4afac57972c9a5cce745224d2dcf34ecddf3dfc0a3f7a6',
    '/lib64/libdl.so': '0d40d030eda384fe7d99441954f82670333cb56eb55cb968098ed589a276b53f',
}
QEMU_HASH = '952f897511177803722efed6fefe79bc3e7e5a51a026112dfc40f9a37fb0dad2'
KERNEL_HASH = '2cede1e9adab630fd69c843a4c9044547fc1d4a489477fd27d9c1330fafd0c6b'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def cpio(entries):
    out = bytearray()
    names = set()
    for inode, (name, data, mode) in enumerate(entries + [('TRAILER!!!', b'', 0)], 1):
        if name.startswith('/') or '..' in name.split('/') or name in names:
            raise ValueError('Invalid or duplicate guest path: ' + name)
        names.add(name)
        encoded = name.encode() + b'\0'
        fields = [inode, mode, 0, 0, 1, 0, len(data), 0, 0, 0, 0, len(encoded), 0]
        out += b'070701' + b''.join(f'{v:08x}'.encode() for v in fields) + encoded
        out += b'\0' * (-len(out) % 4)
        out += data
        out += b'\0' * (-len(out) % 4)
    return out


def reference_400(first, rows, width=11663, height=8749):
    """独立整数参考，不调用生产六帧内核；覆盖帧号、坐标、CFA、偶数舍入和边缘。"""
    cfa = ((0, 1), (3, 2))

    def sample(frame, x, y):
        return (x * 251 + y * 17 + frame * 10007) & 65535

    def integer(x, y, channel):
        for frame, (dx, dy) in enumerate(((0, 0), (0, 1), (1, 0), (1, 1))):
            sy = y + 1 - dy
            if cfa[sy % 2][(x + dx) % 2] == channel:
                return sample(frame, 124 + x + dx, 92 + sy)
        raise AssertionError('Missing integer channel')

    def half(x, y, channel):
        for frame in (4, 5):
            sy = y + (frame == 4)
            got = cfa[sy % 2][(x + 1) % 2]
            if (1 if got == 3 else got) == channel:
                return sample(frame, 125 + x, 92 + sy)
        return None

    def even_mean(values):
        q, r = divmod(sum(values), len(values))
        return q + (r * 2 > len(values) or (r * 2 == len(values) and q % 2 == 1))

    output = bytearray()
    for y in range(first, first + rows):
        top, bottom = [], []
        for x in range(width):
            red = [integer(x, y, 0), integer(min(x + 1, width - 1), y, 0)]
            red += [v for v in (half(x, max(y - 1, 0), 0), half(x, y, 0)) if v is not None]
            blue = [integer(x, y, 2), integer(x, min(y + 1, height - 1), 2)]
            blue += [v for v in (half(x, y, 2), half(max(x - 1, 0), y, 2)) if v is not None]
            top.extend(((integer(x, y, 1) + integer(x, y, 3) + 1) // 2, even_mean(red)))
            bottom.extend((even_mean(blue), half(x, y, 1)))
        output.extend(struct.pack('<' + 'H' * (4 * width), *(top + bottom)))
    return bytes(output)


def source_snapshot(dst):
    """递归冻结本次编译输入，后续不从活动源码读取 include。"""
    pending = [HERE / n for n in ('entry.c', 'guest_init.c', 'scenarios.c', 'assert_abi.c')]
    pending += [SOURCE / n for n in ('capture_memory_merge.c', 'private_capture_group.c', 'raw_stack_core.c',
                                    'check_sixteen_source_edge.c', 'check_quarter_direct_jpeg.c',
                                    'quarter_direct_jpeg.c', 'quarter_single_core.c')]
    inputs, contents = {}, {}
    while pending:
        path = pending.pop().resolve()
        if str(path.relative_to(ROOT)) in inputs:
            continue
        data = path.read_bytes()
        if path.name in contents and contents[path.name] != data:
            raise ValueError('Conflicting include basename: ' + path.name)
        contents[path.name] = data
        inputs[str(path.relative_to(ROOT))] = sha(data)
        (dst / path.name).write_bytes(data)
        for include in re.findall(rb'^\s*#\s*include\s+"([^"\r\n]+)"', data, re.M):
            name = include.decode()
            target = path.parent / name
            if not target.is_file():
                target = SOURCE / name
            if not target.resolve().is_relative_to(ROOT / 'x2d'):
                raise ValueError('Source outside X2D')
            pending.append(target)
    inputs[str(Path(__file__).relative_to(ROOT))] = sha(Path(__file__).read_bytes())
    return inputs


def command(args, env, timeout=180):
    p = subprocess.run([str(a) for a in args], env=env, capture_output=True, text=True,
                       encoding='utf-8', errors='replace', timeout=timeout,
                       creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
    if p.returncode:
        raise RuntimeError(p.stdout + p.stderr)
    return p


def build():
    if Path.cwd().resolve() != ROOT:
        raise RuntimeError('Run from the current Hasselblad workspace')
    import tempfile
    OUT.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix='attempt-', dir=OUT))
    (OUT / 'latest-attempt.json').write_text(json.dumps(dict(path=str(run.relative_to(ROOT)), passed=False,
                                                           stage='preflight'), indent=2), encoding='utf-8')
    try:
        return build_attempt(run)
    except BaseException as error:
        (run / 'report.json').write_text(json.dumps(dict(passed=False, stage='build',
            error=str(error), all_features_passed=False, real_camera=False), indent=2), encoding='utf-8')
        raise


def build_attempt(run):
    qemu, kernel = DEPS / 'qemu/qemu-system-aarch64.exe', DEPS / 'vmlinuz-virt'
    for p, digest in ((qemu, QEMU_HASH), (kernel, KERNEL_HASH)):
        if sha(p.read_bytes()) != digest:
            raise ValueError('Offline dependency hash mismatch: ' + str(p))
    sys.path.insert(0, str(ROOT / 'x2d/tools'))
    from firmware_image import system_file, FirmwareElf
    firmware = {path: system_file(path) for path in PINNED}
    for path, data in firmware.items():
        if sha(data) != PINNED[path]:
            raise ValueError('Firmware profile mismatch: ' + path)
    # 每次运行独立目录，旧成功报告不能掩盖新失败。
    (OUT / 'latest-attempt.json').write_text(json.dumps(dict(path=str(run.relative_to(ROOT)), passed=False,
                                                           stage='building'), indent=2), encoding='utf-8')
    src = run / 'source'; src.mkdir()
    inputs = source_snapshot(src)
    for path, data in firmware.items():
        (run / Path(path).name).write_bytes(data)
    env = dict(os.environ, TEMP=str(run), TMP=str(run), TMPDIR=str(run),
               ZIG_LOCAL_CACHE_DIR=str(OUT / 'zig-local'), ZIG_GLOBAL_CACHE_DIR=str(OUT / 'zig-global'))
    objects = {}
    programs = {
        'sim': ['entry.c', 'scenarios.c', 'capture_memory_merge.c', 'private_capture_group.c', 'raw_stack_core.c'],
        'sixteen-edge': ['entry.c', 'assert_abi.c', 'check_sixteen_source_edge.c'],
        'quarter-pixels': ['entry.c', 'assert_abi.c', 'check_quarter_direct_jpeg.c', 'quarter_direct_jpeg.c', 'quarter_single_core.c'],
    }
    for name in sorted(set(sum(programs.values(), []))):
        target = run / (name + '.o')
        command([ZIG, 'cc', '-target', 'aarch64-linux-gnu', '-std=gnu11', '-O2', '-Wall', '-Wextra', '-Werror',
                 '-Wno-unused-function', '-fPIE', '-fno-builtin', '-fno-stack-protector', '-mno-outline-atomics',
                 '-D__errno_location=__errno', '-D__assert_fail=sim_assert_fail', '-D__isoc99_sscanf=sscanf',
                 '-I', src, '-c', src / name, '-o', target], env)
        objects[name] = target
    exports = set()
    for path in ('/lib64/libc.so', '/lib64/libdl.so'):
        exports.update(s.name for s in FirmwareElf(firmware[path]).elf.get_section_by_name('.dynsym').iter_symbols()
                       if s['st_shndx'] != 'SHN_UNDEF')
    binaries = {}
    for name, names in programs.items():
        binary = run / name
        command([ZIG, 'ld.lld', '-m', 'aarch64elf', '-pie', '--no-undefined', '--allow-shlib-undefined',
                 '-z', 'noexecstack', '-e', '_start', '--dynamic-linker=/system/bin/linker64',
                 *[objects[n] for n in names], run / 'libc.so', run / 'libdl.so', '-o', binary], env)
        elf = FirmwareElf(binary.read_bytes()).elf
        if [p.data() for p in elf.iter_segments() if p['p_type'] == 'PT_INTERP'] != [b'/system/bin/linker64\0']:
            raise ValueError('Unexpected interpreter')
        imports = {s.name for s in elf.get_section_by_name('.dynsym').iter_symbols() if s.name and s['st_shndx'] == 'SHN_UNDEF'}
        if not imports <= exports:
            raise ValueError('Imports not supplied by pinned firmware: ' + str(imports - exports))
        binaries[name] = dict(sha256=sha(binary.read_bytes()), imports=sorted(imports))
    init = run / 'init'
    command([ZIG, 'cc', '-target', 'aarch64-linux-musl', '-static', '-O2', '-Wall', '-Wextra', '-Werror',
             src / 'guest_init.c', '-o', init], env)
    directories = ['dev', 'proc', 'tmp', 'system', 'system/bin', 'system/lib64', 'fixtures']
    entries = [(name, b'', 0o40755) for name in directories]
    entries += [('system' + path, data, 0o100755) for path, data in firmware.items()]
    entries += [(name, (run / name).read_bytes(), 0o100755) for name in programs]
    entries += [('init', init.read_bytes(), 0o100755)]
    fixtures = {}
    for i, (first, rows) in enumerate(((0, 3), (1017, 17), (8746, 3))):
        data = reference_400(first, rows)
        name = f'fixtures/400-{i}.bin'
        entries.append((name, data, 0o100444))
        fixtures[name] = dict(sha256=sha(data), bytes=len(data), first_source_row=first, source_rows=rows)
    initrd = run / 'initramfs.cpio'; initrd.write_bytes(cpio(entries))
    args = [str(qemu), '-machine', 'virt', '-cpu', 'cortex-a53', '-m', '512', '-smp', '2',
            '-nodefaults', '-nic', 'none', '-display', 'none', '-monitor', 'none', '-serial', 'stdio',
            '-no-reboot', '-kernel', str(kernel), '-initrd', str(initrd),
            '-append', 'console=ttyAMA0 rdinit=/init panic=-1 quiet']
    manifest = dict(firmware='X2D 100C 4.2.0', source_sha256=inputs,
                    firmware_files=PINNED, qemu_sha256=QEMU_HASH, kernel_sha256=KERNEL_HASH,
                    compiler_sha256=sha(ZIG.read_bytes()), fixtures=fixtures,
                    executables=binaries, init_sha256=sha(init.read_bytes()),
                    initramfs_sha256=sha(initrd.read_bytes()), command=args,
                    network=False, usb=False, host_shared_directory=False, host_disk_passthrough=False,
                    real_camera=False, full_firmware_boot=False, actual_sensor_timing=False,
                    production_sources_modified=False, installed=False)
    (run / 'build.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
    return run, manifest, env


def execute():
    run, manifest, env = build()
    start = time.monotonic(); expired = False
    try:
        p = subprocess.run(manifest['command'], env=env, capture_output=True, text=True,
                           encoding='utf-8', errors='replace', timeout=300,
                           creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        output, rc = p.stdout + p.stderr, p.returncode
    except subprocess.TimeoutExpired as e:
        raw = e.stdout or b''
        output = raw.decode('utf-8', 'replace') if isinstance(raw, bytes) else raw
        rc, expired = None, True
    (run / 'console.txt').write_text(output, encoding='utf-8')
    lines = re.findall(r'^X2D_SIM_CASE name=(\S+) rc=(\d+) timeout=(\d+) expected=(-?\d+)\s*$', output, re.M)
    expected = [('runtime','0','0','0'), ('400mp','0','0','0'), ('1600mp-source-edge','0','0','0'),
                ('25mp-pixels','0','0','0'), ('stack','0','0','0'),
                ('unknown','64','0','64'), ('expected-failure','19','0','19'),
                ('expected-crash','139','0','139'), ('expected-timeout','137','1','-1'),
                ('missing-runtime','1','0','1'), ('runtime','0','0','0')]
    passed = rc == 0 and not expired and lines == expected and 'X2D_SIM_ALL_PASSED' in output and 'X2D_SIM_ASSERT' not in output
    report = dict(manifest, passed=passed, elapsed_host_seconds=time.monotonic()-start,
                  passed_scope='implemented_component_scenarios', all_features_passed=False,
                  elapsed_is_camera_performance=False, returncode=rc, host_timeout=expired, cases=lines,
                  limitations=['通用 Linux 内核，不是相机内核或整机开机',
                               '四亿为全幅均匀色彩图样逐样本及空间变化23行验证；窗口输入不模拟六帧整幅占用，不是成片显影',
                               'RAW堆栈5种组合用19样本10003帧，65535帧边界为1样本，不是全幅连拍',
                               '十六亿只验证生产底边读取/CFA延拓；25MP为整幅自造NV16缩小及描述符，不含硬件编码',
                               '没有执行原厂ISP/硬件JPEG、DMA、Qt拍摄服务、镜头、传感器或相册登记'])
    (run / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    (OUT / 'latest-attempt.json').write_text(json.dumps(dict(path=str(run.relative_to(ROOT)), passed=passed), indent=2), encoding='utf-8')
    print(output[-14000:])
    print('REPORT=' + str(run / 'report.json'))
    if not passed:
        raise SystemExit(1)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=['build', 'run', 'coverage'])
    opt = parser.parse_args()
    if opt.mode == 'run':
        execute()
    elif opt.mode == 'build':
        print(build()[0])
    else:
        print((HERE / 'coverage.json').read_text(encoding='utf-8'))
