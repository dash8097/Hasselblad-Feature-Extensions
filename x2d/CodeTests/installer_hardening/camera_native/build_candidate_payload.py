"""构建内部原生后端接入载荷。界面迁移未完成时明确禁止作为最终发行包。"""
from pathlib import Path
import argparse,gzip,hashlib,io,json,shutil,sys,tarfile
import build_pilot,generate_install_tables as old,generate_runtime_tables as runtime
import inspect_scriptless_payload
import native_ui_init
from native_worker_contract import validated_worker
from af_speed_contract import validated_af_speed
import elf_metadata_hardening
import quarter_direct_contract
sys.path.insert(0,str(Path(__file__).resolve().parent.parent))
import package_profiles

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
R7=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7'
UNINSTALL=ROOT/'x2d/outputs/4.2.0/installer-hardening-r3/internal-candidates/windows-installer/uninstall'
FLOW=R7/'native-flow/revisions/b3420262b08c8be505b9'
WORKER=R7/'native-worker/revisions/b015f0f36d331f111cf0'
STACK=R7/'stack-capture/revisions/a0b96de9ecf7d0c9c8cf/artifacts/libx2d_raw_stack.so'

def sha_bytes(b):return hashlib.sha256(b).hexdigest()
def encoded(v):return (json.dumps(v,ensure_ascii=False,sort_keys=True,indent=2)+'\n').encode()

def verified_legacy_manifest(path):
    path=path.resolve()
    assert path.is_relative_to(ROOT/'x2d/outputs/4.2.0/raw-stack/continuous-candidate/delivery')
    data=json.loads(path.read_text(encoding='utf8'));identity=data['identity']
    assert identity['firmware']=='4.2.0' and identity['checks']
    for name,digest in identity['checks'].items():
        evidence=(ROOT/name).resolve();assert evidence.is_relative_to(ROOT/'x2d/outputs')
        assert build_pilot.sha(evidence)==digest,'旧版验证证据变化：'+name
    seen=set()
    for row in identity['files']:
        assert row['path'] not in seen;seen.add(row['path'])
        source=(ROOT/row['source']).resolve();assert source.is_relative_to(ROOT/'x2d/outputs')
        assert source.stat().st_size==row['bytes'] and build_pilot.sha(source)==row['sha256']
    return identity['files']
def without_private_probe(data):
    block=b'''    property bool syntheticProbeEnabled: false
    Loader {
        parent:root.parent
        active:root.syntheticProbeEnabled
        source:"file:///data/local/tmp/x2d-playback-debug-r4/PlaybackCropProbe.qml"
    }
'''
    assert data.count(block)==1,'私有探针入口基线变化，须重新审计'
    return data.replace(block,b'')
def without_private_route(data):
    start=data.index('    // 仅自有诊断文件请求 UI 路由核验'.encode())
    end=data.index(b'    function verifyComponents()',start)
    removed=data[start:end]
    assert all(marker in removed for marker in (b'CHECK_UI_R6',b'routeStep',b'routeSelector',b'routeFinish',b'selector.itemSelected'))
    data=data[:start]+data[end:]
    for line in (b'    property int parameterCheckRow: 0\n',b'    property var checkModel: null\n',b'    property string checkPrevious: ""\n'):
        assert data.count(line)==1,'UI route test baseline changed'
        data=data.replace(line,b'')
    assert data.count(b' && !root.checkModel')==2
    return data.replace(b' && !root.checkModel',b'')
def without_private_about(data):
    start=data.index('    // 仅安装核验时的一次性请求'.encode())
    assert data[start:].count(b'CHECK_ABOUT')==1 and data.rstrip().endswith(b'}')
    return data[:start]+b'}\n'
def archive(files):
    raw=io.BytesIO()
    with tarfile.open(fileobj=raw,mode='w',format=tarfile.USTAR_FORMAT) as tar:
        for name,data in sorted(files.items()):
            assert '/' not in name and len(name)<100
            item=tarfile.TarInfo(name);item.size=len(data);item.mode=0o644;item.mtime=0
            item.uid=item.gid=0;item.uname=item.gname=''
            tar.addfile(item,io.BytesIO(data))
    return gzip.compress(raw.getvalue(),compresslevel=9,mtime=0)

def main():
    global FLOW
    assert Path.cwd().resolve()==ROOT
    parser=argparse.ArgumentParser();parser.add_argument('--esflash-report',type=Path,required=True);parser.add_argument('--runtime-revision',required=True);parser.add_argument('--feature-mask',type=int,choices=(248,255),default=255);parser.add_argument('--native-ui-contract',type=Path);parser.add_argument('--continuous-manifest',type=Path);parser.add_argument('--legacy-continuous-manifest',type=Path,action='append',default=[]);parser.add_argument('--legacy-payload',type=Path,action='append',default=[]);args=parser.parse_args()
    assert len(args.runtime_revision)==20 and all(c in '0123456789abcdef' for c in args.runtime_revision)
    FLOW=R7/'native-flow/revisions'/args.runtime_revision
    continuous=None;continuous_files={}
    if args.continuous_manifest:
        source=args.continuous_manifest.resolve()
        base=ROOT/'x2d/outputs/4.2.0/raw-stack/continuous-candidate'
        assert source.is_relative_to(base/'delivery')
        continuous=json.loads(source.read_text(encoding='utf8'))
        identity=continuous['identity']
        assert identity['firmware']=='4.2.0'
        assert identity['builder']==build_pilot.sha(ROOT/'x2d/CodeTests/raw_stack_continuous/prepare_candidate.py')
        # 两档并存时不能依赖最后一次构建覆盖的可变指针；清单摘要绑定冻结证明。
        FLOW=base/'runtime/revisions'/args.runtime_revision
        proof=FLOW/'build-proof.json'
        assert build_pilot.sha(proof)==identity['flow_proof_sha256']
        assert identity['checks'],'连续候选必须保留离线验证证据'
        for name,digest in identity['checks'].items():
            p=(ROOT/name).resolve();assert p.is_relative_to(ROOT/'x2d/outputs')
            assert build_pilot.sha(p)==digest,'连续候选验证证据变化：'+name
        for row in identity['files']:
            assert row['path'] not in continuous_files,'连续候选目标重复'
            p=(ROOT/row['source']).resolve();assert p.is_relative_to(ROOT/'x2d/outputs')
            assert build_pilot.sha(p)==row['sha256'] and p.stat().st_size==row['bytes']
            continuous_files[row['path']]=(p,row)
    old.generate();runtime.generate()
    audit=json.loads(old.AUDIT.read_text(encoding='utf-8'))
    fp=json.loads((FLOW/'build-proof.json').read_text(encoding='utf-8'))
    assert fp['build_identity']['profile']=='runtime'
    assert build_pilot.sha(FLOW/'artifacts/x2d-native-flow')==fp['artifact']['sha256']
    assert (HERE/'native_runtime_tables.hpp').read_text(encoding='utf8')==runtime.generate(),'当前叶节点表与生成器不一致'
    assert fp['source_sha256']['native_runtime_tables.hpp']==build_pilot.sha(HERE/'native_runtime_tables.hpp'),'runtime 的叶节点表不是本次载荷摘要'
    assert fp['build_identity']['elf_metadata_hardener']==build_pilot.sha(Path(elf_metadata_hardening.__file__)),'runtime 未绑定同版元数据加固器'
    for runner in ('native_memory_runner.cpp','native_disk_runner.cpp','native_stack_runner.cpp'):
        runner_path=ROOT/'x2d/CodeTests/raw_stack_continuous'/runner if continuous and runner=='native_stack_runner.cpp' else HERE/runner
        assert fp['source_sha256'][runner]==build_pilot.sha(runner_path),'runtime 未绑定当前统一摘要调用：'+runner
    ui=None;ui_artifacts={};ui_digests={}
    if args.native_ui_contract:
        source=args.native_ui_contract.resolve();assert source.is_relative_to((ROOT/'x2d').resolve())
        ui=json.loads(source.read_text(encoding='utf8'))
        assert ui['schema']=='X2D_NATIVE_UI_CONTRACT_V1' and ui['migration_complete'] is True and ui['eye_protocol']==2
        assert ui['module_path']=='/system/lib64/libx2d_native_menu.so'
        assert ui==fp['build_identity'].get('native_ui_contract'),'runtime未绑定同一原生界面'
        entries=dict(ui['replacement_artifacts']);assert set(entries)=={'/system/lib64/libx2d_face_lock.so'}
        entries[ui['module_path']]={'artifact':ui['artifact'],'sha256':ui['module_sha256']}
        for path,entry in entries.items():
            target=(ROOT/entry['artifact']).resolve();assert target.is_relative_to((ROOT/'x2d/outputs').resolve())
            assert build_pilot.sha(target)==entry['sha256'];ui_artifacts[path]=target;ui_digests[path]=entry['sha256']
    else:assert not fp['build_identity'].get('native_ui_contract'),'原生UI runtime不得混入旧脚本UI'
    worker,worker_proof,worker_check=validated_worker(ROOT)
    if continuous:
        assert continuous_files['/system/bin/x2d-pixel-overclock/worker'][1]['sha256']==build_pilot.sha(worker),'连续清单未包含当前调度器'
    assert build_pilot.sha(STACK)=='4d737e5de2373813860a83394aaf55bcd6ad60ea0250faaa77870a77b671b4df'
    replacements={p:ROOT/src for p,(src,digest) in runtime.REPLACEMENTS.items()}
    af_speed,af_proof,af_checks=validated_af_speed(ROOT)
    replacements['/system/lib64/libx2d_af_base0.so']=af_speed
    replacements['/system/bin/x2d-raw-stack/libx2d_raw_stack.so']=STACK
    replacements['/system/bin/x2d-pixel-overclock/worker']=worker
    replacements.update(ui_artifacts)
    if continuous:
        assert ui,'连续候选必须绑定原生 UI 契约'
        for path,(p,row) in continuous_files.items():
            if path=='/system/lib64/libx2d_af_base0.so':assert p.read_bytes()==af_speed.read_bytes(),'完整包与连续候选AF倍率不一致'
            if path in ui_digests:assert row['sha256']==ui_digests[path]
            # init 仍从原始基线执行同一转换，末尾再与候选逐字节核对。
            # 已转换的 init 不能再次送入 native_init。
            if path!='/system/bin/x2d-native-flow' and not path.startswith('/system/etc/init/'):
                replacements[path]=p
    files={};rows=[];removed=[];pending_ui=[];removed_ui=[];elf_reviews={}
    hardener_sha=build_pilot.sha(Path(elf_metadata_hardening.__file__))
    def harden(name,data):
        if not data.startswith(b'\x7fELF'):return data
        result,review=elf_metadata_hardening.normalize_elf(data)
        elf_reviews[name]=review
        return result
    for r in audit['install_targets']:
        r=dict(r);name,path=r['name'],r['path'];data=(old.BASE/name).read_bytes()
        assert sha_bytes(data)==r['candidate_sha256']
        if path.endswith('.sh'):
            removed.append({'name':name,'path':path,'sha256':sha_bytes(data)});continue
        if ui and path.endswith(('.qml','.js')):
            assert r['original']=='ABSENT','不可直接移除原厂UI'
            removed_ui.append({'name':name,'path':path,'sha256':sha_bytes(data)});continue
        if path in replacements:data=replacements[path].read_bytes()
        if path in ui_digests:assert sha_bytes(data)==ui_digests[path],'UI artifact changed while packaging'
        if name=='p49-PlaybackDiagnostics.qml':data=without_private_probe(data)
        if name=='p22-PixelOverclockHost.qml':data=without_private_route(data)
        if name=='p24-X2dAboutDisplay.qml':data=without_private_about(data)
        if path=='/system/etc/init/x2d-shutter-audio.rc':
            before=b'service x2d-shutter-audio /system/bin/sh /system/etc/x2d-shutter-audio.sh'
            assert data.count(before)==1
            data=data.replace(before,b'service x2d-shutter-audio /system/bin/x2d-native-flow audio')
        if ui and path=='/system/etc/init/camera-gui.rc':
            data=native_ui_init.native_init(data)
        if path.endswith(('.qml','.js')):pending_ui.append(path)
        assert not data.startswith(b'#!'),path
        data=harden(name,data)
        if path in ui_digests:assert sha_bytes(data)==ui_digests[path],'UI 契约产物须先独立冻结，不能在组包时改变'
        r['candidate_sha256']=sha_bytes(data);r['bytes']=len(data)
        files[name]=data;rows.append(r)
    name='p122-x2d-native-flow';data=harden(name,(FLOW/'artifacts/x2d-native-flow').read_bytes())
    assert sha_bytes(data)==fp['artifact']['sha256'],'runtime changed while packaging'
    rows.append({'name':name,'path':'/system/bin/x2d-native-flow','original':'ABSENT','candidate_sha256':sha_bytes(data),'mode':'755','bytes':len(data)})
    files[name]=data
    if continuous:
        assert set(continuous_files)=={r['path'] for r in rows},'连续候选与整包目标集合不一致'
        for row in rows:
            expected=continuous_files[row['path']][1]
            assert (row['name'],row['mode'],row['candidate_sha256'])==(expected['name'],expected['mode'],expected['sha256']),'整包未准确纳入连续候选：'+row['path']
    esflash_path=args.esflash_report.resolve()
    assert esflash_path.is_relative_to(R7/'esflash')
    esflash=json.loads(esflash_path.read_text(encoding='utf8'))
    assert esflash['default_enabled'] is False and esflash['factory_dynamic_compensation_modified'] is False
    assert esflash['identity']['builder']==build_pilot.sha(ROOT/'x2d/CodeTests/sensor_exposure_candidates/build_native_package.py')
    assert esflash['identity']['sources']['esflash_protocol.h']==build_pilot.sha(HERE.parent/'native_ui/esflash_protocol.h')
    assert esflash['startup']=='camera-service-child-v1' and esflash['credentials_or_policy_modified'] is False
    flash_code=ROOT/'x2d/CodeTests/sensor_exposure_candidates'
    assert esflash['identity']['startup_contract']==build_pilot.sha(flash_code/'native_startup_contract.py')
    assert esflash['identity']['sources']['native_parent_start.h']==build_pilot.sha(flash_code/'native_parent_start.h')
    flash_checks={}
    pulse_proof=(ROOT/esflash['pulse_guard_report']).resolve()
    assert pulse_proof.is_relative_to(R7/'esflash-pulse') and build_pilot.sha(pulse_proof)==esflash['identity']['pulse_guard_test']
    pulse=json.loads(pulse_proof.read_text(encoding='utf8'))
    assert pulse['passed'] and pulse['baseline_red_reproduced'] and pulse['scheduler_window_red_reproduced'] and esflash['identity']['pulse_guard_source']==build_pilot.sha(flash_code/'native_hotshoe_resident.c')
    assert pulse['identity']['builder']==build_pilot.sha(flash_code/'test_native_pulse.py')
    assert pulse['identity']['sources']=={name:build_pilot.sha(flash_code/name) for name in ('native_hotshoe_resident.c','native_pulse_test_platform.h','test_native_pulse.c')}
    flash_checks[str(pulse_proof.relative_to(ROOT))]=build_pilot.sha(pulse_proof)
    scheduler_proof=(ROOT/esflash['pulse_scheduler_kernel_report']).resolve()
    assert scheduler_proof.is_relative_to(R7/'esflash-pulse-kernel') and build_pilot.sha(scheduler_proof)==esflash['identity']['pulse_scheduler_kernel_test']
    scheduler=json.loads(scheduler_proof.read_text(encoding='utf8'))
    assert scheduler['passed'] and scheduler['real_kernel_scheduler']
    assert scheduler['identity']['builder']==build_pilot.sha(flash_code/'test_native_pulse_kernel.py')
    assert scheduler['identity']['sources']=={name:build_pilot.sha(flash_code/name) for name in ('native_hotshoe_resident.c','native_pulse_test_platform.h','test_native_pulse.c','test_native_pulse_kernel.c')}
    flash_checks[str(scheduler_proof.relative_to(ROOT))]=build_pilot.sha(scheduler_proof)
    wait_proof=(ROOT/esflash['wait_policy_report']).resolve()
    assert wait_proof.is_relative_to(R7/'esflash-wait-policy') and build_pilot.sha(wait_proof)==esflash['identity']['wait_policy_test']
    wait=json.loads(wait_proof.read_text(encoding='utf8'))
    assert wait['passed'] and wait['real_kernel_poll'] and wait['baseline_budget_rejected']
    assert wait['identity']['builder']==build_pilot.sha(flash_code/'test_native_wait.py')
    assert wait['identity']['sources']=={name:build_pilot.sha(flash_code/name) for name in ('native_wait_policy.h','test_native_wait.c')}
    assert esflash['identity']['sources']['native_wait_policy.h']==build_pilot.sha(flash_code/'native_wait_policy.h')
    flash_checks[str(wait_proof.relative_to(ROOT))]=build_pilot.sha(wait_proof)
    for directory in ('esflash-parent-start','esflash-startup-contract'):
        matched=[]
        for proof in (R7/directory).glob('*/report.json'):
            evidence=json.loads(proof.read_text(encoding='utf8'));sources=evidence.get('sources',{})
            if evidence.get('passed') and sources and all(build_pilot.sha(flash_code/name)==digest for name,digest in sources.items()):matched.append(proof)
        assert len(matched)==1,('missing current flash startup verification',directory,matched)
        flash_checks[str(matched[0].relative_to(ROOT))]=build_pilot.sha(matched[0])
    matched=[]
    for proof in (R7/'esflash-check').glob('*/report.json'):
        evidence=json.loads(proof.read_text(encoding='utf8'));identity=evidence.get('identity',{})
        if evidence.get('passed') and identity.get('report')==build_pilot.sha(esflash_path) and identity.get('builder')==build_pilot.sha(flash_code/'test_native_service.py') and all(build_pilot.sha((HERE.parent/'native_ui' if name=='esflash_protocol.h' else flash_code)/name)==digest for name,digest in identity.get('sources',{}).items()):matched.append(proof)
    assert len(matched)==1,('missing exact Bionic flash protocol/negative-parent verification',matched)
    flash_checks[str(matched[0].relative_to(ROOT))]=build_pilot.sha(matched[0])
    sys.path.insert(0,str(flash_code))
    from native_startup_contract import patch_camera_service
    parent_rows=[r for r in rows if r['path']=='/system/etc/init/camera-service.rc']
    assert len(parent_rows)==1
    parent=parent_rows[0];parent_data=patch_camera_service(files[parent['name']])
    files[parent['name']]=parent_data;parent['candidate_sha256']=sha_bytes(parent_data);parent['bytes']=len(parent_data)
    flash_targets={ '/system/bin/x2d-esflash':('p123-x2d-esflash','755'),
        '/system/lib64/libx2d_esflash_hotshoe.so':('p124-libx2d_esflash_hotshoe.so','644'),
        '/system/etc/init/x2d-esflash.rc':('p125-x2d-esflash.rc','644') }
    assert set(esflash['targets'])==set(flash_targets) and ui
    for path,(name,mode) in flash_targets.items():
        entry=esflash['targets'][path];source=(ROOT/entry['source']).resolve()
        assert source.is_relative_to(esflash_path.parent) and entry['mode']==mode
        data=harden(name,source.read_bytes())
        assert sha_bytes(data)==entry['sha256'] and len(data)==entry['bytes']
        assert name not in files and path not in {r['path'] for r in rows}
        files[name]=data
        rows.append(dict(name=name,path=path,original='ABSENT',candidate_sha256=sha_bytes(data),mode=mode,bytes=len(data)))
    # 恢复软件25MP；冻结runner必须绑定实际交付的存储库。
    quarter_rows=[r for r in rows if r['path']=='/system/lib64/libx2d_quarter_native.so']
    assert len(quarter_rows)==1
    quarter_direct_contract.validate_runner_bindings(FLOW/'private-build/source',quarter_rows[0]['candidate_sha256'])
    package_profiles.profile(args.feature_mask)
    if args.feature_mask == package_profiles.BASIC:
        assert ui and ui.get('feature_mask')==248 and ui['required_capabilities']==4081, '基础档必须绑定对应 UI 回执契约'
        rows=package_profiles.selected_targets(rows,args.feature_mask)
        for row in rows:
            path,name=row['path'],row['name']
            data=files[name]
            if path=='/system/etc/init/camera-service.rc':
                data=package_profiles.service_init(data,args.feature_mask)
            elif path=='/system/etc/init/camera-gui.rc':
                assert data.count(b'X2D_NATIVE_UI_FEATURES 255')==1
                data=data.replace(b'X2D_NATIVE_UI_FEATURES 255',b'X2D_NATIVE_UI_FEATURES 248')
            elif path=='/system/etc/x2d-feature-mask':
                assert data in (b'255',b'255\n')
                data=b'248\n'
            files[name]=data;row['candidate_sha256']=sha_bytes(data);row['bytes']=len(data)
        files={r['name']:files[r['name']] for r in rows}
    else:
        assert not ui or ui.get('feature_mask',255)==255
    for name in ('check-raw-stack-arm',):files[name]=harden(name,(old.BASE/name).read_bytes())
    probe=json.loads((R7/'install-probe/latest-build.json').read_text(encoding='utf-8'))
    files['install-probe']=(ROOT/probe['artifact']['path']).read_bytes()
    assert sha_bytes(files['install-probe'])==probe['artifact']['sha256']
    files['install-probe']=harden('install-probe',files['install-probe'])
    manifest={'schema':'X2D_NATIVE_PAYLOAD_V1','firmware':'4.2.0','feature_mask':args.feature_mask,
              'targets':rows,'guards':audit['baseline_guards'],
              'support_files':{n:sha_bytes(files[n]) for n in ('check-raw-stack-arm','install-probe')},
              'runtime_sha256':fp['artifact']['sha256'],'esflash_report_sha256':build_pilot.sha(esflash_path),'esflash_checks':flash_checks,'hardware_validated':False}
    if ui:
        assert len(removed_ui)==35 and not pending_ui,'必须完整替换旧业务脚本清单'
        manifest['native_ui']={k:ui[k] for k in ('schema','module_path','module_sha256','gui_sha256','required_capabilities','eye_protocol')}
    manifest_data=encoded(manifest);manifest_sha=sha_bytes(manifest_data)
    # 原厂还原文件保持逐个摘要核对；不得把 H3 的 uninstall.sh/cleanup.sh 放进新包。
    unrows=[dict(r,allowed=list(r['allowed'])) for r in audit['uninstall_targets']]
    bypath={r['path']:r for r in unrows}
    # 基础档也必须识别高级档留下的直编库，供降档/完整卸载清理。
    if quarter_direct_contract.TARGET not in bypath:
        u={'id':'u'+str(max(int(x['id'][1:]) for x in unrows)+1),'path':quarter_direct_contract.TARGET,
           'original':'ABSENT','allowed':['ABSENT'],'mode':'644'}
        unrows.append(u);bypath[u['path']]=u
    for r in rows:
        if r['path'] not in bypath:
            u={'id':'u'+str(max(int(x['id'][1:]) for x in unrows)+1),'path':r['path'],
               'original':r['original'],'allowed':['ABSENT',r['candidate_sha256']],'mode':r['mode']}
            assert u['original']=='ABSENT';unrows.append(u);bypath[u['path']]=u
        else:
            u=bypath[r['path']]
            assert u['original']==r['original']
            if r['candidate_sha256'] not in u['allowed']:u['allowed'].append(r['candidate_sha256'])
    legacy_bindings={}
    for source in args.legacy_payload:
        source=source.resolve();assert source.is_relative_to(R7/'native-payload/revisions')
        report=json.loads((source/'report.json').read_text(encoding='utf8'))
        name='private-payload/uninstall/targets.txt'
        assert build_pilot.sha(source/name)==report['files'][name]
        for line in (source/name).read_text(encoding='utf8').splitlines():
            _,path,original,allowed,mode=line.split()
            assert path in bypath and bypath[path]['original']==original and bypath[path]['mode']==mode
            for digest in allowed.split(','):
                if digest not in bypath[path]['allowed']:bypath[path]['allowed'].append(digest)
        legacy_bindings[str(source.relative_to(ROOT))]=build_pilot.sha(source/'report.json')
    for source in args.legacy_continuous_manifest:
        for row in verified_legacy_manifest(source):
            assert row['path'] in bypath,'旧版目标必须已有明确还原规则：'+row['path']
            allowed=bypath[row['path']]['allowed']
            if row['sha256'] not in allowed:allowed.append(row['sha256'])
        legacy_bindings[str(source.resolve().relative_to(ROOT))]=build_pilot.sha(source)
    table=''.join(' '.join((r['id'],r['path'],r['original'],','.join(r['allowed']),r['mode']))+'\n' for r in unrows).encode()
    guards=(UNINSTALL/'guards.txt').read_bytes();assert sha_bytes(guards)==audit['uninstall_guards_sha256']
    unfiles={'targets.txt':table,'guards.txt':guards}
    for r in unrows:
        if r['original']!='ABSENT':
            n='stock-'+r['id'];b=(UNINSTALL/n).read_bytes();assert sha_bytes(b)==r['original'];unfiles[n]=b
    archives={'native-install.gz':archive(files),'native-uninstall.gz':archive(unfiles)}
    inspection=inspect_scriptless_payload.inspect(manifest,archives['native-install.gz'])
    if ui:assert inspection['structural_pass'],inspection['issues']
    for name,data in files.items():
        for private_marker in (b'PlaybackCropProbe.qml',b'syntheticProbeEnabled',b'--compact-ui-selftest',b'INTERNAL_UI_TESTS',b'CHECK_UI_R6',b'CHECK_ABOUT',b'route-check.txt',b'about-check.txt',b'selector.itemSelected'):
            assert private_marker not in data,'载荷携带内部测试入口：'+name
    assert build_pilot.sha(Path(elf_metadata_hardening.__file__))==hardener_sha,'元数据加固器在组包期间变化'
    identity={'builder':build_pilot.sha(Path(__file__)),'package_profiles':build_pilot.sha(Path(package_profiles.__file__)),'elf_metadata_hardener':hardener_sha,'native_ui_init_builder':build_pilot.sha(Path(native_ui_init.__file__)),'manifest_sha256':manifest_sha,
              'archives':{n:sha_bytes(b) for n,b in archives.items()},'uninstall_targets_sha256':sha_bytes(table)}
    if continuous:
        identity['continuous_manifest_sha256']=build_pilot.sha(args.continuous_manifest)
        identity['continuous_checks']=continuous['identity']['checks']
    identity['af_speed_proof_sha256']=build_pilot.sha(af_proof)
    identity['af_speed_contract_sha256']=build_pilot.sha(Path(__file__).with_name('af_speed_contract.py'))
    if legacy_bindings:identity['legacy_continuous_manifests']=legacy_bindings
    revision=sha_bytes(encoded(identity))[:20];out=R7/'native-payload/revisions'/revision
    expected={'manifest.json':manifest_data,'identity.json':encoded(identity)}
    expected['scriptless-inspection.json']=encoded(inspection)
    expected['elf-metadata-hardening.json']=encoded(elf_reviews)
    expected.update(archives)
    for n,b in files.items():expected['private-payload/install/'+n]=b
    for n,b in unfiles.items():expected['private-payload/uninstall/'+n]=b
    text=['#pragma once','#define CN_INSTALL_FEATURE_MASK '+str(args.feature_mask),'namespace cn { namespace install {',
          'struct Target {const char* source;const char* path;const char* original;const char* digest;unsigned mode;};',
          'static constexpr Target targets[]={']
    for r in rows:text.append('{'+','.join(json.dumps(r[k]) for k in ('name','path','original','candidate_sha256'))+',0'+r['mode']+'},')
    text+=['};','struct Guard {const char* path;const char* digest;};','static constexpr Guard guards[]={']
    for r in audit['baseline_guards']:text.append('{'+json.dumps(r['path'])+','+json.dumps(r['sha256'])+'},')
    text+=['};','static constexpr const char* transition_manifest_sha='+json.dumps(manifest_sha)+';',
           'static constexpr const char* core_check_sha='+json.dumps(sha_bytes(files['check-raw-stack-arm']))+';',
           'static constexpr const char* install_probe_sha='+json.dumps(sha_bytes(files['install-probe']))+';',
           'static constexpr unsigned long long payload_bytes='+str(sum(r['bytes'] for r in rows))+'ULL;',
           'static constexpr unsigned long long runtime_check_bytes='+str(sum(len(files[n]) for n in ('p00-libx2d_raw_stack.so','check-raw-stack-arm','p04-render-probe','install-probe')) if args.feature_mask==255 else 0)+'ULL;','} }','']
    expected['include/native_install_tables.hpp']='\n'.join(text).encode()
    text=['#pragma once','namespace cn { namespace setup {',
          'struct LegacyTarget {const char* id;const char* path;const char* original;const char* allowed;unsigned mode;};',
          'static constexpr LegacyTarget legacy_targets[]={']
    for r in unrows:text.append('{'+','.join(json.dumps(v) for v in (r['id'],r['path'],r['original'],','+','.join(r['allowed'])+','))+',0'+r['mode']+'},')
    text+=['};','struct LegacyGuard {const char* path;const char* digest;};','static constexpr LegacyGuard legacy_guards[]={']
    for r in audit['baseline_guards']:text.append('{'+json.dumps(r['path'])+','+json.dumps(r['sha256'])+'},')
    text+=['};','static constexpr const char* legacy_targets_sha='+json.dumps(sha_bytes(table))+';',
           'static constexpr const char* legacy_compat_targets_sha='+json.dumps(audit['uninstall_targets_sha256'])+';',
           'static constexpr const char* legacy_guards_sha='+json.dumps(sha_bytes(guards))+';','} }','']
    expected['include/native_setup_tables.hpp']='\n'.join(text).encode()
    # 类型定义来自 native_host/native_unpack.hpp；两个包只含平面普通文件，落盘为数据权限。
    sys.path.insert(0,str(ROOT/'x2d/tools'))
    from firmware_image import system_file
    decoder=system_file('/xbin/busybox')
    decoder_sha='24c3ee12a59ce8688791accd51c6b745b14b86f3f7668a52e2f5a13c5e2af45d'
    assert sha_bytes(decoder)==decoder_sha and b'\0gzip\0' in decoder and b'\0gunzip\0' in decoder
    text=['#pragma once','#include "native_unpack.hpp"',
          '#ifndef CN_UNPACK_DECODER_SHA256','#define CN_UNPACK_DECODER_SHA256 '+json.dumps(decoder_sha),'#endif',
          'namespace cn { namespace unpack {']
    for kind,fs in (('install',files),('uninstall',unfiles)):
        text+=['static constexpr File '+kind+'_files[]={']
        for n,b in sorted(fs.items()):text.append('{'+json.dumps(n)+','+json.dumps(sha_bytes(b))+','+str(len(b))+'ULL,0644},')
        an='native-'+kind+'.gz';text+=['};','static constexpr Bundle '+kind+'_bundle={'+json.dumps(an)+','+json.dumps(sha_bytes(archives[an]))+','+str(len(archives[an]))+'ULL,'+kind+'_files,sizeof('+kind+'_files)/sizeof('+kind+'_files[0])};']
    text+=['} }',''];expected['include/native_unpack_tables.hpp']='\n'.join(text).encode()
    report={'identity':identity,'release_ready':False,'hardware_executed':False,
            'decoder_static_evidence':{'firmware':'4.2.0','path':'/system/xbin/busybox','sha256':decoder_sha,'bytes':len(decoder),'gzip_applet_offset':decoder.find(b'\0gzip\0'),'gunzip_applet_offset':decoder.find(b'\0gunzip\0'),'executed':False},
            'removed_shell_targets':removed,'pending_ui_source_files':pending_ui,
            'removed_ui_targets':removed_ui,'native_ui_contract':ui,'scriptless_structural_pass':inspection['structural_pass'],
            'install_targets':len(rows),'uninstall_targets':len(unrows),
            'blockers':([] if ui else ['原生 UI 尚未替换，本内部接入载荷仍有 QML/JS'])+['同一最终双包独立回归与抗逆向尚未验收','实机及Mac原生运行未验收'],
            'files':{n:sha_bytes(b) for n,b in expected.items()}}
    expected['report.json']=encoded(report)
    if (out/'report.json').exists():
        for n,b in expected.items():assert (out/n).read_bytes()==b,'缓存被修改：'+n
    else:
        for n,b in expected.items():p=out/n;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b)
    print(json.dumps({'directory':str(out),'install_targets':len(rows),'removed_shell_targets':len(removed),
                      'pending_ui_files':len(pending_ui),'release_ready':False},ensure_ascii=False))
if __name__=='__main__':main()
