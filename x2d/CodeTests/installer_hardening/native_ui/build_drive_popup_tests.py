"""原生拍摄模式选择、关闭提交和队列容量回归；不运行设备代码。"""
from pathlib import Path
import ctypes,hashlib,json,os,subprocess
import build_processing_tests as b
def main():
 assert Path.cwd().resolve()==b.ROOT
 ctypes.windll.kernel32.SetErrorMode(0x8003)
 names=('drive_popup.hpp','drive_popup.cpp','test_drive_popup.cpp','multishot_settings.hpp','pixel_host_state.hpp','pixel_controller.hpp','protocol_status.hpp','protocol_text.hpp','processing_presentation.hpp','build_drive_popup_tests.py')
 paths=[b.HERE/n for n in names];identity={p.name:b.sha(p) for p in paths};identity['compiler']=b.sha(b.CXX)
 out=b.OUT/'native-ui/drive-popup'/hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20];out.mkdir(parents=True,exist_ok=False)
 for p in paths:(out/p.name).write_bytes(p.read_bytes())
 env=dict(os.environ,TEMP=str(out),TMP=str(out),PATH=str(b.CXX.parent)+os.pathsep+os.environ['PATH'])
 cmd=[b.CXX,'-std=c++17','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',out/'drive_popup.cpp',out/'test_drive_popup.cpp','-o',out/'test.exe']
 p=subprocess.run(list(map(str,cmd)),env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=120);(out/'build.log').write_text(p.stdout+p.stderr,encoding='utf8');assert not p.returncode,p.stdout+p.stderr
 p=subprocess.run([str(out/'test.exe')],env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=60);(out/'run.log').write_text(p.stdout+p.stderr,encoding='utf8');assert not p.returncode,p.stdout+p.stderr
 assert all(b.sha(p)==identity[p.name] for p in paths)
 (out/'report.json').write_text(json.dumps(dict(identity=identity,passed=True,binary=b.sha(out/'test.exe'),stdout=p.stdout,camera_operations=0,package_integrated=False),ensure_ascii=False,indent=2),encoding='utf8');print(out);print(p.stdout)
if __name__=='__main__':main()
