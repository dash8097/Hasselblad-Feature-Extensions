"""构建公开通用离线组件；输入自造，不访问相机。"""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import unittest
ROOT = Path(__file__).resolve().parents[1]
PIXEL = ROOT / 'x2d/CodeTests/pixel_shift_rgb'
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True, help='Zig executable')
    parser.add_argument('--build-dir', type=Path, required=True)
    args = parser.parse_args()
    output = args.build_dir.resolve()
    if output == ROOT or output in ROOT.parents or any(
            output == ROOT / p or ROOT / p in output.parents
            for p in ('x1d', 'x2d', 'X1D2', 'scripts', 'docs', 'publication')):
        parser.error('Use a dedicated output directory outside source modules')
    output.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, ZIG_LOCAL_CACHE_DIR=str(output / 'zig-local'),
               ZIG_GLOBAL_CACHE_DIR=str(output / 'zig-global'))
    tests = unittest.defaultTestLoader.discover(str(PIXEL), pattern='test_prototype.py')
    if not unittest.TextTestRunner(verbosity=2).run(tests).wasSuccessful():
        return 1
    tracking = ROOT / 'x2d/subject-tracking'
    cases = [
        ('chunk-fanout', 'cc', [PIXEL / 'check_pixel_chunk_fanout.c'],
         [] if os.name == 'nt' else ['-pthread']),
        ('subject-tracker', 'cc', [tracking / 'subject_tracker.c',
         ROOT / 'x2d/CodeTests/subject_tracking/test_subject_tracker.c'],
         ['-I', str(tracking), '-lm']),
        ('audio-route', 'c++', [ROOT / 'x1d/shutter-effects/CodeTests/audio_route_test.cpp'],
         ['-std=c++11']),
    ]
    for name, driver, sources, flags in cases:
        binary = output / (name + ('.exe' if os.name == 'nt' else ''))
        subprocess.run([args.compiler, driver, '-O2'] + list(map(str, sources)) +
                       flags + ['-o', str(binary)], env=env, check=True, timeout=180)
        subprocess.run([str(binary)], check=True, timeout=60)
    print('PASS: synthetic Python and three host component checks; no device access')
    return 0
if __name__ == '__main__':
    sys.exit(main())
