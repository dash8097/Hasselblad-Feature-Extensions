"""组装隔离的双平台内部客户端；不会更新旧包、生成发行 ZIP 或连接相机。"""
import argparse
import hashlib
import json
import os
import plistlib
from pathlib import Path
import shutil
import subprocess
import sys
import build_checked_start
import build_mac_runtime
import build_windows_runtime
import profile_ui
import windows_package_ui
import build_windows_internal

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
R7=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7'
H3=ROOT/'x2d/outputs/4.2.0/installer-hardening-r3'
H1=ROOT/'x2d/outputs/4.2.0/installer-hardening-r1'
CSC=Path('C:/Windows/Microsoft.NET/Framework64/v4.0.30319/csc.exe')


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def encoded(value):return (json.dumps(value,ensure_ascii=False,sort_keys=True,indent=2)+'\n').encode('utf8')


def read_proof(directory):
    value=json.loads((directory/'report.json').read_text(encoding='utf8'))
    assert not (directory/'INVALID-NOT-FOR-BUILD.txt').exists(), '候选已经明确失效'
    for name,digest in value['files'].items():
        path=(directory/name).resolve();assert path.is_relative_to(directory.resolve())
        assert sha(path)==digest
    return value


def build(sources,compiled):
    assert Path.cwd().resolve()==ROOT
    sources,compiled=sources.resolve(),compiled.resolve()
    assert sources.is_relative_to(R7) and compiled.is_relative_to(R7)
    sp,cp=read_proof(sources),read_proof(compiled)
    assert sp['identity']['source']==cp['identity']['source']
    platform=sp['identity']['platform'];assert platform==cp['identity']['platform']
    legacy=H3/'internal-candidates'/('windows-installer' if platform=='windows' else 'mac-installer/哈苏特调补丁.app/Contents/Resources')
    integrity=json.loads((legacy/'package-integrity.json').read_text(encoding='utf-8-sig'))
    selected=[]
    runtime_revision=None
    if platform=='windows':
        selected=['python311.dll','vcruntime140.dll','vcruntime140_1.dll','X2dTransport.dll',
                  'DOTNET-LICENSE.TXT','DOTNET-THIRD-PARTY-NOTICES.TXT',
                  'CYTHON-LICENSE.txt','runtime-provenance.json']
        selected+=sorted(n for n in integrity if n.startswith('runtime/'))
        selected.remove('runtime/python311.zip')
        runtime_revision=build_windows_runtime.build()
    else:
        selected=sorted(n for n in integrity if n.startswith('usb/') or n in ('CYTHON-LICENSE.txt','THIRD-PARTY.txt','dependency-sources.json'))
        runtime_revision=build_mac_runtime.build()
    for name in selected:assert name in integrity and not (legacy/name).is_symlink() and sha(legacy/name)==integrity[name]
    ui_names=('RawStackInstaller.cs','PatchDiagnosticReport.cs','PatchUpdateState.cs')
    ui_root=HERE/'build_inputs/windows_ui'
    identity=dict(platform=platform,builder=sha(Path(__file__)),profile_ui=sha(Path(profile_ui.__file__)),source_report=sha(sources/'report.json'),
                  compiled_report=sha(compiled/'report.json'),runtime={n:integrity[n] for n in selected},
                  checked_builder=sha(HERE/'build_checked_start.py'),
                  checked_sources={n:sha(HERE/n) for n in ('package_guard.hpp','windows_checked_start.cpp','mac_checked_start.cpp')})
    if platform=='windows':
        identity['ui']={n:sha(ui_root/n) for n in ui_names}
        identity['windows_package_ui']=sha(Path(windows_package_ui.__file__))
        identity['internal_modes']={str(p.relative_to(HERE.parent)):sha(p) for p in (
            HERE/'build_windows_internal.py',HERE/'windows_internal_modes.cs',
            HERE.parent/'windows_engine_host.c',HERE.parent/'WindowsPreflight.cs')}
    identity['runtime_report']=sha(runtime_revision/'report.json')
    revision=hashlib.sha256(encoded(identity)).hexdigest()[:20]
    out=R7/'native-host/assembled'/revision
    assert not out.exists(),'不覆盖组包或失败现场'
    private=out/'private-build';private.mkdir(parents=True)
    package=out/'candidate'
    resource=package if platform=='windows' else package/'哈苏特调补丁.app/Contents/Resources'
    resource.mkdir(parents=True)
    for name in selected:
        dest=resource/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(legacy/name,dest)
    if runtime_revision:
        runtime_proof=read_proof(runtime_revision)
        for name,digest in runtime_proof['files'].items():
            relative=Path(name).relative_to('artifacts');dest=resource/relative
            dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(runtime_revision/name,dest)
            assert sha(dest)==digest
    for path in (sources/'resources').rglob('*'):
        if path.is_file():
            dest=resource/path.relative_to(sources/'resources');dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(path,dest)
    sys.path.insert(0,str(HERE.parent))
    from build import fat
    modules=sorted(Path(n).stem for n in sp['identity']['source'])
    for name in modules:
        if platform=='windows':
            dest=resource/'engine'/(name+'.pyd');dest.parent.mkdir(exist_ok=True)
            shutil.copyfile(compiled/'artifacts/win-x64'/(name+'.pyd'),dest)
        else:
            (resource/(name+'.so')).write_bytes(fat([(compiled/'artifacts'/arch/(name+'.so')).read_bytes() for arch in ('arm64','x86_64')]))
    release=json.loads((legacy/'package-release.json').read_text(encoding='utf-8-sig'))
    release.update(release='R7-INTERNAL',installer_revision='R7-INTERNAL',
                   candidate_acceptance=dict(release_ready=False,hardware_tested=False,
                     camera_payload_changed=True,first_party_shell_files=False,pending_native_ui=not bool(sp.get('native_ui_contract')),
                     third_party_python_runtime_bytecode=True))
    (resource/'package-release.json').write_bytes(encoded(release))
    (resource/'universal-package.txt').write_text('R7 INTERNAL NATIVE CLIENT\n',encoding='utf8')
    if platform=='windows':
        internal_sources=build_windows_internal.build(private,resource)
        required=sorted({p.relative_to(resource).as_posix() for p in resource.rglob('*')
                         if p.is_file() and (p.suffix.lower() not in ('.txt','.md') or p.name=='universal-package.txt')} |
                        {'HasselbladTuningPatchInstaller.exe'})
        ui=private/'ui';ui.mkdir()
        for name in ui_names:
            text=(ui_root/name).read_text(encoding='utf-8-sig')
            if name=='RawStackInstaller.cs':
                metadata=json.loads((sources/'resources/native/client-metadata.json').read_text(encoding='utf8'))
                if 'profiles' in metadata:text=profile_ui.windows(text)
                before='Path.Combine(root,"X2dEngine.exe")'
                assert text.count(before)==1
                text=text.replace(before,'Path.Combine(root,"HasselbladTuningPatchInstaller.exe")').replace('H3 内部候选','R7 内部候选')
                text=windows_package_ui.replace_once(text,'Arguments = "\\"" + root.TrimEnd(Path.DirectorySeparatorChar) + "\\"",',
                    'Arguments = "--engine \\"" + root.TrimEnd(Path.DirectorySeparatorChar) + "\\"",')
                text=windows_package_ui.frontend(text)
            elif name=='PatchDiagnosticReport.cs':
                text=windows_package_ui.diagnostic(text,required)
            (ui/name).write_text(text,encoding='utf-8-sig')
        env=dict(os.environ,TEMP=str(private),TMP=str(private))
        def run(args,log):
            result=subprocess.run([str(a) for a in args],env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=300)
            (private/log).write_text(result.stdout+result.stderr,encoding='utf8')
            assert result.returncode==0,result.stdout+result.stderr
        for target,flags in ((resource/'HasselbladTuningPatchInstaller.exe',[]),
                             (private/'Installer.UiTests.exe',['/define:INTERNAL_UI_TESTS'])):
            run([CSC,'/nologo','/target:winexe','/platform:x64','/optimize+','/debug-',*flags,
                '/reference:System.Windows.Forms.dll','/reference:System.Drawing.dll','/reference:System.Web.Extensions.dll',
                '/reference:System.Management.dll',
                '/out:'+str(target),*[ui/n for n in ui_names],*internal_sources],target.name+'.compile.log')
        # 私有自动 UI 测试必须进入与真实包相同的 universal 三按钮模式。
        (private/'universal-package.txt').write_text('R7 INTERNAL NATIVE CLIENT\n',encoding='utf8')
        import xml.etree.ElementTree as ET
        config=ET.Element('Obfuscator')
        for name,value in [('InPath',str(resource)),('OutPath',str(private/'obfuscated')),('KeepPublicApi','false'),
                           ('HidePrivateApi','true'),('HideStrings','true'),('RenameProperties','true'),('RenameEvents','true')]:
            ET.SubElement(config,'Var',name=name,value=value)
        ET.SubElement(config,'Module',file=str(resource/'HasselbladTuningPatchInstaller.exe'))
        config_path=private/'obfuscator.xml';ET.ElementTree(config).write(config_path,encoding='utf-8',xml_declaration=True)
        run([H1/'build/downloads/obfuscar/tools/Obfuscar.Console.exe',config_path],'obfuscator.log')
        shutil.copyfile(private/'obfuscated/HasselbladTuningPatchInstaller.exe',resource/'HasselbladTuningPatchInstaller.exe')
        (resource/'使用说明.txt').write_text(
            '请完整解压后打开 HasselbladTuningPatchInstaller.exe。\n'
            '安装包只有此一个 EXE；请保留其余 DLL、资源和依赖目录。\n'
            '打开界面只检查安装包；点击安装、卸载或诊断后才会连接相机。\n'
            '安装包检查通过不表示已安装，也不表示相机功能验收通过。\n',
            encoding='utf8')
    else:
        (resource.parent/'MacOS').mkdir()
        original=legacy.parent/'Info.plist'
        info=plistlib.loads(original.read_bytes())
        info['CFBundleShortVersionString']='5.8.52'
        info['CFBundleVersion']='5.8.52.700'
        info['CFBundleDisplayName']='X2D R7 内部候选'
        (resource.parent/'Info.plist').write_bytes(plistlib.dumps(info))
    # 外部 JSON 用于诊断逐项说明；其内容和所有文件再绑定到原生启动器。
    manifest={p.relative_to(resource).as_posix():sha(p) for p in sorted(resource.rglob('*')) if p.is_file()}
    (resource/'package-integrity.json').write_bytes(encoded(manifest))
    guard=None
    if platform!='windows':
        guard=build_checked_start.build(resource.parent,platform)
    assert all(sha(HERE/n)==digest for n,digest in identity['checked_sources'].items()), '组装期间启动器源码已变化'
    if guard:
        shutil.copyfile(guard/'artifacts/X2DInstaller',resource.parent/'MacOS/X2DInstaller')
    else:
        assert [p.name for p in resource.rglob('*.exe')]==['HasselbladTuningPatchInstaller.exe']
    files={p.relative_to(out).as_posix():sha(p) for p in sorted(package.rglob('*')) if p.is_file()}
    proof=dict(identity=identity,files=files,checked_start_report=sha(guard/'report.json') if guard else None,
               camera_requests=0,release_ready=False,public_archive_created=False,
               native_ui_contract=sp.get('native_ui_contract'),third_party_python_runtime_bytecode=True,
               blockers=([] if sp.get('native_ui_contract') else ['QML/JS仍有待迁移'])+['尚待独立完整客户端与反编译验收','Mac原生及相机真实USB未执行'])
    (out/'report.json').write_bytes(encoded(proof))
    print(json.dumps(dict(directory=str(out),platform=platform,release_ready=False),ensure_ascii=False))
    return out


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--sources',type=Path,required=True)
    parser.add_argument('--compiled',type=Path,required=True);args=parser.parse_args();build(args.sources,args.compiled)
