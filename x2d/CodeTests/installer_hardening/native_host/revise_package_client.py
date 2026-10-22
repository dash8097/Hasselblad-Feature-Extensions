"""在已验证冻结源码集上仅替换包检查模块，保留机内载荷与其它业务源码。"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

from assemble_native_client import HERE, R7, ROOT, encoded, read_proof, sha


def build(source):
    assert Path.cwd().resolve() == ROOT
    source = source.resolve()
    assert source.is_relative_to(R7 / 'native-host/client-sources')
    old = read_proof(source)
    support = HERE / 'native_client_support.py'
    identity = {'base_report': sha(source / 'report.json'), 'support': sha(support),
                'builder': sha(Path(__file__))}
    revision = hashlib.sha256(encoded(identity)).hexdigest()[:20]
    out = R7 / 'native-host/client-sources' / revision
    assert not out.exists(), '不覆盖已冻结输入'
    shutil.copytree(source, out)
    shutil.copyfile(support, out / 'source/native_client_support.py')
    old['identity']['source']['native_client_support.py'] = sha(support)
    engine = out / 'source/windows_engine.py'
    text = engine.read_text(encoding='utf8')
    before = "[str(Path(root)/'X2dHostCheck.exe')]"
    assert text.count(before) == 1
    text = text.replace(before, "[str(Path(root)/'HasselbladTuningPatchInstaller.exe'), '--host-check']")
    engine.write_text(text, encoding='utf8')
    old['identity']['source']['windows_engine.py'] = sha(engine)
    patch=out/'source/mac_patch.py'
    text=patch.read_text(encoding='utf8')
    before="        removal_route(snapshot, self.removal)\n        self.status('正在执行原生卸载事务')"
    if before in text:
        assert text.count(before)==1
        text=text.replace(before,"        removal_route(snapshot, self.removal)\n        native_client_support.prepare_uninstall(self)\n        self.status('正在执行原生卸载事务')")
    else:
        assert text.count('native_client_support.prepare_uninstall(self)') == 1
    patch.write_text(text,encoding='utf8')
    old['identity']['source']['mac_patch.py']=sha(patch)
    old['identity']['package_fix'] = identity
    old['files'] = {p.relative_to(out).as_posix(): sha(p)
                    for folder in ('source', 'resources') for p in (out / folder).rglob('*') if p.is_file()}
    old['camera_requests'] = 0
    (out / 'report.json').write_bytes(encoded(old))
    print(out)
    return out


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('source', type=Path)
    build(parser.parse_args().source)
