"""把 CPython 固定宿主构建为真正的 DLL，主机检查编入唯一 GUI EXE。"""
import os
from pathlib import Path
import subprocess
import sys

from build_checked_start import ZIG

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]


def build(private, resource):
    assert sys.version_info[:2] == (3, 11)
    original = HERE.parent / 'windows_engine_host.c'
    text = original.read_text(encoding='utf8')
    before = 'int wmain(int argc,wchar_t **argv) {\n    if(argc!=2) return 2;'
    assert text.count(before) == 1
    text = text.replace(before, '''__declspec(dllexport) int x2d_engine_run(const wchar_t *root) {
    int argc=2;
    wchar_t *argv[]={L"HasselbladTuningPatchInstaller.exe",(wchar_t*)root};''')
    source = private / 'windows_engine_dll.c'
    source.write_text(text, encoding='utf8')
    pyroot = Path(sys.base_prefix)
    env = dict(os.environ, TEMP=str(private), TMP=str(private),
               ZIG_LOCAL_CACHE_DIR=str(private/'zig-local'),
               ZIG_GLOBAL_CACHE_DIR=str(private/'zig-global'))
    args = [ZIG, 'cc', '-target', 'x86_64-windows-gnu', '-O2', '-s', '-shared',
            '-I'+str(pyroot/'include'), '-ffile-prefix-map='+str(ROOT)+'=.',
            source, pyroot/'libs/python311.lib', '-o', resource/'X2dEngine.dll']
    result = subprocess.run([str(a) for a in args], env=env, capture_output=True,
                            text=True, encoding='utf8', errors='replace', timeout=300)
    (private/'engine-dll-build.log').write_text(result.stdout+result.stderr, encoding='utf8')
    assert result.returncode == 0, result.stdout+result.stderr
    # 导入库只供链接使用，不随安装包分发。
    extra = resource/'X2dEngine.lib'
    if extra.exists():
        extra.rename(private/extra.name)
    host = (HERE.parent/'WindowsPreflight.cs').read_text(encoding='utf8')
    assert host.count('static int Main(string[] args)') == 1
    host = host.replace('static int Main(string[] args)', 'internal static int Run(string[] args)')
    host = host.replace('Console.OutputEncoding=new UTF8Encoding(false);', '')
    # 混淆后的字段名不能成为 Python 端 JSON 协议；显式写固定字符串键。
    before='new {snapshot=s,error=reason}'
    assert host.count(before)==1
    host=host.replace(before, '''new Dictionary<string,object>{
        {"snapshot",new Dictionary<string,object>{
            {"Versions",s.Versions},{"Running",s.Running},
            {"DriverPackages",s.DriverPackages},{"WinUsbAvailable",s.WinUsbAvailable},
            {"BindingIssues",s.BindingIssues}}},
        {"error",reason}}''')
    (private/'WindowsPreflight.cs').write_text(host, encoding='utf8')
    return [private/'WindowsPreflight.cs', HERE/'windows_internal_modes.cs']
