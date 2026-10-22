"""只编译公开标准库，保持 Mac 解释器二进制；不含项目业务源码。"""
import hashlib,json,os,subprocess,zipfile
from pathlib import Path
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
R7=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7'
LEGACY=ROOT/'x2d/outputs/4.2.0/installer-hardening-r3/internal-candidates/mac-installer/哈苏特调补丁.app/Contents/Resources'
ZIP=R7/'native-host/dependencies/python-3.12.10-embed-amd64.zip'
COMPILER_SHA='4acbed6dd1c744b0376e3b1cf57ce906f9dc9e95e68824584c8099a63025a3c3'
DROP={'venv','idlelib','turtledemo','tkinter','ensurepip'}
HELPER=r'''
import sys,json,py_compile,importlib.util,marshal,types
from pathlib import Path
assert sys.version_info[:2]==(3,12) and importlib.util.MAGIC_NUMBER==bytes.fromhex('cb0d0d0a')
spec=json.loads(Path(sys.argv[1]).read_text(encoding='utf8'))
for row in spec:
 target=Path(row['target']);target.parent.mkdir(parents=True,exist_ok=True)
 py_compile.compile(row['source'],cfile=str(target),dfile='stdlib:'+row['relative'],doraise=True,invalidation_mode=py_compile.PycInvalidationMode.CHECKED_HASH)
 data=target.read_bytes();assert data[:4]==importlib.util.MAGIC_NUMBER
 code=marshal.loads(data[16:]);assert isinstance(code,types.CodeType) and code.co_filename=='stdlib:'+row['relative']
print('STANDARD_LIBRARY_COMPILED',len(spec),importlib.util.MAGIC_NUMBER.hex())
'''
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def build():
 assert Path.cwd().resolve()==ROOT and sha(ZIP)==COMPILER_SHA
 manifest=json.loads((LEGACY/'package-integrity.json').read_text(encoding='utf-8-sig'))
 names=sorted(n for n in manifest if n.startswith('runtime/'))
 assert all(sha(LEGACY/n)==manifest[n] for n in names)
 identity=dict(builder=sha(Path(__file__)),compiler=COMPILER_SHA,input={n:manifest[n] for n in names},removed_packages=sorted(DROP))
 rev=hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20];out=R7/'native-host/mac-runtime'/rev
 if (out/'report.json').exists():
  old=json.loads((out/'report.json').read_text(encoding='utf8'));assert old['identity']==identity
  assert all(sha(out/n)==h for n,h in old['files'].items());print(out);return out
 assert not out.exists(),'保留未完成现场'
 private=out/'private-build';private.mkdir(parents=True);compiler=private/'compiler';compiler.mkdir()
 with zipfile.ZipFile(ZIP) as z:
  for item in z.infolist():
   rel=Path(item.filename);assert not rel.is_absolute() and '..' not in rel.parts and '\\' not in item.filename
   if item.is_dir():continue
   dest=compiler/rel;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(z.read(item))
 helper=private/'compile_stdlib.py';helper.write_text(HELPER,encoding='utf8');spec=[];removed=[];copied=[]
 for n in names:
  rel=Path(n);parts=rel.parts
  library=len(parts)>4 and parts[2:4]==('lib','python3.12')
  if library and (parts[4] in DROP or '/'.join(parts[4:]) in ('ctypes/macholib/fetch_macholib','ctypes/macholib/fetch_macholib.bat')):
   removed.append(n);continue
  src=LEGACY/n
  if src.suffix=='.py':
   assert library
   dest=out/'artifacts'/rel.with_suffix('.pyc')
   spec.append(dict(source=str(src),target=str(dest),relative='/'.join(parts[4:])))
  else:
   assert src.suffix.lower() not in {'.pyw','.ps1','.bat','.cmd','.sh','.csh','.fish'},'未分类脚本:'+n
   assert not src.read_bytes().startswith(b'#!'),'未分类shebang:'+n
   dest=out/'artifacts'/rel;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(src.read_bytes());copied.append(n)
 spec_path=private/'spec.json';spec_path.write_text(json.dumps(spec),encoding='utf8')
 env=dict(os.environ,TEMP=str(private),TMP=str(private),PYTHONDONTWRITEBYTECODE='1')
 p=subprocess.run([str(compiler/'python.exe'),'-I','-B',str(helper),str(spec_path)],env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=120)
 (private/'compile.log').write_text(p.stdout+p.stderr,encoding='utf8');assert not p.returncode,p.stdout+p.stderr
 files={p.relative_to(out).as_posix():sha(p) for p in (out/'artifacts').rglob('*') if p.is_file()}
 assert all(sha(LEGACY/n)==manifest[n] for n in names),'输入构建中变化'
 report=dict(identity=identity,files=files,removed=removed,copied=copied,compiled_modules=len(spec),compiler_stdout=p.stdout,
  compiler_source='https://www.python.org/downloads/release/python-31210/',mac_runtime_replaced=False,
  qualification='仅离线编译公开标准库和逐pyc解码；标准库字节码仍可反编译，不是业务保护。Mac运行未测。',mac_executed=False,release_ready=False)
 (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8');print(out);print(p.stdout);return out
if __name__=='__main__':build()
