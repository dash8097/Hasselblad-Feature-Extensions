"""Qt 6.4.1 真 C++ 组件离线候选；不连接相机，不作为 ARM64 已验证。"""
from pathlib import Path
import hashlib,json,os,subprocess,ctypes

ROOT=Path(__file__).resolve().parents[4]
HERE=Path(__file__).resolve().parent
OUT=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/native-ui'
SDK=OUT/'qt-sdk/6.4.1/mingw_64'
ZIG=ROOT/'.research-cache/x1d-1.25.0/toolchain/zig-windows-x86_64-0.13.0/zig.exe'

def run(argv,env):
    p=subprocess.run([str(x) for x in argv],cwd=OUT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace')
    (OUT/'build.log').open('a',encoding='utf-8').write(p.stdout+p.stderr)
    if p.returncode:raise RuntimeError(p.stdout+p.stderr)
    return p.stdout

def main():
    ctypes.windll.kernel32.SetErrorMode(0x8003)
    OUT.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ,TEMP=str(OUT),TMP=str(OUT),ZIG_LOCAL_CACHE_DIR=str(OUT/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(OUT/'zig-global'),QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',QML_DISABLE_DISK_CACHE='1')
    env['PATH']=str(SDK/'bin')+os.pathsep+env['PATH']
    include=[f'-I{SDK/"include"}',f'-I{HERE}']
    for module in ['Core','Gui','Qml','Quick','Network']:include.append(f'-I{SDK/"include"/("Qt"+module)}')
    run([SDK/'bin/moc.exe',*include,HERE/'effect_preferences.hpp','-o',OUT/'moc_effect_preferences.cpp'],env)
    flags=['c++','-target','x86_64-windows-gnu','-std=c++17','-O2','-DQT_NO_DEBUG','-DQT_CORE_LIB','-DQT_GUI_LIB','-DQT_QML_LIB','-DQT_QUICK_LIB','-DQT_NETWORK_LIB',*include]
    sources=[HERE/'effect_preferences.cpp',OUT/'moc_effect_preferences.cpp']
    libs=[SDK/'lib'/('libQt6'+m+'.a') for m in ['Quick','Qml','Network','Gui','Core']]
    run([ZIG,*flags,*sources,HERE/'test_effect_preferences.cpp',*libs,'-o',OUT/'native-ui-test.exe'],env)
    run([ZIG,*flags,'-shared',*sources,*libs,'-o',OUT/'x2d-native-ui.dll'],env)
    result=run([OUT/'native-ui-test.exe',OUT/'x2d-native-ui.dll'],env)
    assert 'NATIVE_UI_PASS' in result,result
    binaries={}
    for name in ['native-ui-test.exe','x2d-native-ui.dll']:
        b=(OUT/name).read_bytes();assert b.startswith(b'MZ') and b'qv4cdata' not in b
        binaries[name]={'sha256':hashlib.sha256(b).hexdigest(),'bytes':len(b),'qv4_magic':False}
    report={'qt':'6.4.1','architecture':'Windows x86_64','kind':'hand-written C++ QQuickItem, moc metadata only','test':result.strip(),'binaries':binaries,'source_hashes':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in HERE.glob('*') if p.is_file()},'camera_requests':0,'qml_documents_loaded':0,'arm64_validated':False,'package_integrated':False}
    (OUT/'native-ui-report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report,ensure_ascii=False,indent=2))

if __name__=='__main__':main()
