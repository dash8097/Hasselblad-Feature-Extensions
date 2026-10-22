"""绑定已验证旧 setup，补齐清理表；不改拍摄载荷，不访问相机。"""
import argparse
import contextlib
import hashlib
import io
import json
from pathlib import Path
import re
import sys

from assemble_native_client import HERE, ROOT, R7, encoded, sha


def verified_setup(path):
    path = path.resolve()
    assert path.is_relative_to(R7/'native-flow/revisions')
    proof = json.loads((path/'build-proof.json').read_text(encoding='utf8'))
    assert proof['build_identity']['profile'] == 'setup'
    assert sha(path/'artifacts/x2d-native-flow') == proof['artifact']['sha256']
    for name, digest in proof['source_sha256'].items():
        assert sha(path/'private-build/source'/name) == digest
    return proof


def library_entries(manifest):
    """仅允许有本地实物及独立证据摘要的、明确列出的系统私有库。"""
    if manifest is None:
        return []
    manifest = manifest.resolve()
    assert manifest.is_relative_to(R7)
    entries = json.loads(manifest.read_text(encoding='utf8'))['files']
    seen = set()
    for item in entries:
        path, digest = item['camera_path'], item['sha256']
        assert re.fullmatch(r'/system/lib64/libx2d_[A-Za-z0-9_.-]+', path)
        assert '..' not in path and path not in seen
        seen.add(path)
        assert re.fullmatch('[0-9a-f]{64}', digest)
        artifact = (ROOT / item['artifact']).resolve()
        evidence = (ROOT / item['evidence']).resolve()
        assert artifact.is_relative_to(ROOT) and evidence.is_relative_to(ROOT)
        assert not artifact.is_symlink() and artifact.is_file() and sha(artifact) == digest
        assert not evidence.is_symlink() and evidence.is_file()
        assert sha(evidence) == item['evidence_sha256']
        assert digest in evidence.read_text(encoding='utf8')
    return sorted(entries, key=lambda item:item['camera_path'])


def patch_libraries(source, entries):
    marker = 'static const LooseFile old_libraries[]={'
    assert source.count(marker) == 1
    additions = []
    for item in entries:
        path, digest = item['camera_path'], item['sha256']
        if '"' + path + '"' in source:
            assert '{"'+path+'","'+digest+'"}' in source
            continue
        additions.append('            {"'+path+'","'+digest+'"},\n')
    return source.replace(marker, marker+'\n'+''.join(additions))


def build(base, previous, libraries=None):
    assert Path.cwd().resolve() == ROOT
    base = base.resolve()
    proof = verified_setup(base)
    prior = {str(p.resolve()): verified_setup(p)['artifact']['sha256'] for p in previous}
    assert prior
    entries = library_entries(libraries)
    identity = dict(base=sha(base/'build-proof.json'), previous=prior,
                    libraries=entries, builder=sha(Path(__file__)))
    rev = hashlib.sha256(encoded(identity)).hexdigest()[:20]
    work = R7/'native-host/setup-cleanup-inputs'/rev
    work.mkdir(parents=True, exist_ok=True)
    header = (base/'private-build/source/legacy_cleanup_stages.hpp').read_text(encoding='utf8')
    marker = 'static const char*legacy_helper_hashes[]={'
    assert header.count(marker) == 1
    additions = [h for h in sorted(set(prior.values())) if '"'+h+'"' not in header]
    header = header.replace(marker, marker + '\n' + ''.join('"'+h+'",\n' for h in additions))
    (work/'legacy_cleanup_stages.hpp').write_text(header, encoding='utf8')
    source = (base/'private-build/source/native_data_cleanup.cpp').read_text(encoding='utf8')
    patched = patch_libraries(source, entries)
    (work/'native_data_cleanup.cpp').write_text(patched, encoding='utf8')
    contract = proof['build_identity']['native_ui_contract']
    (work/'ui-contract.json').write_bytes(encoded(contract))
    matches = [p.parent for p in (R7/'native-payload/revisions').glob('*/report.json')
               if sha(p) == proof['payload_report_sha256']]
    assert len(matches) == 1
    payload = matches[0]
    sys.path.insert(0, str(HERE.parent/'camera_native'))
    import build_flow
    original_input, original_argv = build_flow.input_path, sys.argv
    # 仅替换有实物证据的摘要表；清理算法与拍摄载荷不变。
    build_flow.input_path = lambda name: (work/name if name in ('legacy_cleanup_stages.hpp', 'native_data_cleanup.cpp')
                                         else base/'private-build/source'/name)
    sys.argv = ['build_flow', '--profile', 'setup', '--payload', str(payload),
                '--hide-literals', '--native-ui-contract', str(work/'ui-contract.json')]
    try:
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            build_flow.main()
    finally:
        build_flow.input_path, sys.argv = original_input, original_argv
    result = json.loads(output.getvalue().strip().splitlines()[-1])
    rebuilt = Path(result.get('build_directory') or result['reused_verified_build'])
    new = verified_setup(rebuilt)
    changed = {n for n, h in new['source_sha256'].items() if h != proof['source_sha256'][n]}
    expected = set()
    for name in ('legacy_cleanup_stages.hpp', 'native_data_cleanup.cpp'):
        if sha(work/name) != proof['source_sha256'][name]:
            expected.add(name)
    assert changed == expected
    assert new['payload_report_sha256'] == proof['payload_report_sha256']
    result = dict(identity=identity, directory=str(rebuilt), payload=str(payload),
                  added_hashes=additions, changed_sources=sorted(changed), camera_requests=0)
    result['libraries'] = entries
    (work/'report.json').write_bytes(encoded(result))
    print(json.dumps(result, ensure_ascii=False))


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('base', type=Path)
    p.add_argument('--previous', type=Path, action='append', required=True)
    p.add_argument('--libraries', type=Path)
    a = p.parse_args()
    build(a.base, a.previous, a.libraries)
