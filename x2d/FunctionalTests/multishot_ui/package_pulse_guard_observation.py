"""经正常访问检查只读本包helper的固定自有保护状态；拒绝时不换权限或读取路径。"""
import argparse,base64,hashlib,json,re,struct,sys,time
from pathlib import Path
from package_ui import ROOT,OUT,PACKAGE,PACKAGE_ID,transport_lock

HELPER_SHA='e7a58b337db223d6784e864dab56535c46f8d00d819761e93069578435f19324'
GUARD_OFFSET=0x32050

def main():
 assert sys.version_info[:2]==(3,11) and Path.cwd().resolve()==ROOT
 p=argparse.ArgumentParser();p.add_argument('tag');a=p.parse_args();assert re.fullmatch('[A-Za-z0-9_-]+',a.tag)
 target=OUT/(a.tag+'.json');assert not target.exists()
 artifact=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/esflash/c8b36b63ce33d46eb9a9/libx2d_esflash_hotshoe.so'
 assert hashlib.sha256(artifact.read_bytes()).hexdigest()==HELPER_SHA
 sys.path.insert(0,str(ROOT/'x2d/tools'));from firmware_image import FirmwareElf
 import capstone
 e=FirmwareElf(artifact.read_bytes());c=capstone.Cs(capstone.CS_ARCH_ARM64,capstone.CS_MODE_LITTLE_ENDIAN)
 decoded=[(x.mnemonic,x.op_str) for x in c.disasm(e.read(0x1149c,8),0x1149c)]
 assert decoded==[('adrp','x19, #0x32000'),('ldr','x9, [x19, #0x50]')]
 sys.path.insert(0,str(PACKAGE/'engine'));import windows_engine
 r=dict(package=PACKAGE_ID,time=time.time(),helper_sha256=HELPER_SHA,source='installed-helper-fixed-own-guard',memory_writes=0,access_policy_changes=0,photo_contents_read=False)
 with transport_lock():
  w=windows_engine.Transport(PACKAGE);one=lambda command:w.batch([command])[0].strip()
  ids=one('for p in /proc/[0-9]*;do test "$(readlink "$p/exe")" = /system/bin/x2d-esflash && echo "${p##*/}";done;true').split()
  assert 1<=len(ids)<=2 and all(v.isdigit() for v in ids)
  parents={v:one('grep ^PPid: /proc/'+v+'/status').split()[-1] for v in ids}
  owners=[v for v in ids if parents[v] not in ids];assert len(owners)==1;pid=owners[0];r['pid']=int(pid)
  assert one('sha256sum /system/lib64/libx2d_esflash_hotshoe.so').split()[0]==HELPER_SHA
  mappings=one('cat /proc/'+pid+'/maps').splitlines()
  own=[line.split() for line in mappings if line.endswith('/system/lib64/libx2d_esflash_hotshoe.so')]
  r['helper_mapped']=bool(own)
  if not own:
   assert len(ids)==1
   r['cleanup_observed']='helper-unmapped-and-no-guardian'
   r['memory_read_attempted']=False
   target.write_text(json.dumps(r,ensure_ascii=False,indent=2)+'\n',encoding='utf8');print(json.dumps(r,ensure_ascii=False));return
  zero=[v for v in own if int(v[2],16)==0];assert len(zero)==1
  bias=int(zero[0][0].split('-')[0],16);slot=bias+GUARD_OFFSET
  assert any(v[1].startswith('rw') and int(v[0].split('-')[0],16)<=slot and slot+32<=int(v[0].split('-')[1],16) for v in own)
  def read_own(address,size):
   assert size in (16,32)
   reply=one('/system/xbin/busybox dd if=/proc/'+pid+'/mem bs=1 skip='+str(address)+' count='+str(size)+' 2>/dev/null | /system/xbin/busybox base64')
   if not reply:return None
   data=base64.b64decode(''.join(reply.split()),validate=True);assert len(data)==size;return data
  raw=read_own(slot,32);r['read_permitted']=raw is not None
  if raw is not None:
   guard=struct.unpack_from('<Q',raw)[0];guard_pid=struct.unpack_from('<I',raw,24)[0]
   assert str(guard_pid) in ids and parents[str(guard_pid)]==pid
   # Only the helper's 4096-byte shared anonymous guard, never the MMIO pointer.
   rows=[line.split() for line in mappings]
   ranges=[v for v in rows if v[1].startswith('rw') and int(v[0].split('-')[0],16)<=guard and guard+16<=int(v[0].split('-')[1],16) and int(v[0].split('-')[1],16)-int(v[0].split('-')[0],16)==4096 and (len(v)==5 or ' '.join(v[5:])=='/dev/zero (deleted)')]
   assert len(ranges)==1,'Guard mapping not the expected own anonymous page'
   raw=read_own(guard,16);r['guard_read_permitted']=raw is not None
   if raw is not None:
    flags=dict(zip(('ready','stop','active','fault'),struct.unpack('<4I',raw)));assert all(v in (0,1) for v in flags.values());r['guard']=flags;r['guardian_pid']=guard_pid
 target.write_text(json.dumps(r,ensure_ascii=False,indent=2)+'\n',encoding='utf8');print(json.dumps(r,ensure_ascii=False))

if __name__=='__main__':main()
