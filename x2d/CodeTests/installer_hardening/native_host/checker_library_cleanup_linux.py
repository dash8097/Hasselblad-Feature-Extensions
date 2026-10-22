"""精确候选 ELF 的历史库清理测试；不连接设备，不修改 camera_native。"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
from checker_helper_cleanup_linux import ROOT, R7, HERE, verified, linux
from rebuild_setup_cleanup import library_entries


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('revision')
    ap.add_argument('--manifest', type=Path, required=True)
    args = ap.parse_args()
    assert Path.cwd().resolve() == ROOT
    elf, proof = verified(args.revision)
    entries = library_entries(args.manifest)
    assert len(entries) == 13
    frozen = (R7 / 'native-flow/revisions' / args.revision /
              'private-build/source/native_data_cleanup.cpp').read_text(encoding='utf8')
    for item in entries:
        assert '{"' + item['camera_path'] + '","' + item['sha256'] + '"}' in frozen
    source = HERE / 'checker_library_cleanup_init.c'
    identity = dict(candidate=linux.sha(elf), manifest=linux.sha(args.manifest),
                    fixtures=[item['sha256'] for item in entries],
                    harness=linux.sha(source), runner=linux.sha(Path(__file__)))
    revision = hashlib.sha256(json.dumps(identity, sort_keys=True).encode()).hexdigest()[:20]
    out = R7 / 'native-host/checker-library-cleanup' / revision
    assert not out.exists(), '不覆盖已产生的测试证据'
    out.mkdir(parents=True)
    shutil.copyfile(source, out / source.name)
    paths = '#define LIBRARY_COUNT 13u\nstatic const char *library_paths[]={\n'
    paths += ''.join(json.dumps(item['camera_path']) + ',\n' for item in entries) + '};\n'
    (out / 'library_paths.h').write_text(paths, encoding='utf8')
    environment = dict(os.environ, TEMP=str(out), TMP=str(out),
                       ZIG_LOCAL_CACHE_DIR=str(out / 'zig-local'),
                       ZIG_GLOBAL_CACHE_DIR=str(out / 'zig-global'))
    build = [str(linux.ZIG), 'cc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
             '-target', 'aarch64-linux-musl', '-static', '-s', str(out / source.name),
             '-o', str(out / 'init')]
    built = subprocess.run(build, env=environment, capture_output=True, text=True, timeout=180)
    (out / 'build.txt').write_text(built.stdout + built.stderr, encoding='utf8')
    assert built.returncode == 0, built.stderr
    cpio_entries = [(n, b'', 0o40755) for n in ('dev', 'proc', 'tmp', 'data', 'system', 'fixtures')]
    cpio_entries += [('init', (out / 'init').read_bytes(), 0o100755),
                     ('flow', elf.read_bytes(), 0o100755)]
    cpio_entries += [('fixtures/library-' + str(i), (ROOT / item['artifact']).read_bytes(), 0o100644)
                     for i, item in enumerate(entries)]
    archive = out / 'initramfs.cpio'
    archive.write_bytes(linux.cpio(cpio_entries))
    emulator = linux.DEPS / 'qemu/qemu-system-aarch64.exe'
    kernel = linux.DEPS / 'vmlinuz-virt'
    command = [str(emulator), '-machine', 'virt', '-cpu', 'cortex-a53', '-m', '512',
               '-smp', '2', '-nic', 'none', '-display', 'none', '-monitor', 'none',
               '-serial', 'stdio', '-no-reboot', '-kernel', str(kernel),
               '-initrd', str(archive), '-append', 'console=ttyAMA0 rdinit=/init panic=-1 quiet']
    result = subprocess.run(command, capture_output=True, text=True, timeout=180)
    (out / 'console.txt').write_text(result.stdout + result.stderr, encoding='utf8')
    passed = (result.returncode == 0 and 'LIBRARY_CLEANUP_ALL_PASSED cases=10 libraries=13' in result.stdout
              and 'LIBRARY_CHECK_FAILED' not in result.stdout)
    report = dict(identity=identity, passed=passed, independent_checker=True,
                  candidate_revision=args.revision, candidate_source_sha256=proof['source_sha256'],
                  kernel_sha256=linux.sha(kernel), emulator_sha256=linux.sha(emulator),
                  command=command, returncode=result.returncode, camera_requests=0,
                  network=False, usb=False, host_shared_directory=False,
                  cases=['13精确已知库删除', '13单字节变动保留', '13符号链接及目标保留',
                         '13硬链接保留', '硬链接释放后删除', '13打开FD保留',
                         'FD释放后删除', '13内存映射保留', '映射释放后删除',
                         '正确摘要但未知路径仍保留'],
                  limits=['隔离Linux tmpfs，不能证明相机最终无残留',
                          '不替代最终安装包的GUI卸载安装与拍摄验收'])
    (out / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
    print(result.stdout[-6000:])
    print(out)
    assert passed


if __name__ == '__main__':
    main()
