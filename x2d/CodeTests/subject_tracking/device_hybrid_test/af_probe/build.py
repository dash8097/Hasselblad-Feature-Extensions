"""固定固件离线构建；不连接相机，不安装。"""
from pathlib import Path
import hashlib,io,json,os,subprocess,sys,struct
sys.dont_write_bytecode=True
D=Path(__file__).resolve().parent; X=D.parents[3]; R=X.parent
tuned='--tuned-16-24' in sys.argv
afc3=tuned or '--afc-type1-3' in sys.argv
base_scale=1.6 if tuned else 3.0
afc_scale=2.4 if tuned else 3.0
afc_hold=afc3 or '--afc-near-hold' in sys.argv
O=X/('outputs/4.2.0/af-speed-16-24-r1/probe' if tuned else 'outputs/4.2.0/afc-type1-3-r1/probe' if afc3 else 'outputs/4.2.0/afc-slow-hold-r1/probe' if afc_hold else 'outputs/4.2.0/subject-tracking/device-hybrid-test/af-probe')
assert not afc_hold or not (O.parent/'device-apply.txt').exists(), 'Preserve installed evidence'
O.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(X/'tools'))
from firmware_image import system_elf,system_file
from elftools.elf.elffile import ELFFile
SHA='feef8a8dc3a27395e47232c2b25a5da7a7ab335922fbb527e637c439e35bcec7'
b=system_elf('/lib64/libaaa.so',SHA)
spec=json.loads((X/'CodeTests/temporary_af_speed_probe/candidate.json').read_text())
start,end=spec['entry'],spec['end']; code=bytearray(b.read(start,end-start))
assert hashlib.sha256(code).hexdigest()==spec['functionSha256']
private_spec_path=os.environ.get('X2D_PRIVATE_AF_PATCH_SPEC')
if not private_spec_path:raise RuntimeError('Set X2D_PRIVATE_AF_PATCH_SPEC to your local private patch inputs; no vendor code is distributed')
private_spec=json.loads(Path(private_spec_path).read_text(encoding='utf8'))
assert len(private_spec['ranges'])==len(spec['ranges'])
for public,private in zip(spec['ranges'],private_spec['ranges']):
 assert (public['va'],public['length'])==(private['va'],private['length'])
 for field in ('original','candidate'):
  raw=bytes.fromhex(private[field]);assert len(raw)==public['length'] and hashlib.sha256(raw).hexdigest()==public[field+'_sha256']
for p in private_spec['ranges']:
 off=p['va']-start; old=bytes.fromhex(p['original']);new=bytes.fromhex(p['candidate'])
 assert code[off:off+len(old)]==old and len(old)==len(new)
 code[off:off+len(old)]=new
assert hashlib.sha256(code).hexdigest()==spec['candidateSha256']
# 逐条保留原厂/历史候选语义；只重定位 PC-relative 指令。
lines=['.text','.p2align 2','.global af_speed_clone','.hidden af_speed_clone','af_speed_clone:']
literals=[];veneers={}
def literal(value):
 name=f'af_literal_{len(literals)}';literals.append((name,value));return name
for i in b.cs.disasm(bytes(code),start):
 lines.append(f'L_{i.address:x}:')
 m,op=i.mnemonic,i.op_str
 if i.address==0xac3e4:
  assert m=='adrp' and i.operands[-1].imm==0x1a4000
  lines.append('adr x8, af_switch')
 elif i.address==0xac3e8:
  assert op=='x8, x8, #0xb20'
  lines.append('nop')
 elif i.address==0xac730:
  assert m=='fmov' and op=='s0, #3.00000000'
  lines.append('cmp w8, #0')
  if tuned:
   bits=struct.unpack('<I',struct.pack('<f',base_scale))[0]
   lines.extend([f'movz w9, #{bits&65535}',f'movk w9, #{bits>>16}, lsl #16','fmov s0, w9'])
  else:lines.append('fmov s0, #3.00000000')
  lines.extend(['fmov s1, #2.00000000','fcsel s0, s0, s1, eq','cmp w8, #2'])
 elif i.address==0xac734:
  assert m=='fmov' and op=='s1, #2.00000000'
  # 2.8 无浮点立即数编码；使用精确 IEEE-754 单精度常量，不能近似成可编码值。
  bits=struct.unpack('<I',struct.pack('<f',2.8))[0]
  lines.extend([f'movz w9, #{bits&65535}',f'movk w9, #{bits>>16}, lsl #16','fmov s1, w9'])
 elif m in ('adrp','adr'):
  dest=i.operands[-1].imm;reg=op.split(',')[0]
  lines.append(f'ldr {reg}, {literal(dest)}')
 elif m in ('b','bl') or m.startswith('b.') or m in ('cbz','cbnz','tbz','tbnz'):
  dest=i.operands[-1].imm
  if start<=dest<end: target=f'L_{dest:x}'
  else:
   assert m in ('b','bl'),(hex(i.address),m)
   if dest not in veneers:veneers[dest]=literal(dest)
   target=f'V_{dest:x}'
  if ',' in op:op=op.rsplit(',',1)[0]+', '+target
  else:op=target
  lines.append(m+' '+op)
 else:lines.append('.inst 0x'+int.from_bytes(i.bytes,'little').to_bytes(4,'big').hex())
for dest,lit in veneers.items():lines.extend([f'V_{dest:x}:',f'ldr x16, {lit}','br x16'])
lines+=['.p2align 2','af_switch:']
for off in struct.unpack('<4i',b.read(0x1a4b20,16)):
 dest=0x1a4b20+off;assert start<=dest<end
 lines.append(f'.word L_{dest:x} - af_switch')
if afc3:
 # 独立的 AFC 一档副本，复用同一重定位表；原有 AF-S 倍率代码不变。
 extra='\n'.join(lines).replace('af_speed_clone','af_speed_afc_clone').replace('L_','AFC_L_').replace('V_','AFC_V_').replace('af_switch','afc_switch')
 assert extra.count('fmov s1, #2.00000000')==1
 if tuned:
  bits=struct.unpack('<I',struct.pack('<f',afc_scale))[0]
  replacement=f'movz w9, #{bits&65535}\nmovk w9, #{bits>>16}, lsl #16\nfmov s1, w9'
 else:replacement='fmov s1, #3.00000000'
 extra=extra.replace('fmov s1, #2.00000000',replacement)
 lines.extend(extra.splitlines())
lines+=['.data','.p2align 3','.global af_literals','.hidden af_literals','af_literals:']
for name,value in literals:lines.extend([f'{name}:',f'.quad {value}'])
lines+=['.section .note.GNU-stack,"",%progbits']
(O/'speed_clone.S').write_text('\n'.join(lines)+'\n')
hooks=[
 ('hybrid_af_start',1,0),('cdaf_start_prepare',2,1),('cdaf_start_coarse_peak',2,2),
 ('cdaf_start_goto_peak',2,3),('_exec_pdaf_afs_process',2,4),
 ('_exec_pdaf_afc_process',2,5),('exec_fast_cdaf_process',2,6),('cdaf_end_af',9,0),
 ('lens_ctrl_get_focus_speed',3,0),('lens_cmd_push_focus_scan_cmd',4,0),
 ('lens_cmd_push_focus_abs_cmd',5,0),('lens_cmd_push_focus_stop_cmd',6,0),
 ('af_wrap_lens_scan_move_motor',7,0),('af_wrap_lens_abs_move_motor',8,0),
 ('lens_cmd_container_push',0,0),('lens_cmd_container_pop',0,0),
 ('lens_ctrl_set_focus_scan_param',0,0),('lens_ctrl_set_focus_abs_param',0,0),
 ('lens_ctrl_set_focus_cmd',0,0),('cdaf_get_scan_speed_time',0,0),
 ('alg_set_distance_to_peak',0,0),('cdaf_get_scan_speed_type',0,0),('reset_af_alg_assistant_info',0,0)]
dyn=b.elf.get_section_by_name('.dynsym');rel=b.elf.get_section_by_name('.rela.plt')
got={dyn.get_symbol(r['r_info_sym']).name:r['r_offset'] for r in rel.iter_relocations()}
symbols={s.name:s for s in b.symbols}
near_helpers={'DISTANCE':'alg_get_distance_to_peak','RECORDING':'is_stand_recording_af_mode',
 'AFC':'is_stand_afc_mode','PDAF_PARAM':'af_get_pdaf_param','MODE_PARAM':'af_get_pdaf_af_mode_params',
 'FRAME':'af_get_frame_id','CURRENT_TYPE':'cdaf_set_current_speed_type','WAIT_FRAME':'cdaf_set_speed_increase_start_frame_cnt'}
near_header=['/* 自动生成：近焦修正依赖的完整原厂函数校验。 */']
for label,name in near_helpers.items():near_header.append('#define NEAR_%s 0x%xUL'%(label,symbols[name]['st_value']))
near_functions=list(dict.fromkeys(list(near_helpers.values())+['alg_set_distance_to_peak','cdaf_get_scan_speed_type',
 'get_pdaf_distance_to_peak','_exec_pdaf_afs_process','reset_af_alg_assistant_info']))
for i,name in enumerate(near_functions):
 s=symbols[name];near_header.append('static const unsigned char near_bytes_%d[]={%s};'%(i,','.join(str(x) for x in b.read(s['st_value'],s['st_size']))))
near_header.append('static const struct { unsigned long offset;const unsigned char *bytes;unsigned length; } near_checks[]={')
for i,name in enumerate(near_functions):
 s=symbols[name];near_header.append('{0x%x,near_bytes_%d,%d},'%(s['st_value'],i,s['st_size']))
near_header.append('};');(O/'near_hooks.h').write_text('\n'.join(near_header)+'\n')
header=['/* 自动生成，绑定官方 4.2.0。 */',f'#define AF_LITERAL_COUNT {len(literals)}',f'#define AF_HOOK_COUNT {len(hooks)}',
 'static const struct HookSpec { unsigned long offset, slot; unsigned word; int kind, subtype; } specs[AF_HOOK_COUNT]={']
asm=['.text']
for idx,(name,kind,subtype) in enumerate(hooks):
 a=symbols[name]['st_value'];assert name in got
 header.append('{0x%x,0x%x,0x%x,%d,%d},'%(a,got[name],int.from_bytes(b.read(a,4),'little'),kind,subtype))
 # 通用入口探针保留所有标准寄存器实参及隐藏返回指针，不猜测完整 ABI。
 asm+=['.p2align 2',f'.global af_entry_{idx}',f'.hidden af_entry_{idx}',f'af_entry_{idx}:',
 'sub sp,sp,#208','stp x0,x1,[sp,#0]','stp x2,x3,[sp,#16]',
 'stp x4,x5,[sp,#32]','stp x6,x7,[sp,#48]','stp x8,x30,[sp,#64]',
 'stp q0,q1,[sp,#80]','stp q2,q3,[sp,#112]','stp q4,q5,[sp,#144]','stp q6,q7,[sp,#176]',
 f'mov w0,#{idx}','mov x1,sp','bl af_enter','mov x16,x0',
 'ldp q0,q1,[sp,#80]','ldp q2,q3,[sp,#112]','ldp q4,q5,[sp,#144]','ldp q6,q7,[sp,#176]',
 'ldp x0,x1,[sp,#0]','ldp x2,x3,[sp,#16]','ldp x4,x5,[sp,#32]','ldp x6,x7,[sp,#48]',
 'ldp x8,x30,[sp,#64]','add sp,sp,#208','br x16']
header+=['};']
header+=['static const unsigned char original_speed_bytes[] = {'+','.join(str(v) for v in b.read(start,end-start))+'};']
for i in range(len(hooks)):header.append(f'extern void af_entry_{i}(void);')
header+=['static void (*const entries[AF_HOOK_COUNT])(void)={'+','.join(f'af_entry_{i}' for i in range(len(hooks)))+'};']
(O/'hooks.h').write_text('\n'.join(header)+'\n')
(O/'entry.S').write_text('\n'.join(asm)+'\n.section .note.GNU-stack,"",%progbits\n')
rc=system_elf('/lib64/librcam.so','72ebc8deebce4a29047c475e77ab4edbf2860abb1f572fea45260fe17ad0bda5')
rd=rc.elf.get_section_by_name('.dynsym')
rg={rd.get_symbol(r['r_info_sym']).name:r['r_offset'] for r in rc.elf.get_section_by_name('.rela.plt').iter_relocations()}
assert 'memcpy' in rg and 'duss_hal_x2bridge_write' in rg
extra=['#define RCAM_MEMCPY_SLOT 0x%xUL'%rg['memcpy'],'#define RCAM_WRITE_SLOT 0x%xUL'%rg['duss_hal_x2bridge_write']]
for label,addr,size in [('queue_copy',0x10d664,112),('queue_send',0xe28ec,16)]:
 extra.append('static const unsigned char '+label+'_bytes[]={'+','.join(str(v) for v in rc.read(addr,size))+'};')
(O/'send_hooks.h').write_text('\n'.join(extra)+'\n')
# 调用者地址映射来自同一固件；界面不按时间推断来源。
ranges=sorted({(s['st_value'],s['st_value']+s['st_size'],s.name) for s in b.symbols if s['st_info']['type']=='STT_FUNC' and s['st_size'] and any(k in s.name.lower() for k in ['cdaf','pdaf','focus','hybrid_af','_stage','_push_filtered'])})
(O/'StageNames.js').write_text('.pragma library\nvar ranges='+json.dumps(ranges)+';\nfunction lookup(pc){for(var i=0;i<ranges.length;i++){var r=ranges[i];if(pc>r[0]&&pc<=r[1])return r[2]}return "来源未解析"}\n')
zig=R/'.research-cache/x1d-1.25.0/toolchain/zig-windows-x86_64-0.13.0/zig.exe'
env=dict(os.environ,ZIG_GLOBAL_CACHE_DIR=str(O/'zig-global'),ZIG_LOCAL_CACHE_DIR=str(O/'zig-local'))
def run(args):subprocess.run([str(zig),*args],env=env,check=True)
run(['cc','-std=c11','-O2',str(D/'check_core.c'),'-o',str(O/'check_core.exe')])
subprocess.run([str(O/'check_core.exe')],check=True)
run(['cc','-std=c11','-O2',str(D/'check_origin.c'),'-o',str(O/'check_origin.exe')])
subprocess.run([str(O/'check_origin.exe')],check=True)
run(['cc','-std=c11','-O2',str(D/'check_near.c'),'-o',str(O/'check_near.exe')])
subprocess.run([str(O/'check_near.exe')],check=True)
run(['cc','-std=c11','-O2','-I'+str(O),str(D/'check_pipeline.c'),'-o',str(O/'check_pipeline.exe')])
subprocess.run([str(O/'check_pipeline.exe')],check=True)
run(['cc','-std=c11','-O2',str(D/'check_lens_speed_allowlist.c'),'-o',str(O/'check_lens_speed_allowlist.exe')])
subprocess.run([str(O/'check_lens_speed_allowlist.exe')],check=True)
libs=[];exports=set()
for name in ['libc.so','libdl.so']:
 data=system_file('/lib64/'+name);p=O/name;p.write_bytes(data);libs.append(str(p))
 e=ELFFile(io.BytesIO(data));exports.update(s.name for s in e.get_section_by_name('.dynsym').iter_symbols() if s['st_shndx']!='SHN_UNDEF')
target=O/'libx2d_af_probe.so'
run(['cc','-target','aarch64-linux-gnu','-O2','-g0','-fPIC','-shared','-nostdlib','-fno-stack-protector','-mno-outline-atomics',
 '-Wl,--no-undefined','-Wl,-z,noexecstack','-Wl,-soname,libx2d_af_probe.so','-I'+str(O),
 *(['-DX2D_AFC_NEAR_HOLD'] if afc_hold else []),*(['-DX2D_AFC_TYPE1_3'] if afc3 else []),*(['-DX2D_AF_SPEED_16_24'] if tuned else []),str(D/'probe.c'),str(O/'speed_clone.S'),str(O/'entry.S'),*libs,'-o',str(target)])
e=ELFFile(io.BytesIO(target.read_bytes()));imports={s.name for s in e.get_section_by_name('.dynsym').iter_symbols() if s.name and s['st_shndx']=='SHN_UNDEF'}
assert imports<=exports
assert not any((p['p_flags']&3)==3 for p in e.iter_segments())
result=dict(firmware='4.2.0',speedProfile='1.6/2/2.8' if tuned else '3/2/2.8',afcType1Scale=afc_scale if afc3 else 2.0,speedLensAllowlist=['25V','38V','55V','90V','28P','75P','20-35E','35-100E'],nearGuard='near-hold-afc-v1' if afc_hold else 'near-hold-v1',sha256=hashlib.sha256(target.read_bytes()).hexdigest(),hooks=[h[0] for h in hooks],
 literals=literals,imports=sorted(imports),installed=False,deviceValidated=False,cloneMachineValidation=False)
(O/'build.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),'utf-8')
print(json.dumps({k:v for k,v in result.items() if k not in ['literals','imports']},ensure_ascii=False))
