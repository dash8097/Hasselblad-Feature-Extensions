"""25MP上游直编离线候选：官方4.2.0绑定，不连接设备。"""
from pathlib import Path
import hashlib,json,os,subprocess,sys
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[2]
sys.path[:0]=[str(HERE),str(ROOT/'x2d/tools')]
from firmware_image import system_elf,system_file,FirmwareElf
OUT=ROOT/'x2d/outputs/4.2.0/pixel-shift-rgb/quarter-upstream-r2'
PRELOAD=False
ZIG=ROOT/'.research-cache/x1d-1.25.0/toolchain/zig-windows-x86_64-0.13.0/zig.exe'
SHA_SERVICE='fbcf828f73bca13f0c8b95e7dd0b95ac483ae36954ec06179098c8a1a65f9f82'
SHA_PLUGIN='8c03657d6175b6d13e382341816f2c039d04dec8162c4a52026946fc44c9eab2'
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def environment():return dict(os.environ,TEMP=str(OUT),TMP=str(OUT),ZIG_LOCAL_CACHE_DIR=str(OUT/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(OUT/'zig-global'))
def main():
 assert Path.cwd().resolve()==ROOT
 OUT.mkdir(exist_ok=True)
 service=system_elf('/bin/camera-service',SHA_SERVICE)
 plugin=system_elf('/lib64/camera/plugins/ienc/libdcam_ienc_jpeg.so',SHA_PLUGIN)
 expected={0x2150e8:'_ZN24DCAMCaptureEnginePrivate15onCapturedImageEP21capeng_captured_image',
  0x3bfef8:'_ZN11QBasicMutex12lockInternalEv',0x3c00b8:'_ZN11QBasicMutex14unlockInternalEv'}
 for a,s in expected.items():assert service.names[a]==s
 assert [i.mnemonic for i in service.cs.disasm(service.read(0x2150e8,16),0x2150e8)]==['sub','stp','stp','stp']
 rel=plugin.elf.get_section_by_name('.rela.plt');sym=plugin.elf.get_section(rel['sh_link'])
 got=[r['r_offset'] for r in rel.iter_relocations() if sym.get_symbol(r['r_info_sym']).name=='duss_hal_ienc_encfrm']
 assert got==[0x1ffd0]
 # 原厂每帧回调在0x215d50调用CaptureRequest+0x40的functor；全帧完成检查在它之后。
 checks={0x215d38:('ldr','x0, [x19, #0x70]'),0x215d50:('blr','x8'),
  0x215d70:('ldr','w10, [x9, #0x28]'),0x161ecc:('ldr','w1, [x20, #0x24]')}
 for a,expected_instruction in checks.items():
  i=next(service.cs.disasm(service.read(a,4),a));assert (i.mnemonic,i.op_str)==expected_instruction
 # imgMemory真实跳表：1是JPG，2才是RAW。不能由测试自定义枚举。
 table=service.read(0x5f30b3,48)
 assert 0x2197b0+4*table[0]==0x2199d0 and 0x2197b0+4*table[1]==0x219a88
 assert b'fromStreamBuffer:JPG' in service.read(0x5c1c17,64)
 assert b'fromFramebuffer:RAW' in service.read(0x5c1c7b,64)
 vtables=[s['st_value'] for s in service.symbols if s.name.startswith('_ZTV') and 'onSnapshotMetadataCollected' in s.name and 'CaptureFrameResult' in s.name]
 assert vtables==[0x7a6fc8]
 offsets={'QDU_RAW_KIND':2,'QDU_CAPTURE':0x2150e8,'QDU_FUNCTOR':0x7a6fd8,'QDU_LOCK':0x3bfef8,'QDU_UNLOCK':0x3c00b8,'QDU_PLUGIN_ENCODE':0x1778,'QDU_HAL_GOT':got[0]}
 lines=['/* official X2D 100C 4.2.0, generated verified bindings */']+[f'#define {k} 0x{v:x}UL' for k,v in offsets.items()]
 for name,b,a in [('capture',service,0x2150e8),('lock',service,0x3bfef8),('unlock',service,0x3c00b8),('plugin',plugin,0x1778)]:
  lines+=['static const uint8_t qdu_'+name+'_bytes[16]={'+','.join(str(x) for x in b.read(a,16))+'};']
 (OUT/'quarter_upstream_bindings.inc').write_text('\n'.join(lines)+'\n',encoding='utf-8')
 # 复用项目已验证的cache维护实现，不链接存储入口。
 asm=(HERE/'quarter_native_entry.S').read_text(encoding='utf-8').split('.global __clear_cache',1)[1]
 (OUT/'quarter_upstream_cache.S').write_text('.text\n.global __clear_cache'+asm,encoding='utf-8')
 flags=['-target','aarch64-linux-gnu','-O2','-fPIC','-fno-stack-protector','-fno-builtin','-mno-outline-atomics',
  '-funwind-tables','-ffunction-sections','-fdata-sections','-fvisibility=hidden','-Wall','-Wextra','-Werror','-Wno-unused-function',
  '-I'+str(OUT),'-I'+str(HERE),'-D__errno_location=__errno']
 if PRELOAD:flags+=['-DQDU_PRELOAD=1']
 sources=[HERE/n for n in ['quarter_upstream_native.c','quarter_upstream.c','quarter_direct_jpeg.c','quarter_single_core.c']]+[OUT/'quarter_upstream_cache.S']
 objects=[];env=environment()
 for source in sources:
  obj=OUT/(source.stem+'.o');objects.append(str(obj))
  subprocess.run([str(ZIG),'cc',*flags,'-c',str(source),'-o',str(obj)],check=True,env=env)
 libs=[]
 for name in ['libc.so','libdl.so']:
  p=OUT/name;p.write_bytes(system_file('/lib64/'+name));libs.append(str(p))
 target=OUT/'libx2d_quarter_upstream.so'
 subprocess.run([str(ZIG),'ld.lld','-m','aarch64elf','-shared','--gc-sections','--no-undefined','--allow-shlib-undefined','-z','noexecstack','-soname',target.name,*objects,*libs,'-o',str(target)],check=True,env=env)
 elf=FirmwareElf(target.read_bytes());imports=[s.name for s in elf.elf.get_section_by_name('.dynsym').iter_symbols() if s.name and s['st_shndx']=='SHN_UNDEF']
 assert not {'system','fork','execve','kill','posix_spawn','duss_hal_device_start','duss_hal_device_stop'}.intersection(imports)
 assert (elf.elf.get_section_by_name('.init_array') is not None)==PRELOAD
 if PRELOAD:
  sys.path.insert(0,str(ROOT/'x2d/CodeTests/installer_hardening/camera_native'))
  from elf_metadata_hardening import normalize_elf
  packaged,metadata=normalize_elf(target.read_bytes())
  (OUT/'libx2d_quarter_upstream.package.so').write_bytes(packaged)
  (OUT/'metadata-validation.json').write_text(json.dumps(metadata,indent=2)+'\n',encoding='utf8')
 import build_quarter_native_storage as storage
 storage.OUT=OUT;storage.EXTRA_FLAGS=['-DQN_DIRECT_JPEG_BRIDGE=1'];storage.main()
 names=[p.name for p in sources if p.parent==HERE]+['quarter_upstream.h','quarter_direct_jpeg.h','quarter_decision_server.inc','quarter_decision_wire.h','capture_ion_reference.h','build_quarter_upstream.py']
 report=dict(offline=True,installed=False,device_requests=0,firmware='official X2D 100C 4.2.0',
  service_sha256=SHA_SERVICE,plugin_sha256=SHA_PLUGIN,offsets=offsets,module_sha256=digest(target),imports=imports,
  source_hashes={n:digest(HERE/n) for n in names},quality=80,
  explicit_init_only=not PRELOAD,requires_quiescent_pre_capture_loader=not PRELOAD,
  cold_start_preload=PRELOAD,encoder_thread_code_writes=False if PRELOAD else None,
  raw_callback_maps_native_frame_to_exposure=True,storage_snapshot_required=True,
  missing_mapping_or_snapshot_falls_back=True,hal_error_quarantines_one_input=True,
  direct_encoded=[5828,4372],stored_visible=[5828,4371],hardware_not_tested=True)
 (OUT/'upstream-build.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 print('QUARTER_UPSTREAM_AND_STORAGE_BUILT_OFFLINE')
if __name__=='__main__':
 if '--preload' in sys.argv:
  PRELOAD=True;OUT=ROOT/'x2d/outputs/4.2.0/pixel-shift-rgb/quarter-upstream-preload'
 main()
