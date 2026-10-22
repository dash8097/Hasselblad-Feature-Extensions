"""像素/RAW/ISO参数模型与冻结p27原JS行为差分；测试材料不随包。"""
from pathlib import Path
import ctypes,hashlib,json,os,subprocess
import build_processing_tests as b
def main():
 assert Path.cwd().resolve()==b.ROOT
 ctypes.windll.kernel32.SetErrorMode(0x8003)
 oracle=b.ORACLE.with_name('p27-X2dMultishotSettings.qml')
 paths=[b.HERE/n for n in ('multishot_settings.hpp','multishot_settings.cpp','test_multishot_settings.cpp','protocol_text.hpp','protocol_status.hpp','protocol_status.cpp','build_multishot_settings_tests.py','build_processing_tests.py')]+[oracle]
 identity={p.name:b.sha(p) for p in paths};identity['compiler']=b.sha(b.CXX);identity['qt']=b.sha(b.SDK/'bin/Qt6Qml.dll')
 out=b.OUT/'native-ui/multishot-settings'/hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20];out.mkdir(parents=True,exist_ok=False)
 for p in paths:(out/p.name).write_bytes(p.read_bytes())
 env=dict(os.environ,TEMP=str(out),TMP=str(out),PATH=str(b.CXX.parent)+os.pathsep+str(b.SDK/'bin')+os.pathsep+os.environ['PATH'],QML_DISABLE_DISK_CACHE='1')
 cmd=[b.CXX,'-std=c++17','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-DQT_NO_DEBUG','-DQT_CORE_LIB','-DQT_QML_LIB','-I'+str(b.SDK/'include'),'-I'+str(b.SDK/'include/QtCore'),'-I'+str(b.SDK/'include/QtQml'),out/'multishot_settings.cpp',out/'protocol_status.cpp',out/'test_multishot_settings.cpp',b.SDK/'lib/libQt6Qml.a',b.SDK/'lib/libQt6Core.a','-o',out/'test.exe']
 p=subprocess.run(list(map(str,cmd)),env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=120);(out/'build.log').write_text(p.stdout+p.stderr,encoding='utf8');assert not p.returncode,p.stdout+p.stderr
 p=subprocess.run([str(out/'test.exe'),str(out/oracle.name)],env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=60);(out/'run.log').write_text(p.stdout+p.stderr,encoding='utf8');assert not p.returncode,p.stdout+p.stderr
 assert all(b.sha(p)==identity[p.name] for p in paths)
 (out/'report.json').write_text(json.dumps(dict(identity=identity,passed=True,binary=b.sha(out/'test.exe'),stdout=p.stdout,camera_requests=0,original_gui_executed=False,package_integrated=False),ensure_ascii=False,indent=2),encoding='utf8');print(out);print(p.stdout)
if __name__=='__main__':main()
