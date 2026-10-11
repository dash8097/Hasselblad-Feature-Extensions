"""运行明确列出的主机组件回归；所有输入均由测试创建。"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--compiler',required=True,help='Zig 0.13 executable')
    ap.add_argument('--output',required=True,type=Path)
    args=ap.parse_args()
    out=args.output.resolve()
    if out==ROOT or out in ROOT.parents or (ROOT in out.parents and ROOT/'outputs' not in out.parents and out!=ROOT/'outputs'):
        ap.error('Use outputs/ or a dedicated directory outside the source tree')
    out.mkdir(parents=True,exist_ok=True)
    tmp=out/'tmp';tmp.mkdir(exist_ok=True)
    env=dict(os.environ,PYTHONDONTWRITEBYTECODE='1',TEMP=str(tmp),TMP=str(tmp),
             ZIG_LOCAL_CACHE_DIR=str(out/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(out/'zig-global'))
    cases=json.loads((ROOT/'scripts/component_tests.json').read_text(encoding='utf-8'))
    results=[]
    def save():
        (out/'results.json').write_text(json.dumps({'passed':len(results)==len(cases) and all(r['passed'] for r in results),'cases':results},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    save()
    for case in cases:
        name=case['name']; row={'name':name,'passed':False};results.append(row);save()
        commands=[]
        if case['kind']=='python':
            commands.append([sys.executable,'-B',str(ROOT/case['source'])])
        else:
            binary=out/(name+('.exe' if os.name=='nt' else ''))
            cmd=[args.compiler,case['kind'],'-O2','-UNDEBUG']
            if case['kind']=='c++':cmd+=['-std=c++17']
            cmd += [str(ROOT/p) for p in case['sources']]
            for p in case.get('includes',[]):cmd+=['-I',str(ROOT/p)]
            cmd+=case.get('flags',[])
            if os.name!='nt':cmd+=['-pthread']
            commands.append(cmd+['-o',str(binary)])
            commands.append([str(binary)]+[str(out/a) for a in case.get('output_args',[])])
        try:
            for i,cmd in enumerate(commands):
                run=subprocess.run(cmd,cwd=out,env=env,capture_output=True,text=True,errors='replace',timeout=240)
                (out/(name+'-'+str(i)+'.log')).write_text(run.stdout+run.stderr,encoding='utf-8')
                if run.returncode:raise RuntimeError('exit='+str(run.returncode))
            row['passed']=True
        except (OSError,subprocess.SubprocessError,RuntimeError) as e:
            row['error']=str(e)
        save();print(('PASS ' if row['passed'] else 'FAIL ')+name,flush=True)
    return 0 if all(r['passed'] for r in results) else 1

if __name__=='__main__':raise SystemExit(main())
