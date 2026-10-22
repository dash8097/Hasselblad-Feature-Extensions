"""封签已完成离线审查的原生 UI 候选；不操作设备、不证明实机通过。"""
from pathlib import Path
import argparse,hashlib,json,sys
ROOT=Path(__file__).resolve().parents[4]
BASE=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7'
HERE=Path(__file__).resolve().parent
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 parser=argparse.ArgumentParser();parser.add_argument('proof',type=Path);parser.add_argument('--feature-mask',type=int,choices=(248,255),default=255);args=parser.parse_args()
 proof=args.proof.resolve();d=json.loads(proof.read_text(encoding='utf8'));assert proof.is_relative_to(BASE/'native-ui/arm-bridge')
 assert d['production_exports'] and d['source_matches_live_at_finish'] and d['migration_acceptance']=='SEPARATE_CONTRACT_REQUIRED'
 assert d['exports']==['x2d_native_ui_bootstrap_status_v1'] and d['automatic_hooks']
 artifact=ROOT/d['artifact']['path'];assert sha(artifact)==d['artifact']['sha256']
 for name,value in d['identity']['sources'].items():
  p=HERE/'arm_bridge'/name
  if not p.exists():p=HERE/name
  if p.exists():assert sha(p)==value, name
 evidence=['checker-routes/focus-independent-fixed-b3ac1330b1d5/report.json','checker-routes/focus-view-independent-v3-96229eab/report.json','checker-ui/face-popup-routes/a45d34e24f76134c4dd5/report.json','checker-ui/face-live-attachment/818cd0a35cab4a915150/report.json','checker-ui/screen-translation/af7d440532524c07af36/report.json','checker-ui/screen-translation/52a457177606b5995889/report.json']
 evidence += ['native-ui/screen-mode-attachment/796e85f0d7eee9c00e43/report.json','native-ui/screen-attachment/c7d8996a9c11c324d7b8/report.json','native-ui/translation/3ffff435ec03deb6ae4e/report.json','native-ui/pixel-file-io/f6227510db63de313fce/report.json','native-ui/liveview-preferences/2715be43df6367365438/report.json']
 for n in evidence:assert (BASE/n).is_file(),n
 groups=[([10,11],['face_model.cpp','face_popup_view.cpp','face_popup_attachment.cpp']),([12,13,14,15,16],['face_model.cpp','face_session.cpp','face_live_attachment.cpp','face_live_profile.cpp','face_draw_pool.cpp','face_view.cpp','face_live_route.cpp','face_io.c']),([19],['arm_view_entry.cpp','native_ui_arm_bridge.c','native_ui_bootstrap.c']),([21,22],['pixel_controller.cpp','pixel_gui_runtime.cpp','pixel_host_state.cpp','pixel_runtime_service.cpp','pixel_io_pump.cpp','pixel_file_io.c']),([23],['processing_presentation.cpp','processing_stock_view.cpp','processing_host.cpp','exposure_model_provider.cpp']),([24],['about_display.cpp','about_row.cpp','about_attachment.cpp','factory_formatter.cpp']),([25,26,27,28],['drive_popup.cpp','drive_popup_view.cpp','drive_attachment.cpp','drive_settings_session.cpp','drive_proxy_adapter.cpp','multishot_settings.cpp','multishot_settings_view.cpp']),([29,30],['stock_factory.cpp']),([48],['playback_attachment.cpp','pixel_viewport_core.cpp','pixel_viewport_view.cpp']),([49],['playback_sampler.cpp','playback_diagnostics.cpp','passive_output.c']),([65],['quarter_key_policy.hpp']),([69,70],['shutter_animation_core.cpp','native_animation_view.cpp','animation_attachment.cpp','effect_service.c','sound_service.c']),([80,81,82,83,84,85,86,87],['liveview_preferences.hpp','screen_policy.hpp','screen_policy.cpp','screen_controls.cpp','screen_controls_view.cpp','screen_attachment.cpp','screen_mode_attachment.cpp','screen_mode_session.cpp','screen_notice_view.cpp','factory_translation.cpp']),([88,89,90],['effect_service.c','effect_view_core.cpp','effect_menu_attachment.cpp']),([97],['native_feature_contract.h'])]
 mapping={i:files for ids,files in groups for i in ids}
 inventory=json.loads((BASE/'native-ui/h3-ui-inventory.json').read_text(encoding='utf8'));assert len(inventory)==35 and len(mapping)==35
 rows=[]
 for row in inventory:
  i=int(row['file'].split('-')[0][1:]);assert i in mapping
  files=mapping[i]
  for name in files:assert (HERE/name).is_file() or (HERE/'arm_bridge'/name).is_file(),name
  rows.append(dict(file=row['file'],legacy_sha256=row['sha256'],legacy_target=row['target'],replacement=files,status='unreachable_legacy_removed' if i==65 else 'native_integrated',notes=('原 H3 路由不可达，不新增历史快门拦截行为；保留证据 checker-routes/quarter-key-reachability.json' if i==65 else '原厂唯一 TiledImage 借用；保留原图/缩放，使用原厂切图调度；非原 GPU 资源实测' if i==48 else '仅支持基础 248／高级 255；缺失和其他值拒绝，基础不接入扩展拍摄控制' if i==97 else '主动内部 UI 测试入口剥离；原有用户功能和被动诊断保留' if i in (22,24,49) else '原机内既有 qrc 控件通过固定白名单借用，不随包分发原厂资源')))
 backend=BASE/'native-ui/face-backend-v2/01decd36d75ef7ae6a52/libx2d_face_lock_v2.so';br=json.loads((backend.parent/'report.json').read_text(encoding='utf8'));assert sha(backend)==br['artifact_sha256']
 out=proof.parent/('migration-acceptance' if args.feature_mask==255 else 'migration-acceptance-basic');out.mkdir(exist_ok=False)
 contract=dict(schema='X2D_NATIVE_UI_CONTRACT_V1',module_path='/system/lib64/libx2d_native_menu.so',module_sha256=sha(artifact),gui_sha256=d['identity']['gui'],required_capabilities=4095 if args.feature_mask==255 else 4081,feature_mask=args.feature_mask,artifact=artifact.relative_to(ROOT).as_posix(),migration_complete=True,eye_protocol=2,replacement_artifacts={'/system/lib64/libx2d_face_lock.so':dict(artifact=backend.relative_to(ROOT).as_posix(),sha256=sha(backend))})
 (out/'native-ui-contract.json').write_text(json.dumps(contract,ensure_ascii=False,indent=2),encoding='utf8')
 (out/'migration-review.json').write_text(json.dumps(dict(schema='X2D_NATIVE_UI_MIGRATION_REVIEW_V1',contract_sha256=sha(out/'native-ui-contract.json'),build_proof_sha256=sha(proof),files=rows,evidence_sha256={n:sha(BASE/n) for n in evidence},scope='源码和打包资源替换已接入；真实 Qt 与隔离原生测试，不代表原厂 GUI/相机硬件验收',hardware_tested=False,camera_operations=0,requires_final_package_hardware_ui_uninstall_acceptance=True,bootstrap_status={0:'unregistered',1:'registered',2:'queued',3:'active',4:'failed',5:'stopped',6:'disabled marker present',7:'invalid feature config',8:'disable marker unverifiable'}),ensure_ascii=False,indent=2),encoding='utf8')
 print(out/'native-ui-contract.json')
if __name__=='__main__':main()
