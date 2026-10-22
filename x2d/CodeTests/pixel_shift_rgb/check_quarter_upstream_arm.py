"""实际4.2.0 JPEG插件→候选AArch64入口→HAL替身；不连接设备。
系统调用、原厂RAW回调的QMap登记及硬件HAL为替身；整幅缩小另由host测试。
"""
import struct,json,sys
from pathlib import Path
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[2]
sys.path[:0]=[str(HERE),str(ROOT/'x2d/tools'),str(ROOT/'.research-cache/x1d-1.25.0/python')]
import build_quarter_upstream as build
from firmware_image import system_elf,FirmwareElf
from unicorn import Uc,UC_ARCH_ARM64,UC_MODE_ARM,UC_HOOK_CODE
from unicorn.arm64_const import *

def local_hal_fingerprint(symbol,offset,size,expected):
 from firmware_image import system_elf
 import hashlib
 # 用户自行提供的本地合法原厂输入；不解密、不下载、不导出厂家文件。
 binary=system_elf('/lib64/libduml_hal.so','77875f6f47a2137e5461c61789d44fdd8537e7359c8d7b43ec7ee4737fd15bb3')
 addresses={s['st_value'] for s in binary.symbols if s.name==symbol and s['st_value']}
 if len(addresses)!=1:raise ValueError('Local HAL symbol is missing or ambiguous')
 data=binary.read(next(iter(addresses))+offset,size)
 if hashlib.sha256(data).hexdigest()!=expected:raise ValueError('Local HAL fingerprint mismatch')
 return data

class Machine:
 def __init__(self,fail_write=0):
  self.u=Uc(UC_ARCH_ARM64,UC_MODE_ARM);self.stub_next=0x50000000;self.callbacks={};self.imports={}
  self.u.mem_map(self.stub_next,0x100000);self.u.mem_map(0x60000000,0x100000)
  self.u.mem_map(0x70000000,0x100000);self.heap=0x60001000
  self.stop=0x500ff000;self.now=10000;self.mode=1;self.known=1;self.socket_ok=True;self.hal_error=0
  self.fail_write=fail_write;self.writes=0;self.position=0;self.free_count=0;self.hal=[];self.requests=0
  self.map_count=0;self.unmap_count=0;self.closes=0;self.get_entry=0
  self.u.reg_write(UC_ARM64_REG_TPIDR_EL0,0x70000000)
  self.u.reg_write(UC_ARM64_REG_CPACR_EL1,3<<20)
  self.service=0x10000000;self.plugin=0x20000000;self.lib=0x30000000
  self.service_elf=system_elf('/bin/camera-service',build.SHA_SERVICE)
  self.plugin_elf=system_elf('/lib64/camera/plugins/ienc/libdcam_ienc_jpeg.so',build.SHA_PLUGIN)
  self.candidate=FirmwareElf((build.OUT/'libx2d_quarter_upstream.so').read_bytes())
  self.load(self.service_elf,self.service,False);self.load(self.plugin_elf,self.plugin,True)
  self.symbol={s.name:self.lib+s['st_value'] for s in self.candidate.symbols if s['st_shndx']!='SHN_UNDEF'}
  self.load(self.candidate,self.lib,True)
  self.u.hook_add(UC_HOOK_CODE,self.instruction)
  self.input=self.alloc(32);self.output=self.alloc(32);self.private=self.alloc(32);self.ion=self.alloc(32)
  self.input_pixels=self.alloc(32);self.private_pixels=self.alloc(32)
  self.callbacks[self.symbol['qd_resize']]=lambda:1 # host已验证每个实际输出字节
  # ION已有设备借引用分支的固定指纹；open/close仍为可计数替身。
  self.u.mem_write(self.imports['duss_hal_device_open']+0x2a4,local_hal_fingerprint('duss_hal_device_open',0x2a4,20,'78e547b1d03185f787e42aa7d41019f335ef72e1c3b2070fdf7865225a5c66eb')) if 'duss_hal_device_open' in self.imports else None
 def alloc(self,n):
  a=self.heap;self.heap+=(n+15)&~15;self.u.mem_write(a,bytes(n));return a
 def word(self,a,n):self.u.mem_write(a,struct.pack('<Q',n&0xffffffffffffffff))
 def dword(self,a,n):self.u.mem_write(a,struct.pack('<I',n&0xffffffff))
 def r64(self,a):return struct.unpack('<Q',self.u.mem_read(a,8))[0]
 def r32(self,a):return struct.unpack('<I',self.u.mem_read(a,4))[0]
 def arg(self,n):return self.u.reg_read(UC_ARM64_REG_X0+n)
 def text(self,a):
  out=bytearray()
  while (c:=self.u.mem_read(a+len(out),1))!=b'\0':out+=c
  return out.decode()
 def stub(self,name):
  if name not in self.imports:
   a=self.stub_next;self.stub_next+=0x1000;self.imports[name]=a
   self.u.mem_write(a,bytes.fromhex('c0035fd6'));self.callbacks[a]=lambda:self.external(name)
   if name=='duss_hal_device_open':self.u.mem_write(a+0x2a4,local_hal_fingerprint('duss_hal_device_open',0x2a4,20,'78e547b1d03185f787e42aa7d41019f335ef72e1c3b2070fdf7865225a5c66eb'))
   if name=='duss_hal_device_close':self.u.mem_write(a+0x228,local_hal_fingerprint('duss_hal_device_close',0x228,12,'f2091f94d8d66e599fe100be7783e211b2a22f2e7083d526e9d01206a50b9322'))
  return self.imports[name]
 def load(self,b,base,relocate):
  size=max(s['p_vaddr']+s['p_memsz'] for s in b.elf.iter_segments() if s['p_type']=='PT_LOAD')
  self.u.mem_map(base,(size+4095)&~4095)
  for s in b.elf.iter_segments():
   if s['p_type']=='PT_LOAD':self.u.mem_write(base+s['p_vaddr'],s.data())
  if not relocate:return
  for sec in b.elf.iter_sections():
   if sec['sh_type']!='SHT_RELA':continue
   sy=b.elf.get_section(sec['sh_link'])
   for r in sec.iter_relocations():
    t=r['r_info_type'];a=r['r_addend']
    if t==1027:value=base+a
    elif t in (1025,1026,257):
     s=sy.get_symbol(r['r_info_sym']);value=(self.stub(s.name) if s['st_shndx']=='SHN_UNDEF' else base+s['st_value'])+a
    else:raise AssertionError(('relocation',t))
    self.word(base+r['r_offset'],value)
 def instruction(self,u,pc,size,data):
  if pc in self.callbacks:
   ret=self.callbacks[pc]();u.reg_write(UC_ARM64_REG_X0,ret&0xffffffffffffffff);u.reg_write(UC_ARM64_REG_PC,u.reg_read(UC_ARM64_REG_LR))
 def external(self,name):
  a=[self.arg(i) for i in range(6)]
  if name in ('memcpy','memmove'):self.u.mem_write(a[0],bytes(self.u.mem_read(a[1],a[2])));return a[0]
  if name=='memset':self.u.mem_write(a[0],bytes([a[1]&255])*a[2]);return a[0]
  if name=='memcmp':return int(bytes(self.u.mem_read(a[0],a[2]))!=bytes(self.u.mem_read(a[1],a[2])))
  if name=='strcmp':return int(self.text(a[0])!=self.text(a[1]))
  if name=='readlink':s=b'/system/bin/camera-service';self.u.mem_write(a[1],s);return len(s)
  if name=='dl_iterate_phdr':self.word(self.symbol['service'],self.service);self.word(self.symbol['plugin'],self.plugin);return 0
  if name=='dlsym':return self.stub(self.text(a[1]))
  if name=='open':assert self.text(a[0])=='/proc/self/mem';return 7
  if name=='lseek':self.position=a[1];return a[1]
  if name=='write':
   self.writes+=1;n=a[2] if self.writes!=self.fail_write else 4
   self.u.mem_write(self.position,bytes(self.u.mem_read(a[1],n)));return n
  if name=='close':self.closes+=1;return 0
  if name=='mmap':return self.alloc(a[1])
  if name in ('mprotect','munmap','pthread_mutex_lock','pthread_mutex_unlock','pthread_mutex_trylock'):return 0
  if name=='clock_gettime':self.word(a[1],self.now//1000);self.word(a[1]+8,self.now%1000*1000000);return 0
  if name=='dcam_container_get_by_id':assert a[1]==0xbb8;return self.get_entry
  if name=='socket':return 8 if self.socket_ok else -1
  if name=='connect':return 0
  if name=='send':self.query=bytes(self.u.mem_read(a[1],a[2]));self.requests+=1;return a[2]
  if name=='poll':return 1
  if name=='recv':
   magic,exposure,_,_=struct.unpack('<4I',self.query)
   self.u.mem_write(a[1],struct.pack('<4I',magic,exposure,self.known,self.mode));return 16
  if name=='duss_hal_device_open':self.word(a[2],self.ion);return -1004
  if name=='duss_hal_device_close':assert a[0]==self.ion;return 0
  if name=='duss_hal_device_get_state':self.u.mem_write(a[1],b'\2');return 0
  if name=='duss_hal_mem_alloc':assert a[2]==5888*4372*2;self.word(a[1],self.private);return 0
  if name=='duss_hal_mem_get_size':self.dword(a[1],11776*8742*2 if a[0]==self.input else 5888*4372*2);return 0
  if name=='duss_hal_mem_map':self.word(a[1],self.input_pixels if a[0]==self.input else self.private_pixels);self.map_count+=1;return 0
  if name=='duss_hal_mem_unmap':self.unmap_count+=1;return 0
  if name=='duss_hal_mem_sync':return 0
  if name=='duss_hal_mem_free':assert a[0]==self.private;self.free_count+=1;return 0
  if name=='duss_hal_ienc_encfrm':
   f=self.r64(a[1]);s=self.r64(a[1]+8);q=self.r32(a[1]+16)
   self.hal.append((self.r32(f+0x38),self.r32(f+0x3c),q))
   if not self.hal_error:self.dword(s+0x40,8192)
   return self.hal_error
  if name in ('log_mgr_check','duss_log_gen_head'):return 0
  if name=='duss_log_print':return 0
  if name=='__errno':return 0x70000200
  raise AssertionError(('unexpected external',name,a))
 def call(self,addr,*args,until=None):
  self.u.reg_write(UC_ARM64_REG_SP,0x700ff000);self.u.reg_write(UC_ARM64_REG_LR,self.stop)
  for i,a in enumerate(args):self.u.reg_write(UC_ARM64_REG_X0+i,a)
  self.u.emu_start(addr,until or self.stop,count=100000)
  assert self.u.reg_read(UC_ARM64_REG_PC)==(until or self.stop)
  return self.u.reg_read(UC_ARM64_REG_W0)
 def initialize(self):
  assert self.call(self.symbol['qdu_initialize_before_capture'])==1
  assert self.r64(self.plugin+0x1ffd0)==self.symbol['encode']
  assert self.call(self.symbol['qdu_initialize_before_capture'])==0
 def setup(self,raw_kind=2):
  self.initialize()
  # 真跳板执行保存寄存器的16字节，再跳回原函数+16；不模拟整套原厂拍摄状态机。
  t=self.r64(self.symbol['captured_original']);self.call(t,0,0,until=self.service+0x2150f8)
  assert self.u.reg_read(UC_ARM64_REG_SP)==0x700ff000-0x1b0
  engine=self.alloc(0x240);image=self.alloc(0x50);raw=self.alloc(0x100)
  data=self.alloc(0x30);node=self.alloc(0xc0);functor=self.alloc(0x30)
  self.word(engine+0x220,data);self.word(data+0x10,node);self.dword(node+0x20,73)
  self.word(node+0x70,functor);self.word(functor,self.service+0x7a6fd8);self.dword(functor+0x24,902)
  # 执行原厂imgMemory跳表至具体分支；用它判定kind，不用候选自己的常量。
  item=self.alloc(0x30);self.word(item+0x10,image);result=self.alloc(16)
  for kind,target in [(1,0x2199d0),(2,0x219a88)]:
   self.dword(image+0x24,kind);self.u.reg_write(UC_ARM64_REG_X8,result)
   self.call(self.service+0x219758,item,until=self.service+target)
  self.dword(image+0x20,5);self.dword(image+0x24,raw_kind);self.dword(image+0x28,64);self.word(image+0x30,raw);self.dword(raw+0x20,73)
  original=self.stub('synthetic_raw_callback');self.callbacks[original]=lambda:0;self.word(self.symbol['captured_original'],original)
  self.call(self.symbol['captured'],engine,image)
  if raw_kind==2:assert self.r32(self.symbol['mappings'])==5 and self.r32(self.symbol['mappings']+8)==902
  else:assert self.r32(self.symbol['mappings']+24)==0
  assert self.r64(engine+0x198)==0
  f=self.alloc(0x240);s=self.alloc(0x1c8);request=self.alloc(0x30);enc=self.alloc(0x40)
  self.word(f+0x20,self.input);self.dword(f+0x28,103);self.dword(f+0x38,11656);self.dword(f+0x3c,8742)
  for p in range(2):
   for off,v in [(0,11776),(4,p*11776*8742),(8,8742),(12,11776*8742)]:self.dword(f+0x40+p*16+off,v)
  self.dword(f+0x80,2);self.dword(f+0x84,73);self.word(f+0x218,self.alloc(16))
  self.word(s+0x20,self.output);self.dword(s+0x28,8);self.dword(s+0x38,65536)
  self.dword(s+0x30,11656);self.dword(s+0x34,8742)
  self.word(request+0x18,f);self.word(request+0x20,s);self.word(enc+0x20,self.alloc(16))
  self.get_entry=self.alloc(16);tag=self.alloc(20);self.dword(self.get_entry,1);self.dword(self.get_entry+4,20);self.word(self.get_entry+8,tag)
  self.dword(tag,5);self.dword(tag+12,64)
  self.frame,self.stream,self.request,self.encoder,self.tag=f,s,request,enc,tag
  self.before=bytes(self.u.mem_read(f,0x240))
 def run(self):
  rc=self.call(self.plugin+0x1778,self.encoder,self.request)
  assert bytes(self.u.mem_read(self.frame,0x240))==self.before
  return rc

def main():
 checks=[]
 for name,configure,direct in [
  ('raw_snapshot_to_plugin_q80',lambda m:None,True),
  ('ordinary_capture',lambda m:setattr(m,'mode',0),False),
  ('snapshot_not_arrived',lambda m:setattr(m,'known',0),False),
  ('bridge_unavailable',lambda m:setattr(m,'socket_ok',False),False),
  ('old_mapping',lambda m:setattr(m,'now',21001),False),
  ('token_mismatch',lambda m:m.dword(m.tag+12,65),False)]:
  m=Machine();m.setup();configure(m);assert m.run()==0
  assert m.hal==([(5828,4372,80)] if direct else [(11656,8742,90)])
  assert m.free_count==int(direct) and m.map_count==m.unmap_count==2*int(direct)
  assert m.run()==0 and m.hal[-1]==(11656,8742,90) # consumed mapping
  checks.append(name)
 for fail in (1,2):
  m=Machine(fail);assert m.call(m.symbol['qdu_initialize_before_capture'])==0
  assert bytes(m.u.mem_read(m.service+0x2150e8,16))==m.service_elf.read(0x2150e8,16)
  assert m.r64(m.plugin+0x1ffd0)==m.imports['duss_hal_ienc_encfrm'];checks.append('partial_hook_write_rollback_'+str(fail))
 m=Machine();m.setup(raw_kind=1);assert m.run()==0 and m.hal==[(11656,8742,90)]
 checks.append('jpeg_callback_must_not_establish_raw_mapping')
 report=dict(offline=True,device_requests=0,actual_candidate_aarch64=True,actual_factory_jpeg_plugin=True,
  actual_hook_and_rollback=True,cases=checks,passed=True,hal_mock=True,native_raw_dispatch_mock=True,
  image_resize_mock_in_arm=True,image_resize_fullsize_host_test=True,not_hardware_performance=True)
 (build.OUT/'upstream-arm-validation.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 print('QUARTER_UPSTREAM_ARM_PLUGIN_PASS',len(checks))
if __name__=='__main__':main()
