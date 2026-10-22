"""将已验证的完整 Windows ZIP 封入唯一交付 EXE；不运行相机操作。"""
import argparse
import hashlib
from pathlib import Path
import os
import subprocess
import zipfile

from assemble_native_client import CSC, HERE, R7, ROOT, encoded, sha


def build(archive):
    assert Path.cwd().resolve() == ROOT
    archive = archive.resolve()
    assert archive.is_relative_to(R7) and archive.suffix == '.zip'
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
        names = z.namelist()
        packages = {name.split('/')[0] for name in names}
        assert len(packages) == 1 and not any(name.endswith('/') for name in names)
        package = packages.pop()
        assert all(c.isalnum() or c in '-_' for c in package)
    template = HERE / 'single_exe_launcher.cs'
    identity = {'zip_sha256': sha(archive), 'launcher_source': sha(template),
                'builder': sha(Path(__file__))}
    revision = hashlib.sha256(encoded(identity)).hexdigest()[:20]
    out = R7 / 'native-host/single-exe' / revision
    assert not out.exists(), '保留旧交付和失败现场'
    private = out / 'private-build'
    private.mkdir(parents=True)
    delivery = out / 'delivery'
    delivery.mkdir()
    source = template.read_text('utf8').replace('__PAYLOAD_SHA256__', identity['zip_sha256'])
    source = source.replace('__PACKAGE_NAME__', package).replace('__FILE_COUNT__', str(len(names)))
    (private / 'SingleInstaller.cs').write_text(source, encoding='utf-8-sig')
    exe = delivery / '哈苏特调补丁-X2D.exe'
    result = subprocess.run([str(CSC), '/nologo', '/target:winexe', '/platform:x64', '/optimize+',
                             '/reference:System.Windows.Forms.dll', '/reference:System.IO.Compression.dll',
                             '/out:' + str(exe), '/resource:' + str(archive) + ',X2D.Package',
                             str(private / 'SingleInstaller.cs')],
                            env=dict(os.environ, TEMP=str(private), TMP=str(private)),
                            capture_output=True, text=True, encoding='utf8', errors='replace', timeout=120)
    (private / 'compiler.log').write_text(result.stdout + result.stderr, encoding='utf8')
    assert result.returncode == 0, result.stdout + result.stderr
    (out / 'report.json').write_bytes(encoded({
        'identity': identity, 'files': {exe.relative_to(out).as_posix(): sha(exe)},
        'bytes': exe.stat().st_size, 'camera_requests': 0, 'hardware_accepted': False,
        'delivery_files': 1, 'embedded_package_files': len(names),
        'limits': ['单文件已构建；需运行最终 EXE 前台检查；本次未执行相机安装。'],
    }))
    print(out)
    return out


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('archive', type=Path)
    build(parser.parse_args().archive)
