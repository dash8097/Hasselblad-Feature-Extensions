"""实际 Windows 编译模块离线校验两档资源、卸载路由和包完整性。"""
import argparse,hashlib,json,os,subprocess,sys
from pathlib import Path
import build_native_client as builder

PROBE=r'''
import sys,json
from pathlib import Path
from types import SimpleNamespace
root=Path(sys.argv[1]);sys.path.insert(0,str(root/'engine'))
previous=json.loads(sys.argv[2])
import native_client_support as support
assert support.__file__.endswith('.pyd')
cases=[]
for selected,action,expected in ((248,'Install',248),(255,'Install',255),(255,'Uninstall',248),(248,'Uninstall',248)):
    obj=SimpleNamespace(root=root,pending=False,feature_mask=selected,_native_action=action,progress=lambda _:None)
    support.verify_package(obj)
    assert obj.stock['feature_mask']==expected
    assert set(previous).issubset(obj.native_metadata['bootstrap_previous_sha256']), '上一完整包安装器未进入当前档位升级清单'
    cases.append(dict(selected=selected,action=action,actual=expected,targets=len(obj.native_metadata['install_targets'])))
obj=SimpleNamespace(root=root,pending=True,pending_mask=255,feature_mask=248,_native_action='Install',progress=lambda _:None)
support.verify_package(obj);assert obj.stock['feature_mask']==255
print(json.dumps(dict(passed=True,camera_requests=0,compiled_module=True,cases=cases,pending_selection_preserved=True,previous_setup_sha256=previous)))
'''

def main():
    parser=argparse.ArgumentParser();parser.add_argument('assembled',type=Path);parser.add_argument('--previous-setup',type=Path,action='append',default=[]);args=parser.parse_args()
    assembled=args.assembled.resolve();assert assembled.is_relative_to(builder.R7)
    report=json.loads((assembled/'report.json').read_text(encoding='utf8'))
    assert report['identity']['platform']=='windows'
    for name,digest in report['files'].items():
        assert hashlib.sha256((assembled/name).read_bytes()).hexdigest()==digest
    package=assembled/'candidate'
    assert not [p for p in package.rglob('*') if p.is_file() and p.suffix.lower() in ('.py','.sh','.ps1','.bat','.cmd','.qml','.js')]
    previous=[]
    for path in args.previous_setup:
        path=path.resolve();assert path.is_relative_to(builder.R7/'native-flow/revisions') and path.name=='x2d-native-flow'
        proof=json.loads((path.parent.parent/'build-proof.json').read_text(encoding='utf8'))
        digest=hashlib.sha256(path.read_bytes()).hexdigest()
        assert proof['build_identity']['profile']=='setup' and digest==proof['artifact']['sha256']
        previous.append(digest)
    previous=sorted(set(previous))
    rev=hashlib.sha256(Path(__file__).read_bytes()+(assembled/'report.json').read_bytes()+json.dumps(previous).encode('ascii')).hexdigest()[:20]
    out=builder.R7/'native-host/profile-package-check'/rev;out.mkdir(parents=True,exist_ok=False)
    result=subprocess.run([sys.executable,'-I','-B','-c',PROBE,str(package),json.dumps(previous)],env=dict(os.environ,PYTHONDONTWRITEBYTECODE='1'),capture_output=True,text=True,encoding='utf8',errors='replace',timeout=120)
    (out/'console.txt').write_text(result.stdout+result.stderr,encoding='utf8')
    assert result.returncode==0,result.stdout+result.stderr
    evidence=json.loads(result.stdout);evidence['first_party_script_files']=False;evidence['package_report_sha256']=hashlib.sha256((assembled/'report.json').read_bytes()).hexdigest()
    (out/'report.json').write_text(json.dumps(evidence,ensure_ascii=False,indent=2)+'\n',encoding='utf8');print(out)

if __name__=='__main__':main()
