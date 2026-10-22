"""运行时只绑定固定叶节点，避免包含安装器自身或完整发行清单造成摘要循环。"""
from pathlib import Path
import hashlib,json
import generate_install_tables as legacy
from elf_metadata_hardening import normalize_elf

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
REPLACEMENTS={
    '/system/bin/x2d-pixel-overclock/libx2d_private_memory_capture.so':('x2d\\outputs\\4.2.0\\raw-stack\\continuous-candidate\\memory-gate\\20e71cdea1b59c81ec17\\libx2d_private_memory_capture.so','9df3e1da3832cc2a6626c281f605d68314202f19a622e3126fb53b408909be94'),
    '/system/bin/x2d-multi-resolution/libx2d_private_capture.so':(
        'x2d/outputs/4.2.0/installer-hardening-r7/disk-capture/revisions/de056c8530ac603646b5/artifacts/libx2d_private_capture.so',
        'd29606f07e88fbb7dde30217e857737ffa51e7ba3c0a47a540e580e7a7181204'),
    '/system/bin/x2d-multi-resolution/libquarter_capture.so':(
        'x2d/outputs/4.2.0/installer-hardening-r7/disk-capture/revisions/b52d46cd9aa955be93b1/artifacts/libquarter_capture.so',
        '032ad9496b99e99991838db31c4d2ff57e85621dad6b9d8ba81074b94032eb87'),
}
LEAVES={
    '/system/bin/x2d-pixel-overclock/':('libx2d_private_memory_capture.so','render-memory','fix-3fr','pack','preview','inputs','heif','render-probe'),
    '/system/bin/x2d-multi-resolution/':('libquarter_capture.so','finalize25','render25','pack25','merge16','render16','jpeg-grid','pack16','libx2d_private_capture.so'),
    '/system/bin/x2d-raw-stack/':('stack-pack','finalize'),
}
def generate():
    audit=json.loads(legacy.AUDIT.read_text(encoding='utf-8'))
    assert audit['firmware']=='X2D 4.2.0'
    assert legacy.sha(legacy.BASE/'install-manifest.json')==audit['manifest_sha256']
    rows={r['path']:r for r in audit['install_targets']}
    out=['// X2D 4.2.0 固定原生叶节点；不含 runtime/setup 自身摘要。','#pragma once',
         'namespace cn { namespace runtime {','struct Guard {const char* path;const char* digest;};',
         'static constexpr Guard guards[]={']
    digests={}
    for parent,names in LEAVES.items():
        for name in names:
            path=parent+name;r=rows[path]
            artifact=legacy.BASE/r['name'];digest=r['candidate_sha256']
            if path in REPLACEMENTS:
                source,digest=REPLACEMENTS[path];artifact=ROOT/source
            assert legacy.sha(artifact)==digest
            data=artifact.read_bytes();assert data.startswith(b'\x7fELF'),path
            data,_=normalize_elf(data);digest=hashlib.sha256(data).hexdigest();digests[path]=digest
            out.append('    {'+json.dumps(path)+','+json.dumps(digest)+'},')
    out+=['};']
    for symbol,name in [('pixel_inputs_sha','inputs'),('pixel_fix_3fr_sha','fix-3fr')]:
        out.append('static constexpr const char* '+symbol+'='+json.dumps(digests['/system/bin/x2d-pixel-overclock/'+name])+';')
    out+=['} }','']
    return '\n'.join(out)
if __name__=='__main__':
    assert Path.cwd().resolve()==ROOT
    (HERE/'native_runtime_tables.hpp').write_text(generate(),encoding='utf-8')
    print('verified 19 native leaf artifacts; runtime manifest independent of installer')
