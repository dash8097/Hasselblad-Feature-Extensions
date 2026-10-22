"""构建仅处理文件的默认预览封装器；不访问相机。"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
out = root / 'x2d/outputs/4.2.0/pixel-shift-rgb'
parser=argparse.ArgumentParser()
parser.add_argument('--output-job')
job=parser.parse_args().output_job
if job:
    assert job.startswith('memory-pack-r') and job[13:].isdigit()
    out=out/job
    out.mkdir(exist_ok=True)
zig = root / '.research-cache/x1d-1.25.0/toolchain/zig-windows-x86_64-0.13.0/zig.exe'
source = Path(__file__).with_name('package_native_preview.c')
(out/'host-temp').mkdir(exist_ok=True)
env = os.environ.copy()
env.update(TEMP=str(out/'host-temp'), TMP=str(out/'host-temp'),
           ZIG_LOCAL_CACHE_DIR=str(out/'zig-local'),
           ZIG_GLOBAL_CACHE_DIR=str(out/'zig-global'))
report = {}
for target, name in [('x86_64-windows-gnu', 'package_native_preview_host.exe'),
                     ('aarch64-linux-musl', 'package_native_preview_arm')]:
    destination = out/name
    subprocess.run([str(zig), 'cc', '-target', target, '-O2', '-Wall', '-Wextra',
                    '-Werror', '-static', str(source), '-o', str(destination)],
                   env=env, check=True)
    report[name] = {'bytes': destination.stat().st_size,
                   'sha256': hashlib.sha256(destination.read_bytes()).hexdigest()}
(out/'native-preview-build.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report))
