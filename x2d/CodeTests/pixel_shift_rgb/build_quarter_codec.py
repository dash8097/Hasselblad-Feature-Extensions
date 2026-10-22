"""构建本地已有libjpeg-turbo 2.1.5.1的私有PIC编解码器，ARM64 NEON。"""
from pathlib import Path
import hashlib,json,os,re,subprocess
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[2]
SRC=ROOT/'x2d/outputs/4.2.0/pixel-shift-rgb/jpeg-turbo-roi/source'
ZIG=ROOT/'.research-cache/x1d-1.25.0/toolchain/zig-windows-x86_64-0.13.0/zig.exe'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def build(platform):
 assert platform in ('host','arm')
 names=re.search(r'set\(JPEG_SOURCES (.*?)\)',(SRC/'CMakeLists.txt').read_text(),re.S).group(1).split()
 files=[SRC/n for n in names]
 cfgsource=SRC.parent/platform
 if platform=='arm':files += [SRC/'simd/arm'/n for n in ['jcgray-neon.c','jcphuff-neon.c','jcsample-neon.c','jdmerge-neon.c','jdsample-neon.c','jfdctfst-neon.c','jidctred-neon.c','jquanti-neon.c','jccolor-neon.c','jidctint-neon.c','jidctfst-neon.c','aarch64/jchuff-neon.c','jdcolor-neon.c','jfdctint-neon.c','aarch64/jsimd.c']]
 else:files += [SRC/'jsimd_none.c']
 configs=['jconfig.h','jconfigint.h','jversion.h']+(['neon-compat.h'] if platform=='arm' else [])
 identity=dict(platform=platform,builder=sha(Path(__file__)),sources={str(f.relative_to(SRC)):sha(f) for f in SRC.rglob('*') if f.is_file() and f.suffix in ('.h','.c','.in')},config={n:sha(cfgsource/n) for n in configs})
 rev=hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20];out=ROOT/'x2d/outputs/4.2.0/raw-stack/continuous-candidate/jpeg-codec'/rev;out.mkdir(parents=True,exist_ok=True)
 for n in configs:(out/n).write_bytes((cfgsource/n).read_bytes())
 archive=out/'libquarterjpeg.a';proof=out/'build.json'
 if proof.exists():assert json.loads(proof.read_text())['sha256']==sha(archive);return archive,out,SRC,proof
 env=dict(os.environ,TEMP=str(out),TMP=str(out),ZIG_LOCAL_CACHE_DIR=str(out/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(out/'zig-global'))
 flags=['-O3','-fPIC','-fvisibility=hidden','-ffunction-sections','-fdata-sections','-I'+str(out),'-I'+str(SRC),'-I'+str(SRC/'simd'),'-I'+str(SRC/'simd/arm')]
 if platform=='arm':flags+=['-target','aarch64-linux-gnu','-D__errno_location=__errno','-DNEON_INTRINSICS','-DNO_GETENV','-include','errno.h','-mno-outline-atomics','-fno-stack-protector']
 # 固定ARM64 NEON能力，避免旧版Bionic linker不支持的TLSDESC；常量无跨线程写竞争。
 if platform=='arm':
  original=SRC/'simd/arm/aarch64/jsimd.c';text=original.read_text()
  text=text.replace('static THREAD_LOCAL unsigned int simd_support = ~0;', 'static const unsigned int simd_support = JSIMD_NEON;')
  text=text.replace('static THREAD_LOCAL unsigned int simd_huffman = 1;', 'static const unsigned int simd_huffman = 1;')
  text=text.replace('static THREAD_LOCAL unsigned int simd_features =', 'static const unsigned int simd_features =')
  start=text.index('LOCAL(void)\ninit_simd(void)\n{');end=text.index('\nGLOBAL(int)',start)
  text=text[:start]+'LOCAL(void)\ninit_simd(void) {}\n'+text[end:]
  text=re.sub(r'#include "([^"\n]+)"',lambda m:'#include "'+str((original.parent/m.group(1)).resolve()).replace('\\','/')+'"',text)
  fixed=out/'jsimd-fixed.c';fixed.write_text(text,encoding='utf8');files=[fixed if f==original else f for f in files]
 objects=[]
 for i,f in enumerate(files):
  obj=out/(str(i)+'.o');r=subprocess.run([str(ZIG),'cc',*flags,*(['-U__linux__'] if platform=='arm' and f.name=='jsimd-fixed.c' else []),'-c',str(f),'-o',str(obj)],env=env,capture_output=True,text=True);assert not r.returncode,r.stderr;objects.append(str(obj))
 subprocess.run([str(ZIG),'ar','rcs',str(archive),*objects],check=True,env=env)
 proof.write_text(json.dumps(dict(identity=identity,sha256=sha(archive)),indent=2),encoding='utf8');return archive,out,SRC,proof
if __name__=='__main__':
 import sys
 print(build(sys.argv[1]))
