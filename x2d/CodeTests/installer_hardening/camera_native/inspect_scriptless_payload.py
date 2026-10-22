"""检查实际摄像机安装归档，不把改扩展名或嵌入QV4当原生迁移完成。"""
from pathlib import Path
import argparse,gzip,hashlib,io,json,tarfile

ROOT=Path(__file__).resolve().parents[4]
R7=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7'
SCRIPT_SUFFIXES={'.sh','.bash','.zsh','.py','.pyc','.ps1','.bat','.cmd','.command','.qml','.js','.mjs','.jsc'}
def sha(data):return hashlib.sha256(data).hexdigest()
def inspect(manifest,archive_bytes):
 expected={r['name']:r for r in manifest['targets']}
 support=manifest['support_files'];issues=[];seen=set();findings=[]
 with tarfile.open(fileobj=io.BytesIO(gzip.decompress(archive_bytes)),mode='r:') as tar:
  for item in tar:
   if not item.isfile() or '/' in item.name or '\\' in item.name or item.name in seen:
    issues.append({'file':item.name,'reason':'unexpected_archive_member'});continue
   seen.add(item.name);stream=tar.extractfile(item);data=stream.read() if stream else b''
   row=expected.get(item.name);digest=row['candidate_sha256'] if row else support.get(item.name)
   if digest is None or sha(data)!=digest:issues.append({'file':item.name,'reason':'manifest_digest_mismatch'})
   target=row['path'] if row else item.name
   if Path(target).suffix.lower() in SCRIPT_SUFFIXES:issues.append({'file':item.name,'reason':'script_target','target':target})
   if data.startswith(b'#!'):issues.append({'file':item.name,'reason':'interpreter_shebang'})
   # In particular the previous menu-preload ELF embedded multiple QV4 units.
   # Renaming that ELF does not remove the interpreted business logic.
   offsets=[];at=0
   while True:
    at=data.find(b'qv4cdata',at)
    if at<0:break
    offsets.append(at);at+=8
   if offsets:issues.append({'file':item.name,'reason':'embedded_qv4','offsets':offsets})
   if Path(target).suffix.lower()=='.svg' and any(token in data.lower() for token in (b'<script',b'javascript:',b'onload=')):
    issues.append({'file':item.name,'reason':'active_svg'})
   findings.append({'file':item.name,'sha256':sha(data),'bytes':len(data)})
 for name in set(expected)|set(support):
  if name not in seen:issues.append({'file':name,'reason':'missing_archive_member'})
 return {'schema':'X2D_SCRIPTLESS_ARCHIVE_INSPECTION_V1','structural_pass':not issues,'issues':issues,'files':findings,
         'limits':['这是实际归档的结构检查，不证明不存在经编码隐藏的脚本，也不证明反编译成本。','最终抗逆向必须由独立checker检查同一包的哈希。'],'camera_operations':0}
def main():
 assert Path.cwd().resolve()==ROOT
 p=argparse.ArgumentParser();p.add_argument('--payload-revision',required=True);a=p.parse_args()
 assert len(a.payload_revision)==20 and all(c in '0123456789abcdef' for c in a.payload_revision)
 source=R7/'native-payload/revisions'/a.payload_revision;manifest=(source/'manifest.json').read_bytes();archive=(source/'native-install.gz').read_bytes()
 report=inspect(json.loads(manifest),archive);report['payload_revision']=a.payload_revision;report['manifest_sha256']=sha(manifest);report['archive_sha256']=sha(archive);report['inspector_sha256']=sha(Path(__file__).read_bytes())
 identity=sha(json.dumps(report,ensure_ascii=False,sort_keys=True).encode())[:20];out=R7/'native-payload/scriptless-inspection'/identity;out.mkdir(parents=True,exist_ok=True)
 data=(json.dumps(report,ensure_ascii=False,indent=2)+'\n').encode();target=out/'report.json'
 if target.exists():assert target.read_bytes()==data
 else:target.write_bytes(data)
 print(json.dumps({'report':str(target),'structural_pass':report['structural_pass'],'issues':len(report['issues'])},ensure_ascii=False))
if __name__=='__main__':main()
