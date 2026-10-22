"""原厂后台工人适配源的替身验证，复用真实四线程CFA核心。"""
from pathlib import Path
import hashlib,json,os,subprocess
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[2]
OUT=ROOT/'x2d/outputs/4.2.0/pixel-shift-rgb/quarter-native-output-r1'
ZIG=ROOT/'.research-cache/x1d-1.25.0/toolchain/zig-windows-x86_64-0.13.0/zig.exe'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 env=dict(os.environ,TEMP=str(OUT),TMP=str(OUT),ZIG_LOCAL_CACHE_DIR=str(OUT/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(OUT/'zig-global'))
 wrapper=(HERE/'check_quarter_native_output.c').read_text(encoding='utf-8').split('static void put_ifd')[0]
 p=OUT/'mock-core.c';p.write_text(wrapper,encoding='utf-8')
 flags=['-O2','-Wall','-Wextra','-Werror','-Wno-unused-function','-I'+str(HERE),'-I'+str(OUT)]
 objects=[]
 for source in [p,HERE/'quarter_memory_file.c']:
  obj=OUT/(source.stem+'-mock.o');subprocess.run([str(ZIG),'cc',*flags,'-c',str(source),'-o',str(obj)],env=env,check=True);objects.append(str(obj))
 exe=OUT/'quarter-storage-check.exe'
 subprocess.run([str(ZIG),'c++',*flags,'-std=c++17',str(HERE/'check_quarter_native_storage.cpp'),*objects,'-o',str(exe)],env=env,check=True)
 result=subprocess.run([str(exe)],capture_output=True,text=True);print(result.stdout.strip());print(result.stderr.strip());result.check_returncode()
 build=json.loads((OUT/'storage-build.json').read_text(encoding='utf-8'))
 for n,h in build['source_hashes'].items():assert sha(HERE/n)==h
 (OUT/'storage-validation.json').write_text(json.dumps(dict(passed=True,device_requests=0,offline_only=True,
  source_hashes=build['source_hashes'],module_sha256=build['file_sha256'],result=result.stdout.strip(),
  source_buffers_and_notification_unchanged=True,native_next_work_overlaps_quarter_encode=True,
  codec_mock=True,hardware_codec_not_tested=True),indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
