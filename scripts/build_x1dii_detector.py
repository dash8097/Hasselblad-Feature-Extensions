#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build the licensed detector and run a synthetic blank-image check."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess


SMOKE = r'''#include "facedetectcnn.h"
#include <cstdio>
#include <vector>
int main() {
    std::vector<unsigned char> image(160 * 120 * 3, 0);
    std::vector<unsigned char> result(FACEDETECTION_RESULT_BUFFER_SIZE, 0);
    int *faces = facedetect_cnn(result.data(), image.data(), 160, 120, 160 * 3);
    if (!faces || faces[0] < 0 || faces[0] > FACEDETECTION_RESULT_MAX_FACES) return 1;
    std::printf("synthetic_image_faces=%d\n", faces[0]);
    return faces[0] == 0 ? 0 : 2;
}
'''


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True, help='Zig executable, tested with 0.13.0')
    parser.add_argument('--build-dir', required=True, type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = args.build_dir.resolve()
    if out == root or out in root.parents:
        parser.error('Specify a dedicated build output directory')
    out.mkdir(parents=True, exist_ok=True)
    report = {'passed': False, 'device_accessed': False,
              'scope': 'licensed detector build and synthetic blank image only',
              'publisher_built': False}
    report_file = out / 'detector-validation.json'

    def save() -> None:
        report_file.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')

    save()  # A failed build never leaves an earlier success report active.
    source = out / 'detector-smoke.cpp'
    source.write_bytes(SMOKE.encode('utf-8'))
    binary = out / ('detector-smoke.exe' if os.name == 'nt' else 'detector-smoke')
    vendor = root / 'X1D2/face-afs/vendor/libfacedetection'
    compiler = shutil.which(args.compiler)
    if compiler is None:
        candidate = Path(args.compiler)
        if candidate.is_file():
            compiler = str(candidate.resolve())
    if compiler is None:
        report['failure_stage'] = 'compiler_not_found'
        save()
        return 1
    command = [compiler, 'c++', '-std=c++11', '-O2', '-I', str(vendor), str(source)]
    command += [str(vendor / name) for name in
                ('facedetectcnn.cpp', 'facedetectcnn-model.cpp', 'facedetectcnn-data.cpp')]
    command += ['-o', str(binary)]
    try:
        subprocess.run(command, check=True, timeout=180)
        report['detector_built'] = True
        result = subprocess.run([str(binary)], check=True, capture_output=True,
                                text=True, timeout=30)
        if result.stdout.strip() != 'synthetic_image_faces=0':
            raise RuntimeError('Unexpected synthetic detector result')
        report.update(passed=True, synthetic_dimensions=[160, 120], synthetic_faces=0)
    except (OSError, subprocess.SubprocessError, RuntimeError) as error:
        report['failure_stage'] = 'build_or_synthetic_call'
        report['failure_type'] = type(error).__name__
        save()
        return 1
    save()
    print('Detector build and synthetic blank-image smoke check passed; publisher not built.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
