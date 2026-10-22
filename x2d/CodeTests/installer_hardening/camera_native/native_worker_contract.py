"""完整包调度器与当前源码、精确 ARM Linux 验证的绑定；不访问相机。"""
import hashlib,json
from pathlib import Path

REVISION='f2a6e728a760fdd2fbe4'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def validated_worker(root):
    root=Path(root);r7=root/'x2d/outputs/4.2.0/installer-hardening-r7'
    directory=r7/'native-worker/revisions'/REVISION
    proof=directory/'build-proof.json';data=json.loads(proof.read_text(encoding='utf8'))
    source=root/'x2d/CodeTests/pixel_shift_rgb'
    for name,digest in data['identity']['sources'].items():
        assert Path(name).name==name and sha(source/name)==digest, '调度器源码未进入候选：'+name
    assert data['identity']['builder']==sha(Path(__file__).with_name('build_native_worker.py'))
    artifact=directory/'artifacts/worker';assert sha(artifact)==data['artifact']['sha256']
    checks=[];fixture=Path(__file__).with_name('checker_native_worker_init.c')
    for report in (r7/'checker-ui').glob('native-worker-*/report.json'):
        r=json.loads(report.read_text(encoding='utf8'))
        if r.get('passed') and r.get('exact_worker') and r.get('artifact_sha256')==sha(artifact) and r.get('source_sha256')==sha(fixture):checks.append(report)
    assert len(checks)==1, '缺少当前调度器的精确 ARM Linux 验证'
    return artifact,proof,checks[0]
