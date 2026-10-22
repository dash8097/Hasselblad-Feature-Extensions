"""生成精确历史暂存表；不以 x2d 前缀或机内清点结果自动授权新目录。"""
import hashlib,json,re
from pathlib import Path
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
SOURCE=HERE/'legacy_cleanup_stages.json'
def generate(verify_evidence=False):
    records=json.loads(SOURCE.read_text(encoding='utf8'))
    seen=set();lines=['#pragma once','// 由 legacy_cleanup_stages.json 生成；只列已核对来源的精确名称。','namespace cn { namespace tidy {','struct LegacyStage {const char*name;const char*marker;const char*sha;};','static const LegacyStage legacy_stages[]={']
    for item in records:
        name=item['name'];assert re.fullmatch(r'x2d-[A-Za-z0-9_.-]{1,96}',name) and name not in seen and '..' not in name;seen.add(name)
        assert item['evidence']
        for proof in item['evidence']:
            path=(ROOT/proof['path']).resolve();assert path.is_relative_to(ROOT/'x2d')
            assert re.fullmatch('[0-9a-f]{64}',proof['sha256'])
            if verify_evidence:assert hashlib.sha256(path.read_bytes()).hexdigest()==proof['sha256'],proof['path']
        for marker,digest in (item['markers'] or {'':''}).items():
            assert marker=='' or re.fullmatch('[A-Za-z0-9_.-]+',marker)
            assert digest=='' or re.fullmatch('[0-9a-f]{64}',digest)
            lines.append('    {'+','.join(json.dumps(s) for s in (name,marker,digest))+'},')
    lines+=['};','static const char*legacy_helper_hashes[]={']
    for helper in json.loads((HERE/'legacy_cleanup_helpers.json').read_text(encoding='utf8')):
        assert re.fullmatch('[0-9a-f]{64}',helper['sha256'])
        path=(ROOT/helper['path']).resolve();assert path.is_relative_to(ROOT/'x2d')
        if verify_evidence:assert hashlib.sha256(path.read_bytes()).hexdigest()==helper['sha256']
        lines.append('    '+json.dumps(helper['sha256'])+',')
    lines+=['};','} }',''];return '\n'.join(lines)
if __name__=='__main__':
    (HERE/'legacy_cleanup_stages.hpp').write_text(generate(True),encoding='utf8')
