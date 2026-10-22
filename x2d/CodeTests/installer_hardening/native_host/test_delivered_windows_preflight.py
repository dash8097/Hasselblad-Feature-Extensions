"""调用最终 ZIP 内正式 GUI 的真实包检查方法，故障注入不接触相机。"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import zipfile

from assemble_native_client import CSC, R7, ROOT, encoded, sha


HARNESS = r'''
using System;
using System.Linq;
using System.Reflection;
class Probe {
    [STAThread] static int Main(string[] args) {
        Console.OutputEncoding=new System.Text.UTF8Encoding(false);
        var assembly=Assembly.LoadFile(args[0]);
        var flags=BindingFlags.Static|BindingFlags.Public|BindingFlags.NonPublic;
        var type=assembly.GetTypes().Single(t=>t.GetFields(flags).Any(f=>
            f.FieldType==typeof(string[]) && ((string[])f.GetValue(null)).Contains("X2dEngine.dll")));
        var method=type.GetMethods(flags).Single(m=>m.ReturnType==typeof(void) &&
            m.GetParameters().Length==1 && m.GetParameters()[0].ParameterType==typeof(string));
        var held=new System.Collections.Generic.List<System.IO.FileStream>();
        if(args.Length==3 && args[2]=="--lock-docs")
            foreach(string path in System.IO.Directory.GetFiles(args[1],"*.txt",System.IO.SearchOption.AllDirectories))
                if(System.IO.Path.GetFileName(path)!="universal-package.txt")
                    held.Add(new System.IO.FileStream(path,System.IO.FileMode.Open,System.IO.FileAccess.ReadWrite,System.IO.FileShare.None));
        try { method.Invoke(null,new object[]{args[1]}); Console.WriteLine("PACKAGE_GUI_PREFLIGHT_OK"); return 0; }
        catch(TargetInvocationException ex) {
            Console.WriteLine(ex.InnerException.Message); return 1;
        }
        finally { foreach(var file in held) file.Dispose(); }
    }
}
'''


def main(archive):
    assert Path.cwd().resolve() == ROOT
    archive = archive.resolve()
    assert archive.is_relative_to(R7) and archive.suffix == '.zip'
    identity = {'archive_sha256': sha(archive), 'test_sha256': sha(Path(__file__))}
    revision = hashlib.sha256(encoded(identity)).hexdigest()[:20]
    out = R7 / 'native-host/delivered-preflight' / revision
    assert not out.exists(), '保留既有验证结果'
    out.mkdir(parents=True)
    (out / 'Probe.cs').write_text(HARNESS, encoding='utf8')
    env = dict(os.environ, TEMP=str(out), TMP=str(out), PYTHONDONTWRITEBYTECODE='1')
    result = subprocess.run([str(CSC), '/nologo', '/platform:x64', '/reference:System.Core.dll',
                             '/out:' + str(out / 'Probe.exe'), str(out / 'Probe.cs')],
                            env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    work = out / '解压测试 中文路径'
    work.mkdir()
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
        for item in z.infolist():
            assert (work / item.filename).resolve().is_relative_to(work.resolve())
        z.extractall(work)
    roots = list(work.iterdir())
    assert len(roots) == 1 and roots[0].is_dir()
    package = roots[0]
    original = {p.relative_to(package).as_posix(): p.read_bytes()
                for p in package.rglob('*') if p.is_file()}
    cases = []
    assert [n for n in original if n.lower().endswith('.exe')] == ['HasselbladTuningPatchInstaller.exe']

    def internal(name, args, code, expected, data=None):
        result = subprocess.run([str(package/'HasselbladTuningPatchInstaller.exe'), *args],
                                input=data, env=env, capture_output=True, text=True,
                                encoding='utf8', errors='replace', timeout=60)
        assert result.returncode == code, (name, result.returncode, result.stdout, result.stderr)
        assert expected in result.stdout+result.stderr, (name, result.stdout, result.stderr)
        cases.append({'name': name, 'passed': True, 'exit': result.returncode,
                      'output': (result.stdout+result.stderr).strip()})

    def backend(name):
        code = """
import sys
from pathlib import Path
from types import SimpleNamespace
root=Path(sys.argv[1]);sys.path.insert(0,str(root/'engine'))
import native_client_support as support
assert support.__file__.endswith('.pyd')
obj=SimpleNamespace(root=root,pending=False,feature_mask=255,_native_action='Install',progress=lambda _:None)
support.verify_package(obj)
assert obj.stock['feature_mask']==255
print('COMPILED_BACKEND_PREFLIGHT_OK device_requests=0')
"""
        result = subprocess.run([sys.executable, '-I', '-B', '-c', code, str(package)],
                                env=env, capture_output=True, text=True, timeout=60)
        assert result.returncode == 0, (name, result.stdout, result.stderr)
        cases.append({'name': name, 'passed': True, 'exit': 0, 'output': result.stdout.strip()})

    def probe(name, success, reason=None, lock_docs=False):
        result = subprocess.run([str(out / 'Probe.exe'),
                                 str(package / 'HasselbladTuningPatchInstaller.exe'), str(package)]
                                + (['--lock-docs'] if lock_docs else []),
                                env=env, capture_output=True, text=True, encoding='utf8',
                                errors='replace', timeout=90)
        assert (result.returncode == 0) == success, (name, result.returncode, result.stdout, result.stderr)
        if reason:
            assert reason in result.stdout, (name, result.stdout, result.stderr)
        cases.append({'name': name, 'passed': True, 'exit': result.returncode,
                      'output': result.stdout.strip()})

    probe('final-zip-production-gui-clean', True)
    internal('sole-exe-preflight-mode', ['--verify-package'], 0, 'PACKAGE_PREFLIGHT_OK')
    internal('sole-exe-native-dll-rejects-invalid-action-before-usb',
             ['--engine', str(package)], 2, 'NATIVE_ENGINE_REQUEST_REJECTED',
             '{"action":"Shell"}\n')
    internal('sole-exe-rejects-arbitrary-root', ['--engine', str(out)], 2, '')
    backend('final-zip-compiled-backend-clean')
    probe('documentation-exclusively-locked-does-not-block', True, lock_docs=True)
    extra = package / 'unexpected.exe'
    extra.write_bytes(b'not-executable')
    probe('extra-unrelated-file-ignored', True)
    backend('compiled-backend-extra-file-ignored')
    extra.unlink()
    launcher = package / 'X2dEngine.dll'
    launcher.unlink()
    probe('missing-engine-dll-refused', False, '必需文件')
    internal('worker-missing-dll-rejected-before-loading', ['--engine', str(package)], 2, '必需文件')
    launcher.write_bytes(original['X2dEngine.dll'])
    catalog = package / 'feature-catalog.json'
    catalog.write_bytes(original['feature-catalog.json'] + b'\n')
    probe('present-file-not-hash-gated', True)
    manifest_path = package / 'package-integrity.json'
    manifest = json.loads(manifest_path.read_text('utf-8-sig'))
    manifest['feature-catalog.json'] = sha(catalog)
    manifest_path.write_bytes(encoded(manifest))
    probe('external-hash-json-not-an-install-gate', True)
    catalog.write_bytes(original['feature-catalog.json'])
    manifest_path.write_bytes(original['package-integrity.json'])
    docs = [name for name in original if Path(name).suffix.lower() in ('.txt', '.md')
            and name != 'universal-package.txt']
    for name in docs:
        (package / name).unlink()
    probe('documentation-not-required', True)
    backend('compiled-backend-documentation-not-required')
    for name in docs:
        (package / name).write_bytes(original[name])
    marker = package / 'universal-package.txt'
    marker.unlink()
    probe('functional-txt-still-required', False, '必需文件')
    marker.write_bytes(original['universal-package.txt'])
    probe('restored-package-passes', True)
    assert {p.relative_to(package).as_posix(): p.read_bytes()
            for p in package.rglob('*') if p.is_file()} == original
    report = {'identity': identity, 'cases': cases, 'passed': True, 'camera_requests': 0,
              'production_gui_used': True, 'entry': '正式 GUI CheckPackage，通过反射调用',
              'limits': ['这是正式包的电脑端检查，不等于前台点击安装或相机安装验收；前台另行检查']}
    (out / 'report.json').write_bytes(encoded(report))
    print(out)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('archive', type=Path)
    main(parser.parse_args().archive)
