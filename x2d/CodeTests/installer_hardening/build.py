"""客户端加固阶段一；冻结 R52，输出独立候选，不操作设备。"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tarfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
OUT = ROOT/'x2d/outputs/4.2.0/installer-hardening-r1'
WORK = OUT/'build'
BASE = ROOT/'x2d/outputs/4.2.0/raw-stack/full-installer-r52'
WIN = OUT/'windows-installer'
MAC = OUT/'mac-installer'
RES = MAC/'哈苏特调补丁.app/Contents/Resources'
FROZEN_RES = BASE/'mac-installer/哈苏特调补丁.app/Contents/Resources'
ZIG = ROOT/'.research-cache/x1d-1.25.0/toolchain/zig-windows-x86_64-0.13.0/zig.exe'
CSC = Path('C:/Windows/Microsoft.NET/Framework64/v4.0.30319/csc.exe')
MODULES = ('mac_usb', 'mac_diagnostics', 'package_feature_selection', 'mac_patch', 'mac_gui')
ENV = dict(os.environ, TEMP=str(WORK), TMP=str(WORK), PYTHONDONTWRITEBYTECODE='1',
           PYTHONIOENCODING='utf-8', NUGET_PACKAGES=str(WORK/'nuget'),
           DOTNET_CLI_HOME=str(WORK/'dotnet-home'), DOTNET_CLI_TELEMETRY_OPTOUT='1',
           DOTNET_GENERATE_ASPNET_CERTIFICATE='false', DOTNET_SKIP_FIRST_TIME_EXPERIENCE='1',
           PSModulePath='C:/Windows/System32/WindowsPowerShell/v1.0/Modules',
           ZIG_LOCAL_CACHE_DIR=str(WORK/'zig-local'), ZIG_GLOBAL_CACHE_DIR=str(WORK/'zig-global'))

def read(p): return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def write(p, value): p.write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
def run(*args, **kwargs):
    subprocess.run([str(a) for a in args],env=ENV,check=True,timeout=600,**kwargs)
def replace_once(text,old,new):
    assert text.count(old)==1, (old[:100],text.count(old))
    return text.replace(old,new)
def integrity(root):
    write(root/'package-integrity.json',{p.relative_to(root).as_posix():sha(p) for p in sorted(root.rglob('*'))
          if p.is_file() and p.name!='package-integrity.json'})

def native_windows():
    dest=WORK/'native'; dest.mkdir(parents=True,exist_ok=True)
    payload=BASE/'windows-installer/routes/stock/payload'
    original=(payload/'WinUsbReadOnly.cs').read_text(encoding='utf-8-sig')
    methods=(payload/'ShellTransport.inc.cs').read_text(encoding='utf-8-sig').replace('TemporaryUiUsb','Usb')
    audit=(BASE/'windows-installer/AuditInstallation.ps1').read_text(encoding='utf-8-sig')
    fault=audit.split("$extension=@'\n",1)[1].split("\n'@",1)[0]
    fault=fault.replace('ReadFaultOnly()', 'ReadFaultOnly(string[] commands)')
    fault=replace_once(fault,'string[] commands=new string[]{COMMANDS};',
        'if(commands==null||commands.Length==0||commands.Length>512)throw new UsbFailure("BATCH_LIMIT");')
    for namespace,timeout,extension in [('NativeTransport',45000,''),('FaultTransport',2000,fault),('DrainTransport',1000,''),('CaptureTransport',2000,'')]:
        text=original.replace('namespace HasselbladDebug','namespace '+namespace).replace('WinUsbReadOnly','Usb')
        text=replace_once(text,'uint timeout = 2000;',f'uint timeout = {timeout};')
        body=methods
        if namespace=='NativeTransport':
            body=replace_once(body,'output.Add(s.Shell(commands[i],tag+(uint)i));',
                'output.Add(s.Shell(commands[i],tag+(uint)i)); System.Threading.Thread.Sleep(50);')
        text=replace_once(text,'        public void Dispose() {',body+extension+'        public void Dispose() {')
        text=re.sub(r'Marshal.SizeOf\(typeof\((\w+)\)\)',r'Marshal.SizeOf<\1>()',text)
        (dest/(namespace+'.cs')).write_text(text,encoding='utf-8')
    shutil.copyfile(HERE/'NativeExports.cs',dest/'NativeExports.cs')
    values=','.join(json.dumps(x) for x in read(HERE/'decoys.json'))
    (dest/'AnalysisDecoys.cs').write_text('internal static class AnalysisDecoys { internal static readonly string[] Values = new string[] {'+values+'}; }',encoding='utf-8')
    (dest/'X2dTransport.csproj').write_text('''<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup><TargetFramework>net9.0</TargetFramework><PublishAot>true</PublishAot>
  <NativeLib>Shared</NativeLib><AllowUnsafeBlocks>true</AllowUnsafeBlocks>
  <InvariantGlobalization>true</InvariantGlobalization><OptimizationPreference>Size</OptimizationPreference>
  <DebugType>none</DebugType><DebugSymbols>false</DebugSymbols><StripSymbols>true</StripSymbols>
  <IlcGenerateStackTraceData>false</IlcGenerateStackTraceData><Nullable>disable</Nullable>
  </PropertyGroup></Project>''',encoding='utf-8')
    vc=Path('C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Tools/MSVC/14.44.35207')
    sdk=WORK/'downloads/microsoft.windows.sdk.cpp.x64'
    libraries=sorted({p.parent for p in sdk.rglob('*.lib')})
    assert libraries, 'Windows SDK native libraries missing'
    ENV['LIB']=';'.join(str(p) for p in [vc/'lib/x64']+libraries)
    ENV['PATH']=str(vc/'bin/Hostx64/x64')+';'+os.environ['PATH']
    run('dotnet','publish',dest/'X2dTransport.csproj','-c','Release','-r','win-x64','-o',dest/'publish',
        '-p:IlcTreatWarningsAsErrors=true','-p:IlcUseEnvironmentalTools=true')
    run(CSC,'/nologo','/target:library','/optimize+','/debug-',
        '/reference:System.Web.Extensions.dll','/out:'+str(dest/'X2dTransportAdapter.dll'),HERE/'TransportAdapter.cs')
    print('WINDOWS_NATIVE_CORE_BUILT',flush=True)

def prepare_windows():
    assert not WIN.exists(),'Candidate already exists; refuse overwrite'
    shutil.copytree(BASE/'windows-installer',WIN)
    shutil.copyfile(WORK/'native/publish/X2dTransport.dll',WIN/'X2dTransport.dll')
    shutil.copyfile(WORK/'native/X2dTransportAdapter.dll',WIN/'X2dTransportAdapter.dll')
    loader="Add-Type -Path (Join-Path {root} 'X2dTransportAdapter.dll')\n[X2dNativeBridge]::Configure((Join-Path {root} 'X2dTransport.dll'))\n"
    for filename,start,end,root in [
        ('load_tuning_patch.ps1',"$source=Get-Content",'if([TuningPatch.PatchUsb]::SelfTestShell()', '$PackageRoot'),
        ('routes/stock/load_raw_stack_backend.ps1',"$taskSource=Get-Content",'if([StackPublic.StackUsb]::SelfTestShell()',"(Join-Path $PackageRoot '../../..')"),
        ('AutoCapture.ps1',"$source=Get-Content",'if([CaptureSequence.Usb]::SelfTestShell()', '$package')]:
        p=WIN/filename;text=p.read_text(encoding='utf-8-sig')
        a=text.index(start);b=text.index(end,a)
        text=text[:a]+loader.format(root=root)+text[b:]
        text=text.replace("'routes/stock/payload/ShellTransport.inc.cs','routes/stock/payload/WinUsbReadOnly.cs'",
                          "'X2dTransport.dll','X2dTransportAdapter.dll'")
        text=text.replace("'routes/stock/payload/WinUsbReadOnly.cs','routes/stock/payload/ShellTransport.inc.cs'",
                          "'X2dTransport.dll','X2dTransportAdapter.dll'")
        p.write_text(text,encoding='utf-8-sig')
    p=WIN/'AuditInstallation.ps1';text=p.read_text(encoding='utf-8-sig')
    text=text.replace("'routes/stock/payload/WinUsbReadOnly.cs','routes/stock/payload/ShellTransport.inc.cs'",
                      "'X2dTransport.dll','X2dTransportAdapter.dll'")
    a=text.index('$literals=');b=text.index('if([RawStackFault.Usb]::SelfTestShell()',a)
    text=text[:a]+loader.format(root='$package')+text[b:]
    text=text.replace('[RawStackFault.Usb]::ReadFaultOnly()', '[RawStackFault.Usb]::ReadFaultOnly([string[]]@($commands.Values))')
    p.write_text(text,encoding='utf-8-sig')
    for name in ('WinUsbReadOnly.cs','ShellTransport.inc.cs'):
        (WIN/'routes/stock/payload'/name).unlink()
    # Diagnostic renderer itself checks required client files: remove obsolete source requirements
    # in a freshly compiled copy, preserving the frozen UI and diagnostic semantics.
    ui=WORK/'windows-ui';ui.mkdir(exist_ok=True)
    # These sources are saved by the R52 builder beside the package, not reconstructed from IL.
    for name in ('RawStackInstaller.cs','PatchDiagnosticReport.cs','PatchUpdateState.cs'):
        text=(BASE/name).read_text(encoding='utf-8-sig')
        if name=='PatchDiagnosticReport.cs':
            text=replace_once(text,'routes/stock/payload/ShellTransport.inc.cs|routes/stock/payload/WinUsbReadOnly.cs|',
                              'X2dTransport.dll|X2dTransportAdapter.dll|')
        (ui/name).write_text(text,encoding='utf-8-sig')
    run(CSC,'/nologo','/target:winexe','/platform:x64','/optimize+','/debug-',
        '/reference:System.Windows.Forms.dll','/reference:System.Drawing.dll','/reference:System.Web.Extensions.dll',
        '/out:'+str(WIN/'HasselbladTuningPatchInstaller.exe'),
        ui/'RawStackInstaller.cs',ui/'PatchDiagnosticReport.cs',ui/'PatchUpdateState.cs')
    obfuscate_windows()

def obfuscate_windows():
    import xml.etree.ElementTree as ET
    obf=ET.Element('Obfuscator')
    for name,value in [('InPath',str(WIN)),('OutPath',str(WORK/'obfuscated')),('KeepPublicApi','false'),
                       ('HidePrivateApi','true'),('HideStrings','true'),('ReuseNames','true'),
                       ('RenameProperties','true'),('RenameEvents','true'),('RenameFields','true')]:
        ET.SubElement(obf,'Var',name=name,value=value)
    ET.SubElement(obf,'Module',file=str(WIN/'HasselbladTuningPatchInstaller.exe'))
    config=WORK/'obfuscar.xml';ET.ElementTree(obf).write(config,encoding='utf-8')
    run(WORK/'downloads/obfuscar/tools/Obfuscar.Console.exe',config)
    shutil.copyfile(WORK/'obfuscated/HasselbladTuningPatchInstaller.exe',WIN/'HasselbladTuningPatchInstaller.exe')
    release=read(WIN/'package-release.json')
    release['client_hardening']={'stage':1,'windows_transport':'NativeAOT win-x64',
        'powershell_orchestration_still_readable':True,'managed_ui_obfuscated':True,
        'device_tested':False,'camera_payload_changed':False}
    write(WIN/'package-release.json',release)
    native_notices()
    integrity(WIN)
    print('WINDOWS_CANDIDATE_PREPARED',flush=True)

def native_notices():
    source=WORK/'nuget/runtime.win-x64.microsoft.dotnet.ilcompiler/9.0.0'
    for name in ('LICENSE.TXT','THIRD-PARTY-NOTICES.TXT'):
        shutil.copyfile(source/name,WIN/('DOTNET-'+name))

def fat(slices):
    result=bytearray(struct.pack('>II',0xcafebabe,len(slices))); pos=16384
    for data in slices:
        cpu,sub=struct.unpack_from('<II',data,4)
        result.extend(struct.pack('>IIIII',cpu,sub,pos,len(data),14))
        pos=(pos+len(data)+16383)&~16383
    for data in slices:
        result.extend(b'\0'*((-len(result))%16384));result.extend(data)
    return result

def mac_sources():
    src=WORK/'cython';src.mkdir(parents=True,exist_ok=True)
    for name in MODULES:
        text=(FROZEN_RES/(name+'.py')).read_text(encoding='utf-8-sig')
        if name=='mac_patch':
            for module in MODULES:text=text.replace("'"+module+".py'","'"+module+".so'")
        if name=='mac_gui':
            text=replace_once(text,"if __name__ == '__main__':",'def main():')
        if name=='mac_usb':
            text+='\n__analysis_notes__ = '+repr(tuple(read(HERE/'decoys.json')))+'\n'
        (src/(name+'.py')).write_text(text,encoding='utf-8')
    sys.path.insert(0,str(WORK/'deps'))
    from Cython.Compiler import Options, Main
    Options.docstrings=False
    for name in MODULES:
        options=Main.CompilationOptions(Main.default_options,output_file=str(src/(name+'.c')),
            compiler_directives={'language_level':3,'emit_code_comments':False,'embedsignature':False,
                                 'binding':True,'linetrace':False})
        result=Main.compile(str(src/(name+'.py')),options=options)
        assert result.num_errors==0,name
    print('MAC_CYTHON_GENERATED',flush=True)

def compile_mac():
    deps=ROOT/'x2d/outputs/4.2.0/raw-stack/compact-client-r1/dependencies'
    src=WORK/'cython'
    for arch,target,identifier in [('arm64','aarch64-macos.11.0','aarch64'),('x86_64','x86_64-macos.11.0','x86_64')]:
        dest=WORK/arch;dest.mkdir(exist_ok=True)
        with tarfile.open(next(deps.glob('*'+identifier+'*tar.gz'))) as tar:
            for member in tar.getmembers():
                if member.isfile() and member.name.startswith('python/include/python3.12/'):
                    relative=Path(member.name).relative_to('python/include/python3.12')
                    assert '..' not in relative.parts
                    p=dest/'include'/relative;p.parent.mkdir(parents=True,exist_ok=True)
                    p.write_bytes(tar.extractfile(member).read())
        for name in MODULES:
            run(ZIG,'cc','-target',target,'-O2','-s','-fvisibility=hidden','-shared','-undefined','dynamic_lookup',
                '-Wl,-install_name,@rpath/'+name+'.so',
                '-Wno-unused-function','-Wno-deprecated-declarations','-I'+str(dest/'include'),
                '-DCYTHON_CLINE_IN_TRACEBACK=0','-ffile-prefix-map='+str(ROOT)+'=.',
                '-ffile-prefix-map='+ROOT.as_posix()+'=.',src/(name+'.c'),'-o',dest/(name+'.so'))
            from macho_symbols import sanitize
            binary=dest/(name+'.so');raw=binary.read_bytes()
            (dest/(name+'.unstripped.so')).write_bytes(raw)
            cleaned,count=sanitize(raw);binary.write_bytes(cleaned)
            print('MAC_LOCAL_SYMBOL_NAMES_REMOVED',arch,name,count,flush=True)
        print('MAC_NATIVE_BUILT',arch,flush=True)
    compile_host()

def compile_host():
    # Host native compilation enables real behavioral regression without claiming macOS execution.
    src=WORK/'cython'
    host=WORK/'host';host.mkdir(exist_ok=True)
    pyroot=Path(sys.base_prefix)
    for name in MODULES:
        run(ZIG,'cc','-target','x86_64-windows-gnu','-O2','-shared','-Wno-unused-function', '-DMS_WIN64',
            '-I'+str(pyroot/'include'),'-DCYTHON_CLINE_IN_TRACEBACK=0',src/(name+'.c'),
            pyroot/'libs/python311.lib','-o',host/(name+'.pyd'))
    print('HOST_NATIVE_MODULES_BUILT',flush=True)

def prepare_mac():
    assert not MAC.exists(),'Candidate already exists; refuse overwrite'
    shutil.copytree(BASE/'mac-installer',MAC)
    for name in MODULES:
        (RES/(name+'.so')).write_bytes(fat([(WORK/arch/(name+'.so')).read_bytes() for arch in ('arm64','x86_64')]))
        (RES/(name+'.py')).unlink()
    # The native entry point imports compiled GUI; no application Python is extracted at runtime.
    launcher=(ROOT/'x2d/CodeTests/pixel_shift_rgb/mac_launcher.c').read_text(encoding='utf-8')
    launcher=launcher.replace('%s/../Resources/mac_gui.py','%s/../Resources')
    launcher=replace_once(launcher,'char *args[] = {runtime,"-I","-B",gui,NULL};',
        'char *args[] = {runtime,"-I","-B","-c","import sys;sys.path.insert(0,sys.argv[1]);import mac_gui;mac_gui.main()",gui,NULL};')
    source=WORK/'mac_launcher.c';source.write_text(launcher,encoding='utf-8')
    slices=[]
    for arch,target in [('arm64','aarch64-macos.11.0'),('x86_64','x86_64-macos.11.0')]:
        p=WORK/('launcher-'+arch)
        run(ZIG,'cc','-target',target,'-O2','-Wall','-Wextra','-Werror',source,'-o',p)
        slices.append(p.read_bytes())
    (RES.parent/'MacOS/X2DInstaller').write_bytes(fat(slices))
    release=read(RES/'package-release.json')
    release['client_hardening']={'stage':1,'application_modules':'Cython native arm64+x86_64',
        'macOS_runtime_tested':False,'device_tested':False,'camera_payload_changed':False}
    write(RES/'package-release.json',release)
    with (RES/'THIRD-PARTY.txt').open('a',encoding='utf-8') as f:
        f.write('\nApplication modules compiled with Cython 3.1.2 (Apache-2.0).\n')
    license_path=next((WORK/'deps').glob('cython-*.dist-info/licenses/LICENSE.txt'))
    shutil.copyfile(license_path,RES/'CYTHON-LICENSE.txt')
    integrity(RES)
    print('MAC_CANDIDATE_PREPARED',flush=True)

def refresh_native():
    """仅更新本轮候选中的原生构建产物；不会触及冻结 R52。"""
    assert WIN.is_dir() and RES.is_dir()
    shutil.copyfile(WORK/'native/publish/X2dTransport.dll',WIN/'X2dTransport.dll')
    shutil.copyfile(WORK/'native/X2dTransportAdapter.dll',WIN/'X2dTransportAdapter.dll')
    native_notices()
    for name in MODULES:
        (RES/(name+'.so')).write_bytes(fat([(WORK/arch/(name+'.so')).read_bytes() for arch in ('arm64','x86_64')]))
    integrity(WIN);integrity(RES)
    print('CANDIDATE_NATIVE_REFRESHED',flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('step',choices=['native-windows','prepare-windows','obfuscate-windows','mac-sources','compile-mac','compile-host','prepare-mac','refresh-native'])
    args=p.parse_args();WORK.mkdir(parents=True,exist_ok=True)
    globals()[args.step.replace('-','_')]()
