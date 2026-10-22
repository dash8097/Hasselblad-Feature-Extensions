"""仅编译内部 QML 输入，不实例化、不启用网络；保留实际生成 C++ 的证据。"""
from pathlib import Path
import ctypes,hashlib,json,re,subprocess

ROOT=Path(__file__).resolve().parents[4]
OUT=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/native-ui'
BASE=ROOT/'x2d/outputs/4.2.0/installer-hardening-r3/camera-iso-candidate-v4/public-install'
SDK=OUT/'qt-sdk/6.4.1/mingw_64'

def main():
    ctypes.windll.kernel32.SetErrorMode(0x8003)
    probe=OUT/'qmltc-probe';probe.mkdir(exist_ok=True)
    manifest=json.loads((BASE/'install-manifest.json').read_text(encoding='utf-8-sig'))
    rows=[]
    for p in sorted(BASE.iterdir()):
        if p.suffix not in ('.qml','.js') or p.name.startswith('v-'):continue
        s=p.read_text(encoding='utf-8-sig')
        rows.append({'file':p.name,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'target':manifest.get('targets',{}).get(p.name),'imports':re.findall(r'^\s*(?:import|\.import)\s+([^\r\n]+)',s,re.M),'functions':len(re.findall(r'\bfunction\s+\w+\s*\(',s)),'dynamic':[x for x in ['Qt.createQmlObject','Qt.createComponent','Qt.binding','XMLHttpRequest','eval('] if x in s]})
    (OUT/'h3-ui-inventory.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    sources={'TypedProbe.qml':b'import QtQml\nQtObject { property int mode: 0; function choose(next: int): int { return next >= 0 && next <= 2 ? next : mode; } }\n',
             'X2dEffectPreferences.qml':(BASE/'p88-X2dEffectPreferences.qml').read_bytes(),
             'X2dFaceModeController.qml':(BASE/'p10-X2dFaceModeController.qml').read_bytes()}
    results={}
    for name,data in sources.items():
        path=probe/name;path.write_bytes(data);stem=path.stem
        resource=probe/(stem+'.qrc');resource.write_text('<RCC><qresource prefix="/probe"><file>'+name+'</file></qresource></RCC>',encoding='utf-8')
        cpp=probe/(stem+'.cpp');header=probe/(stem+'.h')
        args=[SDK/'bin/qmltc.exe','-I',SDK/'qml','--resource',resource,'--impl',cpp,'--header',header,path]
        p=subprocess.run([str(x) for x in args],capture_output=True,text=True,timeout=30)
        (probe/(stem+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8')
        result={'rc':p.returncode,'input_sha256':hashlib.sha256(data).hexdigest()}
        if not p.returncode:
            code=cpp.read_text(encoding='utf-8');result.update(generated_cpp_sha256=hashlib.sha256(cpp.read_bytes()).hexdigest(),runtime_calls=code.count('executeRuntimeFunction'),dynamic_bindings=code.count('createBindingForNonBindable'))
        results[name]=result
    assert results['TypedProbe.qml']['rc']==0 and results['TypedProbe.qml']['runtime_calls']>0
    assert results['X2dEffectPreferences.qml']['rc']==0 and results['X2dEffectPreferences.qml']['runtime_calls']>0
    assert results['X2dFaceModeController.qml']['rc']!=0
    report={'qt':'6.4.1','qmltc_sha256':hashlib.sha256((SDK/'bin/qmltc.exe').read_bytes()).hexdigest(),'results':results,'camera_requests':0,'components_instantiated':0,'limits':'qmltc generated object C++ still invokes QQmlEnginePrivate::executeRuntimeFunction; it is not all-native business logic.'}
    (OUT/'qmltc-route-report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');print(json.dumps(report,ensure_ascii=False,indent=2))

if __name__=='__main__':main()
