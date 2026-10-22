"""重建卸载修正交付包；仅离线构建，拍摄载荷沿用固定已审查版本。"""
import argparse
import json
from pathlib import Path

from assemble_native_client import ROOT, R7, encoded, sha
from rebuild_setup_cleanup import verified_setup
import build_native_client
import build_native_modules
import assemble_native_client
import revise_package_client
import package_windows_delivery


def build(basic, advanced, baseline):
    assert Path.cwd().resolve() == ROOT
    metadata = json.loads((baseline/'native/client-metadata.json').read_text(encoding='utf8'))
    wanted = set()
    for profile in metadata['profiles'].values():
        wanted.update(profile['bootstrap_previous_sha256'])
        wanted.add(sha(baseline/profile.get('setup_resource', 'native/x2d-native-setup')))
    previous = []
    for p in (R7/'native-flow/revisions').glob('*/build-proof.json'):
        proof = json.loads(p.read_text(encoding='utf8'))
        h = proof['artifact']['sha256']
        if h in wanted:
            verified_setup(p.parent)
            previous.append(p.parent/'artifacts/x2d-native-flow')
    assert {sha(p) for p in previous} == wanted
    def payload(setup):
        proof = verified_setup(setup)
        matches = [p.parent for p in (R7/'native-payload/revisions').glob('*/report.json')
                   if sha(p) == proof['payload_report_sha256']]
        assert len(matches) == 1
        return matches[0]
    fresh = build_native_client.build(payload(advanced), advanced/'artifacts/x2d-native-flow',
                                     'windows', previous, payload(basic), basic/'artifacts/x2d-native-flow')
    source = revise_package_client.build(fresh)
    text = (source/'source/mac_patch.py').read_text(encoding='utf8')
    assert text.count('native_client_support.select_uninstall_helper(self)') == 1
    assert 'self.native_metadata, self.stock, self.full_stock, self.removal = requested' in text
    compiled = build_native_modules.build(source/'source', 'windows')
    assembled = assemble_native_client.build(source, compiled)
    delivery = package_windows_delivery.build(assembled)
    result = dict(sources=str(source), compiled=str(compiled), assembled=str(assembled),
                  delivery=str(delivery), camera_requests=0, hardware_accepted=False)
    (delivery/'cleanup-build.json').write_bytes(encoded(result))
    print(json.dumps(result, ensure_ascii=False))


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('basic', type=Path)
    p.add_argument('advanced', type=Path)
    p.add_argument('baseline', type=Path)
    a = p.parse_args()
    build(a.basic.resolve(), a.advanced.resolve(), a.baseline.resolve())
