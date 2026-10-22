"""将已校验标准库 ZIP 的公开源码编为对应 Python 3.11 字节码。"""
from pathlib import Path
import hashlib,importlib.util,importlib._bootstrap_external,json,marshal,sys,zipfile
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
R7=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7'
LEGACY=ROOT/'x2d/outputs/4.2.0/installer-hardening-r3/internal-candidates/windows-installer'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def build():
 assert Path.cwd().resolve()==ROOT and sys.version_info[:2]==(3,11)
 manifest=json.loads((LEGACY/'package-integrity.json').read_text(encoding='utf-8-sig'))
 source=LEGACY/'runtime/python311.zip';assert sha(source)==manifest['runtime/python311.zip']
 identity=dict(builder=sha(Path(__file__)),input=sha(source),compiler_version=sys.version,compiler=sha(Path(sys.executable)),magic=importlib.util.MAGIC_NUMBER.hex())
 rev=hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20];out=R7/'native-host/windows-runtime'/rev
 if (out/'report.json').exists():
  old=json.loads((out/'report.json').read_text(encoding='utf8'));assert old['identity']==identity
  assert all(sha(out/n)==h for n,h in old['files'].items());return out
 assert not out.exists(),'保留未完成构建'
 target=out/'artifacts/runtime/python311.zip';target.parent.mkdir(parents=True)
 entries={};count=0
 with zipfile.ZipFile(source) as z:
  for info in z.infolist():
   name=info.filename;p=Path(name)
   assert not p.is_absolute() and '..' not in p.parts and '\\' not in name and name not in entries
   if info.is_dir():continue
   content=z.read(info)
   if name.endswith('.py'):
    code=compile(content,'stdlib:'+name,'exec',dont_inherit=True,optimize=0)
    content=importlib._bootstrap_external._code_to_hash_pyc(code,importlib.util.source_hash(content),checked=True)
    assert content[:4]==importlib.util.MAGIC_NUMBER and marshal.loads(content[16:]).co_filename=='stdlib:'+name
    name=name[:-3]+'.pyc';count+=1
   else:
    assert p.suffix.lower() not in ('.pyw','.sh','.ps1','.bat','.cmd','.csh','.fish') and not content.startswith(b'#!'),'未分类脚本:'+name
   assert name not in entries;entries[name]=content
 with zipfile.ZipFile(target,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
  for name,content in sorted(entries.items()):
   item=zipfile.ZipInfo(name,(1980,1,1,0,0,0));item.external_attr=0o100644<<16;item.compress_type=zipfile.ZIP_DEFLATED;z.writestr(item,content)
 with zipfile.ZipFile(target) as z:
  assert set(z.namelist())==set(entries) and all(z.read(n)==v for n,v in entries.items())
 assert sha(source)==identity['input']
 (out/'report.json').write_text(json.dumps(dict(identity=identity,files={'artifacts/runtime/python311.zip':sha(target)},compiled_modules=count,
  original_bytes=source.stat().st_size,compiled_bytes=target.stat().st_size,release_ready=False,
  qualification='只编译公开标准库；不是业务保护，字节码可恢复，完整engine导入另行测试'),ensure_ascii=False,indent=2)+'\n',encoding='utf8')
 print(out);return out
if __name__=='__main__':build()
