"""执行真实 ARM64 选档/分类/存储指令与候选包装；外部图像和镜头读数使用替身，不连接相机。"""
from pathlib import Path
import sys,io,struct,json,hashlib
sys.dont_write_bytecode=True
D=Path(__file__).resolve().parent;X=D.parents[3];R=X.parent
tuned='--tuned-16-24' in sys.argv
afc3=tuned or '--afc-type1-3' in sys.argv
base_scale=1.6 if tuned else 3.0
afc_scale=2.4 if tuned else 3.0
afc_hold=afc3 or '--afc-near-hold' in sys.argv
O=X/('outputs/4.2.0/af-speed-16-24-r1/probe' if tuned else 'outputs/4.2.0/afc-type1-3-r1/probe' if afc3 else 'outputs/4.2.0/afc-slow-hold-r1/probe' if afc_hold else 'outputs/4.2.0/subject-tracking/device-hybrid-test/af-probe')
sys.path[:0]=[str(X/'tools'),str(R/'.research-cache/x1d-1.25.0/python')]
from firmware_image import system_elf
from elftools.elf.elffile import ELFFile
from unicorn import Uc,UC_ARCH_ARM64,UC_MODE_ARM,UC_HOOK_CODE
from unicorn.arm64_const import *
b=system_elf('/lib64/libaaa.so','feef8a8dc3a27395e47232c2b25a5da7a7ab335922fbb527e637c439e35bcec7')
raw=(O/'libx2d_af_probe.so').read_bytes();e=ELFFile(io.BytesIO(raw))
symbols={s.name:s['st_value'] for s in e.get_section_by_name('.symtab').iter_symbols() if s['st_value']}
fw={s.name:s['st_value'] for s in b.symbols if s['st_value']}
BASE,DATA,STACK,STUB=0x3000000,0x2000000,0x2100000,0x6000000
PARAM,LENS,AFINFO,ASSIST,CDAF,TLS=DATA,DATA+0x2000,DATA+0x3000,DATA+0x4000,DATA+0x6000,DATA+0xb000
u=Uc(UC_ARCH_ARM64,UC_MODE_ARM);u.mem_map(0x1000,0xb00000)
for s in b.elf.iter_segments():
 if s['p_type']=='PT_LOAD':u.mem_write(s['p_vaddr'],s.data())
# Android APS2 数据重定位的本测试依赖项；其余外部调用由下面白名单替身处理。
assert fw['p_af_params']==0xa2fa00
u.mem_write(0x1dc848,struct.pack('<Q',fw['p_af_params']))
hi=max(s['p_vaddr']+s['p_memsz'] for s in e.iter_segments() if s['p_type']=='PT_LOAD')
u.mem_map(BASE,(hi+4095)&~4095)
for s in e.iter_segments():
 if s['p_type']=='PT_LOAD':u.mem_write(BASE+s['p_vaddr'],s.data())
u.mem_map(DATA,0x20000);u.mem_map(STACK,0x20000);u.mem_map(STUB,0x10000)
def put(a,fmt,v):u.mem_write(a,struct.pack('<'+fmt,v))
def get(a,fmt):return struct.unpack('<'+fmt,u.mem_read(a,struct.calcsize(fmt)))[0]
for a,p in [(0xa2fa00,PARAM),(0xa2f2f0,LENS),(0xa290c8,AFINFO),(0xa294f0,ASSIST),(0xa299f0,CDAF),(0xa2b0f0,CDAF)]:put(a,'Q',p)
put(LENS+0x30,'I',81470)
for off in [0x9a0,0x9d0,0xa00]:put(PARAM+off,'d',1.0)
put(PARAM+0x868+0x28,'d',.1);put(PARAM+0x868+0x30,'H',0);put(PARAM+0x868+0x32,'B',1)
for off,val in [(0,2.),(8,4.),(16,1.),(24,2.)]:put(PARAM+0x980+off,'d',val)
put(PARAM+0x980+0x28,'B',1)
put(PARAM+0x920+0x57,'B',2);put(PARAM+0x920+0x5a,'B',2)
put(BASE+symbols['originals']+20*8,'Q',fw['alg_set_distance_to_peak'])
put(BASE+symbols['originals']+21*8,'Q',fw['cdaf_get_scan_speed_type'])
put(BASE+symbols['originals']+22*8,'Q',STUB+0xf00)
put(BASE+symbols['originals'],'Q',fw['hybrid_af_start']);put(BASE+symbols['ready'],'I',1)
put(TLS+0x28,'Q',0x12345678);u.reg_write(UC_ARM64_REG_TPIDR_EL0,TLS)
dyn=b.elf.get_section_by_name('.dynsym')
slots={r['r_offset']:dyn.get_symbol(r['r_info_sym']).name for r in b.elf.get_section_by_name('.rela.plt').iter_relocations()}
plt={};last=None;p=b.elf.get_section_by_name('.plt')
for i in b.cs.disasm(p.data(),p['sh_addr']):
 if i.mnemonic=='adrp':last=(i.address,i.operands[-1].imm)
 elif i.mnemonic=='ldr' and last:
  n=slots.get(last[1]+i.operands[-1].mem.disp)
  if n:plt[last[0]]=n
imports={};dyn=e.get_section_by_name('.dynsym')
for i,r in enumerate(e.get_section_by_name('.rela.plt').iter_relocations()):
 addr=STUB+i*16;put(BASE+r['r_offset'],'Q',addr);imports[addr]=dyn.get_symbol(r['r_info_sym']).name
samples=[1000,1001,1002];current,last_pos=100,98;fake_time=1000;reset_args=[]
actual={'alg_assistant_handle','alg_get_distance_to_peak','af_get_cdaf_special_param','af_get_pdaf_af_mode_params','af_get_pdaf_param','is_stand_recording_af_mode',
 'is_stand_afc_mode','get_speed_type_from_peak_distance','cdaf_get_current_speed_type',
 'cdaf_set_current_speed_type','cdaf_get_speed_increase_start_frame_cnt','cdaf_set_speed_increase_start_frame_cnt','cdaf_work_frm_get'}
def return_int(v):
 u.reg_write(UC_ARM64_REG_X0,v&((1<<64)-1));u.reg_write(UC_ARM64_REG_PC,u.reg_read(UC_ARM64_REG_LR))
def hook(u,a,n,_):
 global fake_time
 if a==STUB+0xf00:
  reset_args.append(u.reg_read(UC_ARM64_REG_W0));put(ASSIST+0x218,'I',0);return_int(0x1234);return
 if a in imports:
  name=imports[a]
  if name=='clock_gettime':
   p=u.reg_read(UC_ARM64_REG_X1);fake_time+=1000;put(p,'q',0);put(p+8,'q',fake_time*1000);return_int(0);return
  raise AssertionError(('unexpected candidate import',name))
 if a not in plt:return
 name=plt[a]
 if name in actual:u.reg_write(UC_ARM64_REG_PC,fw[name]);return
 if name in ('log_mgr_check','af_is_zoom_tracking_af_busy'):return_int(0)
 elif name=='cdaf_get_cur_buf_indx':return_int(4)
 elif name=='cdaf_getp_cur_buf_cfa_fv':return_int(100+u.reg_read(UC_ARM64_REG_W1))
 elif name=='cdaf_get_indicator':return_int(samples[u.reg_read(UC_ARM64_REG_X1)-101])
 elif name=='lens_ctrl_get_cur_focus_pos':return_int(current)
 elif name=='lens_ctrl_get_last_focus_pos':return_int(last_pos)
 elif name=='af_get_frame_rate':u.reg_write(UC_ARM64_REG_S0,struct.unpack('<I',struct.pack('<f',60.))[0]);return_int(0)
 elif name=='lens_ctrl_get_steps_per_fdelta_double':
  u.reg_write(UC_ARM64_REG_D0,struct.unpack('<Q',struct.pack('<d',10.))[0]);return_int(0)
 elif name=='cdaf_get_scan_speed_time':return_int((6000,3000,1500)[u.reg_read(UC_ARM64_REG_W1)])
 else:raise AssertionError(('unmodelled firmware call',hex(a),name))
u.hook_add(UC_HOOK_CODE,hook)
def call(addr,*args,lr=STUB+0xfff0):
 u.reg_write(UC_ARM64_REG_SP,STACK+0x10000);u.reg_write(UC_ARM64_REG_LR,lr)
 for i,v in enumerate(args):u.reg_write(globals()['UC_ARM64_REG_X'+str(i)],v&((1<<64)-1))
 try:u.emu_start(addr,lr,count=16000)
 except Exception:
  print('Failed PC',hex(u.reg_read(UC_ARM64_REG_PC)),'x0/x8/x9',[hex(u.reg_read(r)) for r in (UC_ARM64_REG_X0,UC_ARM64_REG_X8,UC_ARM64_REG_X9)])
  raise
 assert u.reg_read(UC_ARM64_REG_PC)==lr,('unfinished',hex(u.reg_read(UC_ARM64_REG_PC)))
 assert u.reg_read(UC_ARM64_REG_SP)==STACK+0x10000
 return u.reg_read(UC_ARM64_REG_X0)
def wrap(name,*args,**kw):return call(BASE+symbols[name],*args,**kw)
def reset():
 u.mem_write(DATA+0xe000,bytes(80))
 assert wrap('af_enter',0,DATA+0xe000)==fw['hybrid_af_start']
 put(ASSIST+0x218,'I',0);put(CDAF+0x3f60,'I',0);put(CDAF+0x3f64,'I',6)
def choose():return wrap('near_select_speed',0)&0xffffffff
def distance(value,frame,caller=0x904d8):
 put(AFINFO+0x44,'I',frame);wrap('near_set_distance',0,1,value,lr=caller)
 return get(ASSIST+0x218,'I')
checks=[]
reset();assert choose()==0
checks.append('initial fast selection unchanged')
# 真实原厂三个 FV 样本先选慢档，随后平坦样本原厂选快；候选应保留慢档。
samples[:]=[100,150,230];assert choose()==1
samples[:]=[1000,1001,1002];assert call(fw['cdaf_get_scan_speed_type'],0)==0
assert choose()==1 and get(CDAF+0x3f64,'I')==1 and get(CDAF+0x3f60,'I')==0
checks.append('original type1-to0 reproduced and corrected, internal current type matches')
# 真实相位距离分类得出很近，再走方向分支清零；原厂会变快，候选保留慢档。
reset();value=call(fw['get_pdaf_distance_to_peak'],0,101,2)&0xffffffff;assert value==2,value
assert distance(value,10)==2 and choose()==2
call(fw['alg_set_distance_to_peak'],0,1,0);assert call(fw['cdaf_get_scan_speed_type'],0)==0
assert distance(2,11)==2;assert distance(0,12,0x90720)==2 and choose()==2
checks.append('original measured-near then direction-clear regression reproduced and corrected')
# 门槛未通过的0不能当作远；同帧重复也不能完成两帧确认。
put(PARAM+0x920+0x57,'B',1)
assert distance(0,13)==2 and distance(0,14)==2 and choose()==2
put(PARAM+0x920+0x57,'B',2)
value=call(fw['get_pdaf_distance_to_peak'],0,200,2)&0xffffffff;assert value==0,value
assert distance(value,15)==2 and distance(value,15)==2 and choose()==2
assert distance(value,16)==0 and choose()==0
checks.append('gated-out zero rejected; two distinct original far classifications restore fast')
# 初始相位方向慢扫本身不冒充近焦；新请求和原厂重置不继承旧慢档。
reset();assert distance(0,20,0x90720)==0 and choose()==0
distance(1,21);assert choose()==1
assert wrap('near_reset_assistant',0)==0x1234 and reset_args[-1]==0 and choose()==0
checks.append('unclassified phase scan does not latch; original reset ABI and state reset')
# 作用域、异常档号、模式、镜头、竞争均不阻塞原厂。
reset();distance(2,22);put(LENS+0x30,'I',0);put(ASSIST+0x218,'I',0);assert choose()==0
put(LENS+0x30,'I',81470);assert choose()==0
reset();distance(2,23);put(AFINFO+9,'B',2);put(ASSIST+0x218,'I',0)
assert choose()==call(fw['cdaf_get_scan_speed_type'],0)
put(AFINFO+9,'B',0);assert choose()==0
reset();distance(2,24);put(AFINFO+8,'B',1);put(ASSIST+0x218,'I',0)
assert choose()==call(fw['cdaf_get_scan_speed_type'],0)
put(AFINFO+8,'B',0);assert choose()==0
reset();distance(2,25);put(BASE+symbols['near_busy'],'B',1);put(ASSIST+0x218,'I',0)
assert choose()==0 and get(BASE+symbols['near_contention'],'I')>0
put(BASE+symbols['near_busy'],'B',0)
checks.append('other lens, AFC, recording and contention use original path without waiting')
# 原厂提速等待开着时，修正后的档位与等待计数仍一致。
reset();samples[:]=[100,150,230];assert choose()==1
samples[:]=[1000,1001,1002];put(PARAM+0x868+0x30,'H',2)
put(CDAF+0x18,'I',100);assert choose()==1
put(CDAF+0x18,'I',102);assert choose()==1 and get(CDAF+0x3f60,'I')==0
checks.append('original acceleration wait remains consistent with corrected type')
if afc_hold:
 put(AFINFO+9,'B',2)
 u.mem_write(PARAM+0x8a0,bytes(u.mem_read(PARAM+0x868,0x38)))
 put(PARAM+0x8a0+0x28,'d',.5);put(PARAM+0x8a0+0x30,'H',0)
 u.mem_write(PARAM+0x9b0,bytes(u.mem_read(PARAM+0x980,0x30)))
 samples[:]=[1000,1001,1002];reset();assert choose()==0
 samples[:]=[100,200,250];assert choose()==1
 samples[:]=[200,220,230]
 for _ in range(120):
  assert call(fw['cdaf_get_scan_speed_type'],0)==0
  assert choose()==1 and get(CDAF+0x3f64,'I')==1
 checks.append('AFC actual native FV slow-to-fast held through 120 flat updates')
 # 不可信的距离0、同帧重复以及间隔过长，均不能解锁快档。
 put(PARAM+0x920+0x57,'B',1)
 distance(0,100);distance(0,101);assert choose()==1
 put(PARAM+0x920+0x57,'B',2)
 distance(0,102);distance(0,102);assert choose()==1
 distance(0,200);assert choose()==1
 distance(0,201);assert choose()==0
 checks.append('AFC fresh adjacent far classifications restore fast; stale duplicate and gated zeros do not')
 reset();distance(2,300);assert choose()==2
 assert distance(0,301,0x90720)==2 and choose()==2
 distance(0,302);assert choose()==2
 distance(0,303);assert choose()==0
 checks.append('AFC phase distance clearing cannot accelerate; original confirmed far path recovers')
 samples[:]=[100,200,250];assert choose()==1
 samples[:]=[200,220,230];reset();assert choose()==0
 checks.append('AFC new autofocus request resets slow latch')
 samples[:]=[100,200,250];assert choose()==1
 samples[:]=[1000,1001,1002];put(PARAM+0x868+0x30,'H',0)
 put(AFINFO+9,'B',0);assert choose()==0
 put(AFINFO+9,'B',2);assert choose()==0
 checks.append('AFC to AFS and back clears previous mode latch')
 assert not any(seg['p_type']=='PT_TLS' for seg in e.iter_segments())
 checks.append('No compiler TLS; existing native interfaces and hook set retained')
result=dict(status='PASS',nearGuard='near-hold-v1',librarySha256=hashlib.sha256(raw).hexdigest(),checks=checks,
 firmwareInstructionsExecuted=True,externalImageAndLensInputsMocked=True,deviceAccess=False,physicalOvershootVerified=False)
(O/'near-machine-check.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),'utf-8')
print(json.dumps(result,ensure_ascii=False))
