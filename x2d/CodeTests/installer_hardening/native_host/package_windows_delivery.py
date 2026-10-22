"""从已组装产物封装单个 Windows ZIP，再完整解压；不连接相机。"""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

from assemble_native_client import R7, ROOT, encoded, read_proof, sha


def build(assembled):
    assert Path.cwd().resolve() == ROOT
    assembled = assembled.resolve()
    assert assembled.is_relative_to(R7 / 'native-host/assembled')
    proof = read_proof(assembled)
    assert proof['identity']['platform'] == 'windows'
    assert [p.name for p in (assembled/'candidate').rglob('*.exe')] == ['HasselbladTuningPatchInstaller.exe']
    identity = {'assembled_report': sha(assembled / 'report.json'),
                'packager': sha(Path(__file__))}
    revision = hashlib.sha256(encoded(identity)).hexdigest()[:20]
    out = R7 / 'native-host/delivery-candidates' / revision
    assert not out.exists(), '保留既有交付与失败现场'
    out.mkdir(parents=True)
    name = 'X2D-Windows-R7-PackageFix'
    archive = out / (name + '.zip')
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for p in sorted((assembled / 'candidate').rglob('*')):
            assert not p.is_symlink()
            if p.is_file():
                info = zipfile.ZipInfo(name + '/' + p.relative_to(assembled / 'candidate').as_posix(),
                                       date_time=(2026, 1, 1, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                z.writestr(info, p.read_bytes())
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
        for item in z.infolist():
            target = (out / item.filename).resolve()
            assert target.is_relative_to(out.resolve())
        z.extractall(out)
    package = out / name
    files = {p.relative_to(package).as_posix(): sha(p) for p in package.rglob('*') if p.is_file()}
    assert files == {k.removeprefix('candidate/'): v for k, v in proof['files'].items()}
    report = {'identity': identity, 'zip_sha256': sha(archive), 'files': files,
              'package': name, 'camera_requests': 0, 'hardware_accepted': False,
              'limits': ['已封包并回读；须继续从本次解压包做前台检查和真实安装验收']}
    (out / 'report.json').write_bytes(encoded(report))
    print(out)
    return out


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('assembled', type=Path)
    build(parser.parse_args().assembled)
