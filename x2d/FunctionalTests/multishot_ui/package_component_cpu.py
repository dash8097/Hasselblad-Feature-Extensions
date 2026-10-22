"""在实拍前后读取当前包自有进程CPU计数；不修改采样器或相机设置。"""
import argparse,json,re,sys,time
from package_ui import ROOT,OUT,PACKAGE,PACKAGE_ID,transport_lock,read_status,idle_ui_allowed

def main():
    assert sys.version_info[:2]==(3,11) and ROOT==__import__('pathlib').Path.cwd().resolve()
    p=argparse.ArgumentParser();p.add_argument('tag');a=p.parse_args();assert re.fullmatch('[A-Za-z0-9_-]+',a.tag)
    path=OUT/(a.tag+'.json');assert not path.exists()
    host=json.loads((OUT/(PACKAGE_ID[:4]+'-service-host.json')).read_text(encoding='utf8'))
    loaded=json.loads((OUT/(PACKAGE_ID+'-loaded-reboot-check.json')).read_text(encoding='utf8'))
    assert host['package']==PACKAGE_ID==loaded['package']
    ids={'camera-service':host['camera_service_pid'],'camera-gui':loaded['processes']['camera-gui']['pid'],'flash-worker':host['pid'],'flash-guardian':host['guardian_pid']}
    sys.path.insert(0,str(PACKAGE/'engine'));import windows_engine
    result={'package':PACKAGE_ID,'time':time.time(),'processes':{},'photo_contents_read':False}
    with transport_lock():
        w=windows_engine.Transport(PACKAGE);one=lambda c:w.batch([c])[0].strip()
        assert idle_ui_allowed(read_status(one))
        cpu=one('cat /proc/stat').splitlines()
        total=next(l for l in cpu if l.startswith('cpu ')).split()[1:9]
        result['cpu_total_ticks']=sum(map(int,total));result['cpu_count']=sum(bool(re.match(r'^cpu[0-9]+ ',l)) for l in cpu)
        for name,pid in ids.items():
            assert type(pid) is int and pid>0
            exe=one('readlink /proc/'+str(pid)+'/exe')
            assert exe=='/system/bin/'+('x2d-esflash' if name.startswith('flash-') else name)
            stat=one('cat /proc/'+str(pid)+'/stat');fields=stat[stat.rindex(')')+2:].split()
            result['processes'][name]={'pid':pid,'user_ticks':int(fields[11]),'system_ticks':int(fields[12]),'threads':int(fields[17]),'start_ticks':int(fields[19])}
        for core in (0,4):
            result['cpu'+str(core)+'_khz']=one('cat /sys/devices/system/cpu/cpu'+str(core)+'/cpufreq/scaling_cur_freq 2>/dev/null || echo UNAVAILABLE')
    path.write_text(json.dumps(result,indent=2),encoding='utf8');print(json.dumps(result))

if __name__=='__main__':main()
