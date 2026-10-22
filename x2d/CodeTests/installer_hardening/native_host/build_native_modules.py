"""冻结已生成的业务源码，分别编译 Windows 和 macOS 原生模块；不组装发行包。"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tarfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
HARDENING = HERE.parent
R7 = ROOT / 'x2d/outputs/4.2.0/installer-hardening-r7'
ZIG = ROOT / '.research-cache/x1d-1.25.0/toolchain/zig-windows-x86_64-0.13.0/zig.exe'
DEPS = ROOT / 'x2d/outputs/4.2.0/installer-hardening-r1/build/deps'
MAC_DEPS = ROOT / 'x2d/outputs/4.2.0/raw-stack/compact-client-r1/dependencies'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build(source, platform):
    assert Path.cwd().resolve() == ROOT
    assert sys.version_info[:2] == (3, 11), '使用固定 Python 3.11 构建宿主'
    source = source.resolve()
    assert source.is_relative_to(R7.resolve()) and source.is_dir()
    paths = sorted(source.glob('*.py'))
    assert paths and all(re.fullmatch('[a-z][a-z0-9_]*', p.stem) for p in paths)
    assert not any(p.is_symlink() for p in paths)
    inputs = {p.name: sha(p) for p in paths}
    pyroot = Path(sys.base_prefix)
    identity = dict(source=inputs, platform=platform, builder=sha(Path(__file__)),
                    zig=sha(ZIG), cython=sha(DEPS/'Cython/Compiler/Main.py'),
                    symbol_filter=sha(HARDENING/'macho_symbols.py'))
    archives = []
    if platform == 'mac':
        for identifier in ('aarch64', 'x86_64'):
            candidates = list(MAC_DEPS.glob('*'+identifier+'*tar.gz'))
            assert len(candidates) == 1
            archives.append(candidates[0])
        identity['python_headers'] = {p.name: sha(p) for p in archives}
    else:
        identity['python_headers'] = {p.relative_to(pyroot).as_posix(): sha(p)
            for p in sorted((pyroot/'include').rglob('*.h'))}
        identity['python_library'] = sha(pyroot/'libs/python311.lib')
    revision = hashlib.sha256(json.dumps(identity, sort_keys=True).encode()).hexdigest()[:20]
    out = R7/'native-host/compiled'/revision
    report = out/'report.json'
    if report.exists():
        value = json.loads(report.read_text(encoding='utf8'))
        assert value['identity'] == identity
        for name, digest in value['files'].items():
            assert sha(out/name) == digest
        print(json.dumps(dict(directory=str(out), reused=True, release_ready=False)))
        return out
    assert not out.exists(), '保留失败构建；不覆盖未完成目录'
    private = out/'private-build'
    frozen = private/'source'
    frozen.mkdir(parents=True)
    for p in paths:
        shutil.copyfile(p, frozen/p.name)
    env = dict(os.environ, TEMP=str(private), TMP=str(private), PYTHONDONTWRITEBYTECODE='1',
               ZIG_LOCAL_CACHE_DIR=str(private/'zig-local'), ZIG_GLOBAL_CACHE_DIR=str(private/'zig-global'))
    sys.path.insert(0, str(DEPS))
    from Cython.Compiler import Main, Options
    Options.docstrings = False
    generated = private/'generated'
    generated.mkdir()
    for p in paths:
        options = Main.CompilationOptions(Main.default_options, output_file=str(generated/(p.stem+'.c')),
            compiler_directives={'language_level': 3, 'emit_code_comments': False,
                                 'embedsignature': False, 'binding': True, 'linetrace': False})
        assert Main.compile(str(frozen/p.name), options=options).num_errors == 0
        # Cython 的 code.co_filename 不受编译器 prefix-map 处理；只改其生成的
        # 固定源码文件名常量，保留模块/行号诊断，不把本机构建布局带入产物。
        generated_file = generated/(p.stem+'.c')
        text = generated_file.read_text(encoding='utf8')
        filenames = (frozen/p.name).as_posix(), (frozen/p.name).relative_to(ROOT).as_posix()
        for name in filenames:
            text = text.replace(json.dumps(name), json.dumps('native:'+p.stem))
        assert all(name not in text for name in filenames), '生成 C 中仍有冻结源码路径'
        generated_file.write_text(text, encoding='utf8')
    log = private/'compiler.log'

    def run(args):
        result = subprocess.run([str(a) for a in args], env=env, capture_output=True,
                                text=True, encoding='utf8', errors='replace', timeout=600)
        with log.open('a', encoding='utf8') as stream:
            stream.write(json.dumps([str(a) for a in args])+'\n'+result.stdout+result.stderr+'\n')
        if result.returncode:
            raise RuntimeError('编译失败，保留日志：'+str(log))

    targets = [('win-x64', 'x86_64-windows-gnu', pyroot/'include')]
    if platform == 'mac':
        targets = []
        for arch, target, archive in zip(('arm64', 'x86_64'),
                ('aarch64-macos.11.0', 'x86_64-macos.11.0'), archives):
            include = private/arch/'include'
            include.mkdir(parents=True)
            with tarfile.open(archive) as tar:
                for member in tar:
                    prefix = 'python/include/python3.12/'
                    if not member.isfile() or not member.name.startswith(prefix):
                        continue
                    rel = Path(member.name[len(prefix):])
                    assert not rel.is_absolute() and '..' not in rel.parts and '\\' not in member.name
                    dest = include/rel
                    dest.parent.mkdir(parents=True, exist_ok=True)
                    dest.write_bytes(tar.extractfile(member).read())
            targets.append((arch, target, include))
    sys.path.insert(0, str(HARDENING))
    from macho_symbols import sanitize
    for arch, target, include in targets:
        artifact = out/'artifacts'/arch
        artifact.mkdir(parents=True)
        for p in paths:
            binary = artifact/(p.stem+('.pyd' if platform == 'windows' else '.so'))
            flags = ['-DMS_WIN64'] if platform == 'windows' else ['-undefined', 'dynamic_lookup',
                      '-Wl,-install_name,@rpath/'+p.stem+'.so']
            libraries = [pyroot/'libs/python311.lib'] if platform == 'windows' else []
            run([ZIG, 'cc', '-target', target, '-O2', '-s', '-shared', '-fvisibility=hidden',
                 '-Wno-unused-function', '-Wno-deprecated-declarations', '-DCYTHON_CLINE_IN_TRACEBACK=0',
                 '-ffile-prefix-map='+str(ROOT)+'=.', '-ffile-prefix-map='+ROOT.as_posix()+'=.',
                 '-I'+str(include), *flags, generated/(p.stem+'.c'), *libraries, '-o', binary])
            if platform == 'mac':
                raw = binary.read_bytes()
                (private/(arch+'-'+p.stem+'.unstripped.so')).write_bytes(raw)
                binary.write_bytes(sanitize(raw)[0])
            else:
                extra = binary.with_suffix('.lib')
                if extra.exists():
                    shutil.move(str(extra), str(private/extra.name))
        print('NATIVE_MODULES_BUILT '+arch, flush=True)
    assert all(sha(source/n) == digest for n, digest in inputs.items()), '构建期间源文件被修改'
    files = {p.relative_to(out).as_posix(): sha(p) for base in (frozen, generated, out/'artifacts')
             for p in sorted(base.rglob('*')) if p.is_file()}
    report.write_text(json.dumps(dict(identity=identity, files=files, release_ready=False,
        hardware_executed=False, mac_native_executed=False,
        scope='仅编译业务模块；无完整包、签名、界面或设备验收'), ensure_ascii=False, indent=2)+'\n', encoding='utf8')
    print(json.dumps(dict(directory=str(out), modules=len(paths), release_ready=False), ensure_ascii=False))
    return out


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--platform', choices=('windows', 'mac'), required=True)
    args = parser.parse_args()
    build(args.source, args.platform)
