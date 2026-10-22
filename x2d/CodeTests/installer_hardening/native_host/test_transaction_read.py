"""隔离 ARM64 Linux 文件系统/进程测试；不透传目录、设备或网络。"""
from pathlib import Path
import hashlib,json,os,shutil,subprocess,sys
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[3]
sys.path.insert(0,str(HERE.parent/'camera_native'));sys.path.insert(0,str(HERE.parent))
import checker_linux_env as linux
from native_adapter import NativeAdapter,Journal,Ticket
from test_native_adapter import Fake
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
names=['native_transaction_read.cpp','test_transaction_read.cpp','native_adapter.py','test_native_adapter.py']
hashes={n:sha(HERE/n) for n in names};identity=hashlib.sha256(json.dumps(hashes,sort_keys=True).encode()).hexdigest()[:20]
out=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/native-host/revisions'/identity;build=out/'private-build';build.mkdir(parents=True,exist_ok=True)
for n in names:shutil.copyfile(HERE/n,build/n)
shutil.copyfile(HERE.parent/'native_transaction_contract.py',build/'native_transaction_contract.py')
env=dict(os.environ,TEMP=str(build),TMP=str(build),ZIG_LOCAL_CACHE_DIR=str(build/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(build/'zig-global'))
cmd=[str(linux.ZIG),'c++','-target','aarch64-linux-musl','-O2','-std=c++17','-Wall','-Wextra','-Werror','-static','-s',str(build/'test_transaction_read.cpp'),'-o',str(build/'init')]
r=subprocess.run(cmd,env=env,capture_output=True,text=True,timeout=240);assert r.returncode==0,r.stdout+r.stderr
initrd=build/'initramfs.cpio';initrd.write_bytes(linux.cpio([('dev',b'',0o40755),('init',(build/'init').read_bytes(),0o100755)]))
cmd=[str(linux.DEPS/'qemu/qemu-system-aarch64.exe'),'-machine','virt','-cpu','cortex-a53','-m','256','-smp','1','-nic','none','-display','none','-monitor','none','-serial','stdio','-no-reboot','-kernel',str(linux.DEPS/'vmlinuz-virt'),'-initrd',str(initrd),'-append','console=ttyAMA0 rdinit=/init panic=-1 quiet']
r=subprocess.run(cmd,capture_output=True,text=True,timeout=45);(out/'console.txt').write_text(r.stdout+r.stderr,encoding='utf8');assert 'NATIVE_READER_ACTUAL_LINUX_PASSED' in r.stdout,r.stdout+r.stderr
sample=json.loads(next(line.split('=',1)[1] for line in r.stdout.splitlines() if line.startswith('READER_SAMPLE=')))
host=Fake();ticket=Ticket('uninstall','/data/local/tmp/x2d-un-11223344','1234567890abcdef');adapter=NativeAdapter(host,Journal(out/'sample-journal'),lambda p:True)
if adapter.journal_missing(ticket):adapter.dispatch(ticket)
host.reply=sample;assert adapter.observe(ticket).completed
report=dict(source_sha256=hashes,contract_sha256=sha(HERE.parent/'native_transaction_contract.py'),passed=True,reader_linux=True,reader_to_host_roundtrip=True,hardware_calls=0,production_ready=False,limitations=['源代码作者测试，待独立checker。','真实Linux读取用机内固定路径的临时RAM文件；未读设备。','未验证完整bootstrap、载荷提取、最终主机编译集成。'])
(out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8');print(r.stdout);print(out)
