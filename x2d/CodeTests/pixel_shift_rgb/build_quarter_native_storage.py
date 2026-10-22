"""固定4.2.0原厂单拍后台转换器：离线ABI绑定/构建，不连接设备。"""
from pathlib import Path
import hashlib,json,os,subprocess,sys
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[2]
sys.path[:0]=[str(HERE),str(ROOT/'x2d/tools')]
from audit_private_frame_writer import load,SHA
from firmware_image import system_file,FirmwareElf
OUT=ROOT/'x2d/outputs/4.2.0/pixel-shift-rgb/quarter-native-output-r1'
ZIG=ROOT/'.research-cache/x1d-1.25.0/toolchain/zig-windows-x86_64-0.13.0/zig.exe'
EXTRA_FLAGS=[]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 assert Path.cwd().resolve()==ROOT
 OUT.mkdir(exist_ok=True)
 from build_quarter_codec import build
 codec,cfg,vendor,codec_proof=build("arm")
 b,names=load()
 offsets={'B_NEW':0x3bc00,'B_DELETE':0x3b9b0,'B_ARRAY_FREE':0x3b9a0,'B_SHARED_DESTROY':0x70dd0,
  'B_RECEIVE':0xb0510,'B_PREPARE':0x101948,'B_WRITE_CALLER':0xfab1c,'B_EXPOSURE_ID':0x95ec0,
  'B_DUSS_CREATE':0x113790,'B_CPU_SYNC':0x1b5ac8,'B_DEVICE_SYNC':0x1b6538,
  'B_APPEND':0xf4fb0,'B_CONFIG_DELETE':0xc1a40,'B_DEST_FORMAT':0x1599a8,
  'B_IMAGE_ALLOCATE':0x191510,'B_IMAGE_SET_SIZE':0x192850,'B_TRANSFORM':0x15a430,
  'B_FULLSIZE':0xaafa0,'B_CROP':0xaaea8,'B_METADATA':0x117c10}
 expected={'B_RECEIVE':'_ZN14StorageService15addExposureItemERKN5Cache16ExposureItemInfoERK14QSharedPointerI19MessageNotificationE',
  'B_PREPARE':'_ZN6FileIO20prepareWriteElementsERK14QSharedPointerIN2IO9WriteWorkEERx',
  'B_EXPOSURE_ID':'_ZNK14WriteCacheItem10exposureIdEv',
  'B_CPU_SYNC':'_ZN12DussMemShare10syncForCpuEv','B_DEVICE_SYNC':'_ZNK12DussMemShare13syncForDeviceEv',
  'B_FULLSIZE':'_ZN16WriteFileContent12fullsizeInfoERK7QStringN9HblmTypes10E_FileTypeE',
  'B_CROP':'_ZN16WriteFileContent13cropRectangleERK7QStringN9HblmTypes10E_FileTypeE',
  'B_METADATA':'_ZNK8Metadata12fullsizeInfoEN9HblmTypes10E_FileTypeE'}
 for k,name in expected.items():assert names.get(offsets[k])==name,(k,names.get(offsets[k]))
 for key in ('B_PREPARE','B_METADATA'):
  for ins in b.cs.disasm(b.read(offsets[key],16),offsets[key]):
   assert ins.mnemonic in ('sub','str','stp') and '#' in ins.op_str
 assert int.from_bytes(b.read(0x2489e0,8),'little')==offsets['B_FULLSIZE']
 assert int.from_bytes(b.read(0x2489d0,8),'little')==offsets['B_CROP']
 text=['/* 官方 X2D 100C 4.2.0；camera-storage SHA256 '+SHA+' */']
 text += [f'#define {k} 0x{v:x}UL' for k,v in offsets.items()]
 text+=['static const struct {uintptr_t address;uint8_t bytes[16];} qn_bindings[]={']
 records=[]
 for k,v in offsets.items():
  code=b.read(v,16);text+=['{0x%x,{%s}},'%(v,','.join('0x%02x'%i for i in code))]
  records.append(dict(name=k,address=hex(v),symbol=names.get(v),bytes=code.hex()))
 text+=['};']
 for name,key in [('prepare','B_PREPARE'),('metadata','B_METADATA')]:
  text+=['static const uint8_t qn_'+name+'_original_bytes[16]={'+','.join('0x%02x'%i for i in b.read(offsets[key],16))+'};']
 (OUT/'quarter_native_bindings.inc').write_text('\n'.join(text)+'\n',encoding='utf-8')
 env=dict(os.environ,TEMP=str(OUT),TMP=str(OUT),ZIG_LOCAL_CACHE_DIR=str(OUT/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(OUT/'zig-global'))
 flags=['-target','aarch64-linux-gnu','-O2','-fPIC','-fno-stack-protector','-fno-builtin','-mno-outline-atomics','-funwind-tables','-ffunction-sections','-fdata-sections','-fvisibility=hidden','-Wall','-Wextra','-Werror','-Wno-unused-function','-I'+str(OUT),'-I'+str(HERE),'-D__errno_location=__errno']
 flags+=EXTRA_FLAGS+["-I"+str(cfg),"-I"+str(vendor)]
 objects=[]
 for name in ['quarter_native_storage.cpp','quarter_native_output.c','quarter_memory_file.c','quarter_native_entry.S','factory_private_frame_abi.S','quarter_software_jpeg.c']:
  obj=OUT/(Path(name).stem+'.o')
  extra=['-std=c++17','-fexceptions','-fno-rtti'] if name.endswith('.cpp') else []
  subprocess.run([str(ZIG),'cc',*flags,*extra,'-c',str(HERE/name),'-o',str(obj)],env=env,check=True);objects.append(str(obj))
 # qm_raw_write不参与后台模块；由--gc-sections去掉其文件写入依赖。
 libraries=[]
 for name in ['libc.so','libdl.so','libc++.so']:
  p=OUT/name;p.write_bytes(system_file('/lib64/'+name));libraries.append(str(p))
 target=OUT/'libx2d_quarter_native.so'
 subprocess.run([str(ZIG),'ld.lld','-m','aarch64elf','-shared','--gc-sections','--no-undefined','--allow-shlib-undefined','-z','noexecstack','--strip-all','-soname',target.name,*objects,str(codec),*libraries,'-o',str(target)],env=env,check=True)
 check=OUT/'quarter-native-check.o'
 subprocess.run([str(ZIG),'cc',*flags,'-c',str(HERE/'quarter_native_load_check.c'),'-o',str(check)],env=env,check=True)
 subprocess.run([str(ZIG),'ld.lld','-m','aarch64elf','-pie','-e','_start','--dynamic-linker','/system/bin/linker64','--no-undefined','--allow-shlib-undefined','-z','noexecstack','--strip-all',str(check),*libraries,'-o',str(OUT/'quarter-native-check')],env=env,check=True)
 elf=FirmwareElf(target.read_bytes());imports=[s.name for s in elf.elf.get_section_by_name('.dynsym').iter_symbols() if s.name and s['st_shndx']=='SHN_UNDEF']
 forbidden={'system','fork','execve','kill','posix_spawn'};assert not forbidden.intersection(imports)
 # 4.2.0旧Bionic未正确解析ARM64 TLSDESC；不得仅检查导入符号。
 assert not any(seg['p_type']=='PT_TLS' for seg in elf.elf.iter_segments()),'TLS unsupported by target loader'
 for sec in elf.elf.iter_sections():
  if sec['sh_type'] in ('SHT_RELA','SHT_REL'):
   assert all(r['r_info_type'] not in (1028,1029,1030,1031) for r in sec.iter_relocations()),'TLS relocation rejected'

 report=dict(firmware='official X2D 100C 4.2.0',storage_sha256=SHA,bindings=records,
  codec_proof=str(codec_proof.relative_to(ROOT)),codec_proof_sha256=sha(codec_proof),file_sha256=sha(target),imports=imports,installed=False,device_requests=0,
  source_hashes={n:sha(HERE/n) for n in ['quarter_native_storage.cpp','quarter_native_output.c','quarter_memory_file.c','quarter_native_jpeg.h','quarter_native_entry.S','factory_private_frame_abi.S','quarter_decision_server.inc','quarter_decision_wire.h','quarter_software_jpeg.c','quarter_software_jpeg.h','build_quarter_codec.py']},
  native_single_capture=True,trigger_capture=False,own_capture_queue=False,global_task_lock=False,
  raw_stored=[5952,4421],effective=[5828,4371],jpeg_quality=80,native_queue_and_completion_retained=True,
  remaining=['mock worker ownership and metadata checks','native profile startup verification','user manual single-shot test'])
 (OUT/'storage-build.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 print('QUARTER_NATIVE_STORAGE_ARM_BUILT_NO_DEVICE_OPERATION')
if __name__=='__main__':main()

