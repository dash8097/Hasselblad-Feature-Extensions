"""真实 ARM64 Linux 解包测试；gzip 用 Zig 标准库替身，不代表原厂 busybox 已联调。"""
from pathlib import Path
import gzip,hashlib,io,json,os,re,shutil,subprocess,sys,tarfile
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[3]
sys.path.insert(0,str(HERE.parent/'camera_native'))
import checker_linux_env as linux
PAYLOAD=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/native-payload/revisions/c152776589b5aae35acd'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
names=['native_unpack.cpp','native_unpack.hpp','test_native_unpack.cpp','test_native_unpack.py','test_unpack_gzip.zig']
inputs={n:HERE/n for n in names}
inputs.update({n:HERE.parent/'camera_native'/n for n in ('native_common.hpp','native_common.cpp')})
inputs['receipt_sha256.h']=HERE.parents[1]/'pixel_shift_rgb/receipt_sha256.h'
inputs['native_unpack_tables.hpp']=PAYLOAD/'include/native_unpack_tables.hpp'
hashes={n:sha(p) for n,p in inputs.items()};identity=hashlib.sha256(json.dumps(hashes,sort_keys=True).encode()).hexdigest()[:20]
out=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/native-host/unpack'/identity;build=out/'private-build';build.mkdir(parents=True,exist_ok=True)
for n,p in inputs.items():shutil.copyfile(p,build/n)
env=dict(os.environ,TEMP=str(build),TMP=str(build),ZIG_LOCAL_CACHE_DIR=str(build/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(build/'zig-global'))
def tar(members):
    b=io.BytesIO()
    with tarfile.open(fileobj=b,mode='w',format=tarfile.USTAR_FORMAT) as f:
        for name,data,kind in members:
            entry=tarfile.TarInfo(name);entry.size=len(data);entry.mode=0o644;entry.type=kind
            if kind==tarfile.SYMTYPE:entry.linkname='outside'
            f.addfile(entry,io.BytesIO(data))
    return b.getvalue()
good=tar([('item',b'abc',tarfile.REGTYPE)])
fixtures=[('standard-10240-padding',good,True),('minimal-end',good[:2048],True),
 ('truncated',good[:700],False),('unknown',tar([('extra',b'abc',tarfile.REGTYPE)]),False),
 ('duplicate',tar([('item',b'abc',tarfile.REGTYPE)]*2),False),('traversal',tar([('../item',b'abc',tarfile.REGTYPE)]),False),
 ('symlink',tar([('item',b'',tarfile.SYMTYPE)]),False),('wrong-sha',tar([('item',b'xyz',tarfile.REGTYPE)]),False),
 ('missing',bytes(10240),False),('trailing-nonzero',good+b'X',False),('excessive-padding',good+bytes(11000),False),
 ('bad-checksum',b'X'+good[1:],False)]
entries=[(d,b'',0o40755) for d in ('dev','tmp','system','system/xbin','data','data/local','data/local/tmp','data/local/tmp/x2d-rs1','data/local/tmp/x2d-un-11223344','fixtures')]
definitions=['struct Case {const char*label;const char*path;bool success;const cn::unpack::File*files;size_t count;};',
 'static constexpr cn::unpack::File sample_files[]={{"item","'+hashlib.sha256(b'abc').hexdigest()+'",3,0644}};','static const Case cases[]={']
for i,(label,data,success) in enumerate(fixtures):
    path=f'fixtures/{i}.gz';entries.append((path,gzip.compress(data,mtime=0),0o100600));definitions.append('{"'+label+'","/'+path+'",'+str(success).lower()+',sample_files,1},')
definitions+=['};'];(build/'unpack_cases.hpp').write_text('\n'.join(definitions),encoding='ascii')
def run(cmd):
    result=subprocess.run(cmd,env=env,capture_output=True,text=True,timeout=240)
    assert result.returncode==0,result.stdout+result.stderr
run([str(linux.ZIG),'build-exe',str(build/'test_unpack_gzip.zig'),'-target','aarch64-linux-musl','-O','ReleaseSmall','-femit-bin='+str(build/'gzip')])
header=(build/'native_unpack_tables.hpp').read_text();header,count=re.subn(r'#define CN_UNPACK_DECODER_SHA256 "[0-9a-f]{64}"','#define CN_UNPACK_DECODER_SHA256 "'+sha(build/'gzip')+'"',header);assert count==1
(build/'native_unpack_tables.hpp').write_text(header,encoding='ascii')
run([str(linux.ZIG),'c++','-target','aarch64-linux-musl','-std=c++17','-O2','-Wall','-Wextra','-Werror','-static','-s','-ffunction-sections','-fdata-sections',str(build/'test_native_unpack.cpp'),str(build/'native_common.cpp'),'-Wl,--gc-sections','-o',str(build/'init')])
entries += [('init',(build/'init').read_bytes(),0o100755),('system/xbin/busybox',(build/'gzip').read_bytes(),0o100755),('data/local/tmp/x2d-rs1/native-install.gz',(PAYLOAD/'native-install.gz').read_bytes(),0o100600),('data/local/tmp/x2d-un-11223344/native-uninstall.gz',(PAYLOAD/'native-uninstall.gz').read_bytes(),0o100600)]
initrd=build/'initramfs.cpio';initrd.write_bytes(linux.cpio(entries))
cmd=[str(linux.DEPS/'qemu/qemu-system-aarch64.exe'),'-machine','virt','-cpu','cortex-a53','-m','512','-smp','1','-nic','none','-display','none','-monitor','none','-serial','stdio','-no-reboot','-kernel',str(linux.DEPS/'vmlinuz-virt'),'-initrd',str(initrd),'-append','console=ttyAMA0 rdinit=/init panic=-1 quiet']
r=subprocess.run(cmd,capture_output=True,text=True,timeout=90);(out/'console.txt').write_text(r.stdout+r.stderr,encoding='utf8');print(r.stdout);assert 'NATIVE_UNPACK_LINUX_PASSED' in r.stdout,r.stdout+r.stderr
(out/'report.json').write_text(json.dumps(dict(source_sha256=hashes,test_table_sha256=sha(build/'native_unpack_tables.hpp'),test_decoder_sha256=sha(build/'gzip'),test_table_transform='Only CN_UNPACK_DECODER_SHA256 replaced with fixture digest',payload=str(PAYLOAD),passed=True,hardware_calls=0,decoder='Zig 0.13 gzip surrogate, actual decompression; not firmware busybox integration',author_tests=True,independent_approval=False),indent=2)+'\n');print(out)
