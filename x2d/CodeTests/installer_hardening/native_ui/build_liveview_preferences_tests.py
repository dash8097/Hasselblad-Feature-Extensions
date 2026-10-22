"""取景设置异步确认、超时和旧回执测试，不连接相机。"""
import hashlib,json,os,subprocess
from pathlib import Path
import build_processing_tests as b
def main():
    assert Path.cwd().resolve()==b.ROOT
    names=['liveview_preferences.hpp','pixel_file_io.h','screen_policy.hpp','test_liveview_preferences.cpp','build_liveview_preferences_tests.py']
    identity={n:b.sha(b.HERE/n) for n in names};identity['compiler']=b.sha(b.CXX)
    out=b.OUT/'native-ui/liveview-preferences'/hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20]
    out.mkdir(parents=True,exist_ok=False)
    for n in names:(out/n).write_bytes((b.HERE/n).read_bytes())
    env=dict(os.environ,TEMP=str(out),TMP=str(out),PATH=str(b.CXX.parent)+os.pathsep+os.environ['PATH'])
    cmd=[b.CXX,'-std=c++17','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',out/'test_liveview_preferences.cpp','-o',out/'test.exe']
    r=subprocess.run(list(map(str,cmd)),env=env,capture_output=True,text=True,timeout=60);(out/'build.log').write_text(r.stdout+r.stderr,encoding='utf-8');assert not r.returncode,r.stdout+r.stderr
    r=subprocess.run([str(out/'test.exe')],env=env,capture_output=True,text=True,timeout=20);(out/'run.log').write_text(r.stdout+r.stderr,encoding='utf-8');assert not r.returncode,r.stdout+r.stderr
    assert all(b.sha(b.HERE/n)==identity[n] for n in names)
    (out/'report.json').write_text(json.dumps(dict(identity=identity,passed=True,stdout=r.stdout,camera_operations=0),indent=2),encoding='utf-8');print(out);print(r.stdout)
if __name__=='__main__':main()
