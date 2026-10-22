"""将已冻结双平台目录封成离线候选 ZIP；保留 Mac 启动器执行权限。"""
import argparse, hashlib, json, stat, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
R7 = ROOT / 'x2d/outputs/4.2.0/installer-hardening-r7'

def sha(data):
    return hashlib.sha256(data).hexdigest()

def main():
    assert Path.cwd().resolve() == ROOT
    parser = argparse.ArgumentParser()
    parser.add_argument('--dual-build', type=Path, required=True)
    parser.add_argument('--payload', type=Path, required=True)
    args = parser.parse_args()
    dual = args.dual_build.resolve()
    assert dual.is_relative_to(R7 / 'native-host/dual-build')
    raw = (dual / 'report.json').read_bytes()
    report = json.loads(raw)
    assert not report['errors'] and set(report['results']) == {'windows', 'mac'}
    payload = args.payload.resolve()
    assert payload.is_relative_to(R7 / 'native-payload/revisions')
    payload_bytes = (payload / 'report.json').read_bytes()
    assert sha(payload_bytes) == report['identity']['payload']
    payload_proof = json.loads(payload_bytes)
    contract = payload_proof['native_ui_contract']
    assert contract['migration_complete'] is True and contract['eye_protocol'] == 2
    assert payload_proof['scriptless_structural_pass'] is True
    for name, digest in payload_proof['files'].items():
        target = (payload / name).resolve()
        assert target.is_relative_to(payload) and sha(target.read_bytes()) == digest, name
    projected_contract = {k: contract[k] for k in ('schema', 'module_path', 'module_sha256', 'gui_sha256', 'required_capabilities', 'eye_protocol')}
    inputs = {}
    for platform, row in report['results'].items():
        source = Path(row['assembled']).resolve()
        assert source.is_relative_to(R7)
        proof_bytes = (source / 'report.json').read_bytes()
        proof = json.loads(proof_bytes)
        expected = proof['files']
        actual = {p.relative_to(source).as_posix(): p for p in (source / 'candidate').rglob('*') if p.is_file()}
        assert set(actual) == set(expected)
        for name, path in actual.items():
            assert not path.is_symlink() and sha(path.read_bytes()) == expected[name], name
        assert proof['native_ui_contract'] == projected_contract
        inputs[platform] = (source, proof_bytes, proof)
    identity = {'builder': sha(Path(__file__).read_bytes()), 'dual_build_report': sha(raw),
                'assembled_reports': {k: sha(v[1]) for k, v in inputs.items()}}
    revision = sha(json.dumps(identity, sort_keys=True).encode())[:20]
    out = R7 / 'native-host/delivery-candidates' / revision
    out.mkdir(parents=True, exist_ok=False)
    archives = {}
    for platform, (source, proof_bytes, proof) in inputs.items():
        filename = 'X2D-' + ('Windows' if platform == 'windows' else 'macOS') + '-R7-Native-Candidate.zip'
        path = out / filename
        entries = {}
        mac_executables = {'哈苏特调补丁.app/Contents/MacOS/X2DInstaller',
                           '哈苏特调补丁.app/Contents/Resources/runtime/arm64/bin/python3.12',
                           '哈苏特调补丁.app/Contents/Resources/runtime/x86_64/bin/python3.12'}
        if platform == 'mac':
            assert mac_executables <= {Path(n).relative_to('candidate').as_posix() for n in proof['files']}
        with zipfile.ZipFile(path, 'x', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as z:
            for name, digest in sorted(proof['files'].items()):
                original = source / name
                relative = Path(name).relative_to('candidate').as_posix()
                data = original.read_bytes()
                assert sha(data) == digest, name
                executable = platform == 'mac' and relative in mac_executables
                mode = 0o755 if executable else 0o644
                item = zipfile.ZipInfo(relative, (2026, 1, 1, 0, 0, 0))
                item.create_system = 3
                item.external_attr = (stat.S_IFREG | mode) << 16
                item.compress_type = zipfile.ZIP_DEFLATED
                z.writestr(item, data)
                entries[relative] = {'sha256': digest, 'mode': mode}
        with zipfile.ZipFile(path) as z:
            assert z.testzip() is None
            assert len(z.infolist()) == len(entries) and set(z.namelist()) == set(entries)
            for item in z.infolist():
                expected = entries[item.filename]
                assert sha(z.read(item)) == expected['sha256']
                assert stat.S_IMODE(item.external_attr >> 16) == expected['mode']
        assert (source / 'report.json').read_bytes() == proof_bytes
        archives[platform] = {'path': str(path.relative_to(ROOT)), 'sha256': sha(path.read_bytes()),
                              'bytes': path.stat().st_size, 'entries': entries}
    assert (dual / 'report.json').read_bytes() == raw
    assert (payload / 'report.json').read_bytes() == payload_bytes
    result = {'identity': identity, 'archives': archives, 'release_ready': False,
              'camera_operations': 0, 'published': False,
              'qualification': '仅离线候选封装；最终同哈希独立检查、Mac原生运行及实机验证另行记录。'}
    (out / 'report.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
    print(json.dumps({'directory': str(out), 'archives': {k: {n: v[n] for n in ('path', 'sha256', 'bytes')} for k, v in archives.items()}}, ensure_ascii=False))

if __name__ == '__main__':
    main()
