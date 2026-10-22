"""RAW stack 原生迁移局部 Linux 测试；不宣称完整流程或原厂 ABI 已验证。"""
from pathlib import Path
import hashlib,json,os,shutil,subprocess
import sys
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[2]
NATIVE=ROOT/'x2d/CodeTests/installer_hardening/camera_native'
sys.path.insert(0,str(NATIVE))
import checker_linux_env as linux
def source(n):return HERE/n if (HERE/n).is_file() else NATIVE/n
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 names=['check_runner.py','native_stack_runner.cpp','native_stack_runner.hpp','native_stack_capture.c','native_stack_abort.inc','native_common.hpp','native_process.hpp','native_runtime_tables.hpp','camera_native.h','checker_stack_abort.c','checker_stack_contract.cpp']
 hashes={n:sha(source(n)) for n in names};identity=hashlib.sha256(json.dumps(hashes,sort_keys=True).encode()).hexdigest()[:20]
 out=ROOT/'x2d/outputs/4.2.0/raw-stack/continuous-candidate/runner-check'/identity;build=out/'private-build';build.mkdir(parents=True,exist_ok=True)
 for n in names:shutil.copyfile(source(n),build/n)
 env=dict(os.environ,TEMP=str(build),TMP=str(build),ZIG_LOCAL_CACHE_DIR=str(build/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(build/'zig-global'))
 flags=['-target','aarch64-linux-musl','-O2','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections']
 commands=[[str(linux.ZIG),'cc',*flags,'-std=c11','-c',str(build/'checker_stack_abort.c'),'-o',str(build/'abort.o')],[str(linux.ZIG),'c++',*flags,'-std=c++17','-static','-s','-Wl,--gc-sections',str(build/'checker_stack_contract.cpp'),str(build/'abort.o'),'-o',str(build/'init')]]
 for cmd in commands:
  result=subprocess.run(cmd,env=env,capture_output=True,text=True,timeout=240);assert result.returncode==0,result.stdout+result.stderr
 initrd=build/'initramfs.cpio';initrd.write_bytes(linux.cpio([('dev',b'',0o40755),('proc',b'',0o40755),('tmp',b'',0o40755),('init',(build/'init').read_bytes(),0o100755)]))
 run=[str(linux.DEPS/'qemu/qemu-system-aarch64.exe'),'-machine','virt','-cpu','cortex-a53','-m','256','-smp','1','-nic','none','-display','none','-monitor','none','-serial','stdio','-no-reboot','-kernel',str(linux.DEPS/'vmlinuz-virt'),'-initrd',str(initrd),'-append','console=ttyAMA0 rdinit=/init panic=-1 quiet']
 result=subprocess.run(run,capture_output=True,text=True,timeout=90);(out/'console.txt').write_text(result.stdout+result.stderr,encoding='utf8')
 passed=result.returncode==0 and 'STACK_RUNNER_CONTRACT_PASSED' in result.stdout and 'STACK_ABORT_MODEL_PASSED cases=13' in result.stdout and 'STACK_ADMISSION_REGRESSION_PASSED cases=16' in result.stdout
 report={'passed':passed,'independent_checker':False,'camera_calls':0,'network':False,'host_shared_directory':False,'source_sha256':hashes,'test_binary_sha256':sha(build/'init'),'build_commands':commands,'run_command':run,'limitations':['实际Linux测试仅固定子进程与输入/时间/ISO控制；abort里的RAW/Qt/HAL对象用严格桩，不是原厂运行。','实际连续曝光通知顺序和停拍在途帧数尚未实测。','未执行完整准备、连拍切换、拍摄、渲染或发布。']}
 for n in names:assert hashes[n]==sha(source(n)),'测试期间源码变化'
 (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8');print(result.stdout);print(out);assert passed
if __name__=='__main__':main()
