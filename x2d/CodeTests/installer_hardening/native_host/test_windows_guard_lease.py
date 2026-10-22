"""Windows句柄租约作者测试；自造子进程，无USB/真实客户端。"""
from pathlib import Path
import hashlib,json,os,shutil,subprocess

def main():
    HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[3]
    def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
    inputs={name:HERE/name for name in ('package_guard.hpp','windows_checked_start.cpp','test_windows_guard_lease.py')}
    hashes={name:sha(path) for name,path in inputs.items()}
    revision=hashlib.sha256(json.dumps(hashes,sort_keys=True).encode()).hexdigest()[:20]
    OUT=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/native-host/guard-lease'/revision
    import secrets
    OUT=OUT/("run-"+secrets.token_hex(6))
    BUILD=OUT/'private-build';BUILD.mkdir(parents=True,exist_ok=True)
    for name,path in inputs.items():shutil.copyfile(path,BUILD/name)
    assert all(sha(BUILD/name)==value for name,value in hashes.items())
    old=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/native-host/checked-start/f0ee1ff7fced9c1493f6/private-build/source'
    shutil.copyfile(old/'portable_sha256.h',BUILD/'portable_sha256.h')
    zig=ROOT/'.research-cache/x1d-1.25.0/toolchain/zig-windows-x86_64-0.13.0/zig.exe'
    env=dict(os.environ,TEMP=str(BUILD),TMP=str(BUILD),ZIG_LOCAL_CACHE_DIR=str(BUILD/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(BUILD/'zig-global'))
    def compile(source,target,unicode=False):
     command=[str(zig),'c++','-target','x86_64-windows-gnu','-Oz','-std=c++17','-static',str(BUILD/source),'-o',str(BUILD/target)]
     if unicode:command.insert(7,'-municode')
     result=subprocess.run(command,env=env,capture_output=True,text=True,timeout=300)
     assert result.returncode==0,result.stderr
    (BUILD/'fixture_engine.cpp').write_text('#include <windows.h>\n#include <stdio.h>\nint main(){puts("CHILD_READY");fflush(stdout);Sleep(1500);puts("CHILD_DONE");return 0;}\n',encoding='ascii')
    compile('fixture_engine.cpp','fixture_engine.exe')
    contents={'native/data.txt':b'verified-data\n','package-integrity.json':b'{}\n','X2dEngine.exe':(BUILD/'fixture_engine.exe').read_bytes()}
    (BUILD/'package_entries.hpp').write_text('#pragma once\nstruct PackageEntry{const char*name;const char*sha256;unsigned long long bytes;};\nstatic const PackageEntry package_entries[]={\n'+''.join('{'+json.dumps(name)+','+json.dumps(hashlib.sha256(data).hexdigest())+','+str(len(data))+'ULL},\n' for name,data in contents.items())+'};\n',encoding='ascii')
    compile('windows_checked_start.cpp','exact.exe',True)
    timing=(BUILD/'windows_checked_start.cpp').read_text(encoding='utf8');needle='    const auto engine='
    assert timing.count(needle)==1
    timing=timing.replace(needle,'    fputs("LEASE_READY\\n",stdout);fflush(stdout);Sleep(1500);\n'+needle)
    (BUILD/'timing.cpp').write_text(timing,encoding='utf8');compile('timing.cpp','timing.exe',True)
    def package(name,timed=False):
     root=OUT/name/'ancestor'/'中文 安装包';root.mkdir(parents=True)
     for name,data in contents.items():
      target=root/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
     (root/'X2dStart.exe').write_bytes((BUILD/('timing.exe' if timed else 'exact.exe')).read_bytes())
     return root
    cases=[]
    def finished(child):
     output,error=child.communicate(timeout=15)
     assert child.returncode==0 and 'CHILD_DONE' in output,(child.returncode,output,error)
     return output,error
    root=package('正常中文路径')
    result=subprocess.run([str(root/'X2dStart.exe'),str(root)],env=env,capture_output=True,text=True,timeout=15)
    assert result.returncode==0 and 'CHILD_READY' in result.stdout,(result.returncode,result.stderr)
    cases.append(dict(name='baseline-exact-source',exit=result.returncode))
    result=subprocess.run([str(root/'X2dStart.exe'),'--verify-package',str(root)],env=env,capture_output=True,text=True,timeout=15)
    assert result.returncode==0 and result.stdout=='PACKAGE_NATIVE_PREFLIGHT_OK\n' and not result.stderr
    assert 'CHILD_READY' not in result.stdout
    cases.append(dict(name='verify-only-does-not-start-engine',exit=result.returncode))
    result=subprocess.run([str(root/'X2dStart.exe'),'--verify-package',str(root.parent)],env=env,capture_output=True,text=True,timeout=15)
    assert result.returncode==2 and 'CHILD_READY' not in result.stdout
    cases.append(dict(name='verify-only-rejects-wrong-root',exit=result.returncode))
    for phase in ('after-verify','child-running'):
     root=package(phase,timed=phase=='after-verify')
     child=subprocess.Popen([str(root/'X2dStart.exe'),str(root)],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
     marker=child.stdout.readline();assert marker==('LEASE_READY\n' if phase=='after-verify' else 'CHILD_READY\n'),(marker,child.stderr.read())
     actions={
      'engine-overwrite':lambda:(root/'X2dEngine.exe').write_bytes(b'replaced'),
      'engine-delete':lambda:(root/'X2dEngine.exe').unlink(),
      'engine-rename':lambda:(root/'X2dEngine.exe').rename(root/'replacement.exe'),
      'resource-overwrite':lambda:(root/'native/data.txt').write_bytes(b'replaced'),
      'manifest-overwrite':lambda:(root/'package-integrity.json').write_bytes(b'{}'),
      'subdirectory-rename':lambda:(root/'native').rename(root/'native-old'),
      'root-rename':lambda:root.rename(root.with_name('moved-root')),
      'ancestor-rename':lambda:root.parent.rename(root.parent.with_name('moved-ancestor')),
     }
     for name,action in actions.items():
      try:action()
      except OSError as exc:cases.append(dict(name=phase+'/'+name,blocked=True,winerror=exc.winerror))
      else:raise AssertionError((phase,name,'replacement accepted'))
     output,error=finished(child)
     # 正常退出释放所有保护句柄，证明没有留下持久目录锁。
     (root/'native/data.txt').write_bytes(b'after-exit');root.rename(root.with_name('after-exit'))
     cases.append(dict(name=phase+'/released-after-child-exit',passed=True))
    root=package('启动前缺失');(root/'native/data.txt').unlink()
    result=subprocess.run([str(root/'X2dStart.exe'),str(root)],env=env,capture_output=True,text=True,timeout=15)
    assert result.returncode==2 and 'CHILD_READY' not in result.stdout
    cases.append(dict(name='missing-required-file-refused',exit=result.returncode))
    report=dict(source_sha256=hashes,exact_launcher_sha256=sha(BUILD/'exact.exe'),timing_launcher_sha256=sha(BUILD/'timing.exe'),cases=cases,count=len(cases),passed=True,author_tests=True,independent_approval=False,hardware_operations=0,limits=['固定三文件替身清单，不是完整客户端','计时窗口测试增加观察点/等待，守卫原码不变','不证明抵抗有权终止启动器或替换启动器的本机用户','Mac分支未执行；本修复不改变Mac策略','句柄阻止已验证对象写入/删除/重命名，不声称阻止新增任意子文件'])
    (OUT/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps(dict(directory=str(OUT),cases=len(cases),passed=True),ensure_ascii=False))

if __name__ == "__main__":main()
