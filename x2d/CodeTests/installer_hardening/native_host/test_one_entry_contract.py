"""最终混淆 EXE 的主机 JSON 契约、真 DLL、同源码 UI 自测及事务回归。无设备请求。"""
import argparse
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import time

from assemble_native_client import ROOT, R7, HERE, encoded, sha, read_proof


def main(package, assembled, payload_build=None):
    assert Path.cwd().resolve()==ROOT
    package,assembled=package.resolve(),assembled.resolve()
    assert package.is_relative_to(R7) and assembled.is_relative_to(R7)
    proof=read_proof(assembled)
    for name,digest in proof['files'].items():
        assert sha(package/name.removeprefix('candidate/'))==digest
    out=R7/'native-host/one-entry-tests'/str(time.time_ns())
    out.mkdir(parents=True)
    env=dict(os.environ,TEMP=str(out),TMP=str(out),PYTHONDONTWRITEBYTECODE='1')
    cases=[]
    def run(name,argv):
        r=subprocess.run([str(v) for v in argv],env=env,capture_output=True,
                         timeout=120,creationflags=0x08000000)
        (out/(name+'.log')).write_bytes(r.stdout+r.stderr)
        cases.append(dict(name=name,exit=r.returncode,passed=r.returncode==0))
        assert r.returncode==0,(name,r.returncode,r.stdout,r.stderr)
        return r
    assert len(list(package.rglob('*.exe')))==1
    raw=(package/'X2dEngine.dll').read_bytes()
    pe=struct.unpack_from('<I',raw,0x3c)[0]
    assert raw[pe:pe+4]==b'PE\0\0' and struct.unpack_from('<H',raw,pe+22)[0]&0x2000
    result=run('production-host-check',[package/'HasselbladTuningPatchInstaller.exe','--host-check'])
    reply=json.loads(result.stdout.decode('utf8'))
    assert set(reply)=={'snapshot','error'} and set(reply['snapshot'])=={
        'Versions','Running','DriverPackages','WinUsbAvailable','BindingIssues'}
    private=assembled/'private-build'
    for mode in ('profile','progress','compact-ui','diagnostic-refresh','restart','update'):
        result_path=out/(mode+'.json')
        run('ui-'+mode,[private/'Installer.UiTests.exe','--'+mode+'-selftest',result_path])
        assert result_path.is_file() and result_path.stat().st_size
    run('ui-diagnostic',[private/'Installer.UiTests.exe','--diagnostic-selftest'])
    for name in ('test_native_adapter','test_native_bootstrap','test_native_operations',
                 'test_native_network_upload','test_cleanup_completion','test_receipt_cleanup','test_profile_metadata','test_profile_ui',
                 'test_uninstall_helper_profile','test_uninstall_preparation'):
        run(name,[sys.executable,'-X','utf8','-B',HERE/(name+'.py')])
    if payload_build is None:
        baseline=R7/'native-host/delivery-candidates/16b06d45157daac4a80f/X2D-Windows-R7-PackageFix'
        assert {p.relative_to(package/'native').as_posix():sha(p) for p in (package/'native').rglob('*.gz')} == {
            p.relative_to(baseline/'native').as_posix():sha(p) for p in (baseline/'native').rglob('*.gz')}
    else:
        payload_build=payload_build.resolve()
        assert payload_build.is_relative_to(R7/'native-host/album-build-inputs')
        bound=json.loads(payload_build.read_text(encoding='utf8'))
        assert len(bound['profiles'])==2 and bound['camera_requests']==0
        for prefix,item in zip(('native/basic','native'),bound['profiles']):
            payload=Path(item['payload']).resolve()
            assert payload.is_relative_to(R7/'native-payload/revisions')
            manifest=json.loads((payload/'report.json').read_text(encoding='utf8'))
            for name in ('native-install.gz','native-uninstall.gz'):
                assert sha(package/prefix/name)==sha(payload/name)==manifest['files'][name]
    (out/'report.json').write_bytes(encoded(dict(passed=True,cases=cases,camera_requests=0,
        actual_exe_sha256=sha(package/'HasselbladTuningPatchInstaller.exe'),
        one_exe=True,engine_is_dll=True,payload_unchanged=payload_build is None,
        payload_build_report=str(payload_build) if payload_build else None,
        limitation='电脑侧契约和离线检查；不等于真实安装卸载或相机功能通过')))
    print(out)


if __name__=='__main__':
    p=argparse.ArgumentParser()
    p.add_argument('package',type=Path);p.add_argument('assembled',type=Path)
    p.add_argument('--payload-build',type=Path)
    a=p.parse_args();main(a.package,a.assembled,a.payload_build)
