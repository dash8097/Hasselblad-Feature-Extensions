"""可重复的25MP直编离线验收；不加载到相机，不制作安装器。"""
from pathlib import Path
import hashlib,json,subprocess,sys,tempfile
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE))
import build_quarter_upstream as build
def run(args):
 p=subprocess.run([str(a) for a in args],env=build.environment(),check=True,capture_output=True,text=True)
 print(p.stdout.strip());return p.stdout.strip()
def main():
 build.main();out=build.OUT
 flags=['-O2','-Wall','-Wextra','-Werror','-Wno-unused-function','-I'+str(HERE)]
 core=['quarter_upstream.c','quarter_direct_jpeg.c','quarter_single_core.c']
 exe=out/'upstream-check.exe'
 run([build.ZIG,'cc',*flags,*[HERE/n for n in core],HERE/'check_quarter_upstream.c','-o',exe])
 host=run([exe])
 import check_quarter_upstream_arm as arm
 arm.main()
 import check_quarter_native_storage as storage
 storage.OUT=out;storage.main()
 # 原生产C函数裁去编码器对齐行，真实JPEG解码后逐像素比较，排除二次编码。
 codec=out/'direct-jpeg-check.exe'
 run([build.ZIG,'cc',*flags,HERE/'quarter_direct_jpeg.c',HERE/'quarter_single_core.c',HERE/'check_quarter_direct_jpeg.c','-o',codec])
 fullimage=run([codec])
 from PIL import Image
 from io import BytesIO
 import numpy as np
 x=np.arange(5828,dtype=np.uint32)[None,:];y=np.arange(4372,dtype=np.uint32)[:,None]
 rgb=np.empty((4372,5828,3),dtype=np.uint8)
 rgb[:,:,0]=(3*x+5*y)%256;rgb[:,:,1]=(x+7*y)%256;rgb[:,:,2]=(9*x+y)%256;rgb[-1]=rgb[-2]
 bio=BytesIO();Image.fromarray(rgb).save(bio,format='JPEG',quality=80,subsampling=1);raw=bio.getvalue()
 with tempfile.TemporaryDirectory(prefix='synthetic-only-',dir=out) as d:
  before=Path(d)/'input.jpg';after=Path(d)/'output.jpg';before.write_bytes(raw)
  run([codec,before,after]);patched=after.read_bytes()
 assert len(raw)==len(patched) and sum(a!=b for a,b in zip(raw,patched))<=2
 first=Image.open(BytesIO(raw));first.load();last=Image.open(BytesIO(patched));last.load()
 assert last.size==(5828,4371) and np.array_equal(np.asarray(first)[:4371],np.asarray(last))
 manifest=json.loads((out/'upstream-build.json').read_text())
 for n,h in manifest['source_hashes'].items():assert build.digest(HERE/n)==h
 names=core+['quarter_upstream_native.c','quarter_upstream.h','quarter_direct_jpeg.h','quarter_single_core.h','factory_jpeg_layout.h',
  'quarter_native_storage.cpp','quarter_native_jpeg.h','quarter_decision_server.inc','quarter_decision_wire.h','capture_ion_reference.h',
  'check_quarter_upstream.c','check_quarter_upstream_arm.py','check_quarter_upstream.py','build_quarter_upstream.py',
  'build_quarter_native_storage.py','check_quarter_native_storage.cpp','check_quarter_native_storage.py','check_quarter_direct_jpeg.c']
 report=dict(passed=True,offline=True,device_requests=0,installed=False,installer_modified=False,
  actual_factory_plugin_and_candidate_abi=True,host_result=host,fullimage_result=fullimage,
  jpeg_quality=80,software_jpeg_decode_pixels_equal=True,source_hashes={n:build.digest(HERE/n) for n in names},
  modules={n:build.digest(out/n) for n in ['libx2d_quarter_upstream.so','libx2d_quarter_native.so']},
  limitations=['HAL、ION、系统调用为替身，不是完整相机模拟器',
   '上游需在拍摄线程启动前且原厂插件加载后显式初始化；未接入现有安装器或启动脚本',
   'RAW回调已映射且存储逐拍快照已就绪才直编，否则继续原厂编码及存储缩小路径',
   '硬件编码尺寸、真实回调时序、颜色、资源峰值和耗时尚待实机验证'])
 (out/'upstream-validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 print('QUARTER_UPSTREAM_OFFLINE_VALIDATED_NO_CAMERA_OPERATION')
if __name__=='__main__':main()
