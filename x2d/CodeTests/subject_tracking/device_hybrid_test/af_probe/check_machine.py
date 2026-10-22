"""离线执行重定位后的 ARM64 倍率函数和入参转发，不连接设备。"""
from pathlib import Path
import sys,io,struct,json,random,importlib.util,hashlib
sys.dont_write_bytecode=True
D=Path(__file__).resolve().parent;X=D.parents[3];R=X.parent
tuned='--tuned-16-24' in sys.argv
afc3=tuned or '--afc-type1-3' in sys.argv
base_scale=1.6 if tuned else 3.0
afc_scale=2.4 if tuned else 3.0
O=X/('outputs/4.2.0/af-speed-16-24-r1/probe' if tuned else 'outputs/4.2.0/afc-type1-3-r1/probe' if afc3 else 'outputs/4.2.0/afc-slow-hold-r1/probe' if '--afc-near-hold' in sys.argv else 'outputs/4.2.0/subject-tracking/device-hybrid-test/af-probe')
sys.path[:0]=[str(X/'tools'),str(R/'.research-cache/x1d-1.25.0/python')]
spec=importlib.util.spec_from_file_location('prior',X/'CodeTests/temporary_af_speed_probe/verify_candidate.py')
prior=importlib.util.module_from_spec(spec);spec.loader.exec_module(prior)
from elftools.elf.elffile import ELFFile
from unicorn import Uc,UC_ARCH_ARM64,UC_MODE_ARM,UC_HOOK_CODE
from unicorn.arm64_const import *
binary=prior.system_elf('/lib64/libaaa.so',prior.SHA)
raw=(O/'libx2d_af_probe.so').read_bytes();elf=ELFFile(io.BytesIO(raw))
symbols={s.name:s['st_value'] for table in ['.symtab','.dynsym'] for s in elf.get_section_by_name(table).iter_symbols() if s['st_value']}
BASE=0x3000000
def load(u):
 hi=max(s['p_vaddr']+s['p_memsz'] for s in elf.iter_segments() if s['p_type']=='PT_LOAD')
 u.mem_map(BASE,(hi+4095)&~4095)
 for s in elf.iter_segments():
  if s['p_type']=='PT_LOAD':u.mem_write(BASE+s['p_vaddr'],s.data())
class ProfileOracle(prior.Runner):
 def hook(self,u,a,n,unused):
  super().hook(u,a,n,unused)
  t=self.case['type']&0xffff
  if a==0xac724 and t in (0,1,2):
   scale=prior.fvalue(prior.fbits((base_scale,afc_scale if self.case.get('afc3') else 2.0,2.8)[t]))
   u.reg_write(UC_ARM64_REG_S10,prior.fbits(prior.fvalue(u.reg_read(UC_ARM64_REG_S10))*scale))
  if a==0xac6c4 and t in (0,1,2) and self.case['mode']==2:
   u.reg_write(UC_ARM64_REG_S0,prior.fbits(min(21844,max(0,prior.fvalue(u.reg_read(UC_ARM64_REG_S0))))))
original=prior.Runner(binary);oracle=ProfileOracle(binary);candidate=prior.Runner(binary)
load(candidate.u)
entry=BASE+symbols['af_speed_clone'];delta=entry-prior.ENTRY
assert delta%4==0 and -(1<<27)<=delta<(1<<27)
candidate.u.mem_write(prior.ENTRY,struct.pack('<I',0x14000000|((delta//4)&0x3ffffff)))
base=dict(type=0,mode=2,lens_speed=5200,aperture=2.5,fps_limit_factor=1,lens_factor=.01175,focal=55,
 steps=1100,close_scale=1,slow_scale=.5,maximum=10000,fps=60,fallback=0)
cases=[]
for t in range(8):
 for mode in (0,1,2):
  for fps in (15,30,38,60,98,120):
   for fallback in (0,1):cases.append(dict(base,type=t,mode=mode,fps=fps,fallback=fallback))
for t in range(6):
 for change in ({'aperture':0},{'steps':0},{'lens_error':-1},{'aperture_error':1},{'factor_error':1},
                {'lens_factor':0},{'focal':20,'close_scale':.7}):cases.append(dict(base,type=t,**change))
rng=random.Random(5515)
for n in range(1000):
 cases.append(dict(base,type=rng.choice([0,1,2,3,4,5,6,65536,65537,65538]),mode=rng.choice([0,1,2]),
  fps=rng.choice([24,38,60,98]),steps=rng.randrange(1,2000),lens_factor=rng.uniform(.0001,.02),
  aperture=rng.choice([2.5,4,8,11]),slow_scale=rng.choice([.25,.5,.75,1]),
  lens_speed=rng.randrange(1,10001)))
for t in range(3):
 for speed in (0,21844,21845,32767,65535):
  for fps in (15,60,120):cases.append(dict(base,type=t,lens_speed=speed,fallback=1,fps=fps))
 for factor in (.000001,.0001,.001):
  for fps in (15,60,120):cases.append(dict(base,type=t,lens_factor=factor,fps=fps))
changed=0
for i,c in enumerate(cases):
 a=original.run(c);expected=oracle.run(c);actual=candidate.run(c)
 assert expected==actual,(i,c,a,expected,actual)
 if c['type']&0xffff not in (0,1,2):assert a==actual,('unmodified type',i,c,a,actual)
 if a!=actual:changed+=1
afc3_cases=0
if afc3:
 candidate=prior.Runner(binary);load(candidate.u)
 entry35=BASE+symbols['af_speed_afc_clone'];delta35=entry35-prior.ENTRY
 candidate.u.mem_write(prior.ENTRY,struct.pack('<I',0x14000000|((delta35//4)&0x3ffffff)))
 for c in cases:
  if not tuned and c['type']&0xffff!=1:continue
  c=dict(c,afc3=True)
  actual=candidate.run(c);expected=oracle.run(c)
  assert actual==expected,('AFC profile',c,actual,expected)
  afc3_cases+=1
# 直接执行实际包装函数，确认拍照三档走候选，其他档及镜头不匹配回退原厂。
route=Uc(UC_ARCH_ARM64,UC_MODE_ARM);load(route)
route.mem_map(0x1000,0xb00000);route.mem_map(prior.PARAM,0x20000);route.mem_map(prior.STACK,0x20000)
ORIGINAL=0x700000;route_target=[None]
route.mem_write(BASE+symbols['originals']+8*8,struct.pack('<Q',ORIGINAL))
route.mem_write(0xa2f2f0,struct.pack('<Q',prior.LENS));route.mem_write(0xa2fa00,struct.pack('<Q',prior.PARAM))
for off in (0x9a0,0x9d0,0xa00):route.mem_write(prior.PARAM+off,struct.pack('<d',1.0))
fw={s.name:s['st_value'] for s in binary.symbols}
def set_modes(afc,recording):
 for name,value in [('is_stand_afc_mode',afc),('is_stand_recording_af_mode',recording)]:
  route.mem_write(fw[name],struct.pack('<II',0x52800000|(int(value)<<5),0xd65f03c0))
  route.ctl_remove_cache(fw[name],fw[name]+8)
if afc3:set_modes(False,False)
def dispatch(u,a,n,_):
 if a not in (ORIGINAL,BASE+symbols['af_speed_clone'],BASE+symbols.get('af_speed_afc_clone',0)):return
 route_target[0]=a
 assert u.reg_read(UC_ARM64_REG_W0)==0 and u.reg_read(UC_ARM64_REG_W1)==request_type
 assert u.reg_read(UC_ARM64_REG_X2)==prior.OUT and u.reg_read(UC_ARM64_REG_S0)==prior.fbits(60)
 u.emu_stop()
route.hook_add(UC_HOOK_CODE,dispatch)
dispatch_cases=0
allowed_keys=(0x12323,0x13131,0x13e3e,0x14f4f,0x24f4f,0x32727,0x14949,0x12f1c,0x1532f)
rejected_keys=(0,0x12d2d,0x34f4f,0x113e3e,0xffffffff)
for key in allowed_keys+rejected_keys:
 matched=key in allowed_keys
 for request_type in (0,1,2,3,65536,65537,65538):
  route.mem_write(prior.LENS+0x30,struct.pack('<I',key))
  route.reg_write(UC_ARM64_REG_SP,prior.STACK+0x10000);route.reg_write(UC_ARM64_REG_LR,prior.HALT)
  route.reg_write(UC_ARM64_REG_W0,0);route.reg_write(UC_ARM64_REG_W1,request_type)
  route.reg_write(UC_ARM64_REG_X2,prior.OUT);route.reg_write(UC_ARM64_REG_S0,prior.fbits(60))
  route_target[0]=None
  route.emu_start(BASE+symbols['speed'],prior.HALT,count=500)
  expected=BASE+symbols['af_speed_clone'] if matched and request_type&65535 in (0,1,2) else ORIGINAL
  assert route_target[0]==expected,(key,request_type,route_target[0],expected)
  dispatch_cases+=1
# 白名单命中也不能跳过后续原厂加速因子的范围检查。
request_type=0
route.mem_write(prior.LENS+0x30,struct.pack('<I',0x13e3e))
for offset in (0x9a0,0x9d0,0xa00):
 for factor in (0.0,1.5,1.5001,-.001,float('nan'),float('inf')):
  route.mem_write(prior.PARAM+offset,struct.pack('<d',factor))
  route.reg_write(UC_ARM64_REG_SP,prior.STACK+0x10000);route.reg_write(UC_ARM64_REG_LR,prior.HALT)
  route.reg_write(UC_ARM64_REG_W0,0);route.reg_write(UC_ARM64_REG_W1,request_type)
  route.reg_write(UC_ARM64_REG_X2,prior.OUT);route.reg_write(UC_ARM64_REG_S0,prior.fbits(60))
  route_target[0]=None;route.emu_start(BASE+symbols['speed'],prior.HALT,count=500)
  expected=BASE+symbols['af_speed_clone'] if 0<=factor<=1.5 else ORIGINAL
  assert route_target[0]==expected,(offset,factor,route_target[0],expected)
  dispatch_cases+=1
 route.mem_write(prior.PARAM+offset,struct.pack('<d',1.0))
if afc3:
 for afc,recording in ((False,False),(True,False),(True,True),(False,False)):
  set_modes(afc,recording)
  for key in allowed_keys+rejected_keys:
   route.mem_write(prior.LENS+0x30,struct.pack('<I',key))
   for request_type in (0,1,2,3,65537):
    route.reg_write(UC_ARM64_REG_SP,prior.STACK+0x10000);route.reg_write(UC_ARM64_REG_LR,prior.HALT)
    route.reg_write(UC_ARM64_REG_W0,0);route.reg_write(UC_ARM64_REG_W1,request_type)
    route.reg_write(UC_ARM64_REG_X2,prior.OUT);route.reg_write(UC_ARM64_REG_S0,prior.fbits(60))
    route_target[0]=None;route.emu_start(BASE+symbols['speed'],prior.HALT,count=700)
    expected=ORIGINAL
    if key in allowed_keys and request_type&65535<=2:
     expected=BASE+symbols['af_speed_afc_clone' if afc and not recording and request_type&65535==1 else 'af_speed_clone']
    assert route_target[0]==expected,(afc,recording,key,request_type,route_target[0],expected)
    dispatch_cases+=1
# 通用入口不能改变整数/浮点/隐藏返回实参、调用者 LR 或栈实参。
u=Uc(UC_ARCH_ARM64,UC_MODE_ARM);load(u)
STACK=0x5000000;HALT=0x6000000
u.mem_map(STACK,0x10000);u.mem_map(HALT,0x1000)
def intercept(u,a,n,_):
 if a==HALT:u.emu_stop()
 if a==BASE+symbols['af_enter']:
  idx=u.reg_read(UC_ARM64_REG_W0);saved=u.reg_read(UC_ARM64_REG_X1)
  assert idx==current and saved==STACK+0x8000-208
  assert struct.unpack('<Q',u.mem_read(saved+72,8))[0]==HALT+8
  ret=u.reg_read(UC_ARM64_REG_LR)
  for j in range(19):u.reg_write(globals()['UC_ARM64_REG_X'+str(j)],0xf000000+j)
  for j in range(8):u.reg_write(globals()['UC_ARM64_REG_Q'+str(j)],j+99)
  u.reg_write(UC_ARM64_REG_X0,HALT);u.reg_write(UC_ARM64_REG_PC,ret)
u.hook_add(UC_HOOK_CODE,intercept)
hook_count=len(json.loads((O/'build.json').read_text())['hooks'])
for current in range(hook_count):
 u.reg_write(UC_ARM64_REG_SP,STACK+0x8000);u.reg_write(UC_ARM64_REG_LR,HALT+8)
 for j in range(9):u.reg_write(globals()['UC_ARM64_REG_X'+str(j)],0x112200+j)
 for j in range(8):u.reg_write(globals()['UC_ARM64_REG_Q'+str(j)],(0xfedcba9876543210<<64)+j)
 u.mem_write(STACK+0x8000,b'stack arguments!')
 u.emu_start(BASE+symbols[f'af_entry_{current}'],HALT+4,count=100)
 assert u.reg_read(UC_ARM64_REG_PC)==HALT
 assert u.reg_read(UC_ARM64_REG_SP)==STACK+0x8000
 assert u.reg_read(UC_ARM64_REG_LR)==HALT+8
 for j in range(9):assert u.reg_read(globals()['UC_ARM64_REG_X'+str(j)])==0x112200+j
 for j in range(8):assert u.reg_read(globals()['UC_ARM64_REG_Q'+str(j)])==(0xfedcba9876543210<<64)+j
 assert bytes(u.mem_read(STACK+0x8000,16))==b'stack arguments!'
result=dict(status='PASS',speedProfile='1.6/2/2.8' if tuned else '3/2/2.8',afcType1Scale=afc_scale if afc3 else 2.0,afcType1Cases=afc3_cases,librarySha256=hashlib.sha256(raw).hexdigest(),speedCases=len(cases),changedCases=changed,speedDispatchCases=dispatch_cases,entryForwardingCases=hook_count,
 deviceModified=False,physicalSpeedMeasured=False,viewfinderOverheadMeasured=False)
(O/'machine-check.json').write_text(json.dumps(result,indent=2),'utf-8')
print(json.dumps(result))
