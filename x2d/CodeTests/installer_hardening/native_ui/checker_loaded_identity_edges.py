"""Independent loaded-module file/maps boundary tests, isolated ARM Linux only."""
from pathlib import Path
import hashlib,json,os,subprocess,sys
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[3]
sys.path.insert(0,str(HERE.parent/'camera_native'))
import checker_linux_env as linux

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 assert Path.cwd().resolve()==ROOT
 paths=[HERE/n for n in ('loaded_module_identity.h','loaded_module_identity.c','ui_receipt.h','ui_receipt.c','ui_receipt_worker.c','test_loaded_module_identity.c')]+[ROOT/'x2d/CodeTests/pixel_shift_rgb/receipt_sha256.h',Path(__file__)]
 ids={p.name:sha(p) for p in paths};ids['compiler']=sha(linux.ZIG)
 out=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/checker-ui/loaded-identity-edges'/hashlib.sha256(json.dumps(ids,sort_keys=True).encode()).hexdigest()[:20];b=out/'private-build';b.mkdir(parents=True,exist_ok=False)
 for p in paths:(b/p.name).write_bytes(p.read_bytes())
 source=(b/'test_loaded_module_identity.c').read_text()
 source=source.replace('#include <stdint.h>','#include <stdint.h>\n#include <sys/syscall.h>\n#include <errno.h>')
 # Boundary fault injection changes the actual file only after an actual read.
 source=source.replace('static int checks;',r"""
static int fault, hits;
ssize_t checker_read(int fd,void*buf,size_t len){
 ssize_t n=syscall(SYS_read,fd,buf,len); struct stat s,t;
 if(fault&&n>0&&!fstat(fd,&s)&&!lstat("/system/lib64/libx2d_native_menu.so",&t)&&s.st_dev==t.st_dev&&s.st_ino==t.st_ino){int mode=fault;fault=0;++hits;
  if(mode==1){int w=open("/system/lib64/libx2d_native_menu.so",O_WRONLY);unsigned char v=123;pwrite(w,&v,1,2);close(w);}
  if(mode==2)truncate("/system/lib64/libx2d_native_menu.so",64);
  if(mode==3){rename("/system/lib64/libx2d_native_menu.so","/system/lib64/displaced.so");int w=open("/system/lib64/libx2d_native_menu.so",O_CREAT|O_WRONLY,0644);ftruncate(w,8192);close(w);}
  if(mode==4)chmod("/system/lib64/libx2d_native_menu.so",0666);
  if(mode==5)chown("/system/lib64/libx2d_native_menu.so",123,123);
  if(mode==6){rename("/system/lib64","/system/real");symlink("real","/system/lib64");}
 }
 return n;
}
static int checks;
""")
 marker=' const char*gui='
 assert marker in source
 insert=r"""
 // Actual Linux permission/mapping identities beyond author baseline.
 ck(!mprotect(address,sizeof bytes,PROT_READ|PROT_WRITE|PROT_EXEC));ck(!x2d_loaded_module_hash(hash));ck(!mprotect(address,sizeof bytes,PROT_READ|PROT_EXEC));
 ck(!chown(path,123,123));ck(!x2d_loaded_module_hash(hash));ck(!chown(path,0,0));
 ck(!rename("/system","/real-system")&&!symlink("real-system","/system"));ck(!x2d_loaded_module_hash(hash));ck(!unlink("/system")&&!rename("/real-system","/system"));
 ck(!munmap(address,sizeof bytes));
 // Same prefix is not an identity, and a MAP_SHARED executable mapping is rejected.
 save("/system/lib64/libx2d_native_menu.so.extra",bytes,sizeof bytes);fd=open("/system/lib64/libx2d_native_menu.so.extra",O_RDONLY);ck(fd>=0);address=mmap(NULL,sizeof bytes,PROT_READ|PROT_EXEC,MAP_PRIVATE,fd,0);close(fd);ck(address!=MAP_FAILED);x2d_identity_test_address=(uintptr_t)address+16;ck(!x2d_loaded_module_hash(hash));ck(!munmap(address,sizeof bytes));
 fd=open(path,O_RDONLY);ck(fd>=0);address=mmap(NULL,sizeof bytes,PROT_READ|PROT_EXEC,MAP_SHARED,fd,0);close(fd);ck(address!=MAP_FAILED);x2d_identity_test_address=(uintptr_t)address+16;ck(!x2d_loaded_module_hash(hash));ck(!munmap(address,sizeof bytes));
 for(int mode=1;mode<=6;mode++){
  save(path,bytes,sizeof bytes);fd=open(path,O_RDONLY);ck(fd>=0);address=mmap(NULL,sizeof bytes,PROT_READ|PROT_EXEC,MAP_PRIVATE,fd,0);close(fd);ck(address!=MAP_FAILED);x2d_identity_test_address=(uintptr_t)address+16;
  int old=hits;fault=mode;ck(!x2d_loaded_module_hash(hash)&&hash[0]==0);ck(hits==old+1&&fault==0);ck(!munmap(address,sizeof bytes));
  if(mode==3)ck(!unlink(path)&&!rename("/system/lib64/displaced.so",path));
  if(mode==4)ck(!chmod(path,0644));
  if(mode==5)ck(!chown(path,0,0));
  if(mode==6)ck(!unlink("/system/lib64")&&!rename("/system/real","/system/lib64"));
 }
 save(path,bytes,sizeof bytes);fd=open(path,O_RDONLY);ck(fd>=0);address=mmap(NULL,sizeof bytes,PROT_READ|PROT_EXEC,MAP_PRIVATE,fd,0);close(fd);ck(address!=MAP_FAILED);x2d_identity_test_address=(uintptr_t)address+16;ck(x2d_loaded_module_hash(hash)&&!strcmp(hash,expected));
 printf("INDEPENDENT_MUTATION_HITS %d\n",hits);
"""
 source=source.replace(marker,insert+marker)
 (b/'test_loaded_module_identity.c').write_text(source)
 env=dict(os.environ,TEMP=str(b),TMP=str(b),ZIG_LOCAL_CACHE_DIR=str(b/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(b/'zig-global'))
 base=[str(linux.ZIG),'cc','-target','aarch64-linux-musl','-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-pthread','-DX2D_UI_VERIFY_LOADED=1','-DX2D_IDENTITY_TEST=1']
 commands=[base+['-Dread=checker_read','-c',str(b/'loaded_module_identity.c'),'-o',str(b/'identity.o')],base+['-static',str(b/'identity.o'),str(b/'ui_receipt.c'),str(b/'ui_receipt_worker.c'),str(b/'test_loaded_module_identity.c'),'-o',str(b/'init')]]
 for i,cmd in enumerate(commands):
  r=subprocess.run(cmd,env=env,capture_output=True,text=True,timeout=240);(out/f'build-{i}.log').write_text(r.stdout+r.stderr);assert not r.returncode,r.stdout+r.stderr
 (b/'init.cpio').write_bytes(linux.cpio([('dev',b'',0o40755),('proc',b'',0o40755),('init',(b/'init').read_bytes(),0o100755)]))
 cmd=[linux.DEPS/'qemu/qemu-system-aarch64.exe','-machine','virt','-cpu','cortex-a53','-m','128','-smp','2','-nic','none','-display','none','-monitor','none','-serial','stdio','-no-reboot','-kernel',linux.DEPS/'vmlinuz-virt','-initrd',b/'init.cpio','-append','console=ttyAMA0 rdinit=/init panic=-1 quiet']
 r=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,timeout=60);(out/'run.log').write_text(r.stdout+r.stderr);passed=not r.returncode and 'LOADED_IDENTITY_PASS' in r.stdout and 'INDEPENDENT_MUTATION_HITS 6' in r.stdout
 (out/'report.json').write_text(json.dumps(dict(identity=ids,passed=passed,stdout=r.stdout,real_ARM_Linux=True,mapped_code_address_is_fixture=True,read_boundary_mutations=6,in_process_attestation=False,hardware_operations=0,binary=sha(b/'init')),indent=2));print(out);print(r.stdout);assert passed
if __name__=='__main__':main()
