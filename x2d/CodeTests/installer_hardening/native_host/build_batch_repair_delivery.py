"""从整包验收基线重建本轮修正，保留其拍摄依赖和完整卸载摘要。"""
import argparse,contextlib,hashlib,io,json,re,sys
from pathlib import Path
from assemble_native_client import ROOT,R7,encoded,sha
from build_album_stream_delivery import archive
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE.parent/'camera_native'))
import build_flow,inspect_scriptless_payload

def digest(b):return hashlib.sha256(b).hexdigest()
def frozen(base):
 d=json.loads((base/'build-proof.json').read_text(encoding='utf8'))
 assert sha(base/'artifacts/x2d-native-flow')==d['artifact']['sha256']
 for n,h in d['source_sha256'].items():assert sha(base/'private-build/source'/n)==h,n
 return d

def compile_flow(base,contract,profile,payload=None):
 d=frozen(base);work=R7/'native-host/batch-repair-inputs'/digest(encoded([sha(Path(__file__)),str(base),sha(contract)]))[:20]
 work.mkdir(parents=True,exist_ok=True)
 for n in d['source_sha256']:
  data=(base/'private-build/source'/n).read_bytes()
  if n=='native_disk_runner.cpp':data=(HERE.parent/'camera_native'/n).read_bytes()
  if n=='native_data_cleanup.cpp':
   # 旧包追加过实机历史库摘要，必须保留，不以当前基础表覆盖。
   old=data.decode('utf8');new=(HERE.parent/'camera_native'/n).read_text(encoding='utf8')
   begin='        static const LooseFile old_libraries[]={';end='        };'
   a=old.index(begin);b=old.index(end,a)
   x=new.index(begin);y=new.index(end,x)
   new=new[:x]+old[a:b]+new[y:];data=new.encode('utf8')
  if profile=='setup' and n=='legacy_cleanup_stages.hpp':
   text=data.decode('utf8');marker='static const char*legacy_helper_hashes[]={'
   assert text.count(marker)==1
   for previous in ('86575372e58b27ea252d','46680c41fa0dacdaec37'):
    old=R7/'native-flow/revisions'/previous;h=frozen(old)['artifact']['sha256']
    if '"'+h+'"' not in text:text=text.replace(marker,marker+'\n"'+h+'",')
   data=text.encode('utf8')
  (work/n).write_bytes(data)
 old_input,old_argv=build_flow.input_path,sys.argv
 build_flow.input_path=lambda n:work/n
 sys.argv=['build_flow','--profile',profile,'--hide-literals','--native-ui-contract',str(contract)]
 if payload:sys.argv+=['--payload',str(payload)]
 try:
  buf=io.StringIO()
  with contextlib.redirect_stdout(buf):build_flow.main()
 finally:build_flow.input_path,sys.argv=old_input,old_argv
 r=json.loads(buf.getvalue().strip().splitlines()[-1]);out=Path(r.get('build_directory') or r['reused_verified_build'])
 print('FLOW',profile,out,flush=True);return out

def payload(base,runtime,contract,all_runtime):
 old=json.loads((base/'report.json').read_text(encoding='utf8'));files={}
 for n,h in old['files'].items():assert sha(base/n)==h;files[n]=(base/n).read_bytes()
 m=json.loads(files['manifest.json']);ui=json.loads(contract.read_text(encoding='utf8'))
 replacements={'/system/bin/x2d-native-flow':runtime/'artifacts/x2d-native-flow',ui['module_path']:ROOT/ui['artifact']}
 changes=[];oldtotal=sum(r['bytes'] for r in m['targets'])
 for r in m['targets']:
  if r['path'] in replacements:
   data=replacements[r['path']].read_bytes();changes.append((r['path'],r['candidate_sha256'],digest(data)))
   r['candidate_sha256']=digest(data);r['bytes']=len(data);files['private-payload/install/'+r['name']]=data
 assert len(changes)==2
 m['runtime_sha256']=sha(runtime/'artifacts/x2d-native-flow');m['native_ui']={k:ui[k] for k in ('schema','module_path','module_sha256','gui_sha256','required_capabilities','eye_protocol')};m['hardware_validated']=False
 oldmanifest=files['manifest.json'];files['manifest.json']=encoded(m)
 oldtargets=files['private-payload/uninstall/targets.txt'];lines=[]
 for line in oldtargets.decode().splitlines():
  fields=line.split();allowed=fields[3].split(',')
  extra=all_runtime if fields[1]=='/system/bin/x2d-native-flow' else [ui['module_sha256']] if fields[1]==ui['module_path'] else []
  for h in extra:
   if h not in allowed:allowed.append(h)
  fields[3]=','.join(allowed);lines.append(' '.join(fields))
 files['private-payload/uninstall/targets.txt']=('\n'.join(lines)+'\n').encode()
 h=files['include/native_install_tables.hpp'].decode()
 for path,oldh,newh in changes:
  assert oldh in h;h=h.replace(oldh,newh)
 h=h.replace(digest(oldmanifest),digest(files['manifest.json'])).replace('payload_bytes='+str(oldtotal)+'ULL','payload_bytes='+str(sum(r['bytes'] for r in m['targets']))+'ULL')
 files['include/native_install_tables.hpp']=h.encode()
 h=files['include/native_setup_tables.hpp'].decode()
 for before,after in zip(oldtargets.decode().splitlines(),lines):
  a=before.split();b=after.split()
  if a[3]!=b[3]:
   matches=[line for line in h.splitlines() if '"'+a[1]+'"' in line];assert len(matches)==1
   row=matches[0];parts=re.findall(r'"([^"\n]*)"',row);assert len(parts)==4 and parts[:3]==a[:3]
   allowed=parts[3].strip(',').split(',')
   for value in b[3].split(','):
    if value not in allowed:allowed.append(value)
   revised=row.replace('"'+parts[3]+'"','",'+','.join(allowed)+',"')
   h=h.replace(row,revised)
 h=h.replace(digest(oldtargets),digest(files['private-payload/uninstall/targets.txt']));files['include/native_setup_tables.hpp']=h.encode()
 groups={kind:{n.rsplit('/',1)[1]:b for n,b in files.items() if n.startswith('private-payload/'+kind+'/')} for kind in ('install','uninstall')}
 header=['#pragma once','#include "native_unpack.hpp"','#ifndef CN_UNPACK_DECODER_SHA256','#define CN_UNPACK_DECODER_SHA256 '+json.dumps(old['decoder_static_evidence']['sha256']),'#endif','namespace cn { namespace unpack {']
 for kind,items in groups.items():
  an='native-'+kind+'.gz';files[an]=archive(items)
  header+=['static constexpr File '+kind+'_files[]={']
  header+=['{'+json.dumps(n)+','+json.dumps(digest(b))+','+str(len(b))+'ULL,0644},' for n,b in sorted(items.items())]
  header+=['};','static constexpr Bundle '+kind+'_bundle={'+json.dumps(an)+','+json.dumps(digest(files[an]))+','+str(len(files[an]))+'ULL,'+kind+'_files,sizeof('+kind+'_files)/sizeof('+kind+'_files[0])};']
 files['include/native_unpack_tables.hpp']=('\n'.join(header+['} }',''])).encode()
 inspection=inspect_scriptless_payload.inspect(m,files['native-install.gz']);assert inspection['structural_pass'],inspection
 files['scriptless-inspection.json']=encoded(inspection)
 identity=dict(builder=sha(Path(__file__)),base_report=sha(base/'report.json'),runtime_proof=sha(runtime/'build-proof.json'),ui_contract=sha(contract),manifest_sha256=digest(files['manifest.json']))
 files['identity.json']=encoded(identity)
 report=dict(old,identity=identity,native_ui_contract=ui,hardware_executed=False,release_ready=False,files={n:digest(b) for n,b in files.items()})
 out=R7/'native-payload/revisions'/digest(encoded(identity))[:20];files['report.json']=encoded(report)
 for n,b in files.items():
  p=out/n;p.parent.mkdir(parents=True,exist_ok=True)
  if p.exists():assert p.read_bytes()==b
  else:p.write_bytes(b)
 print('PAYLOAD',out,flush=True);return out

def main():
 assert Path.cwd().resolve()==ROOT
 p=argparse.ArgumentParser();p.add_argument('ui',type=Path);a=p.parse_args();ui=a.ui.resolve()
 pairs=[]
 for mask,r,p,s,c in [(248,'7d3c799fa3cfb9b28deb','0778882a6dc8e71c0329','86575372e58b27ea252d','migration-acceptance-basic'),(255,'0be8ec786e033884d9ef','1f1cd240d70cfb0fb0c1','46680c41fa0dacdaec37','migration-acceptance')]:
  contract=ui/c/'native-ui-contract.json';runtime=compile_flow(R7/'native-flow/revisions'/r,contract,'runtime');pairs.append((mask,runtime,p,s,contract))
 hashes=[sha(r/'artifacts/x2d-native-flow') for _,r,_,_,_ in pairs];result=[]
 for mask,r,p,s,c in pairs:
  pl=payload(R7/'native-payload/revisions'/p,r,c,hashes);setup=compile_flow(R7/'native-flow/revisions'/s,c,'setup',pl);result.append(dict(mask=mask,runtime=str(r),payload=str(pl),setup=str(setup)))
 out=R7/'native-host/batch-repair-inputs'/sha(Path(__file__))[:20];out.mkdir(exist_ok=True,parents=True);(out/'report.json').write_bytes(encoded(dict(profiles=result,camera_operations=0,hardware_accepted=False)))
 print(out/'report.json',flush=True)
if __name__=='__main__':main()
