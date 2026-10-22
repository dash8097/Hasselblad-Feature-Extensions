"""固定4.2.0标量Qt桥接候选。只构建，不运行相机GUI。"""
from pathlib import Path
import hashlib,json,os,subprocess,sys
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[3];SRC=HERE/'arm_bridge'
sys.path.insert(0,str(ROOT/'x2d/tools'))
from firmware_image import system_elf,FirmwareElf
from generate_object_profile import generate
from generate_stock_profile import generate as generate_stock
from generate_key_profile import generate as generate_key
from generate_binding_profile import generate as generate_binding
from generate_factory_proxy import generate as generate_factory_proxy
from generate_album_profile import generate as generate_album
from generate_formatter_profile import generate as generate_formatter
from generate_about_profile import generate as generate_about
from generate_drive_attachment_profile import generate as generate_drive
from generate_screen_profile import generate as generate_screen
from generate_screen_key_profile import generate as generate_screen_key
from generate_screen_live_profile import generate as generate_screen_live
from generate_animation_image_profile import generate as generate_animation
from face_generate_factory import generate as generate_face_factory
from face_popup_profile import generate as generate_face_popup
from generate_focus_profile import generate as generate_focus
from face_generate_live import generate as generate_face_live
sys.path.insert(0,str(HERE.parent.parent/"temporary_af_speed_probe"))
from inspect_menu_resources import resources
GUI_SHA='16391452abdc69de9e0807e065c0f4ab3f1ccb5fc288f6fc4e6f5cb3bdca12e0'
ZIG=ROOT/'.research-cache/x1d-1.25.0/toolchain/zig-windows-x86_64-0.13.0/zig.exe'
DEPS={'libm.so':ROOT/'x2d/outputs/4.2.0/subject-tracking/link-inputs/libm.so','libc.so':ROOT/'x2d/outputs/4.2.0/raw-stack/backend-r1/libc.so','libdl.so':ROOT/'x2d/outputs/4.2.0/pixel-shift-rgb/memory-session-r1/libdl.so'}
EXPORTS={'x2d_ui_check_existing_runtime_v1','x2d_ui_post_once_v1','x2d_nearest_eye_v1','x2d_eye_step_v1','x2d_native_effect_view_create_v1','x2d_native_effect_view_update_v1','x2d_native_effect_view_close_v1','x2d_native_effect_menu_create_v1','x2d_native_effect_menu_tick_v1','x2d_native_effect_menu_close_v1','x2d_native_effect_menu_start_v1','x2d_native_stock_create_v1','x2d_native_ui_register_v1','x2d_native_ui_bootstrap_status_v1','x2d_native_ui_stop_v1'}
def sha(b):return hashlib.sha256(b).hexdigest()
def main():
 production="--production" in sys.argv[1:]
 automatic=production or "--bootstrap" in sys.argv[1:]
 exports={"x2d_native_ui_bootstrap_status_v1"} if production else EXPORTS
 b=system_elf('/bin/camera-gui',GUI_SHA)
 symbols={s.name:s for s in b.symbols}
 names=['_ZNK7QObject6threadEv','_ZN7QThread13currentThreadEv','_ZN15QGuiApplication4execEv']
 entries=[]
 for name in names:
  symbol=symbols[name];address=symbol['st_value'];entries.append('{0x%xUL,{%s}}'%(address,','.join('0x%02x'%v for v in b.read(address,16))))
 profile=('static const UiProfile gui_profile={'+','.join(entries)+',0x%xUL};\n'%symbols['_ZN16QCoreApplication4selfE']['st_value']+'static const unsigned char gui_sha256[32]={'+','.join('0x'+GUI_SHA[i:i+2] for i in range(0,64,2))+'};\n#define GUI_FILE_SIZE %dL\n'%len(b.data)).encode()
 single=symbols['_ZN6QTimer14singleShotImplEiN2Qt9TimerTypeEPK7QObjectPN9QtPrivate15QSlotObjectBaseE'];address=single['st_value']
 profile+=('static const UiCode gui_single_shot={0x%xUL,{%s}};\n'%(address,','.join('0x%02x'%v for v in b.read(address,16)))).encode()
 pre=symbols['_Z14qAddPreRoutinePFvvE'];address=pre['st_value']
 profile+=('static const UiCode gui_pre_routine={0x%xUL,{%s}};\n'%(address,','.join('0x%02x'%v for v in b.read(address,16)))).encode()
 files={n:(SRC/n).read_bytes() for n in ['bridge_core.h','slot_core.h','native_ui_arm_bridge.c','receipt_sha256.h','selection_scalar.cpp','test_bionic_bridge.c','arm_view_entry.cpp']}
 for n in ['effect_view_core.hpp','effect_view_core.cpp','abi_view_api.hpp','abi_view_api.cpp','local_heap.hpp','menu_discovery.hpp','menu_discovery.cpp','effect_menu_attachment.hpp','effect_menu_attachment.cpp','effect_service.h','effect_service.c','signal_bridge.hpp','signal_bridge.cpp','tree_observer.hpp','tree_observer.cpp','stock_factory.hpp','stock_factory.cpp','key_filter.hpp','key_filter.cpp','key_mapping.hpp','processing_stock_view.hpp','processing_stock_view.cpp','processing_presentation.hpp','processing_presentation.cpp','processing_host.hpp','exposure_model_provider.hpp','processing_host.cpp','exposure_model_provider.cpp']:files[n]=(HERE/n).read_bytes()
 for n in ['pixel_gui_runtime.hpp','pixel_gui_runtime.cpp','pixel_io_pump.hpp','pixel_io_pump.cpp','pixel_host_state.hpp','pixel_host_state.cpp','pixel_controller.hpp','pixel_controller.cpp','protocol_status.hpp','protocol_status.cpp','protocol_text.hpp','pixel_file_io.h','pixel_file_io.c','factory_proxy.hpp','factory_proxy.cpp','pixel_viewport_core.hpp','pixel_viewport_core.cpp','pixel_viewport_view.hpp','pixel_viewport_view.cpp','binding_lease.hpp','binding_lease.cpp','playback_attachment.hpp','playback_attachment.cpp','album_adapter.hpp','album_adapter.cpp','pixel_runtime_service.hpp','pixel_runtime_service.cpp','stock_metrics.hpp','stock_metrics.cpp','multishot_settings.hpp','multishot_settings_view.hpp','drive_popup.hpp','drive_popup_view.hpp']:files[n]=(HERE/n).read_bytes()
 for n in ['passive_output.h','passive_output.c','playback_sampler.hpp','playback_sampler.cpp','playback_diagnostics.hpp','playback_diagnostics.cpp','about_attachment.hpp','about_attachment.cpp','about_row.hpp','about_row.cpp','about_display.hpp','about_display.cpp','factory_formatter.hpp','factory_formatter.cpp']:files[n]=(HERE/n).read_bytes()
 for n in ['drive_attachment.hpp', 'drive_attachment.cpp', 'drive_settings_session.hpp', 'drive_settings_session.cpp', 'drive_proxy_adapter.hpp', 'drive_proxy_adapter.cpp', 'drive_popup.cpp', 'drive_popup_view.cpp', 'multishot_settings.cpp', 'multishot_settings_view.cpp']:files[n]=(HERE/n).read_bytes()
 for n in ['liveview_preferences.hpp','screen_attachment.hpp', 'screen_attachment.cpp', 'screen_mode_attachment.hpp', 'screen_mode_attachment.cpp', 'screen_mode_session.hpp', 'screen_mode_session.cpp', 'screen_notice_view.hpp', 'screen_notice_view.cpp', 'screen_factory_profile.hpp', 'screen_factory_profile.cpp', 'screen_key_profile.hpp', 'screen_key_profile.cpp', 'screen_controls.hpp', 'screen_controls.cpp', 'screen_controls_view.hpp', 'screen_controls_view.cpp', 'screen_policy.hpp', 'screen_policy.cpp']:files[n]=(HERE/n).read_bytes()
 for n in ['animation_attachment.hpp', 'animation_attachment.cpp', 'native_animation_view.hpp', 'native_animation_view.cpp', 'shutter_animation_core.hpp', 'shutter_animation_core.cpp', 'sound_service.h', 'sound_service.c']:files[n]=(HERE/n).read_bytes()
 for n in ['factory_translation.hpp','factory_translation.cpp','native_startup_guard.h','native_startup_guard.c','native_feature_contract.h','ui_receipt.h', 'ui_receipt.c', 'ui_receipt_worker.c', 'loaded_module_identity.h', 'loaded_module_identity.c']:files[n]=(HERE/n).read_bytes()
 for n in ['face_popup_attachment.hpp','face_popup_attachment.cpp','face_factory.hpp','face_factory.cpp','face_popup_view.hpp','face_popup_view.cpp','face_model.hpp','face_model.cpp']:files[n]=(HERE/n).read_bytes()
 for n in ['dashboard_icons.hpp','focus_attachment.hpp','focus_attachment.cpp','focus_factory.hpp','focus_factory.cpp','focus_popup_view.hpp','focus_popup_view.cpp','focus_model.hpp','focus_model.cpp']:files[n]=(HERE/n).read_bytes()
 for n in ['face_live_attachment.hpp', 'face_live_attachment.cpp', 'face_live_profile.hpp', 'face_live_profile.cpp', 'face_session.hpp', 'face_session.cpp', 'face_draw_pool.hpp', 'face_draw_pool.cpp', 'face_view.hpp', 'face_view.cpp', 'face_io.h', 'face_io.c','face_live_route.hpp','face_live_route.cpp']:files[n]=(HERE/n).read_bytes()
 for n in ['esflash_client.h','esflash_client.c','esflash_protocol.h','esflash_timing.hpp']:files[n]=(HERE/n).read_bytes()
 files['object_profile.hpp'],object_evidence=generate(b)
 files['stock_profile.hpp'],stock_evidence=generate_stock(b,dict(resources(b)))
 files['key_profile.hpp'],key_evidence=generate_key(b)
 files['binding_profile.hpp'],binding_evidence=generate_binding(b)
 files['factory_proxy_profile.hpp'],factory_evidence=generate_factory_proxy(b)
 files['album_profile.hpp'],album_evidence=generate_album(b)
 files['formatter_profile.hpp'],formatter_evidence=generate_formatter(b)
 files['about_profile.hpp'],about_evidence=generate_about(b,dict(resources(b)))
 files['drive_profile.hpp'],drive_evidence=generate_drive(b)
 files['screen_profile.hpp'],screen_evidence=generate_screen(b)
 files['screen_keys.hpp'],screen_keys_evidence=generate_screen_key(b)
 files['screen_live.hpp'],screen_live_evidence=generate_screen_live(b)
 files['animation_profile.hpp'],animation_evidence=generate_animation(b,dict(resources(b)))
 files['face_factory_profile.hpp'],face_factory_evidence=generate_face_factory(b)
 files['face_popup_profile.hpp'],face_popup_evidence=generate_face_popup(b)
 files['focus_profile.hpp'],focus_evidence=generate_focus(b,dict(resources(b)))
 files['face_live_generated.hpp'],face_live_evidence=generate_face_live(b)
 files['face_live_generated.hpp']+=('static const struct{uintptr_t address;size_t size;} face_live_data[]={'+','.join('{0x%xUL,56}'%a for a in face_live_evidence['data'].values())+'};\n').encode()
 style_codes=[]
 for name in ['_ZN15QGuiApplication10styleHintsEv','_ZNK11QStyleHints17startDragDistanceEv']:
  address=symbols[name]['st_value'];style_codes.append('{0x%xUL,{%s}}'%(address,','.join('0x%02x'%v for v in b.read(address,16))))
 files['face_style_profile.hpp']=('static const UiCode face_style_functions[]={'+','.join(style_codes)+'};\n').encode()
 address=symbols['_ZN16QCoreApplication9translateEPKcS1_S1_i']['st_value']
 files['translation_profile.hpp']=('static const UiCode translation_function={0x%xUL,{%s}};\n'%(address,','.join('0x%02x'%v for v in b.read(address,16)))).encode()
 if automatic:files['native_ui_bootstrap.c']=(SRC/'native_ui_bootstrap.c').read_bytes()
 files['exports.map']=('{global:\n'+''.join(n+';\n' for n in sorted(exports))+'local:*;};\n').encode()
 files['gui_profile.h']=profile;files['selection_policies.hpp']=(HERE/'selection_policies.hpp').read_bytes()
 identity={'production_exports':production,'automatic_pre_routine_bootstrap':automatic,'sources':{n:sha(v) for n,v in files.items()},'dependencies':{n:sha(p.read_bytes()) for n,p in DEPS.items()},'builder':sha(Path(__file__).read_bytes()),'profile_generator':sha((HERE/'generate_object_profile.py').read_bytes()),'key_generator':sha((HERE/'generate_key_profile.py').read_bytes()),'binding_generator':sha((HERE/'generate_binding_profile.py').read_bytes()),'factory_proxy_generator':sha((HERE/'generate_factory_proxy.py').read_bytes()),'album_generator':sha((HERE/'generate_album_profile.py').read_bytes()),'formatter_generator':sha((HERE/'generate_formatter_profile.py').read_bytes()),'animation_generator':sha((HERE/'generate_animation_image_profile.py').read_bytes()),'screen_generators':{n:sha((HERE/n).read_bytes()) for n in ['generate_screen_profile.py','generate_screen_key_profile.py','generate_screen_live_profile.py']},'drive_generator':sha((HERE/'generate_drive_attachment_profile.py').read_bytes()),'about_generator':sha((HERE/'generate_about_profile.py').read_bytes()),'focus_generator':sha((HERE/'generate_focus_profile.py').read_bytes()),'face_generators':{n:sha((HERE/n).read_bytes()) for n in ['face_generate_factory.py','face_popup_profile.py','face_generate_live.py']},'stock_generator':sha((HERE/'generate_stock_profile.py').read_bytes()),'resource_reader':sha((HERE.parent.parent/'temporary_af_speed_probe/inspect_menu_resources.py').read_bytes()),'zig':sha(ZIG.read_bytes()),'gui':GUI_SHA}
 revision=sha(json.dumps(identity,sort_keys=True).encode())[:20]
 out=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/native-ui/arm-bridge'/revision;source=out/'private-build/source';source.mkdir(parents=True,exist_ok=True)
 for n,v in files.items():(source/n).write_bytes(v)
 for n,p in DEPS.items():(source/n).write_bytes(p.read_bytes())
 # Preserve relative header include while compiling a frozen source set.
 (source.parent/'selection_policies.hpp').write_bytes(files['selection_policies.hpp'])
 env=dict(os.environ,TEMP=str(source.parent),TMP=str(source.parent),ZIG_LOCAL_CACHE_DIR=str(source.parent/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(source.parent/'zig-global'))
 commands=[]
 def run(args):
  commands.append([str(a) for a in args]);p=subprocess.run(commands[-1],env=env,capture_output=True,text=True,timeout=120);assert not p.returncode,p.stdout+p.stderr
 objects=[]
 for n in (['native_ui_bootstrap.c'] if automatic else [])+['esflash_client.c','native_ui_arm_bridge.c','selection_scalar.cpp','effect_view_core.cpp','abi_view_api.cpp','menu_discovery.cpp','effect_menu_attachment.cpp','effect_service.c','signal_bridge.cpp','tree_observer.cpp','stock_factory.cpp','key_filter.cpp','processing_stock_view.cpp','processing_presentation.cpp','processing_host.cpp','exposure_model_provider.cpp','pixel_gui_runtime.cpp','pixel_io_pump.cpp','pixel_host_state.cpp','pixel_controller.cpp','protocol_status.cpp','pixel_file_io.c','factory_proxy.cpp','pixel_viewport_core.cpp','pixel_viewport_view.cpp','binding_lease.cpp','playback_attachment.cpp','album_adapter.cpp','pixel_runtime_service.cpp','stock_metrics.cpp','passive_output.c','playback_sampler.cpp','playback_diagnostics.cpp','about_attachment.cpp','about_row.cpp','about_display.cpp','factory_formatter.cpp','drive_attachment.cpp','drive_settings_session.cpp','drive_proxy_adapter.cpp','drive_popup.cpp','drive_popup_view.cpp','multishot_settings.cpp','multishot_settings_view.cpp','screen_attachment.cpp','screen_mode_attachment.cpp','screen_mode_session.cpp','screen_notice_view.cpp','screen_factory_profile.cpp','screen_key_profile.cpp','screen_controls.cpp','screen_controls_view.cpp','screen_policy.cpp','animation_attachment.cpp','native_animation_view.cpp','shutter_animation_core.cpp','sound_service.c','ui_receipt.c','ui_receipt_worker.c','loaded_module_identity.c','native_startup_guard.c','factory_translation.cpp','face_popup_attachment.cpp','face_factory.cpp','face_popup_view.cpp','face_model.cpp','focus_attachment.cpp','focus_factory.cpp','focus_popup_view.cpp','focus_model.cpp','face_live_attachment.cpp','face_live_profile.cpp','face_session.cpp','face_draw_pool.cpp','face_view.cpp','face_io.c','face_live_route.cpp','arm_view_entry.cpp']:
  if production and n=='selection_scalar.cpp':continue
  obj=source.parent/(n+'.o');objects.append(obj);cpp=n.endswith('.cpp')
  run([ZIG,'c++' if cpp else 'cc','-target','aarch64-linux-gnu','-O2','-Wall','-Wextra','-Werror','-Wno-unused-function','-fPIC','-fvisibility=hidden','-fno-builtin','-fno-stack-protector','-mno-outline-atomics','-D__errno_location=__errno','-DX2D_UI_VERIFY_LOADED=1',*(['-DX2D_UI_PRODUCTION=1','-ffunction-sections','-fdata-sections'] if production else []),*(['-std=c++17','-fno-exceptions','-fno-rtti'] if cpp else []),'-I',source,'-c',source/n,'-o',obj])
 binary=out/('libx2d_native_menu.so' if production else 'libx2d_ui_abi_probe.so');run([ZIG,'ld.lld','-m','aarch64elf','-shared','--no-undefined','--allow-shlib-undefined','--version-script='+str(source/'exports.map'),'-z','noexecstack','--strip-all','-soname',binary.name,*(['--gc-sections'] if production else []),*objects,*[source/n for n in DEPS],'-o',binary])
 elf=FirmwareElf(binary.read_bytes()).elf;dynamic=elf.get_section_by_name('.dynsym')
 imports={s.name for s in dynamic.iter_symbols() if s.name and s['st_shndx']=='SHN_UNDEF'};actual_exports={s.name for s in dynamic.iter_symbols() if s.name and s['st_shndx']!='SHN_UNDEF'}
 supported=set()
 for n in DEPS:supported.update(s.name for s in FirmwareElf((source/n).read_bytes()).elf.get_section_by_name('.dynsym').iter_symbols() if s['st_shndx']!='SHN_UNDEF')
 assert imports<=supported,imports-supported
 assert actual_exports==exports,(actual_exports,exports)
 assert any(t.entry.d_tag in ('DT_INIT','DT_INIT_ARRAY') for t in elf.get_section_by_name('.dynamic').iter_tags())==automatic
 probe=None
 if not production:
  probe_obj=source.parent/'test_bionic_bridge.o'
  run([ZIG,'cc','-target','aarch64-linux-gnu','-O2','-Wall','-Wextra','-Werror','-fPIE','-fno-stack-protector','-fno-builtin','-mno-outline-atomics','-I',source,'-c',source/'test_bionic_bridge.c','-o',probe_obj])
  probe=out/'ui-bionic-bridge-test'
  run([ZIG,'ld.lld','-m','aarch64elf','-pie','--dynamic-linker','/system/bin/linker64','-e','_start','--no-undefined','--allow-shlib-undefined','-z','noexecstack',probe_obj,binary,*[source/n for n in DEPS],'-o',probe])
 report={'test_artifact':({'path':str(probe.relative_to(ROOT)),'sha256':sha(probe.read_bytes()),'bytes':probe.stat().st_size} if probe else None),'production_exports':production,'migration_acceptance':'SEPARATE_CONTRACT_REQUIRED','identity':identity,'artifact':{'path':str(binary.relative_to(ROOT)),'sha256':sha(binary.read_bytes()),'bytes':binary.stat().st_size},'imports':sorted(imports),'exports':sorted(exports),'commands':commands,'hardware_tested':False,'ui_attached':False,'constructors':automatic,'automatic_hooks':automatic,'explicit_post_api':'requires exact GUI identity and current GUI thread; caller/module resident until context release; zero means submitted only'}
 (out/'focus-evidence.json').write_text(json.dumps(focus_evidence,indent=2)+'\n',encoding='utf8')
 (out/'face-evidence.json').write_text(json.dumps([face_factory_evidence,face_popup_evidence,face_live_evidence],indent=2)+'\n',encoding='utf8')
 (out/'animation-evidence.json').write_text(json.dumps(animation_evidence,indent=2)+'\n',encoding='utf8')
 (out/'screen-evidence.json').write_text(json.dumps([screen_evidence,screen_keys_evidence,screen_live_evidence],indent=2)+'\n',encoding='utf8')
 (out/'drive-evidence.json').write_text(json.dumps(drive_evidence,indent=2)+'\n',encoding='utf8')
 (out/'formatter-evidence.json').write_text(json.dumps(formatter_evidence,indent=2)+'\n',encoding='utf-8')
 (out/'about-evidence.json').write_text(json.dumps(about_evidence,indent=2)+'\n',encoding='utf-8')
 (out/'album-abi-evidence.json').write_text(json.dumps(album_evidence,indent=2)+'\n',encoding='utf-8')
 (out/'factory-proxy-evidence.json').write_text(json.dumps(factory_evidence,indent=2)+'\n',encoding='utf-8')
 (out/'binding-abi-evidence.json').write_text(json.dumps(binding_evidence,indent=2)+'\n',encoding='utf-8')
 (out/'key-abi-evidence.json').write_text(json.dumps(key_evidence,indent=2)+'\n',encoding='utf-8')
 (out/'stock-abi-evidence.json').write_text(json.dumps(stock_evidence,indent=2)+'\n',encoding='utf-8')
 (out/'object-abi-evidence.json').write_text(json.dumps(object_evidence,indent=2)+'\n',encoding='utf-8')
 changed=[n for n,v in files.items() if ((SRC/n).is_file() and (SRC/n).read_bytes()!=v) or (not (SRC/n).is_file() and (HERE/n).is_file() and (HERE/n).read_bytes()!=v)]
 report['source_matches_live_at_finish']=not changed;report['changed_source_during_build']=changed
 (out/'build-proof.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps({'directory':str(out),'artifact':report['artifact']}))
if __name__=='__main__':main()
