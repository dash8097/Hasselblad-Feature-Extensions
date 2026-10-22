"""冻结当前 worker 与本地头文件后，只替换原生派发入口；不改活跃源目录。"""
from pathlib import Path
import hashlib,json,os,re,subprocess
import build_pilot
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
SOURCE=ROOT/'x2d/CodeTests/pixel_shift_rgb'
OUT=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/native-worker/revisions'


def transform(text):
    text=text.replace('\r\n','\n')
    def replace(old,new):
        nonlocal text
        assert text.count(old)==1,old
        text=text.replace(old,new)
    replace('#define PO_RUNNER "/system/etc/x2d-pixel-overclock/launch.sh"',
            '#define PO_RUNNER "/system/bin/x2d-native-flow"')
    replace('execl(PO_SHELL,"sh","/system/etc/x2d-raw-stack/launch.sh",job,frames,delay,noise,initial,"--prepare",(char*)0);',
            'execl(PO_RUNNER,PO_RUNNER,"launch-stack",job,frames,delay,noise,initial,"--prepare",(char*)0);')
    replace('execl(PO_SHELL,"sh","/system/etc/x2d-multi-resolution/launch.sh",job,delay,frames,"--prepare",(char*)0);',
            'execl(PO_RUNNER,PO_RUNNER,"launch-multi",job,delay,frames,"--prepare",(char*)0);')
    replace('execl(PO_SHELL,"sh",PO_RUNNER,job,delay,frames,"--prepare",(char*)0);',
            'execl(PO_RUNNER,PO_RUNNER,"launch-memory",job,delay,frames,"--prepare",(char*)0);')
    # 新流程自己按 inode 持有并释放锁，不再用旧的空标记猜测全局锁归属。
    start=text.index('static void release_finished_lock(')
    end=text.index('static int status_file(',start)
    text=text[:start]+'static void release_finished_lock(const char *job){(void)job;}\n'+text[end:]
    # 所有登记模式在此前分支已自行收尾；拒绝落入未登记的旧清理器。
    start=text.index('      child=fork();',text.index('diagnostic_result(job,cleaning?'))
    end=text.index('     }else{diagnostic_result(job,"album_verify_failed"',start)
    text=text[:start]+'      child=0;strcpy(stage,"failed");strcpy(error,"成片已保存，模式清理器未登记");\n'+text[end:]
    # 所有三个子进程标准流都可能已关闭，复制到 >=3 后再重定向。
    replace('  dup2(fd,1);dup2(fd,2);close(fd);\n#ifdef PO_RAW_STACK',
            '  int log_copy=fcntl(fd,F_DUPFD_CLOEXEC,3);if(log_copy<0||dup2(log_copy,1)<0||dup2(log_copy,2)<0)_exit(100);close(log_copy);if(fd!=1&&fd!=2)close(fd);\n#ifdef PO_RAW_STACK')
    replace('  int ready=access(PO_RUNNER,R_OK)==0&&access("/tmp/x2d-multishot-active",F_OK)!=0;',
            '  int ready=access(PO_RUNNER,X_OK)==0&&access("/tmp/x2d-multishot-active",F_OK)!=0;')
    return text


def main():
    assert Path.cwd().resolve()==ROOT
    files={}
    def freeze(name):
        assert '/' not in name and '\\' not in name
        if name in files:return
        data=(SOURCE/name).read_bytes();files[name]=data
        for header in re.findall(r'^\s*#include\s+"([^"]+)"',data.decode('utf-8-sig'),re.M):freeze(header)
    freeze('pixel_overclock_worker.c')
    hashes={name:hashlib.sha256(data).hexdigest() for name,data in files.items()}
    identity=dict(sources=hashes,builder=build_pilot.sha(Path(__file__)),zig=build_pilot.sha(build_pilot.ZIG))
    revision=hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20]
    out=OUT/revision
    if (out/'build-proof.json').exists():
        old=json.loads((out/'build-proof.json').read_text(encoding='utf-8'))
        assert old['identity']==identity and old['artifact']['sha256']==build_pilot.sha(out/'artifacts/worker')
        for name,digest in hashes.items():
            assert build_pilot.sha(out/'private-build/source'/name)==digest,'冻结源码摘要不匹配：'+name
        transformed=out/'private-build/native-worker.c'
        assert build_pilot.sha(transformed)==old['transformed_source_sha256'],'变换源码摘要不匹配'
        assert transformed.read_bytes()==transform(files['pixel_overclock_worker.c'].decode('utf-8-sig')).encode('utf-8'),'变换结果不匹配'
        print(json.dumps(old,ensure_ascii=False));return
    source=out/'private-build/source';source.mkdir(parents=True,exist_ok=True)
    for name,data in files.items():(source/name).write_bytes(data)
    compiled=out/'private-build/native-worker.c'
    compiled.write_text(transform(files['pixel_overclock_worker.c'].decode('utf-8-sig')),encoding='utf-8',newline='\n')
    artifact=out/'artifacts/worker';artifact.parent.mkdir(exist_ok=True)
    env=dict(os.environ,TEMP=str(out/'private-build'),TMP=str(out/'private-build'),ZIG_LOCAL_CACHE_DIR=str(out/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(out/'zig-global'))
    flags=['-DPO_RAW_STACK=1','-DPO_FULL_PREPARE=1','-DPO_MULTI_RESOLUTION=1','-DPO_NATIVE_DISK400=1','-DPO_DEVELOPMENT_SIXTEEN=1']
    command=[str(build_pilot.ZIG),'cc','-target','aarch64-linux-musl','-static','-Oz','-Wall','-Wextra','-Werror','-Wno-unused-function','-fstack-protector-strong','-s',*flags,'-I'+str(source),str(compiled),'-o',str(artifact)]
    result=subprocess.run(command,env=env,capture_output=True,text=True,timeout=180)
    assert not result.returncode,result.stdout+result.stderr
    data=artifact.read_bytes()
    for token in (b'/system/bin/sh',b'launch.sh',b'cleanup-job.sh',b'mode-prepare.sh'):assert token not in data,token
    for name,digest in hashes.items():assert build_pilot.sha(SOURCE/name)==digest,'构建期间活跃源变化，丢弃候选'
    report=dict(identity=identity,artifact=build_pilot.elf_check(artifact),transformed_source_sha256=build_pilot.sha(compiled),flags=flags,release_ready=False,hardware_tested=False,requires=['native launch-stack/launch-multi/launch-memory','independent worker UI and mode-cross regressions','complete package integration'])
    (out/'build-proof.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(directory=str(out),artifact=report['artifact'],release_ready=False),ensure_ascii=False))


if __name__=='__main__':main()
