"""从固定 R52 生成原生事务客户端源码集和净化元数据；不修改原包。"""
import argparse
import ast
import hashlib
import json
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
R7 = ROOT/'x2d/outputs/4.2.0/installer-hardening-r7'
BASE = HERE/'build_inputs/client'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def encoded(value):
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2)+'\n').encode('utf8')


def native_ui_diagnostic_command():
    # Passive state only: no request/nonce, wake-up, marker removal or restart.
    return (
        'd=/tmp; f=/tmp/x2d-native-ui.status; if test ! -L $d && test ! -L $f && test -f $f; then head -c 512 $f; else echo ABSENT; fi')


def native_ui_disable_diagnostic_command():
    return (
        'd=/blackbox; f=$d/x2d-native-menu.disable; '
        'printf "\\nDISABLE_MARKER="; '
        'if [ -L $d ] || [ ! -d $d ] || [ ! -r $d ] || [ ! -x $d ];then echo UNCONFIRMED; '
        'elif [ -e $f ] || [ -L $f ];then echo PRESENT;else echo ABSENT;fi')


def transformed_patch(native_ui=False):
    source = (BASE/'mac_patch.py').read_text(encoding='utf-8-sig')
    assert source.count('X2D R5.8.52 · 固件 4.2.0') == 1
    source = source.replace('X2D R5.8.52 · 固件 4.2.0', 'X2D R7 内部候选 · 固件 4.2.0')
    tree = ast.parse(source)
    # Historical retained paths exceed the wire limit with the verbose probe.
    # Preserve NOFOLLOW classification and all four response tokens.
    snapshot = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == 'snapshot_command')
    assignment = next(n for n in snapshot.body if isinstance(n, ast.Assign) and ast.unparse(n.targets[0]) == 'command')
    suffix = '; r=UNSUPPORTED_TYPE; [ ! -e "$f" ] && r=ABSENT; [ -f "$f" ] && r=FILE; [ -L "$f" ] && r=UNSUPPORTED_LINK; echo "$r"'
    assignment.value = ast.parse("'f=' + path + " + repr(suffix), mode='eval').body
    template = ast.parse((HERE/'native_client_methods.py').read_text(encoding='utf8'))
    methods = {n.name: n for n in template.body[1].body if isinstance(n, ast.FunctionDef)}
    assert set(methods) == {'check_package', 'data_cleanup', 'cleanup', 'install', 'uninstall'}
    cls = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == 'Installer')
    replaced = set()
    body = []
    for node in cls.body:
        if isinstance(node, ast.FunctionDef):
            if node.name in methods:
                replaced.add(node.name)
                node = methods[node.name]
            elif node.name == 'preserve_stage':
                continue
        body.append(node)
    cls.body = body
    ui_changes=[]
    if native_ui:
        verify=next(n for n in cls.body if isinstance(n,ast.FunctionDef) and n.name=='verify_current')
        assert isinstance(verify.body[1],ast.Assign) and ast.unparse(verify.body[1].targets[0])=='menu'
        assert isinstance(verify.body[2],ast.If) and 'menu' in ast.unparse(verify.body[2].test)
        assert 'MENU_AND_AFC_POINTERS_READY' in ast.unparse(verify.body[3])
        verify.body[1:4]=ast.parse('native_client_support.verify_ui(self)').body
        class NativeUiChecks(ast.NodeTransformer):
            def visit_Expr(self,node):
                text=ast.unparse(node)
                if 'rawStackUI=3 popup=1' in text:
                    ui_changes.append('legacy-ui-marker');return ast.parse("self.log.append('原生界面已取得本次请求回执；不以历史 UI 文件判定就绪。')").body[0]
                if "obj.get('eyeProtocol') == 1" in text:
                    ui_changes.append('eye-protocol');return ast.parse(text.replace("obj.get('eyeProtocol') == 1", "obj.get('eyeProtocol') == 2")).body[0]
                return self.generic_visit(node)
        NativeUiChecks().visit(verify)
        assert sorted(ui_changes)==['eye-protocol','legacy-ui-marker']
    assert replaced == set(methods)
    run_method = next(n for n in cls.body if isinstance(n, ast.FunctionDef) and n.name == 'run')
    preserved_run = ast.dump(run_method, include_attributes=False)
    run_method.body.insert(0, ast.parse('native_client_support.begin_action(self, action)').body[0])
    tree.body.insert(0, ast.Import(names=[ast.alias(name='native_client_support')]))
    ast.fix_missing_locations(tree)
    result = ast.unparse(tree)+'\n'
    for forbidden in ('sh install.sh', 'sh ./uninstall.sh', 'cleanup_patch_data_r33.sh',
                      'sh ./cleanup.sh', 'sh -n s.sh', 'def preserve_stage'):
        assert forbidden not in result, '旧脚本逻辑仍在生成代码：'+forbidden
    # 完整诊断、USB上传与人工重启逻辑逐方法保持 AST 一致。
    original = next(n for n in ast.parse(source).body if isinstance(n, ast.ClassDef) and n.name == 'Installer')
    before = {n.name: ast.dump(n, include_attributes=False) for n in original.body if isinstance(n, ast.FunctionDef)}
    after = {n.name: ast.dump(n, include_attributes=False) for n in cls.body if isinstance(n, ast.FunctionDef)}
    preserved = sorted(set(before)-set(methods)-{'preserve_stage','run'}-({'verify_current'} if native_ui else set()))
    assert all(before[n] == after[n] for n in preserved)
    assert before['run'] == preserved_run
    return result.encode('utf8'), preserved


def build(payload, setup, platform, previous_setups=(), basic_payload=None, basic_setup=None):
    assert Path.cwd().resolve() == ROOT
    payload, setup = payload.resolve(), setup.resolve()
    assert payload.is_relative_to(R7) and setup.is_relative_to(R7)
    report = json.loads((payload/'report.json').read_text(encoding='utf8'))
    for name, digest in report['files'].items():
        path = (payload/name).resolve()
        assert path.is_relative_to(payload) and sha(path.read_bytes()) == digest
    proof = json.loads((setup.parent.parent/'build-proof.json').read_text(encoding='utf8'))
    assert proof['build_identity']['profile'] == 'setup'
    assert proof['artifact']['sha256'] == sha(setup.read_bytes())
    assert proof['payload_report_sha256'] == sha((payload/'report.json').read_bytes())
    # bootstrap 的命名/锁/协议正在迁移时，禁止旧 ELF 与新主机实现混包。
    for name, current in (('native_bootstrap.cpp', HERE/'native_bootstrap.cpp'),
                          ('native_common.cpp', HERE.parent/'camera_native/native_common.cpp')):
        assert proof['source_sha256'][name] == sha(current.read_bytes()), '原生引导实现与 setup 不一致：'+name
    manifest = json.loads((payload/'manifest.json').read_text(encoding='utf8'))
    native_ui=manifest.get('native_ui')
    rows = [{k: r[k] for k in ('name','path','original','candidate_sha256','mode','bytes')}
            for r in manifest['targets']]
    uninstall = []
    for line in (payload/'private-payload/uninstall/targets.txt').read_text().splitlines():
        id_, path, original, allowed, mode = line.split(' ')
        uninstall.append(dict(id=id_, path=path, original=original, allowed=allowed.split(','), mode=mode))
    paths = {r['path'] for r in rows}
    def included(names):
        hits = [any(p.endswith('/'+name) for p in paths) for name in names]
        assert not any(hits) or all(hits), '功能载荷只含部分必需组件：'+repr(names)
        return all(hits)
    stock = dict(targets={r['name']: r['path'] for r in rows},
                 original={r['name']: None if r['original']=='ABSENT' else r['original'] for r in rows},
                 required_guards={r['path']: r['sha256'] for r in manifest['guards']},
                 capture_included=included(('worker','libx2d_raw_stack.so')),
                 face_eye_included=included(('libx2d_face_lock.so','libx2d_face_continuity.so','libx2d_face_publication.so')),
                 af_speed_included='/system/lib64/libx2d_af_base0.so' in paths,
                 feature_mask=manifest['feature_mask'])
    metadata = dict(schema='X2D_NATIVE_CLIENT_V1', firmware='4.2.0', stock=stock,
                    removal=dict(targets=uninstall), install_targets=rows,
                    manifest_sha256=sha((payload/'manifest.json').read_bytes()))
    previous_hashes=[]
    cleanup_table=setup.parent.parent/'private-build/source/legacy_cleanup_stages.hpp'
    cleanup_bytes=cleanup_table.read_bytes()
    assert sha(cleanup_bytes)==proof['source_sha256']['legacy_cleanup_stages.hpp']
    for previous in previous_setups:
        previous=previous.resolve()
        assert previous.is_relative_to(R7/'native-flow/revisions') and previous.name=='x2d-native-flow'
        previous_proof=json.loads((previous.parent.parent/'build-proof.json').read_text(encoding='utf8'))
        assert previous_proof['build_identity']['profile']=='setup'
        previous_hash=sha(previous.read_bytes())
        assert previous_hash==previous_proof['artifact']['sha256']
        assert ('"'+previous_hash+'"').encode('ascii') in cleanup_bytes, '允许升级的旧安装工具未进入清理表：'+previous_hash
        previous_hashes.append(previous_hash)
    metadata['bootstrap_previous_sha256']=sorted(set(previous_hashes))
    if native_ui:metadata['native_ui']=native_ui
    resources = {'native/client-metadata.json': encoded(metadata),
                 'native/x2d-native-setup': setup.read_bytes()}
    for name in ('native-install.gz', 'native-uninstall.gz'):
        resources['native/'+name] = (payload/name).read_bytes()
    for name in ('diagnostic-commands.json', 'feature-catalog.json'):
        resources[name] = (BASE/name).read_bytes()
    if native_ui:
        commands=json.loads(resources['diagnostic-commands.json'].decode('utf-8-sig'))
        menu=[r for r in commands if r['name']=='menu'];assert len(menu)==1
        menu[0]['path']='/tmp/x2d-native-ui.status'
        menu[0]['command']=native_ui_diagnostic_command()
        commands.append(dict(name='menu_disable',path='/blackbox/x2d-native-menu.disable',command=native_ui_disable_diagnostic_command()))
        for row in commands:
            assert len(row['command'].encode('ascii')) <= 231, '诊断命令超过 USB 上限：'+row['name']
        resources['diagnostic-commands.json']=encoded(commands)
    if basic_payload is not None or basic_setup is not None:
        assert basic_payload is not None and basic_setup is not None and stock['feature_mask']==255
        basic_sources=build(basic_payload,basic_setup,platform,previous_setups)
        base=json.loads((basic_sources/'resources/native/client-metadata.json').read_text(encoding='utf8'))
        assert base['stock']['feature_mask']==248 and base['stock']['required_guards']==stock['required_guards']
        # 基础清理事务是整包卸载入口：必须能识别本次高级的每一个字节。
        removal={r['path']:r for r in base['removal']['targets']}
        for row in rows:
            assert row['path'] in removal and row['candidate_sha256'] in removal[row['path']]['allowed'], '基础卸载表未包含本次高级：'+row['path']
            assert removal[row['path']]['original']==row['original']
        advanced_removal={r['path']:r for r in metadata['removal']['targets']}
        for row in base['install_targets']:
            assert row['path'] in advanced_removal and row['candidate_sha256'] in advanced_removal[row['path']]['allowed'], '高级卸载表未包含本次基础：'+row['path']
            assert advanced_removal[row['path']]['original']==row['original']
        advanced=dict(metadata)
        # 两个工具均由此包校验绑定，允许在基础／高级切换时识别另一工具。
        base['bootstrap_previous_sha256']=sorted(set(base['bootstrap_previous_sha256']+[sha(setup.read_bytes())]))
        advanced['bootstrap_previous_sha256']=sorted(set(advanced['bootstrap_previous_sha256']+[sha(basic_setup.read_bytes())]))
        base['setup_resource']='native/basic/x2d-native-setup'
        base['payload_prefix']='native/basic/'
        for name in ('x2d-native-setup','native-install.gz','native-uninstall.gz'):
            resources['native/basic/'+name]=(basic_sources/'resources/native'/name).read_bytes()
        metadata=dict(schema='X2D_NATIVE_CLIENT_V1',firmware='4.2.0',default_feature_mask=248,
                      profiles={'248':base,'255':advanced})
        resources['native/client-metadata.json']=encoded(metadata)
        resources['feature-catalog.json']=encoded({'features':[
            {'label':'基础','bit':248,'default':True},
            {'label':'高级（包含基础）','bit':255,'default':False}]})
    source = {}
    for name in ('mac_usb', 'mac_diagnostics', 'package_feature_selection'):
        source[name+'.py'] = (BASE/(name+'.py')).read_bytes()
    source['mac_patch.py'], preserved = transformed_patch(bool(native_ui))
    for name in ('native_adapter', 'native_operations', 'native_bootstrap', 'native_client_support', 'native_network_upload', 'native_receipt_cleanup'):
        source[name+'.py'] = (HERE/(name+'.py')).read_bytes()
    source['native_transaction_contract.py'] = (HERE.parent/'native_transaction_contract.py').read_bytes()
    if platform == 'windows':
        source['windows_engine.py'] = (HERE.parent/'windows_engine.py').read_bytes()
    else:
        text = (BASE/'mac_gui.py').read_text(encoding='utf-8-sig')
        if 'profiles' in metadata:
            import profile_ui
            text=profile_ui.mac(text)
        assert text.count('X2D 100C  /  固件 4.2.0  /  R5.8.52') == 1
        text = text.replace('X2D 100C  /  固件 4.2.0  /  R5.8.52', 'X2D 100C  /  固件 4.2.0  /  R7 内部候选')
        assert text.count("if __name__ == '__main__':") == 1
        source['mac_gui.py'] = text.replace("if __name__ == '__main__':", 'def main():').encode('utf8')
    modules = sorted([Path(n).stem for n in source]+['native_client_resources'])
    required = [('engine/'+n+'.pyd') if platform == 'windows' else (n+'.so') for n in modules]
    anchors = {n: sha(b) for n, b in resources.items()}
    decoys = json.loads((HERE.parent/'decoys.json').read_text(encoding='utf8'))
    assert isinstance(decoys, list) and all(isinstance(s,str) and 0 < len(s) < 512 for s in decoys)
    source['native_client_resources.py'] = ('PLATFORM = '+repr(platform)+'\nRESOURCE_HASHES = '+repr(anchors)
        +'\nREQUIRED_MODULES = '+repr(required)
        +'\n__analysis_profiles__ = '+repr(tuple(decoys))+'\n').encode('utf8')
    identity = dict(builder=sha(Path(__file__).read_bytes()), platform=platform,
                    source={n: sha(b) for n, b in source.items()}, resources=anchors,
                    template=sha((HERE/'native_client_methods.py').read_bytes()),
                    inert_analysis_decoys=sha((HERE.parent/'decoys.json').read_bytes()),
                    canonical=sha((BASE/'mac_patch.py').read_bytes()))
    revision = sha(encoded(identity))[:20]
    out = R7/'native-host/client-sources'/revision
    expected = {'source/'+n: b for n, b in source.items()}
    expected.update({'resources/'+n: b for n, b in resources.items()})
    value = dict(identity=identity, files={n: sha(b) for n,b in expected.items()},
                 preserved_installer_methods=preserved, release_ready=False, camera_requests=0,
                 native_ui_contract=native_ui,
                 blockers=['完整原生启动器清单锚未组装','完整包交叉测试和独立对抗未验收']+([] if native_ui else ['原生UI迁移未完成']))
    expected['report.json'] = encoded(value)
    if out.exists():
        assert all((out/n).read_bytes() == b for n,b in expected.items()), '冻结源码集被修改'
    else:
        for n,b in expected.items():
            path=out/n;path.parent.mkdir(parents=True, exist_ok=True);path.write_bytes(b)
    print(json.dumps(dict(directory=str(out), modules=len(modules),
                          preserved_methods=len(preserved), release_ready=False), ensure_ascii=False))
    return out


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--payload', type=Path, required=True)
    parser.add_argument('--setup', type=Path, required=True)
    parser.add_argument('--platform', choices=('windows','mac'), required=True)
    parser.add_argument('--basic-payload', type=Path)
    parser.add_argument('--basic-setup', type=Path)
    parser.add_argument('--previous-setup', type=Path, action='append', default=[])
    args = parser.parse_args()
    build(args.payload, args.setup, args.platform, args.previous_setup, args.basic_payload, args.basic_setup)
