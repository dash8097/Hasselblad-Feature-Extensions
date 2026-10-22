"""正式混淆 GUI 的真实 Invoke/JSON/子进程链；仅替换 USB DLL 为离线哨兵。"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import time
from assemble_native_client import ROOT,R7,CSC,encoded,sha
from build_checked_start import ZIG

SENTINEL=r'''
#include <stddef.h>
__declspec(dllexport) void *x2d_call(void *p,int n) {
    (void)p;(void)n;
    return "{\"error\":\"OFFLINE_TRANSPORT_SENTINEL\",\"win32\":0,\"values\":[]}";
}
__declspec(dllexport) void x2d_free(void *p) {(void)p;}
'''
PROBE=r'''
using System;using System.Linq;using System.Reflection;using System.Collections.Generic;
using System.Threading.Tasks;using System.Windows.Forms;using System.Web.Script.Serialization;
class Probe {
 [STAThread] static int Main(string[] a) {
  var asm=Assembly.LoadFile(a[0]);var flags=BindingFlags.Instance|BindingFlags.NonPublic|BindingFlags.Public;
  var type=asm.GetTypes().Single(t=>t.BaseType==typeof(Form));
  using(var form=(Form)Activator.CreateInstance(type,true)) {
   var h=form.Handle;
   var root=type.GetFields(flags).Single(f=>f.FieldType==typeof(string) && Object.Equals(f.GetValue(form),AppDomain.CurrentDomain.BaseDirectory));
   root.SetValue(form,a[1]);
   var mask=type.GetFields(flags).Single(f=>f.FieldType==typeof(int) && Object.Equals(f.GetValue(form),248));
   mask.SetValue(form,int.Parse(a[3]));
   var invoke=type.GetMethods(flags).Single(m=>m.ReturnType==typeof(Task<int>) && m.GetParameters().Length==1 && m.GetParameters()[0].ParameterType==typeof(string));
   var task=(Task<int>)invoke.Invoke(form,new object[]{a[2]});
   var until=DateTime.UtcNow.AddSeconds(90);
   while(!task.IsCompleted && DateTime.UtcNow<until){Application.DoEvents();System.Threading.Thread.Sleep(10);}
   if(!task.IsCompleted)throw new Exception("timeout");
   if(task.Result!=1)throw new Exception("expected sentinel failure");
   Application.DoEvents();
   var reports=type.GetFields(flags).Where(f=>f.FieldType==typeof(Dictionary<string,object>))
       .Select(f=>f.GetValue(form) as Dictionary<string,object>).Where(d=>d!=null).ToArray();
   if(!reports.Any(d=>d.ContainsKey("action") && Convert.ToString(d["action"])==a[2] &&
       d.ContainsKey("reason") && Convert.ToString(d["reason"]).Contains("OFFLINE_TRANSPORT_SENTINEL")))
       throw new Exception("normal request did not reach offline transport");
   Console.WriteLine("PRODUCTION_GUI_REQUEST_OK action="+a[2]+" mask="+a[3]+" usb=0");
  }return 0;
 }
}
'''

def main(package):
    assert Path.cwd().resolve()==ROOT
    package=package.resolve();assert package.is_relative_to(R7)
    out=R7/'native-host/gui-request-tests'/str(time.time_ns())
    out.mkdir(parents=True)
    work=out/'fixture';shutil.copytree(package,work)
    (out/'sentinel.c').write_text(SENTINEL,encoding='utf8')
    (out/'Probe.cs').write_text(PROBE,encoding='utf8')
    env=dict(os.environ,TEMP=str(out),TMP=str(out),ZIG_LOCAL_CACHE_DIR=str(out/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(out/'zig-global'))
    def run(name,args):
        r=subprocess.run([str(a) for a in args],env=env,capture_output=True,timeout=120)
        (out/(name+'.log')).write_bytes(r.stdout+r.stderr)
        assert r.returncode==0,(name,r.stdout,r.stderr)
        return r
    run('compile-sentinel',[ZIG,'cc','-target','x86_64-windows-gnu','-shared','-O2',out/'sentinel.c','-o',work/'X2dTransport.dll'])
    run('compile-probe',[CSC,'/nologo','/platform:x64','/reference:System.Core.dll',
        '/reference:System.Windows.Forms.dll','/reference:System.Web.Extensions.dll','/out:'+str(out/'Probe.exe'),out/'Probe.cs'])
    cases=[]
    for mask in (248,255):
        for action in ('Install','Uninstall','Diagnose'):
            r=run(action+str(mask),[out/'Probe.exe',work/'HasselbladTuningPatchInstaller.exe',work,action,mask])
            cases.append(dict(action=action,mask=mask,passed=True,output=r.stdout.decode('utf8')))
    unchanged={p.relative_to(package).as_posix():sha(p) for p in package.rglob('*') if p.is_file() and p.name!='X2dTransport.dll'}
    assert all(sha(work/n)==h for n,h in unchanged.items())
    (out/'report.json').write_bytes(encoded(dict(passed=True,cases=cases,camera_requests=0,
        final_exe_sha256=sha(package/'HasselbladTuningPatchInstaller.exe'),
        fixture='only USB transport DLL replaced, production GUI/engine/pyd unchanged',
        limitation='调用正式GUI业务函数的离线链路测试；真实前台/实机另验')))
    print(out)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('package',type=Path);main(p.parse_args().package)
