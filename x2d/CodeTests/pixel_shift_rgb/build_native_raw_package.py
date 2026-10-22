"""构建原厂数值域 3FR 机内封装器，不访问设备。"""
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT/'x2d/outputs/4.2.0/pixel-shift-rgb'
HERE = Path(__file__).resolve().parent


def main():
    env = dict(os.environ, TEMP=str(OUT/'host-temp'), TMP=str(OUT/'host-temp'),
               ZIG_LOCAL_CACHE_DIR=str(OUT/'zig-local'), ZIG_GLOBAL_CACHE_DIR=str(OUT/'zig-global'))
    zig = ROOT/'.research-cache/x1d-1.25.0/toolchain/zig-windows-x86_64-0.13.0/zig.exe'
    report = {}
    for arch, name, source in [('x86_64-windows-gnu', 'package_native_raw_host.exe', 'package_native_raw.c'),
                               ('aarch64-linux-musl', 'package_native_raw_arm', 'package_native_raw.c'),
                               ('x86_64-windows-gnu', 'native_raw_layout_host.exe', 'native_raw_layout.c'),
                               ('aarch64-linux-musl', 'native_raw_layout_arm', 'native_raw_layout.c')]:
        target = OUT/name
        subprocess.run([str(zig), 'cc', '-target', arch, '-O2', '-Wall', '-Wextra', '-Werror',
                        '-static', str(HERE/source), '-o', str(target)], env=env, check=True)
        report[name] = dict(bytes=target.stat().st_size,
                            sha256=hashlib.sha256(target.read_bytes()).hexdigest())
    (OUT/'native-raw-package-build.json').write_text(json.dumps(report, indent=2)+'\n')
    print('NATIVE_RAW_PRIVATE_PACKAGER_HOST_AND_ARM_BUILT_NOT_INSTALLED')


if __name__ == '__main__':
    main()
