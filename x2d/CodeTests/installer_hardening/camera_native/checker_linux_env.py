"""独立 ARM64 Linux 沙箱：公开依赖只解包，无安装、网络/USB/主机目录透传。"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,tarfile,urllib.request,gzip,struct,concurrent.futures
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
R5=ROOT/'x2d/outputs/4.2.0/installer-hardening-r5'
OUT=R5/'checker-ui/linux'
DEPS=R5/'deps'
ZIG=ROOT/'.research-cache/x1d-1.25.0/toolchain/zig-windows-x86_64-0.13.0/zig.exe'
def sha(p,kind='sha256'):return hashlib.new(kind,p.read_bytes()).hexdigest()
def download(url,path):
    if path.exists():return
    if url.endswith('.exe'):
        req=urllib.request.Request(url,headers={'Range':'bytes=0-0'})
        with urllib.request.urlopen(req,timeout=30) as r:
            total=int(r.headers['Content-Range'].split('/')[-1]);r.read()
        partial=path.with_suffix(path.suffix+'.partial')
        block=256*1024
        def fetch(start):
                end=min(total-1,start+block-1)
                req=urllib.request.Request(url,headers={'Range':f'bytes={start}-{end}'})
                for attempt in range(3):
                    try:
                        with urllib.request.urlopen(req,timeout=30) as r:data=r.read()
                        break
                    except Exception:
                        if attempt==2:raise
                assert len(data)==end-start+1
                return start,end,data
        with partial.open('wb') as dst,concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
            for start,end,data in pool.map(fetch,range(0,total,block)):
                dst.write(data);dst.flush()
                if start%(block*20)==0:print(f'DOWNLOAD {end+1}/{total}',flush=True)
        partial.replace(path);return
    with urllib.request.urlopen(url,timeout=90) as src,path.with_suffix(path.suffix+'.partial').open('wb') as dst:
        shutil.copyfileobj(src,dst)
    path.with_suffix(path.suffix+'.partial').replace(path)
def deps():
    DEPS.mkdir(parents=True,exist_ok=True)
    url='https://qemu.weilnetz.de/w64/qemu-w64-setup-20260811.exe'
    setup=DEPS/'qemu-w64-setup-20260811.exe';check=DEPS/(setup.name+'.sha512')
    download(url.removesuffix('.exe')+'.sha512',check);download(url,setup)
    expected=re.search(r'[0-9a-fA-F]{128}',check.read_text()).group(0).lower()
    assert sha(setup,'sha512')==expected
    qdir=DEPS/'qemu';qdir.mkdir(exist_ok=True)
    if not (qdir/'qemu-system-aarch64.exe').exists():
        r=subprocess.run(['C:/Program Files/7-Zip/7z.exe','x','-y','-o'+str(qdir),str(setup)],capture_output=True,text=True,timeout=180)
        assert r.returncode==0,r.stdout+r.stderr
    kernelurl='https://dl-cdn.alpinelinux.org/alpine/v3.22/main/aarch64/linux-virt-6.12.112-r0.apk'
    apk=DEPS/'linux-virt-6.12.112-r0.apk';download(kernelurl,apk)
    with tarfile.open(apk,'r:gz',ignore_zeros=True) as tf:
        entry=next(i for i in tf.getmembers() if i.name.endswith('/vmlinuz-virt'))
        data=tf.extractfile(entry).read()
    # Alpine EFI zboot contains a gzip ARM64 Image, not a QEMU direct-boot Image.
    wrapped_sha=hashlib.sha256(data).hexdigest()
    if data[4:8]==b'zimg':
        offset,size=struct.unpack_from('<II',data,8)
        assert offset+size<=len(data) and data[offset:offset+2]==b'\x1f\x8b'
        data=gzip.decompress(data[offset:offset+size])
    elif data[:2]==b'\x1f\x8b':data=gzip.decompress(data)
    assert data[56:60]==b'ARMd'
    kernel=DEPS/'vmlinuz-virt';kernel.write_bytes(data)
    proof=dict(qemu_url=url,qemu_sha512=expected,qemu_sha512_verified=True,installer_executed=False,
      qemu_binary_sha256=sha(qdir/'qemu-system-aarch64.exe'),kernel_apk_url=kernelurl,kernel_apk_sha256=sha(apk),
      kernel_sha256=sha(kernel),kernel_zboot_sha256=wrapped_sha,kernel_authentication='HTTPS retrieval; recorded local SHA256, no separate APK signature verification',
      system_installation=False)
    (DEPS/'linux-dependencies.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(proof),flush=True)
def cpio(entries):
    out=bytearray();ino=0
    for name,data,mode in entries+[('TRAILER!!!',b'',0)]:
        ino+=1;encoded=name.encode()+b'\0'
        fields=[ino,mode,0,0,1,0,len(data),0,0,0,0,len(encoded),0]
        out+=b'070701'+b''.join(f'{v:08x}'.encode() for v in fields)+encoded
        out+=b'\0'*((-len(out))%4);out+=data;out+=b'\0'*((-len(out))%4)
    return bytes(out)
def run(launch=True,artifact=None):
    OUT.mkdir(parents=True,exist_ok=True);build=OUT/'build';build.mkdir(exist_ok=True)
    source=build/'source';source.mkdir(exist_ok=True)
    names=['camera_native.h','camera_native.c','camera_posix.c','checker_linux_init.c','checker_linux_child.c']
    baseline=ROOT/'x2d/outputs/4.2.0/installer-hardening-r4/native-pilot/private-build/source'
    for name in names:shutil.copyfile((baseline if name.startswith('camera_') else HERE)/name,source/name)
    env=dict(os.environ,TEMP=str(build),TMP=str(build),ZIG_LOCAL_CACHE_DIR=str(build/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(build/'zig-global'))
    bins={}
    for name,inputs in [('flow',['camera_native.c','camera_posix.c']),('init',['checker_linux_init.c']),('child',['checker_linux_child.c'])]:
        target=build/name
        proc=subprocess.run([str(ZIG),'cc','-std=c11','-D_GNU_SOURCE','-O2','-Wall','-Wextra','-Werror','-Wno-macro-redefined','-target','aarch64-linux-musl','-static','-s',*[str(source/i) for i in inputs],'-o',str(target)],env=env,capture_output=True,text=True,timeout=180)
        assert proc.returncode==0,proc.stdout+proc.stderr
        bins[name]=target
    tested_existing_artifact=None
    if artifact:
        artifact=artifact.resolve();assert artifact.is_relative_to(ROOT/'x2d/outputs')
        frozen_proof=json.loads((baseline.parent.parent/'build-proof.json').read_text(encoding='utf-8'))
        assert sha(artifact)==frozen_proof['artifact']['sha256']
        assert all(sha(source/n)==h for n,h in frozen_proof['source_sha256'].items() if n.startswith('camera_'))
        shutil.copyfile(artifact,bins['flow']);tested_existing_artifact=str(artifact.relative_to(ROOT))
    dirs=['dev','proc','tmp','system','system/bin','system/bin/x2d-pixel-overclock','system/lib64','mnt','mnt/media_rw','mnt/media_rw/ssd','mnt/media_rw/ssd/multishot']
    entries=[(d,b'',0o40755) for d in dirs]
    entries += [('init',bins['init'].read_bytes(),0o100755),('flow',bins['flow'].read_bytes(),0o100755),
      ('system/bin/x2d-pixel-overclock/heif',bins['child'].read_bytes(),0o100755),('system/bin/sleep',bins['child'].read_bytes(),0o100755)]
    initrd=build/'initramfs.cpio';initrd.write_bytes(cpio(entries))
    if not launch:
        print('LINUX_FIXTURE_BUILT_NOT_EXECUTED');return
    argv=[str(DEPS/'qemu/qemu-system-aarch64.exe'),'-machine','virt','-cpu','cortex-a53','-m','1024','-smp','2','-nic','none',
      '-display','none','-monitor','none','-serial','stdio','-no-reboot','-kernel',str(DEPS/'vmlinuz-virt'),'-initrd',str(initrd),
      '-append','console=ttyAMA0 rdinit=/init panic=-1 quiet']
    proc=subprocess.run(argv,capture_output=True,text=True,timeout=120)
    label='artifact-' if artifact else ''
    (OUT/(label+'console.txt')).write_text(proc.stdout+proc.stderr,encoding='utf-8')
    report=dict(command=argv,returncode=proc.returncode,output=proc.stdout,stderr=proc.stderr,
      source_sha256={n:sha(source/n) for n in names},binaries={n:sha(p) for n,p in bins.items()},initramfs_sha256=sha(initrd),
      kernel_sha256=sha(DEPS/'vmlinuz-virt'),network=False,usb=False,host_shared_directory=False,guest_disk=False,
      tested_existing_artifact=tested_existing_artifact,
      arm64_executed=True,posix_adapter_executed=True,real_camera_executed=False,camera_requests=0,
      passed=proc.returncode==0 and 'LINUX_POSIX_ALL_PASSED' in proc.stdout and 'CHECK_FAILED' not in proc.stdout)
    (OUT/(label+'linux-report.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(passed=report['passed'],returncode=proc.returncode,output=proc.stdout[-5000:]),ensure_ascii=False))
    assert report['passed']
if __name__=='__main__':
    assert Path.cwd().resolve()==ROOT
    ap=argparse.ArgumentParser();ap.add_argument('mode',choices=['deps','build','run']);ap.add_argument('--artifact',type=Path);a=ap.parse_args()
    deps() if a.mode=='deps' else run(a.mode=='run',a.artifact)
