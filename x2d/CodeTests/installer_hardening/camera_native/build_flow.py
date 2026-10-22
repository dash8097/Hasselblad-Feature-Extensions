"""冻结原生编排输入，构建 ARM64 程序；完整发行门禁另行验证。"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess
import build_pilot,generate_setup_tables,generate_install_tables,generate_runtime_tables,harden_literals,generate_cleanup_stages
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
OUT=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/native-flow'
NAMES=['camera_native.h','camera_native.c','camera_posix.c','native_common.hpp','native_common.cpp','native_counter.cpp','native_publish.cpp','native_process.hpp','native_process.cpp','native_render.cpp','native_prepare.cpp','native_heif.cpp','native_setup.cpp','native_setup_tables.hpp','native_uninstall.cpp','native_install.cpp','native_install_tables.hpp','native_data_cleanup.cpp','native_memory_runner.cpp','native_memory_runner.hpp','native_disk_runner.cpp','native_disk_runner.hpp','native_launch.cpp']
NAMES+=['native_stack_runner.cpp','native_stack_runner.hpp','native_dispatch.cpp']
NAMES+=['native_transaction_read.cpp']
NAMES+=['native_runtime_tables.hpp','native_memory_request.hpp']
NAMES+=['legacy_cleanup_stages.hpp']
NAMES+=['native_ui_contract.hpp','native_ui_ready.hpp','native_ui_ready.cpp','native_ui_check.cpp','ui_receipt.h','ui_receipt.c']
SETUP_ONLY={'native_setup.cpp','native_setup_tables.hpp','native_uninstall.cpp','native_install.cpp','native_install_tables.hpp','native_dispatch.cpp','native_transaction_read.cpp'}
def input_path(name):
    if name in ('ui_receipt.h','ui_receipt.c'):return HERE.parent/'native_ui'/name
    return HERE.parent/'native_host'/name if name in ('native_transaction_read.cpp','native_unpack.cpp','native_unpack.hpp','native_bootstrap.cpp') else HERE/name

def main():
    assert Path.cwd().resolve()==ROOT
    assert (HERE/'legacy_cleanup_stages.hpp').read_text(encoding='utf8')==generate_cleanup_stages.generate(), '历史清理表与来源清单不一致'
    assert (HERE/'native_setup_tables.hpp').read_text(encoding='utf-8')==generate_setup_tables.generate(),'固定事务表与审计来源不一致'
    assert (HERE/'native_install_tables.hpp').read_text(encoding='utf-8')==generate_install_tables.generate(),'安装事务表与审计来源不一致'
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,default=OUT);parser.add_argument('--profile',choices=('combined','runtime','setup'),default='combined');parser.add_argument('--payload',type=Path);parser.add_argument('--hide-literals',action='store_true');parser.add_argument('--native-ui-contract',type=Path);args=parser.parse_args()
    assert (HERE/'native_runtime_tables.hpp').read_text(encoding='utf-8')==generate_runtime_tables.generate(),'运行时叶节点表与固定产物不一致'
    names=[n for n in NAMES if args.profile!='runtime' or n not in SETUP_ONLY]
    ui_contract=None;ui_header=None;contract_digest=None
    if args.native_ui_contract:
        contract_path=args.native_ui_contract.resolve();assert contract_path.is_relative_to((ROOT/'x2d').resolve())
        contract_digest=build_pilot.sha(contract_path)
        ui_contract=json.loads(contract_path.read_text(encoding='utf8'))
        assert ui_contract['schema']=='X2D_NATIVE_UI_CONTRACT_V1'
        assert ui_contract['module_path']=='/system/lib64/libx2d_native_menu.so'
        for key in ('module_sha256','gui_sha256'):
            digest=ui_contract[key];assert isinstance(digest,str) and len(digest)==64 and all(c in '0123456789abcdef' for c in digest)
        assert ui_contract['gui_sha256']=='16391452abdc69de9e0807e065c0f4ab3f1ccb5fc288f6fc4e6f5cb3bdca12e0'
        caps=ui_contract['required_capabilities'];assert type(caps) is int and 0<caps<2**64
        artifact=(ROOT/ui_contract['artifact']).resolve();assert artifact.is_relative_to((ROOT/'x2d/outputs').resolve())
        assert build_pilot.sha(artifact)==ui_contract['module_sha256']
        ui_header=('#pragma once\n#define CN_NATIVE_UI_ENABLED 1\n#define CN_NATIVE_UI_SHA256 "'+ui_contract['module_sha256']+'"\n#define CN_NATIVE_GUI_SHA256 "'+ui_contract['gui_sha256']+'"\n#define CN_NATIVE_UI_CAPABILITIES '+str(caps)+'ULL\n').encode()
    if args.hide_literals:names+=['native_literal.hpp']
    payload=None
    if args.profile=='setup':
        assert args.payload is not None,'setup 必须绑定冻结载荷'
        payload=args.payload.resolve();assert payload.is_relative_to((ROOT/'x2d/outputs').resolve())
        report=json.loads((payload/'report.json').read_text(encoding='utf-8'))
        assert report.get('native_ui_contract')==ui_contract,'setup 与载荷必须绑定同一原生界面；不得遗漏或换用其他契约'
        for name,digest in report['files'].items():
            target=(payload/name).resolve();assert target.is_relative_to(payload)
            assert build_pilot.sha(target)==digest,'冻结载荷被修改：'+name
        names+=['native_unpack.cpp','native_unpack.hpp','native_unpack_tables.hpp','native_bootstrap.cpp']
    else:assert args.payload is None
    def source_path(name):
        if payload and name in ('native_install_tables.hpp','native_setup_tables.hpp','native_unpack_tables.hpp'):return payload/'include'/name
        return input_path(name)
    def source_bytes(name):
        return ui_header if name=='native_ui_contract.hpp' and ui_header is not None else source_path(name).read_bytes()
    def verify_contract_inputs():
        assert build_pilot.sha(HERE/'elf_metadata_hardening.py')==build_identity['elf_metadata_hardener'],'构建中元数据加固器被更改'
        if ui_contract:
            assert build_pilot.sha(contract_path)==contract_digest,'构建中UI契约被更改'
            assert build_pilot.sha(artifact)==ui_contract['module_sha256'],'构建中UI产物被更改'
    output=args.output.resolve();assert output.is_relative_to((ROOT/'x2d/outputs').resolve())
    hashes={n:hashlib.sha256(source_bytes(n)).hexdigest() for n in names}
    sha_header=ROOT/'x2d/CodeTests/pixel_shift_rgb/receipt_sha256.h'
    hashes['receipt_sha256.h']=build_pilot.sha(sha_header)
    build_identity={'profile':args.profile,'source':hashes,'builder':build_pilot.sha(Path(__file__)),'runtime_generator':build_pilot.sha(HERE/'generate_runtime_tables.py'),'table_generator':build_pilot.sha(HERE/'generate_setup_tables.py'),'install_generator':build_pilot.sha(HERE/'generate_install_tables.py'),'table_audit':build_pilot.sha(generate_setup_tables.SOURCE),'zig':build_pilot.sha(build_pilot.ZIG)}
    build_identity['hide_literals']=args.hide_literals
    build_identity['elf_metadata_hardener']=build_pilot.sha(HERE/'elf_metadata_hardening.py')
    if ui_contract:build_identity['native_ui_contract']=ui_contract
    build_identity['elf_checker']=build_pilot.sha(Path(build_pilot.__file__))
    if args.hide_literals:build_identity['literal_transformer']=build_pilot.sha(HERE/'harden_literals.py')
    if payload:build_identity['payload_report_sha256']=build_pilot.sha(payload/'report.json')
    revision=hashlib.sha256(json.dumps(build_identity,sort_keys=True).encode()).hexdigest()[:20]
    output=output/'revisions'/revision
    proof=output/'build-proof.json'
    if proof.exists():
        prior=json.loads(proof.read_text(encoding='utf-8'))
        assert prior['build_identity']==build_identity
        for n,digest in hashes.items():assert build_pilot.sha(output/'private-build/source'/n)==digest
        for n,digest in prior.get('compiled_source_sha256',{}).items():assert build_pilot.sha(output/'private-build/compile-source'/n)==digest
        if args.hide_literals:
            for n in names:
                expected=(output/'private-build/source'/n).read_bytes()
                if n.endswith('.cpp'):expected=harden_literals.transform(expected.decode('utf-8'))[0].encode('utf-8')
                assert (output/'private-build/compile-source'/n).read_bytes()==expected
        assert build_pilot.sha(output/'artifacts/x2d-native-flow')==prior['artifact']['sha256']
        verify_contract_inputs()
        print(json.dumps({'reused_verified_build':str(output),'artifact':prior['artifact'],'release_ready':False},ensure_ascii=False));return
    private=output/'private-build';source=private/'source';source.mkdir(parents=True,exist_ok=True)
    for n in names:
        (source/n).write_bytes(source_bytes(n))
        assert build_pilot.sha(source/n)==hashes[n]
    shutil.copyfile(sha_header,source/'receipt_sha256.h')
    assert build_pilot.sha(source/'receipt_sha256.h')==hashes['receipt_sha256.h']
    compile_source=source;compiled_hashes={};literal_count=0
    if args.hide_literals:
        assert harden_literals.self_test();compile_source=private/'compile-source';compile_source.mkdir(exist_ok=True)
        for n in hashes:
            data=(source/n).read_bytes()
            if n.endswith('.cpp'):
                transformed,count=harden_literals.transform(data.decode('utf-8'));data=transformed.encode('utf-8');literal_count+=count
            (compile_source/n).write_bytes(data);compiled_hashes[n]=build_pilot.sha(compile_source/n)
    env=dict(os.environ,TEMP=str(private),TMP=str(private),ZIG_LOCAL_CACHE_DIR=str(private/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(private/'zig-global'))
    def run(args):
        r=subprocess.run([str(a) for a in args],env=env,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=240)
        if r.returncode:raise RuntimeError(r.stdout+r.stderr)
    common=['-target','aarch64-linux-musl','-Oz','-Wall','-Wextra','-Werror','-fvisibility=hidden','-ffunction-sections','-fdata-sections','-fstack-protector-strong']
    objects=[]
    for n in names:
        if not n.endswith(('.c','.cpp')):continue
        obj=private/(n+'.o');objects.append(obj)
        language='c++' if n.endswith('.cpp') else 'cc'
        defines=['-DCN_FULL_FLOW=1']+(['-DCN_RUNTIME_ONLY=1'] if args.profile=='runtime' else [])+(['-DCN_PACKAGED_PAYLOAD=1'] if args.profile=='setup' else [])
        if args.hide_literals:defines+=['-DCN_LITERAL_SEED=0x'+revision[:16]+'ULL']
        run([build_pilot.ZIG,language,*common,'-std=c++17' if language=='c++' else '-std=c11',*defines,'-c',compile_source/n,'-o',obj])
    artifacts=output/'artifacts';artifacts.mkdir(exist_ok=True);binary=artifacts/'x2d-native-flow'
    run([build_pilot.ZIG,'c++',*common,'-static','-Wl,--gc-sections','-s',*objects,'-o',binary])
    # setup 必须退出自删除的原生 ELF，原厂固定 shell OS 原语才可重挂只读。
    # 不允许包装业务脚本；仅绑定已单独审计的 bootstrap 实现。
    fixed_cleanup=args.profile=='setup'
    if fixed_cleanup:
        # 2026-10-18: 固定 OS 挂载兼容；Linux 45428188b6c41504a672，914 checks。
        assert hashes['native_bootstrap.cpp']=='2a228af33b65a75715f2c907ec6895efe5ca463089f2eeed5640aa0bfd021b56', '收尾依赖变更须重新审计'
    info=build_pilot.elf_check(binary,fixed_cleanup_shell=fixed_cleanup)
    for n in names:assert hashlib.sha256(source_bytes(n)).hexdigest()==hashes[n],'构建中输入被更改：'+n
    verify_contract_inputs()
    assert build_pilot.sha(sha_header)==hashes['receipt_sha256.h'],'构建中摘要实现被更改'
    report={'offline':True,'camera_requests':0,'source_sha256':hashes,'artifact':info,'build_identity':build_identity,
            'commands':['audio','heif-disk','counter-sync','publish-stack','publish-quarter','publish-memory','publish-legacy-memory','publish-disk','render-memory','render-legacy-memory','render-sixteen','mode-prepare','heif-restart','stage-init','retained-cleanup','uninstall','install-stock','data-cleanup','memory-run','memory-restore','memory-watchdog','sixteen-run','sixteen-restore','sixteen-watchdog','quarter-run','quarter-restore','quarter-watchdog','launch-memory','launch-multi','launch-stack','stack-run','stack-restore','stack-watchdog','dispatch','transaction-read'],'release_ready':False,
            'qualified':False,'hardware_executed':False,'linux_tests':None,
            'not_migrated':([] if ui_contract and ui_contract.get('migration_complete') is True else ['QML/JS']),
            'package_integration_verified_by_this_build':False,
            'unqualified_transactions':['install-stock','uninstall','retained-cleanup','data-cleanup'],
            'qualification':'仅交叉编译，不证明运行正确或整包无脚本'}
    report['compiled_source_sha256']=compiled_hashes
    if ui_contract:report['commands'].append('ui-ready')
    report['hidden_diagnostic_literals']=literal_count
    report['hardening_claim']='仅保护部分静态诊断字面量；不阻止动态恢复，不证明逆向成本高于重写'
    if args.profile=='runtime':
        report['commands']=[c for c in report['commands'] if c not in ('stage-init','retained-cleanup','uninstall','install-stock','dispatch','transaction-read')]
        report['unqualified_transactions']=['data-cleanup']
    if args.profile=='setup':
        report['commands']+=['payload-unpack','bootstrap-commit','bootstrap-read','bootstrap-cleanup']
        report['payload_report_sha256']=build_identity['payload_report_sha256']
    proof.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'build_directory':str(output),'artifact':info,'release_ready':False},ensure_ascii=False))

if __name__=='__main__':main()


