"""自动电子引闪时序与故障状态机离线回归，不连接相机。"""
from pathlib import Path
import hashlib,json,os,subprocess
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
BASE=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/native-ui'
CXX=BASE/'host-mingw/sdk/Tools/mingw1120_64/bin/g++.exe'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    names=['esflash_timing.hpp','esflash_client.h','esflash_protocol.h','factory_proxy.hpp','abi_view_api.hpp','effect_view_core.hpp','local_heap.hpp','test_esflash_timing.cpp','build_esflash_timing_tests.py']
    identity={'sources':{n:sha(HERE/n) for n in names},'compiler':sha(CXX)}
    rev=hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20]
    out=BASE/'esflash-timing'/rev;out.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ,PATH=str(CXX.parent)+os.pathsep+os.environ['PATH'])
    binary=out/'test.exe'
    p=subprocess.run([str(CXX),'-std=c++17','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',str(HERE/'test_esflash_timing.cpp'),'-o',str(binary)],env=env,capture_output=True,text=True,timeout=60)
    (out/'build.log').write_text(p.stdout+p.stderr);assert not p.returncode,p.stderr
    p=subprocess.run([str(binary)],env=env,capture_output=True,text=True,timeout=15)
    (out/'console.txt').write_text(p.stdout+p.stderr);assert not p.returncode,p.stdout+p.stderr
    assert identity['sources']=={n:sha(HERE/n) for n in names}
    (out/'report.json').write_text(json.dumps({'identity':identity,'binary':sha(binary),'stdout':p.stdout,'camera_requests':0,'lamp_mode_commands':0},indent=2)+'\n')
    print(out);print(p.stdout)
if __name__=='__main__':main()
