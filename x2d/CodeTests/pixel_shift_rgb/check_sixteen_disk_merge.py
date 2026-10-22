"""自造 RAW：验证相位、跨块 halo、原厂数值域、首帧 metadata 与失败保留。"""
from pathlib import Path
import ctypes, hashlib, json, os, struct, subprocess, tempfile
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
OUT=ROOT/'x2d/outputs/4.2.0/pixel-shift-rgb/sixteen-disk-r1'
ZIG=ROOT/'.research-cache/x1d-1.25.0/toolchain/zig-windows-x86_64-0.13.0/zig.exe'
W,H=17,21
PHASE=[(x,y) for x in range(4) for y in (range(4) if x%2==0 else range(3,-1,-1))]
CFA=((0,1),(3,2))
def scene(x,y,c):return 4096+(x*x*11+y*33+c*2000+(x+y)%3*149)%27000
def sparse(f):
    if os.name=='nt':
        import msvcrt
        done=ctypes.c_ulong()
        call=ctypes.windll.kernel32.DeviceIoControl
        call.argtypes=[ctypes.c_void_p,ctypes.c_ulong,ctypes.c_void_p,ctypes.c_ulong,
                       ctypes.c_void_p,ctypes.c_ulong,ctypes.POINTER(ctypes.c_ulong),ctypes.c_void_p]
        assert call(msvcrt.get_osfhandle(f.fileno()),0x900c4,None,0,None,0,ctypes.byref(done),None)
def fixture(path,index):
    b=bytearray(16384);b[:8]=b'II*\0'+struct.pack('<I',8)
    def ifd(offset,tags):
        struct.pack_into('<H',b,offset,len(tags))
        for i,t in enumerate(tags):struct.pack_into('<HHII',b,offset+2+12*i,*t)
    ifd(8,[(256,4,1,3888),(257,4,1,2918),(259,3,1,7),(273,4,1,210526720),
           (279,4,1,4),(330,4,1,256),(34665,4,1,512),(50721,10,9,1024),(50728,5,3,1104)])
    ifd(256,[(256,4,1,11904),(257,4,1,8842),(258,3,1,16),(259,3,1,1),
             (262,3,1,32803),(273,4,1,16384),(277,3,1,1),(278,4,1,8842),
             (279,4,1,210510336),(50714,5,1,1136),(50717,4,1,32768),
             (50719,4,2,1400),(50720,4,2,1408)])
    ifd(512,[(37500,4,1,768)])
    ifd(768,[(33,9,4,1152),(0x59,3,6,1300)])
    struct.pack_into('<18i',b,1024,*([1,1]*9));struct.pack_into('<6I',b,1104,*([1,1]*3))
    struct.pack_into('<II',b,1136,4096,1);struct.pack_into('<4i',b,1152,16,index,0,0)
    struct.pack_into('<6H',b,1300,1,124,92,11656,8742,777)
    struct.pack_into('<4I',b,1400,124,92,11656,8742)
    dx,dy=PHASE[index];sx=(dx+1)//2;sy=(4-dy)//2;hx=2*sx-dx;hy=2*sy+dy-3
    with path.open('xb') as f:
        sparse(f);f.write(b)
        for y in range(H):
            row=struct.pack('<'+'H'*W,*(scene(hx+2*x,hy+2*y,CFA[(sy+y)%2][(sx+x)%2]) for x in range(W)))
            f.seek(16384+(92+sy+y)*23808+2*(124+sx));f.write(row)
        f.seek(210526720);f.write(b'\xff\xd8\xff\xd9')
    return b
def expected_samples():
    rows=[]
    for y in range(2*H):
        a=[];b=[];yn=min(y+1,2*H-1)
        for x in range(2*W):
            xn=min(x+1,2*W-1)
            a.extend((scene(x,y,0),(scene(x,y,1)+scene(xn,y,1)+1)//2))
            b.extend(((scene(x,y,3)+scene(x,yn,3)+1)//2,
                      (scene(x,y,2)+scene(xn,y,2)+scene(x,yn,2)+scene(xn,yn,2)+2)//4))
        rows.extend(a+b)
    return struct.pack('<'+'H'*len(rows),*rows)
def main():
    OUT.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ,TEMP=str(OUT),TMP=str(OUT),ZIG_LOCAL_CACHE_DIR=str(OUT/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(OUT/'zig-global'))
    def build(name,flags=()):
        target=OUT/name
        subprocess.run([str(ZIG),'cc','-O2','-Wall','-Wextra','-Werror','-Wno-unused-function',
                        f'-DSIXTEEN_WIDTH={W}',f'-DSIXTEEN_HEIGHT={H}',*flags,str(HERE/'sixteen_disk_merge.c'),'-o',str(target)],env=env,check=True)
        return target
    exe=build('merge-small.exe');bad=build('merge-short.exe',('-DSIXTEEN_TEST_READ_FAILURE=1',))
    cases=[]
    with tempfile.TemporaryDirectory(prefix='generated-',dir=OUT) as d:
        root=Path(d);sources=[root/f'frame-{i}.3FR' for i in range(16)]
        headers=[fixture(p,i) for i,p in enumerate(sources)]
        expected=expected_samples()
        def run(bin,path,rows):return subprocess.run([str(bin),str(path),str(root),str(rows),'32'],capture_output=True,text=True,timeout=60)
        baseline=None
        for rows in (1,2,7,1024):
            path=root/f'out-{rows}.3fr';r=run(exe,path,rows);assert r.returncode==0,(r.stdout,r.stderr)
            b=path.read_bytes();assert len(b)%4096==0
            assert b[16384:16384+len(expected)]==expected
            assert b[16384+len(expected):16388+len(expected)]==b'\xff\xd8\xff\xd9'
            raw_ifd=struct.unpack_from('<I',b,8+2+12*5+8)[0]
            def value(tag):
                count=struct.unpack_from('<H',b,raw_ifd)[0]
                for k in range(count):
                    t,typ,n,v=struct.unpack_from('<HHII',b,raw_ifd+2+12*k)
                    if t==tag:return typ,n,v
                raise AssertionError(tag)
            assert value(256)[2]==4*W and value(257)[2]==4*H and value(50717)[2]==32768
            assert b[1152:1168]==headers[0][1152:1168]
            assert struct.unpack_from('<6H',b,1300)==(1,0,0,4*W,4*H,777)
            if baseline is None:baseline=b
            else:assert b==baseline
            cases.append(f'rows_{rows}_analytical_scene_exact_halo_edges_native_domain_4k')
        path=root/'out-7.3fr';assert run(exe,path,7).returncode!=0 and path.read_bytes()==baseline
        cases.append('existing_output_untouched')
        failure=root/'short.3fr';assert run(bad,failure,2).returncode!=0 and not failure.exists()
        assert all(p.exists() for p in sources);cases.append('read_failure_stops_all_workers_output_removed_inputs_retained')
        p=sources[9];b=bytearray(headers[9]);struct.pack_into('<i',b,1156,8)
        with p.open('r+b') as f:f.write(b)
        failure=root/'group.3fr';assert run(exe,failure,7).returncode!=0 and not failure.exists()
        cases.append('wrong_group_sequence_rejected_before_output')
    subprocess.run([str(ZIG),'cc','-target','aarch64-linux-gnu','-O2','-Wall','-Wextra','-Werror',
                    '-Wno-unused-function','-c',str(HERE/'sixteen_disk_merge.c'),'-o',str(OUT/'sixteen_disk_merge_arm.o')],env=env,check=True)
    subprocess.run([str(ZIG),'cc','-O2',str(HERE/'check_private_capture_group.c'),'-o',str(OUT/'groups.exe')],env=env,check=True)
    subprocess.run([str(OUT/'groups.exe')],check=True)
    report=dict(cases=cases,arm_object_built=True,installed=False,source_reads='synthetic fixtures only',
                jpeg_quality=60,jpeg_capacity=536870912,source_sha256=hashlib.sha256((HERE/'sixteen_disk_merge.c').read_bytes()).hexdigest())
    (OUT/'validation.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('SIXTEEN_DISK_4_THREADS_1024_ROWS_NATIVE_DOMAIN_HALO_METADATA_AND_FAILURES_PASSED')
if __name__=='__main__':main()
