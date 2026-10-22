"""在已验收来源的冻结载荷上只替换发布编排，重算安装和统一卸载清单。"""
import contextlib
import gzip
import hashlib
import io
import json
from pathlib import Path
import sys
import tarfile
from album_stream_patch import patch, replace_once
from assemble_native_client import ROOT, R7, encoded, sha

HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE.parent/'camera_native'))
import build_flow
import inspect_scriptless_payload

def digest(b):
    return hashlib.sha256(b).hexdigest()

def archive(files):
    raw=io.BytesIO()
    with tarfile.open(fileobj=raw,mode='w',format=tarfile.USTAR_FORMAT) as tar:
        for name,data in sorted(files.items()):
            assert '/' not in name
            item=tarfile.TarInfo(name);item.size=len(data);item.mode=0o644;item.mtime=0
            tar.addfile(item,io.BytesIO(data))
    return gzip.compress(raw.getvalue(),compresslevel=9,mtime=0)

def flow(base, work, profile, payload=None):
    proof=json.loads((base/'build-proof.json').read_text(encoding='utf8'))
    assert sha(base/'artifacts/x2d-native-flow')==proof['artifact']['sha256']
    work.mkdir(parents=True,exist_ok=True)
    for name,h in proof['source_sha256'].items():
        source=base/'private-build/source'/name
        assert sha(source)==h
        data=source.read_bytes()
        if profile=='runtime' and name in ('native_common.cpp','native_common.hpp','native_publish.cpp'):
            data=patch(name,data.decode('utf8').replace('\r\n','\n')).encode()
        if profile=='setup' and name=='legacy_cleanup_stages.hpp':
            text=data.decode('utf8')
            for revision in ('cacef6d4620165078c72','b1d9cdf37f926433374d'):
                previous=R7/'native-flow/revisions'/revision
                prior=json.loads((previous/'build-proof.json').read_text(encoding='utf8'))
                h=sha(previous/'artifacts/x2d-native-flow')
                assert h==prior['artifact']['sha256']
                if '"'+h+'"' not in text:
                    text=replace_once(text,'static const char*legacy_helper_hashes[]={',
                                      'static const char*legacy_helper_hashes[]={\n"'+h+'",')
            data=text.encode()
        (work/name).write_bytes(data)
    (work/'ui.json').write_bytes(encoded(proof['build_identity']['native_ui_contract']))
    old_input,old_argv=build_flow.input_path,sys.argv
    build_flow.input_path=lambda name:work/name
    sys.argv=['build_flow','--profile',profile,'--hide-literals','--native-ui-contract',str(work/'ui.json')]
    if payload:sys.argv+=['--payload',str(payload)]
    try:
        result=io.StringIO()
        with contextlib.redirect_stdout(result):build_flow.main()
    finally:build_flow.input_path,sys.argv=old_input,old_argv
    data=json.loads(result.getvalue().strip().splitlines()[-1])
    return Path(data.get('build_directory') or data['reused_verified_build'])

def payload(base, runtime, check, runtime_hashes):
    old=json.loads((base/'report.json').read_text(encoding='utf8'))
    files={}
    for n,h in old['files'].items():
        assert sha(base/n)==h
        files[n]=(base/n).read_bytes()
    manifest=json.loads(files['manifest.json'])
    old_runtime=manifest['runtime_sha256']
    binary=(runtime/'artifacts/x2d-native-flow').read_bytes()
    new_runtime=digest(binary)
    row=next(r for r in manifest['targets'] if r['path']=='/system/bin/x2d-native-flow')
    assert row['candidate_sha256']==old_runtime
    old_size=row['bytes'];row['candidate_sha256']=new_runtime;row['bytes']=len(binary)
    manifest['runtime_sha256']=new_runtime;manifest['hardware_validated']=False
    files['private-payload/install/'+row['name']]=binary
    files['manifest.json']=encoded(manifest)
    old_targets=files['private-payload/uninstall/targets.txt']
    lines=[]
    for line in old_targets.decode().splitlines():
        fields=line.split()
        if fields[1]=='/system/bin/x2d-native-flow':
            assert old_runtime in fields[3].split(',')
            fields[3]+=','+','.join(h for h in runtime_hashes if h not in fields[3].split(','))
        lines.append(' '.join(fields))
    files['private-payload/uninstall/targets.txt']=('\n'.join(lines)+'\n').encode()
    ins={n.rsplit('/',1)[1]:b for n,b in files.items() if n.startswith('private-payload/install/')}
    uns={n.rsplit('/',1)[1]:b for n,b in files.items() if n.startswith('private-payload/uninstall/')}
    files['native-install.gz']=archive(ins);files['native-uninstall.gz']=archive(uns)
    header=files['include/native_install_tables.hpp'].decode()
    header=replace_once(header,old_runtime,new_runtime)
    header=replace_once(header,digest((base/'manifest.json').read_bytes()),digest(files['manifest.json']))
    total=sum(r['bytes'] for r in manifest['targets'])
    header=replace_once(header,'payload_bytes='+str(total-len(binary)+old_size)+'ULL','payload_bytes='+str(total)+'ULL')
    files['include/native_install_tables.hpp']=header.encode()
    header=files['include/native_setup_tables.hpp'].decode()
    # 统一卸载同时承认上一版和新编排，原厂还原摘要保持不变。
    target_line=next(s for s in header.splitlines() if '"/system/bin/x2d-native-flow"' in s)
    additions=','.join(h for h in runtime_hashes if h not in target_line)
    new_line=target_line.replace(old_runtime+',',old_runtime+','+additions+',')
    assert new_line!=target_line
    header=replace_once(header,target_line,new_line)
    header=replace_once(header,digest(old_targets),digest(files['private-payload/uninstall/targets.txt']))
    files['include/native_setup_tables.hpp']=header.encode()
    header=['#pragma once','#include "native_unpack.hpp"',
            '#ifndef CN_UNPACK_DECODER_SHA256',
            '#define CN_UNPACK_DECODER_SHA256 '+json.dumps(old['decoder_static_evidence']['sha256']),
            '#endif','namespace cn { namespace unpack {']
    for kind,items in (('install',ins),('uninstall',uns)):
        header+=['static constexpr File '+kind+'_files[]={']
        for n,b in sorted(items.items()):
            header+=['{'+json.dumps(n)+','+json.dumps(digest(b))+','+str(len(b))+'ULL,0644},']
        an='native-'+kind+'.gz'
        header+=['};','static constexpr Bundle '+kind+'_bundle={'+json.dumps(an)+','+json.dumps(digest(files[an]))+','+str(len(files[an]))+'ULL,'+kind+'_files,sizeof('+kind+'_files)/sizeof('+kind+'_files[0])};']
    files['include/native_unpack_tables.hpp']=('\n'.join(header+['} }',''])).encode()
    inspection=inspect_scriptless_payload.inspect(manifest,files['native-install.gz'])
    assert inspection['structural_pass'],inspection
    files['scriptless-inspection.json']=encoded(inspection)
    identity=dict(base_report=sha(base/'report.json'),builder=sha(Path(__file__)),
                  runtime_proof=sha(runtime/'build-proof.json'),check=sha(check/'report.json'),
                  patch=sha(HERE/'album_stream_patch.py'),manifest_sha256=digest(files['manifest.json']),
                  runtime_hashes=runtime_hashes,
                  archives={n:digest(files[n]) for n in ('native-install.gz','native-uninstall.gz')})
    files['identity.json']=encoded(identity)
    # 旧 ELF 报告仅作基线来源；新 runtime 的检查绑定新 build-proof。
    files['elf-metadata-hardening.json']=encoded(dict(base_report=sha(base/'elf-metadata-hardening.json'),
                                                     runtime_proof=sha(runtime/'build-proof.json')))
    out=R7/'native-payload/revisions'/digest(encoded(identity))[:20]
    report=dict(old,identity=identity,hardware_executed=False,release_ready=False,
                files={n:digest(b) for n,b in files.items()})
    files['report.json']=encoded(report)
    for n,b in files.items():
        target=out/n;target.parent.mkdir(parents=True,exist_ok=True)
        if target.exists():assert target.read_bytes()==b
        else:target.write_bytes(b)
    return out

def main():
    assert Path.cwd().resolve()==ROOT
    check=R7/'native-host/album-stream/2bd61005244d669d99c0'
    checked=json.loads((check/'report.json').read_text(encoding='utf8'))
    assert checked['passed'] and checked['identity']['patch']==sha(HERE/'album_stream_patch.py')
    work=R7/'native-host/album-build-inputs'/sha(Path(__file__))[:20]
    prepared=[]
    for p,s,r in [('2123f51468b87743b195','cacef6d4620165078c72','8fdbb4328dcf08b79c93'),
                  ('343f8825ae0a2c249b34','b1d9cdf37f926433374d','8393c8c97e45d6efbc04')]:
        base=ROOT/'x2d/outputs/4.2.0/raw-stack/continuous-candidate/runtime/revisions'/r
        manifest=json.loads((R7/'native-payload/revisions'/p/'manifest.json').read_text(encoding='utf8'))
        assert sha(base/'artifacts/x2d-native-flow')==manifest['runtime_sha256']
        runtime=flow(base,work/r,'runtime')
        for name in ('native_common.cpp','native_common.hpp','native_publish.cpp'):
            assert sha(runtime/'private-build/source'/name)==checked['identity'][name], name
        prepared.append((p,s,runtime))
    runtime_hashes=sorted({sha(runtime/'artifacts/x2d-native-flow') for _,_,runtime in prepared})
    pairs=[]
    for p,s,runtime in prepared:
        new_payload=payload(R7/'native-payload/revisions'/p,runtime,check,runtime_hashes)
        setup=flow(R7/'native-flow/revisions'/s,work/s,'setup',new_payload)
        pairs.append(dict(payload=str(new_payload),setup=str(setup),runtime=str(runtime)))
    result=dict(profiles=pairs,camera_requests=0,hardware_accepted=False)
    (work/'report.json').write_bytes(encoded(result))
    print(json.dumps(result,ensure_ascii=False))
if __name__=='__main__':main()
