"""固定白名单实机状态；仅经当前完整包读取，不读取照片或完整系统日志。"""
import argparse,json,sys,time
from pathlib import Path
from package_ui import ROOT,OUT,PACKAGE,PACKAGE_ID,transport_lock,read_status

def run():
    assert sys.version_info[:2]==(3,11)
    assert Path.cwd().resolve()==ROOT
    ap=argparse.ArgumentParser();ap.add_argument('tag');ap.add_argument('--ready',action='store_true');a=ap.parse_args()
    assert a.tag.replace('-','').replace('_','').isalnum()
    target=OUT/(a.tag+'.json');assert not target.exists()
    sys.path.insert(0,str(PACKAGE/'engine'));import windows_engine
    w=windows_engine.Transport(PACKAGE)
    i=windows_engine.mac_patch.Installer(PACKAGE,run_batch=w.batch)
    r={'package':PACKAGE_ID,'time':time.time()}
    for n in ('drive_mode','exit_drive_mode','exposure_status','image_bit_depth','image_format','tv_current','live_view_state','focus_mode','face_detection','exp_mode','eshutter_current'):
        r[n]=i.property('camera',n)
    r['recognition_options']={n:i.property('system',n) for n in ('debug_mode','debug_options')}
    r['active_error']=i.property('error','active_error')
    for k,c in (
        ('ui','cat /tmp/x2d-pixel-overclock/ui.txt'),
        ('mount','awk \'$2=="/system"{print $4}\' /proc/mounts'),
        ('shutter','cat /tmp/x2d-pixel-overclock/shutter-status.json'),
        ('status',None)):
        s=w.batch([c])[0].strip() if c else ''
        if k=='status':
            v=read_status(lambda c:w.batch([c])[0].strip())
            # Never retain album/photo metadata. Keep only known task-state keys.
            s={n:v[n] for n in ('state','busy','ready','error','stage','frames','stack_frames','stack_gap_ms','stack_remaining','stack_frame_count','session','ack','prepared','completed','total','current','phase','lock_released') if n in v}
        r[k]=s
    if a.ready:r['ready_probe']=w.batch(['/system/bin/x2d-native-flow ui-ready; r=$?; echo PROBE_EXIT=$r; true'])[0].strip()
    target.write_text(json.dumps(r,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps(r,ensure_ascii=False))

def main():
    with transport_lock():run()

if __name__=='__main__':main()
