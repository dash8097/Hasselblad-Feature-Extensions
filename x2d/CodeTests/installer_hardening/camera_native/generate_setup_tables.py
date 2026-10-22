"""从已审计的固定 H3 事务清单生成原生只读表；不执行来源脚本。"""
from pathlib import Path
import hashlib,json,re
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
SOURCE=ROOT/'x2d/outputs/4.2.0/installer-hardening-r5/checker-routes/transactions.json'

def generate():
    d=json.loads(SOURCE.read_text(encoding='utf-8'))
    assert d['firmware']=='X2D 4.2.0'
    for record in d['source_files'].values():
        assert hashlib.sha256((ROOT/record['path']).read_bytes()).hexdigest()==record['sha256']
    rows=d['uninstall_targets'];assert len(rows)==168
    assert len({r['id'] for r in rows})==len(rows)
    text=['// 固定 X2D 4.2.0 / H3 卸载兼容表，由内部构建工具生成。','#pragma once',
          'namespace cn { namespace setup {',
          'struct LegacyTarget { const char* id; const char* path; const char* original; const char* allowed; unsigned mode; };',
          'static constexpr LegacyTarget legacy_targets[]={']
    for r in rows:
        assert re.fullmatch(r'u[0-9]{2,3}',r['id'])
        assert r['path'].startswith('/system/') and '/..' not in r['path']
        for v in [r['original'],*r['allowed']]:assert v in ('ABSENT','PROJECT_FILE') or re.fullmatch('[0-9a-f]{64}',v)
        values=[r['id'],r['path'],r['original'],','+','.join(r['allowed'])+',']
        text.append('    {'+','.join(json.dumps(x) for x in values)+',0'+r['mode']+'},')
    text+=['};','struct LegacyGuard { const char* path; const char* digest; };','static constexpr LegacyGuard legacy_guards[]={']
    assert len(d['baseline_guards'])==5
    for g in d['baseline_guards']:
        assert g['path'].startswith('/system/') and re.fullmatch('[0-9a-f]{64}',g['sha256'])
        text.append('    {'+json.dumps(g['path'])+','+json.dumps(g['sha256'])+'},')
    text+=['};','static constexpr const char* legacy_targets_sha='+json.dumps(d['uninstall_targets_sha256'])+';',
           'static constexpr const char* legacy_guards_sha='+json.dumps(d['uninstall_guards_sha256'])+';',
           '} }','']
    return '\n'.join(text)

if __name__=='__main__':
    assert Path.cwd().resolve()==ROOT
    (HERE/'native_setup_tables.hpp').write_text(generate(),encoding='utf-8')
    print('verified 168 fixed targets; generated native table')
