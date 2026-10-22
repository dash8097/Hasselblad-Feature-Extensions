"""绑定八镜头 1.6/2/2.8 与 AFC type1=2.4 的离线 ARM 验证，不操作相机。"""
import hashlib,json
from pathlib import Path

def validated_af_speed(root):
    root=Path(root).resolve()
    proof=root/'x2d/outputs/4.2.0/af-speed-16-24-r1/package-proof.json'
    data=json.loads(proof.read_text(encoding='utf8'))
    assert data['firmware']=='4.2.0' and data['speedProfile']=='1.6/2/2.8' and data['afcType1Scale']==2.4
    def checked(name,digest):
        p=(root/name).resolve()
        assert p.is_relative_to(root/'x2d') and hashlib.sha256(p.read_bytes()).hexdigest()==digest,name
        return p
    for name,digest in data['source_sha256'].items():checked(name,digest)
    checks=[]
    for name,digest in data['checks'].items():
        p=checked(name,digest);r=json.loads(p.read_text(encoding='utf8'))
        assert r['status']=='PASS' and r['librarySha256']==data['raw_sha256']
        if p.name=='machine-check.json':
            assert r['speedProfile']==data['speedProfile'] and r['afcType1Scale']==2.4 and r['speedCases']==r['afcType1Cases']==1402
        checks.append(p)
    assert len(checks)==2 and data['soname_only_before_hardening']
    artifact=checked(data['artifact']['path'],data['artifact']['sha256'])
    return artifact,proof,checks
