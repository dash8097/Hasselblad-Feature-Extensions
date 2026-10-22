"""完整交付包的实机 UI 驱动；只用触屏事件和原厂屏幕截图，不上传功能代码。"""
import os
import argparse,base64,gzip,hashlib,json,re,socket,struct,sys,time,zlib
from contextlib import contextmanager
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
OUT=Path(os.environ.get('X2D_UI_EVIDENCE_DIR',str(ROOT/'x2d/outputs/4.2.0/installer-hardening-r7/package-device-ui-20261018'))).resolve()
assert OUT.is_relative_to(ROOT/'x2d/outputs'), '测试证据须位于当前仓库x2d/outputs'
OUT.mkdir(parents=True,exist_ok=True)
PACKAGE_ID='1c54fe460cadcc701eab'
PACKAGE=Path(os.environ.get('X2D_TEST_PACKAGE',str(OUT/('package-'+PACKAGE_ID)))).resolve()
assert PACKAGE.is_relative_to(ROOT), '完整测试包必须位于当前仓库'
if os.environ.get('X2D_TEST_PACKAGE'):
    metadata=(PACKAGE/'native/client-metadata.json').read_bytes()
    contract=json.loads(metadata)
    assert contract.get('schema')=='X2D_NATIVE_CLIENT_V1' and contract.get('firmware')=='4.2.0', '当前UI适配器仅接受X2D 4.2.0完整包'
    PACKAGE_ID='metadata-'+hashlib.sha256(metadata).hexdigest()[:20]

def idle_ui_allowed(state):
    # Cancellation is a completed user action. Preserve its diagnostic text,
    # while allowing the same ordinary menu keys available on the real camera.
    if state.get('busy') is not False:return False
    if not state.get('error'):return True
    return state.get('ready') is True and state.get('prepared') is False and state.get('stage')=='failed' and state.get('error')=='拍摄已取消'

def read_status(one):
    # The control transport accepts ASCII replies. Localized task failures are
    # UTF-8, so encode this one fixed status file; never print/store its raw body.
    encoded=one('/system/xbin/busybox base64 /tmp/x2d-pixel-overclock/status.json')
    assert len(encoded)<131072
    value=json.loads(base64.b64decode(''.join(encoded.split()),validate=True).decode('utf8'))
    assert type(value) is dict
    return value

def tap_command(x,y):
    assert type(x) is int and type(y) is int and 0<=x<1024 and 0<=y<768
    rows=['3 47 0','3 57 502',f'3 53 {x}',f'3 54 {y}','1 330 1','0 0 0','w','3 57 -1','1 330 0','0 0 0']
    command='for e in '+' '.join('"'+r+'"' for r in rows)+';do if [ "$e" = w ];then sleep .08;else sendevent /dev/input/event8 $e;fi;done'
    assert len(command.encode('ascii'))<=231
    return command

@contextmanager
def transport_lock():
    # Fail before opening USB if another UI action/state probe is still running.
    import msvcrt
    with (OUT/'camera-transport.lock').open('a+b') as f:
        if not f.tell():f.write(b'0');f.flush()
        f.seek(0);msvcrt.locking(f.fileno(),msvcrt.LK_NBLCK,1)
        try:yield
        finally:f.seek(0);msvcrt.locking(f.fileno(),msvcrt.LK_UNLCK,1)

def swipe_command(x,y,tx,ty):
    assert all(type(v) is int and 0<=v<(1024 if i%2==0 else 768) for i,v in enumerate((x,y,tx,ty)))
    def coordinate(start,end):
        return str(start) if start==end else f'$(({start}+({end-start})*i/8))'
    command='e(){ sendevent /dev/input/event8 $*;};e 3 47 0;e 3 57 501;e 1 330 1;for i in 0 1 2 3 4 5 6 7 8;do e 3 53 '+coordinate(x,tx)+';e 3 54 '+coordinate(y,ty)+';e 0 0 0;sleep .025;done;e 3 57 -1;e 1 330 0;e 0 0 0'
    assert len(command.encode('ascii'))<=231
    return command

def run():
    assert sys.version_info[:2]==(3,11), '包内编译模块使用 Python 3.11；请用 py -3.11 运行，尚未连接相机'
    ap=argparse.ArgumentParser();ap.add_argument('action',choices=['snapshot','tap','swipe','tap-snapshot']);ap.add_argument('values',nargs='*',type=int);a=ap.parse_args()
    assert Path.cwd().resolve()==ROOT
    sys.path.insert(0,str(PACKAGE/'engine'));import windows_engine
    w=windows_engine.Transport(PACKAGE)
    def one(c):return w.batch([c])[0].strip()
    record={'time':time.time(),'package':PACKAGE_ID,'action':a.action,'values':list(a.values)}
    capture_id=None
    if a.action=='tap-snapshot':
        assert len(a.values)==3
        capture_id=a.values.pop();a.action='tap'
    if a.action in ('tap','swipe'):
        assert len(a.values)==(2 if a.action=='tap' else 4)
        assert all(0<=v<(1024 if i%2==0 else 768) for i,v in enumerate(a.values))
        assert one('cat /sys/class/input/event8/device/name')=='himax-touchscreen'
        x,y=a.values[:2]
        def events(rows):w.batch(['/system/bin/sendevent /dev/input/event8 '+r for r in rows])
        if a.action=='tap':
            # Stock ClickArea rejects releases after singleClickTimeoutMs.
            # Keep press/hold/release on-device; USB round trips are not hold time.
            command=tap_command(x,y)
            try:w.batch([command])
            except BaseException:
                events(['3 47 0','3 57 -1','1 330 0','0 0 0'])
                raise
            record['tap_hold_ms']=80
            record['tap_transport']='single-command'
        else:
            command=swipe_command(*a.values)
            try:w.batch([command])
            except BaseException:
                events(['3 47 0','3 57 -1','1 330 0','0 0 0'])
                raise
            record['swipe_duration_ms']=225
            record['swipe_transport']='single-command'
        # Take settled-state evidence after a tap just as after a plain action;
        # an immediate compositor capture can contain an unfinished transition.
        time.sleep(.8)
        record['settle_ms']=800
    if a.action=='snapshot' or capture_id is not None:
        if capture_id is not None:a.values=[capture_id]
        assert len(a.values)==1 and 0<a.values[0]<10000
        tag=f'screen-{a.values[0]:04d}';local=OUT/(tag+'.png');assert not local.exists()
        remote='/tmp/x2d-package-ui'
        assert one(f'test ! -L {remote} && mkdir -p {remote} && echo READY')=='READY'
        path=remote+'/'+tag
        assert one(f'test ! -e {path}.raw && test ! -L {path}.raw && test ! -e {path}.gz && test ! -L {path}.gz && echo FRESH')=='FRESH'
        reply=one(f'/system/bin/odin-output -d -o "Back/EVF" -s {path}.raw 2>&1')
        size_match=re.search(r'Wrote sceenshot of size: (\d+)x(\d+) to ',reply)
        assert 'screenshot_done' in reply and size_match
        width,height=map(int,size_match.groups())
        assert (width,height) in ((1024,768),(1600,1200))
        record['capture_reply']=reply
        record['capture_width']=width;record['capture_height']=height
        meta=one(f'/system/xbin/busybox gzip -c {path}.raw > {path}.gz && wc -c < {path}.gz && sha256sum {path}.gz').split()
        size=int(meta[0]);digest=meta[1];assert 0<size<4194304
        assert one("if grep -q ':7661 .* 0A ' /proc/net/tcp;then echo BUSY;else echo FREE;fi")=='FREE'
        one(f'nohup /bin/toybox timeout -k 1 15 /bin/toybox netcat -l -s 192.168.42.2 -p 30305 <{path}.gz >/dev/null 2>&1 &')
        with socket.socket() as s:
            s.bind(('192.168.42.1',0));s.settimeout(10);s.connect(('192.168.42.2',30305));data=b''
            while len(data)<size:
                piece=s.recv(min(65536,size-len(data)));assert piece;data+=piece
        assert hashlib.sha256(data).hexdigest()==digest
        raw=gzip.decompress(data);assert len(raw)==width*height*4
        rgb=bytearray(width*height*3);rgb[0::3]=raw[2::4];rgb[1::3]=raw[1::4];rgb[2::3]=raw[0::4]
        record['transport_sha256']=digest
        record['nonblack_pixels']=sum(bool(rgb[i] or rgb[i+1] or rgb[i+2]) for i in range(0,len(rgb),3))
        record['near_black_capture']=sum(max(rgb[i:i+3])>32 for i in range(0,len(rgb),3))<width*height*.02
        if record['near_black_capture']:
            print('CAPTURE_INCONCLUSIVE: near-black compositor capture is not evidence of a black device UI')
        def chunk(t,b):return struct.pack('>I',len(b))+t+b+struct.pack('>I',zlib.crc32(t+b)&0xffffffff)
        stride=width*3
        pixels=b''.join(b'\0'+rgb[y*stride:(y+1)*stride] for y in range(height))
        local.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(pixels))+chunk(b'IEND',b''))
        one(f'rm {path}.raw {path}.gz');record['screenshot']=str(local);print(local)
    with (OUT/'ui-actions.jsonl').open('a',encoding='utf8') as f:f.write(json.dumps(record)+'\n')
    print('UI_ACTION_COMPLETED')

def main():
    with transport_lock():run()

if __name__=='__main__':main()
