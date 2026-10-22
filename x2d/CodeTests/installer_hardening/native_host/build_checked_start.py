"""为已冻结包编译启动前校验清单，启动器不构成签名或防篡改绝对保证。"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
R7 = ROOT/'x2d/outputs/4.2.0/installer-hardening-r7'
ZIG = ROOT/'.research-cache/x1d-1.25.0/toolchain/zig-windows-x86_64-0.13.0/zig.exe'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def build(package, platform):
    assert Path.cwd().resolve() == ROOT
    package=package.resolve()
    assert package.is_relative_to(R7)
    self_name = 'X2dStart.exe' if platform == 'windows' else 'MacOS/X2DInstaller'
    entries={}
    for p in sorted(package.rglob('*')):
        assert not p.is_symlink() and not (getattr(p.lstat(),'st_file_attributes',0)&0x400)
        if p.is_file():
            name=p.relative_to(package).as_posix()
            # 许可证/说明不参与启动前检查，亦不持有它们的句柄。
            # universal-package.txt 是决定界面流程的必需配置，不属于说明文件。
            if platform == 'windows' and p.suffix.lower() in ('.txt', '.md') and name != 'universal-package.txt':
                continue
            if name!=self_name:entries[name]=dict(sha256=sha(p.read_bytes()), bytes=p.stat().st_size)
    assert entries and len(entries)<=20000
    original=(ROOT/'x2d/CodeTests/pixel_shift_rgb/receipt_sha256.h').read_text(encoding='utf8')
    portable=original.replace('#ifdef __aarch64__','#if defined(__aarch64__) && defined(__linux__)')
    source_name='windows_checked_start.cpp' if platform=='windows' else 'mac_checked_start.cpp'
    source={source_name:(HERE/source_name).read_bytes(),'package_guard.hpp':(HERE/'package_guard.hpp').read_bytes(),
            'portable_sha256.h':portable.encode('utf8')}
    header=['#pragma once','struct PackageEntry{const char*name;const char*sha256;unsigned long long bytes;};',
            'static const PackageEntry package_entries[]={']
    for name,value in entries.items():
        assert all(p not in ('','.','..') for p in name.split('/')) and '\\' not in name
        header.append('{'+json.dumps(name,ensure_ascii=False)+','+json.dumps(value['sha256'])+','+str(value['bytes'])+'ULL},')
    source['package_entries.hpp']=('\n'.join(header+['};',''])).encode('utf8')
    identity=dict(platform=platform,entries=entries,builder=sha(Path(__file__).read_bytes()),
                  sources={n:sha(b) for n,b in source.items()},zig=sha(ZIG.read_bytes()))
    revision=sha(json.dumps(identity,sort_keys=True).encode())[:20]
    out=R7/'native-host/checked-start'/revision
    proof=out/'report.json'
    if proof.exists():
        report=json.loads(proof.read_text(encoding='utf8'))
        assert report['identity']==identity
        assert all(sha((out/n).read_bytes())==h for n,h in report['files'].items())
        print(json.dumps(dict(directory=str(out),reused=True)));return out
    assert not out.exists(), '失败目录保留，不覆盖'
    frozen=out/'private-build/source';frozen.mkdir(parents=True)
    for n,b in source.items():(frozen/n).write_bytes(b)
    artifact=out/'artifacts';artifact.mkdir()
    env=dict(os.environ,TEMP=str(out/'private-build'),TMP=str(out/'private-build'),
             ZIG_LOCAL_CACHE_DIR=str(out/'private-build/zig-local'),ZIG_GLOBAL_CACHE_DIR=str(out/'private-build/zig-global'))
    targets=[('windows','x86_64-windows-gnu',['-municode','-static'],'X2dStart.exe')] if platform=='windows' else [
        ('arm64','aarch64-macos.11.0',[],'X2DInstaller-arm64'),('x86_64','x86_64-macos.11.0',[],'X2DInstaller-x86_64')]
    for arch,target,flags,name in targets:
        args=[ZIG,'c++','-target',target,'-Oz','-std=c++17','-Wall','-Wextra','-Werror','-s',*flags,
              '-ffile-prefix-map='+str(ROOT)+'=.',frozen/source_name,'-o',artifact/name]
        result=subprocess.run([str(a) for a in args],env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=300)
        (out/(arch+'-compiler.log')).write_text(result.stdout+result.stderr,encoding='utf8')
        assert result.returncode==0, result.stdout+result.stderr
    if platform=='mac':
        sys.path.insert(0,str(HERE.parent))
        from build import fat
        (artifact/'X2DInstaller').write_bytes(fat([(artifact/n).read_bytes() for n in ('X2DInstaller-arm64','X2DInstaller-x86_64')]))
    assert all(sha((package/n).read_bytes())==v['sha256'] for n,v in entries.items()),'构建期间包输入变动'
    files={p.relative_to(out).as_posix():sha(p.read_bytes()) for base in (frozen,artifact) for p in base.rglob('*') if p.is_file()}
    proof.write_text(json.dumps(dict(identity=identity,files=files,hardware_executed=False,
        release_ready=False,qualification='仅编译启动前全文件核验；自身仍需平台签名/可信分发，尚未完成包验收'),ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps(dict(directory=str(out),files=len(entries),release_ready=False),ensure_ascii=False))
    return out


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--package',type=Path,required=True)
    parser.add_argument('--platform',choices=('windows','mac'),required=True);args=parser.parse_args()
    build(args.package,args.platform)
