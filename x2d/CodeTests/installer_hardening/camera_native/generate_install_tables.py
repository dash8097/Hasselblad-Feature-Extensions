"""固定 H3/4.2.0 过渡安装表；不执行来源脚本，不构成无脚本发行包。"""
from pathlib import Path
import hashlib
import json
import re

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
BASE = ROOT / 'x2d/outputs/4.2.0/installer-hardening-r3/camera-iso-candidate-v4/public-install'
AUDIT = ROOT / 'x2d/outputs/4.2.0/installer-hardening-r5/checker-routes/transactions.json'

def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def generate():
    audit = json.loads(AUDIT.read_text(encoding='utf-8'))
    assert audit['firmware'] == 'X2D 4.2.0'
    manifest_path = BASE / 'install-manifest.json'
    assert sha(manifest_path) == audit['manifest_sha256']
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    source = audit['source_files']['install']
    assert sha(ROOT / source['path']) == source['sha256'] == manifest['install_sha256']
    script = (ROOT / source['path']).read_text(encoding='utf-8')
    assert 'test "$feature_mask" = 255 || exit 2' in script
    rows = audit['install_targets']
    assert len(rows) == 112 and len({r['path'] for r in rows}) == 112
    assert len({r['name'] for r in rows}) == 112
    out = ['// 固定 H3 过渡载荷。仍含旧业务脚本；不得标成最终无脚本发行包。', '#pragma once',
           'namespace cn { namespace install {',
           'struct Target { const char* source; const char* path; const char* original; const char* digest; unsigned mode; };',
           'static constexpr Target targets[]={']
    for row in rows:
        name, path = row['name'], row['path']
        assert re.fullmatch(r'p[0-9]{2,3}-[A-Za-z0-9_.-]+', name)
        assert path.startswith('/system/') and all(c not in ('', '.', '..') for c in path.split('/')[1:])
        assert path == manifest['targets'][name]
        assert row['original'] == (manifest['original'].get(name) or 'ABSENT')
        assert row['candidate_sha256'] == manifest['files'][name] == sha(BASE / name)
        assert row['mode'] in ('644', '755')
        assert f'chmod {row["mode"]} {path}.rs-next' in script
        values = [name, path, row['original'], row['candidate_sha256']]
        out.append('    {' + ','.join(json.dumps(v) for v in values) + ',0' + row['mode'] + '},')
    out += ['};', 'struct Guard { const char* path; const char* digest; };', 'static constexpr Guard guards[]={']
    guards = audit['baseline_guards']
    assert len(guards) == 5
    assert {r['path']:r['sha256'] for r in guards} == manifest['required_guards']
    for row in guards:
        out.append('    {' + json.dumps(row['path']) + ',' + json.dumps(row['sha256']) + '},')
    core = 'check-raw-stack-arm'
    assert sha(BASE / core) == manifest['files'][core]
    out += ['};', 'static constexpr const char* core_check_sha=' + json.dumps(manifest['files'][core]) + ';',
            'static constexpr const char* transition_manifest_sha=' + json.dumps(sha(manifest_path)) + ';']
    proof_path=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/install-probe/latest-build.json'
    proof=json.loads(proof_path.read_text(encoding='utf-8'))
    assert proof['firmware']=='X2D 4.2.0' and proof['inputs']['source']==sha(HERE/'native_install_probe.c')
    assert proof['inputs']['builder']==sha(HERE/'build_install_probe.py')
    assert sha(ROOT/proof['artifact']['path'])==proof['artifact']['sha256']
    total=sum((BASE/r['name']).stat().st_size for r in rows)
    check=sum((BASE/n).stat().st_size for n in ('p00-libx2d_raw_stack.so',core,'p04-render-probe'))+proof['artifact']['bytes']
    assert 0<total<2**63 and 0<check<2**63
    out += ['static constexpr const char* install_probe_sha='+json.dumps(proof['artifact']['sha256'])+';',
            f'static constexpr unsigned long long payload_bytes={total}ULL;',
            f'static constexpr unsigned long long runtime_check_bytes={check}ULL;', '} }', '']
    return '\n'.join(out)

if __name__ == '__main__':
    assert Path.cwd().resolve() == ROOT
    (HERE / 'native_install_tables.hpp').write_text(generate(), encoding='utf-8')
    print('verified 112 payloads / 5 guards / original SHA and modes; transitional only')
