"""执行精确候选 ELF 的 helper 清理检查。仅本地隔离 QEMU，不连接相机。"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
R7 = ROOT / 'x2d/outputs/4.2.0/installer-hardening-r7'
sys.path.insert(0, str(HERE.parent / 'camera_native'))
import checker_linux_env as linux


def verified(revision):
    assert len(revision) == 20 and set(revision) <= set('0123456789abcdef')
    directory = R7 / 'native-flow/revisions' / revision
    proof = json.loads((directory / 'build-proof.json').read_text(encoding='utf8'))
    elf = directory / 'artifacts/x2d-native-flow'
    assert proof['build_identity']['profile'] == 'setup'
    assert linux.sha(elf) == proof['artifact']['sha256']
    for name, digest in proof['source_sha256'].items():
        assert linux.sha(directory / 'private-build/source' / name) == digest
    return elf, proof


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('revision')
    args = ap.parse_args()
    assert Path.cwd().resolve() == ROOT
    elf, proof = verified(args.revision)
    old = [verified(r)[0] for r in ('8495b2ab08214e139e70', '8afaa09e169d891574aa')]
    source = HERE / 'checker_helper_cleanup_init.c'
    identity = dict(candidate=linux.sha(elf), fixtures=[linux.sha(p) for p in old],
                    harness=linux.sha(source), runner=linux.sha(Path(__file__)))
    revision = hashlib.sha256(json.dumps(identity, sort_keys=True).encode()).hexdigest()[:20]
    out = R7 / 'native-host/checker-helper-cleanup' / revision
    assert not out.exists(), '不覆盖已有测试证据'
    out.mkdir(parents=True)
    shutil.copyfile(source, out / source.name)
    environment = dict(os.environ, TEMP=str(out), TMP=str(out),
                       ZIG_LOCAL_CACHE_DIR=str(out / 'zig-local'),
                       ZIG_GLOBAL_CACHE_DIR=str(out / 'zig-global'))
    build = [str(linux.ZIG), 'cc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
             '-target', 'aarch64-linux-musl', '-static', '-s', str(out / source.name),
             '-o', str(out / 'init')]
    built = subprocess.run(build, env=environment, capture_output=True, text=True, timeout=180)
    (out / 'build.txt').write_text(built.stdout + built.stderr, encoding='utf8')
    assert built.returncode == 0, built.stderr
    entries = [(n, b'', 0o40755) for n in ('dev', 'proc', 'tmp', 'data', 'system', 'fixtures')]
    entries += [('init', (out / 'init').read_bytes(), 0o100755),
                ('flow', elf.read_bytes(), 0o100755)]
    entries += [('fixtures/old-' + str(i), p.read_bytes(), 0o100755) for i, p in enumerate(old)]
    archive = out / 'initramfs.cpio'
    archive.write_bytes(linux.cpio(entries))
    emulator = linux.DEPS / 'qemu/qemu-system-aarch64.exe'
    kernel = linux.DEPS / 'vmlinuz-virt'
    command = [str(emulator), '-machine', 'virt', '-cpu', 'cortex-a53', '-m', '512',
               '-smp', '2', '-nic', 'none', '-display', 'none', '-monitor', 'none',
               '-serial', 'stdio', '-no-reboot', '-kernel', str(kernel),
               '-initrd', str(archive), '-append', 'console=ttyAMA0 rdinit=/init panic=-1 quiet']
    result = subprocess.run(command, capture_output=True, text=True, timeout=180)
    (out / 'console.txt').write_text(result.stdout + result.stderr, encoding='utf8')
    passed = (result.returncode == 0 and 'HELPER_CLEANUP_ALL_PASSED cases=9' in result.stdout
              and 'HELPER_CHECK_FAILED' not in result.stdout)
    report = dict(identity=identity, passed=passed, independent_checker=True,
                  candidate_revision=args.revision, candidate_source_sha256=proof['source_sha256'],
                  kernel_sha256=linux.sha(kernel), emulator_sha256=linux.sha(emulator),
                  command=command, returncode=result.returncode, camera_requests=0,
                  network=False, usb=False, host_shared_directory=False,
                  limits=['隔离Linux tmpfs，不证明相机全部文件系统和权限状态',
                          '仅执行新增helper清理路径，不替代完整GUI卸载安装验收'])
    (out / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
    print(result.stdout[-6000:])
    print(out)
    assert passed


if __name__ == '__main__':
    main()
