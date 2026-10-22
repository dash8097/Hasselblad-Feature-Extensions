"""真实 Qt JS oracle 与 C++ 策略差分；JS 仅保留在内部测试输入。"""
from pathlib import Path
import hashlib,json,os,subprocess,ctypes
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
R7=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7'
SDK=R7/'native-ui/qt-sdk/6.4.1/mingw_64'
ZIG=ROOT/'.research-cache/x1d-1.25.0/toolchain/zig-windows-x86_64-0.13.0/zig.exe'
BASE=ROOT/'x2d/outputs/4.2.0/installer-hardening-r3/camera-iso-candidate-v4/public-install'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    assert Path.cwd().resolve()==ROOT
    ctypes.windll.kernel32.SetErrorMode(0x8003)
    paths=[HERE/'selection_policies.hpp',HERE/'test_selection_policies.cpp',Path(__file__),BASE/'p13-EyePreferencePolicy.js',BASE/'p16-NearestFacePolicy.js']
    manifest=json.loads((BASE/'install-manifest.json').read_text(encoding='utf-8'))
    for p in paths[-2:]:assert sha(p)==manifest['files'][p.name]
    identity={p.name:sha(p) for p in paths};identity['zig']=sha(ZIG)
    revision=hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20]
    out=R7/'native-ui/selection-tests'/revision;out.mkdir(parents=True,exist_ok=True)
    assert not (out/'report.json').exists(),'不覆盖已完成证据'
    for p in paths:(out/p.name).write_bytes(p.read_bytes())
    env=dict(os.environ,TEMP=str(out),TMP=str(out),ZIG_LOCAL_CACHE_DIR=str(out/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(out/'zig-global'),QT_QPA_PLATFORM='offscreen',QML_DISABLE_DISK_CACHE='1')
    env['PATH']=str(SDK/'bin')+os.pathsep+env['PATH']
    command=[str(ZIG),'c++','-target','x86_64-windows-gnu','-std=c++17','-O2','-Wall','-Wextra','-Werror','-DQT_NO_DEBUG','-DQT_CORE_LIB','-DQT_QML_LIB','-isystem',str(SDK/'include'),'-isystem',str(SDK/'include/QtCore'),'-isystem',str(SDK/'include/QtQml'),str(out/'test_selection_policies.cpp'),str(SDK/'lib/libQt6Qml.a'),str(SDK/'lib/libQt6Core.a'),'-o',str(out/'selection-test.exe')]
    result=subprocess.run(command,env=env,capture_output=True,text=True,timeout=180)
    (out/'build.log').write_text(result.stdout+result.stderr,encoding='utf-8')
    assert not result.returncode,result.stderr
    result=subprocess.run([str(out/'selection-test.exe'),str(out/'p16-NearestFacePolicy.js'),str(out/'p13-EyePreferencePolicy.js')],env=env,capture_output=True,text=True,timeout=60)
    (out/'console.txt').write_text(result.stdout+result.stderr,encoding='utf-8')
    passed=not result.returncode and 'SELECTION_POLICIES_PASS' in result.stdout
    for p in paths:assert sha(p)==identity[p.name],'源码在验证期间变化'
    (out/'report.json').write_text(json.dumps(dict(passed=passed,identity=identity,test_binary_sha256=sha(out/'selection-test.exe'),qt='6.4.1',architecture='Windows x86_64',test=result.stdout,camera_requests=0,production_script_inputs=False,arm64_tested=False,package_integrated=False),ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(result.stdout+result.stderr);print(out);assert passed
if __name__=='__main__':main()
