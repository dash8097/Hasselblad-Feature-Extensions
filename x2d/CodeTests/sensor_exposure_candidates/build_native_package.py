"""冻结已验证 C3 采样链，生成无脚本包内服务；不操作相机。"""
import hashlib,json,os,re,subprocess,sys
from pathlib import Path
from catalog import ROOT,HERE,OUT,RCAM_SHA
sys.path.insert(0,str(ROOT/'x2d/tools'))
from firmware_image import system_file,FirmwareElf
sys.path.insert(0,str(ROOT/'x2d/CodeTests/installer_hardening/camera_native'))
import elf_metadata_hardening

def sha(b):return hashlib.sha256(b).hexdigest()
def replace(s,a,b):
    assert s.count(a)==1,a[:120]
    return s.replace(a,b)
def main():
    assert Path.cwd().resolve()==ROOT
    pulse_checks=[]
    pulse_sources=('native_hotshoe_resident.c','native_pulse_test_platform.h','test_native_pulse.c')
    for path in (ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/esflash-pulse').glob('*/report.json'):
        proof=json.loads(path.read_text(encoding='utf8'));ident=proof.get('identity',{})
        if proof.get('passed') and proof.get('baseline_red_reproduced') and proof.get('scheduler_window_red_reproduced') and ident.get('builder')==sha((HERE/'test_native_pulse.py').read_bytes()) and ident.get('sources')=={n:sha((HERE/n).read_bytes()) for n in pulse_sources}:pulse_checks.append(path)
    assert len(pulse_checks)==1,('missing current pulse guard regression',pulse_checks)
    pulse_check=pulse_checks[0]
    scheduler_checks=[]
    scheduler_sources=(*pulse_sources,'test_native_pulse_kernel.c')
    for path in (ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/esflash-pulse-kernel').glob('*/report.json'):
        proof=json.loads(path.read_text(encoding='utf8'));ident=proof.get('identity',{})
        if proof.get('passed') and proof.get('real_kernel_scheduler') and ident.get('builder')==sha((HERE/'test_native_pulse_kernel.py').read_bytes()) and ident.get('sources')=={n:sha((HERE/n).read_bytes()) for n in scheduler_sources}:scheduler_checks.append(path)
    assert len(scheduler_checks)==1,('missing current kernel scheduler regression',scheduler_checks)
    scheduler_check=scheduler_checks[0]
    wait_checks=[]
    wait_sources=('native_wait_policy.h','test_native_wait.c')
    for path in (ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/esflash-wait-policy').glob('*/report.json'):
        proof=json.loads(path.read_text(encoding='utf8'));ident=proof.get('identity',{})
        if proof.get('passed') and proof.get('real_kernel_poll') and proof.get('baseline_budget_rejected') and ident.get('builder')==sha((HERE/'test_native_wait.py').read_bytes()) and ident.get('sources')=={n:sha((HERE/n).read_bytes()) for n in wait_sources}:wait_checks.append(path)
    assert len(wait_checks)==1,('missing current wait policy regression',wait_checks)
    wait_check=wait_checks[0]
    ui=ROOT/'x2d/CodeTests/installer_hardening/native_ui'
    legacy=ROOT/'x2d/CodeTests/subject_tracking/device_hybrid_test/x2d_eshutter_flash/front_button_trial_v3'
    files={n:(HERE/n).read_bytes() for n in ('recorder.c','perf_core.h','flash_core.h','native_page.h','native_commands.h','native_entry.h','native_wait_policy.h')}
    files['esflash_protocol.h']=(ui/'esflash_protocol.h').read_bytes()
    files['layout.h']=(OUT/'layout.h').read_bytes()
    rcam=system_file('/lib64/librcam.so');assert sha(rcam)==RCAM_SHA
    elf=FirmwareElf(rcam)
    sites=re.findall(rb'\{"[^"]+",0x([0-9a-f]+)ull,0x([0-9a-f]+)u,',files['layout.h']);assert len(sites)==20
    for address,word in sites:
        assert int.from_bytes(elf.read(int(address,16),4),'little')==int(word,16)
    page=(HERE/'flash_page.h').read_text(encoding='utf8')
    hashfn=page[page.index('static int file_hash('):page.index('static void page_status(')]
    files['native_page.h']+=hashfn.encode()
    helper=(HERE/'native_hotshoe_resident.c').read_text(encoding='utf8').replace('#include "../hotshoe_once.c"','#include "hotshoe_base.h"')
    files['hotshoe_resident.c']=helper.encode()
    base=(legacy.parent/'hotshoe_once.c').read_text(encoding='utf8')
    start=base.index('static int selected_process(void) {');end=base.index('static int verified_binary(void)',start)
    old=base[start:end];assert 'return equal(arg0, "/system/bin/true");' in old
    base=base[:start]+'#include "native_host_identity.h"\nstatic int selected_process(void){return native_selected_host();}\n\n'+base[end:]
    files['native_host_identity.h']=(HERE/'native_host_identity.h').read_bytes()
    files['native_parent_start.h']=(HERE/'native_parent_start.h').read_bytes()
    files['hotshoe_resident.c']+=b'\n#include "native_parent_start.h"\n__attribute__((constructor)) static void native_camera_child_constructor(void){native_start_camera_child();}\n'
    files['hotshoe_base.h']=base[:base.index('__attribute__((constructor))')].encode()
    worker=(HERE/'flash_worker.c').read_text(encoding='utf8')
    worker=replace(worker,'#include "flash_page.h"\n#include "exposure_front_input.c"','#include "native_page.h"\n#include "native_wait_policy.h"')
    worker=replace(worker,'#define HELPER "/system/lib64/libx2d_sensor_hotshoe_r2.so"','#define HELPER "/system/lib64/libx2d_esflash_hotshoe.so"')
    worker=replace(worker,'static int start_sampling(unsigned source){','static int start_sampling(unsigned source){\n    target_pid=native_find_service();if(!target_pid)return fail("service_not_unique");')
    start=worker.index('static char command_buffer[');end=worker.index('static int worker(void){',start)
    worker=worker[:start]+'#include "native_commands.h"\n'+worker[end:]
    start=worker.index('    if(!number(getenv("X2D_SENSOR_PID")');end=worker.index('    if(!file_hash(',start)
    worker=worker[:start]+'    allowed_mask=4;\n'+worker[end:]
    worker=replace(worker,'    core.source=1;core.consumed=1;','    core.source=3;core.delay_us=168000;core.consumed=1;')
    worker=replace(worker,'        int stop=open(PAGE_DIR "stop",0x20000);if(stop>=0){close(stop);break;}','')
    wait_start=worker.index('        uint64_t wait_ns=1000000ull,wait_now=now();')
    wait_end=worker.index('        TimeSpec timeout=',wait_start)
    old_wait=worker[wait_start:wait_end]
    assert 'if(core.pending)' in old_wait and 'remain<wait_ns' in old_wait
    worker=replace(worker,old_wait,'        uint64_t wait_now=now();\n        if(!wait_now){fail("clock_wait");break;}\n        uint64_t wait_ns=native_flash_wait_ns(sampling,core.pending,core.deadline_ns,wait_now);\n')
    worker=replace(worker,'* 仍受内核调度影响；最多 1 ms 返回检查页面命令。 */','* perf就绪可提前唤醒；空闲10ms/关闭50ms检查命令，临近截止精确等待。 */')
    worker=replace(worker,'PulseTiming timing={0};helper_ok=pulse_fire&&pulse_fire(&timing)==0;','PulseTiming timing={0};helper_ok=pulse_fire&&pulse_fire(&timing)==0;\n            if(!helper_ok)fail("pulse_failed_no_retry"); /* preserve helper errno before logging */')
    worker=worker[:worker.index('#ifndef X2D_FLASH_WORKER_TEST')]+'#include "native_entry.h"\n'
    files['native_worker.c']=worker.encode()
    files['x2d-esflash.rc']=b'# The fixed component is started by camera-service; no init security-domain override.\nservice x2d-esflash /system/bin/x2d-esflash\n    class core\n    disabled\n    user root\n    group root\n    oneshot\n'
    libs={n:system_file('/lib64/'+n) for n in ('libc.so','libcrypto.so','libdl.so')}
    identity={'builder':sha(Path(__file__).read_bytes()),'startup_contract':sha((HERE/'native_startup_contract.py').read_bytes()),'pulse_guard_source':sha((HERE/'native_hotshoe_resident.c').read_bytes()),'pulse_guard_test':sha(pulse_check.read_bytes()),'pulse_scheduler_kernel_test':sha(scheduler_check.read_bytes()),'wait_policy_test':sha(wait_check.read_bytes()),'hardener':sha(Path(elf_metadata_hardening.__file__).read_bytes()),'sources':{n:sha(b) for n,b in files.items()},'dependencies':{n:sha(b) for n,b in libs.items()},'shared_worker':sha((HERE/'flash_worker.c').read_bytes())}
    rev=sha(json.dumps(identity,sort_keys=True).encode())[:20];out=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/esflash'/rev
    assert not (out/'report.json').exists();src=out/'source';src.mkdir(parents=True,exist_ok=True)
    for n,b in {**files,**libs}.items():(src/n).write_bytes(b)
    zig=ROOT/'.research-cache/x1d-1.25.0/toolchain/zig-windows-x86_64-0.13.0/zig.exe'
    env=dict(os.environ,TEMP=str(out),TMP=str(out),ZIG_LOCAL_CACHE_DIR=str(out/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(out/'zig-global'))
    def run(args):
        p=subprocess.run([str(x) for x in args],env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=180)
        assert not p.returncode,p.stdout+p.stderr
    flags=['-target','aarch64-linux-gnu','-O2','-D_GNU_SOURCE','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-fPIC','-nostdlib','-fno-stack-protector','-mno-outline-atomics','-Wl,--no-undefined','-Wl,-z,noexecstack','-Wl,--strip-all']
    helper=out/'libx2d_esflash_hotshoe.so'
    run([zig,'cc',*flags,'-shared',src/'hotshoe_resident.c',*[src/n for n in libs],'-o',helper])
    helper.write_bytes(elf_metadata_hardening.normalize_elf(helper.read_bytes())[0])
    exe=out/'x2d-esflash'
    obj=out/'native_worker.o'
    run([zig,'cc',*[f for f in flags if f!='-nostdlib' and not f.startswith('-Wl,')],'-fPIE','-DX2D_FLASH_HELPER_SHA="'+sha(helper.read_bytes())+'"','-c',src/'native_worker.c','-o',obj])
    run([zig,'ld.lld','-m','aarch64elf','-pie','--dynamic-linker','/system/bin/linker64','-e','_start','--no-undefined','--allow-shlib-undefined','-z','noexecstack','--strip-all',obj,*[src/n for n in libs],'-o',exe])
    exe.write_bytes(elf_metadata_hardening.normalize_elf(exe.read_bytes())[0])
    constructors=FirmwareElf(helper.read_bytes()).elf.get_section_by_name('.init_array')
    assert constructors is not None and constructors['sh_size']==8
    exports=set()
    for b in libs.values():exports.update(s.name for s in FirmwareElf(b).elf.get_section_by_name('.dynsym').iter_symbols() if s['st_shndx']!='SHN_UNDEF')
    for p in (helper,exe):
        imports={s.name for s in FirmwareElf(p.read_bytes()).elf.get_section_by_name('.dynsym').iter_symbols() if s.name and s['st_shndx']=='SHN_UNDEF'}
        assert imports<=exports,imports-exports
        assert not imports&{'system','popen','ptrace','setcon','setexeccon','setuid','setgid','capset'}
        if p==exe:assert 'execve' not in imports
    (out/'x2d-esflash.rc').write_bytes(files['x2d-esflash.rc'])
    targets={'/system/bin/x2d-esflash':exe,'/system/lib64/libx2d_esflash_hotshoe.so':helper,'/system/etc/init/x2d-esflash.rc':out/'x2d-esflash.rc'}
    report={'identity':identity,'pulse_guard_report':str(pulse_check.relative_to(ROOT)),'pulse_scheduler_kernel_report':str(scheduler_check.relative_to(ROOT)),'wait_policy_report':str(wait_check.relative_to(ROOT)),'targets':{k:{'source':str(p.relative_to(ROOT)),'sha256':sha(p.read_bytes()),'bytes':p.stat().st_size,'mode':'755' if k.endswith('/x2d-esflash') else '644'} for k,p in targets.items()},'camera_requests':0,'default_enabled':False,'startup':'camera-service-child-v1','credentials_or_policy_modified':False,'factory_dynamic_compensation_modified':False,'hardware_verified':False,'release_ready':False}
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8');print(out)
if __name__=='__main__':main()
