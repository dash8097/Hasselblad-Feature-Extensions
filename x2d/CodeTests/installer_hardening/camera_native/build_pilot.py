"""构建两个机内原生叶节点的离线样例；不修改 H3、打发行包或连接设备。"""
from pathlib import Path
import hashlib
import json
import os
import shutil
import struct
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
OUT = ROOT / 'x2d/outputs/4.2.0/installer-hardening-r4/native-pilot'
BASE = ROOT / 'x2d/outputs/4.2.0/installer-hardening-r3/camera-iso-candidate-v4/public-install'
ZIG = ROOT / '.research-cache/x1d-1.25.0/toolchain/zig-windows-x86_64-0.13.0/zig.exe'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def elf_check(path, *, fixed_cleanup_shell=False):
    data = path.read_bytes()
    assert data[:6] == b'\x7fELF\x02\x01'
    assert struct.unpack_from('<H', data, 18)[0] == 183, '必须 ARM64'
    phoff = struct.unpack_from('<Q', data, 32)[0]
    phsize, phnum = struct.unpack_from('<HH', data, 54)
    assert all(struct.unpack_from('<I', data, phoff+i*phsize)[0] not in (2, 3)
               for i in range(phnum)), '样例必须静态链接，无动态解释器'
    shoff = struct.unpack_from('<Q', data, 40)[0]
    shsize, shnum = struct.unpack_from('<HH', data, 58)
    assert all(struct.unpack_from('<I', data, shoff+i*shsize+4)[0] != 2
               for i in range(shnum)), '不能携带完整符号表'
    forbidden = [b'#!/', b'set -eu', b'CN_FIXTURE']
    # 带起止 NUL 边界，避免 /bin/sh 被误识别为 /system/bin/sh 的后缀。
    assert b'\0/bin/sh\0' not in data, '不允许独立 /bin/sh'
    if not fixed_cleanup_shell:
        forbidden.append(b'/system/bin/sh\0')
    for token in forbidden:
        assert token not in data, ('不应出现的样例标记', token)
    return {'machine': 'AArch64', 'static': True, 'symbol_table_stripped': True,
            'bytes': len(data), 'sha256': sha(path),
            'fixed_cleanup_shell_dependency': fixed_cleanup_shell}


def main():
    assert Path.cwd().resolve() == ROOT
    OUT.mkdir(parents=True, exist_ok=True)
    proof = OUT / 'build-proof.json'
    proof.unlink(missing_ok=True)
    source = OUT / 'private-build/source'
    source.mkdir(parents=True, exist_ok=True)
    names = ['camera_native.h', 'camera_native.c', 'camera_posix.c']
    # 固定每次编译输入，失败时不沿用旧成功证明。
    hashes = {name: sha(HERE/name) for name in names}
    for name in names:
        shutil.copyfile(HERE/name, source/name)
        assert sha(source/name) == hashes[name]
    build = OUT / 'private-build'
    env = dict(os.environ, TEMP=str(build), TMP=str(build),
               ZIG_LOCAL_CACHE_DIR=str(build/'zig-local'), ZIG_GLOBAL_CACHE_DIR=str(build/'zig-global'))

    def run(argv):
        result = subprocess.run([str(a) for a in argv], env=env, capture_output=True,
                                encoding='utf-8', errors='replace', timeout=180)
        if result.returncode:
            raise RuntimeError(result.stdout+result.stderr)
        return result.stdout

    artifact = OUT / 'artifacts/x2d-native-flow'
    artifact.parent.mkdir(exist_ok=True)
    flags = ['cc', '-std=c11', '-Oz', '-Wall', '-Wextra', '-Werror', '-fvisibility=hidden',
             '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections', '-s']
    run([ZIG, *flags, '-target', 'aarch64-linux-musl', '-static',
         source/'camera_native.c', source/'camera_posix.c', '-o', artifact])
    info = elf_check(artifact)
    tests = None
    check = HERE/'checker_core.c'
    if check.exists():
        shutil.copyfile(check, source/check.name)
        hashes[check.name] = sha(source/check.name)
        exe = build/'check-native-core.exe'
        output = run([ZIG, 'cc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                      source/'camera_native.c', source/check.name, '-o', exe])
        output = run([exe])
        (OUT/'core-test-output.txt').write_text(output, encoding='utf-8')
        tests = {'host': 'Windows', 'kind': '实际同一 C 核心，系统适配器替身',
                 'binary_sha256': sha(exe), 'output': output.strip()}
    references = ['p79-convert.sh', 'p93-x2d-shutter-audio.sh']
    report = {'offline': True, 'camera_requests': 0, 'release_ready': False,
              'firmware_baseline': 'X2D 100C 4.2.0', 'source_sha256': hashes,
              'baseline_sha256': {n: sha(BASE/n) for n in references},
              'artifact': info, 'core_tests': tests, 'arm64_executed': False,
              'posix_adapter_executed': False, 'installed_or_packaged': False,
              'scope': ['音效服务启动', '无原厂服务重启的磁盘 HEIF 转换'],
              'remaining': ['安装/卸载事务及其他 shell 尚未迁移', 'QML/JS 尚未整体迁移',
                            '调用方、安装清单及卸载清单尚未接入', 'POSIX 适配器须 Linux 离线验证',
                            '目标机兼容性及资源/信号行为未验证', '未证明逆向成本高于重新实现']}
    proof.write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({'artifact': info, 'core_tests_run': tests is not None,
                      'release_ready': False}, ensure_ascii=False))


if __name__ == '__main__':
    main()
