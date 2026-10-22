"""以匹配 minor 的 Windows解释器运行公开标准库；不冒充Mac运行验证。"""
from pathlib import Path
import hashlib,json,os,subprocess,sys
import build_mac_runtime,build_windows_runtime
ROOT=build_mac_runtime.ROOT;R7=build_mac_runtime.R7
PROBE=r'''
import sys,importlib,json
sys.path.insert(0,sys.argv[1])
modules=['base64','pathlib','hashlib','ctypes','secrets','subprocess','platform','tempfile','zipfile','tarfile','threading','socket','urllib.parse','queue','traceback','logging','datetime','concurrent.futures']
loaded={}
for name in modules:
 m=importlib.import_module(name);path=getattr(m,'__file__','')
 assert path.startswith(sys.argv[1]) and path.endswith('.pyc'),(name,path)
 loaded[name]=path[len(sys.argv[1]):]
from dataclasses import dataclass
@dataclass
class Row:value:int
assert Row(4).value==4
import base64,hashlib,threading,zipfile,io,traceback,pathlib
assert base64.b64decode(base64.b64encode(b'test'))==b'test'
assert hashlib.sha256(b'test').hexdigest()=='9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08'
done=[];t=threading.Thread(target=lambda:done.append(1));t.start();t.join();assert done==[1]
buf=io.BytesIO()
with zipfile.ZipFile(buf,'w') as z:z.writestr('fixture','ok')
with zipfile.ZipFile(buf) as z:assert z.read('fixture')==b'ok'
try:raise ValueError('diagnostic fixture')
except ValueError:assert 'diagnostic fixture' in traceback.format_exc()
assert str(pathlib.PurePosixPath('/tmp')/'fixture')=='/tmp/fixture'
print(json.dumps(dict(modules=loaded,checks=7,platform=sys.platform,version=sys.version)))
'''
def main():
 assert Path.cwd().resolve()==ROOT and sys.version_info[:2]==(3,11)
 mac=build_mac_runtime.build();win=build_windows_runtime.build()
 identity=dict(script=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),mac=mac.name,windows=win.name)
 rev=hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20];out=R7/'native-host/runtime-tests'/rev;out.mkdir(parents=True,exist_ok=False)
 results={}
 for name,exe,path in [('windows',Path(sys.executable),win/'artifacts/runtime/python311.zip'),
                       ('mac-stdlib-arm64-on-windows',mac/'private-build/compiler/python.exe',mac/'artifacts/runtime/arm64/lib/python3.12'),
                       ('mac-stdlib-x86_64-on-windows',mac/'private-build/compiler/python.exe',mac/'artifacts/runtime/x86_64/lib/python3.12')]:
  env=dict(os.environ,TEMP=str(out),TMP=str(out),PYTHONDONTWRITEBYTECODE='1')
  p=subprocess.run([str(exe),'-I','-B','-c',PROBE,str(path)],env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=30)
  (out/(name+'.log')).write_text(p.stdout+p.stderr,encoding='utf8');assert not p.returncode,p.stdout+p.stderr;results[name]=json.loads(p.stdout)
 (out/'report.json').write_text(json.dumps(dict(identity=identity,results=results,passed=True,mac_native_executed=False,camera_requests=0),ensure_ascii=False,indent=2),encoding='utf8');print(out)
if __name__=='__main__':main()
