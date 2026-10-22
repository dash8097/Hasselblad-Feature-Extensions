"""编译实际双档 WinForms 并执行离线控件选择；不显示窗口、不访问设备。"""
import hashlib,json,os,subprocess
from pathlib import Path
import profile_ui
import build_native_client as builder

def main():
    root=builder.ROOT/'x2d/outputs/4.2.0/installer-hardening-r3/build/windows-native/ui'
    names=('RawStackInstaller.cs','PatchDiagnosticReport.cs','PatchUpdateState.cs')
    source={n:(root/n).read_text(encoding='utf-8-sig') for n in names}
    source[names[0]]=profile_ui.windows(source[names[0]])
    revision=hashlib.sha256(json.dumps(source,sort_keys=True).encode()).hexdigest()[:20]
    out=builder.R7/'native-host/profile-ui'/revision
    out.mkdir(parents=True,exist_ok=False)
    for n,s in source.items():(out/n).write_text(s,encoding='utf-8-sig')
    (out/'universal-package.txt').write_text('R7 INTERNAL NATIVE CLIENT\n')
    compiler=Path('C:/Windows/Microsoft.NET/Framework64/v4.0.30319/csc.exe')
    exe=out/'ProfileUiTests.exe'
    command=[str(compiler),'/nologo','/target:winexe','/platform:x64','/define:INTERNAL_UI_TESTS','/reference:System.Windows.Forms.dll','/reference:System.Drawing.dll','/reference:System.Web.Extensions.dll','/out:'+str(exe),*[str(out/n) for n in names]]
    result=subprocess.run(command,capture_output=True,text=True,timeout=60)
    (out/'compile.txt').write_text(result.stdout+result.stderr,encoding='utf8');assert result.returncode==0,result.stdout+result.stderr
    proof=out/'selection.txt'
    result=subprocess.run([str(exe),'--profile-selftest',str(proof)],capture_output=True,text=True,timeout=30,creationflags=0x08000000)
    assert result.returncode==0 and proof.read_text()=='PROFILE_UI_PASSED device_requests=0 checks=3'
    (out/'report.json').write_text(json.dumps({'passed':True,'camera_requests':0,'checks':3,'source_sha256':{n:hashlib.sha256(s.encode()).hexdigest() for n,s in source.items()}},indent=2)+'\n')
    print(out)

if __name__=='__main__':main()
