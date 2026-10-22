"""执行 Android 1.0.2 原厂 JPEG 有效性分支；Exif 解析用显式宿主解析器。
不运行整个 APK；GetBackModel 以解析得到的型号返回值代替。
"""
import hashlib
import json
import struct
from probe_mobile_correction_scale import machine, M, BASE, DATA
from unicorn import UC_HOOK_CODE
from unicorn.arm64_const import *


def entries(data, offset):
    assert 8 <= offset <= len(data)-6
    n, = struct.unpack_from('<H', data, offset)
    assert n <= 128 and offset+6+n*12 <= len(data)
    result = {}
    for i in range(n):
        at = offset+2+i*12
        tag, kind, count, value = struct.unpack_from('<HHII', data, at)
        assert tag not in result
        size = {1:1,2:1,3:2,4:4,5:8,7:1,9:4,10:8}.get(kind,0)*count
        assert size
        p = at+8 if size <= 4 else value
        assert p+size <= len(data)
        result[tag] = (kind, count, value, data[p:p+size])
    return result


def exif_from_jpeg(path):
    with path.open('rb') as f:
        assert f.read(2) == b'\xff\xd8'
        while True:
            assert f.read(1) == b'\xff'
            marker = f.read(1)[0]
            if marker == 0xda:
                raise AssertionError('No Exif before scan')
            length, = struct.unpack('>H', f.read(2))
            payload = f.read(length-2)
            assert len(payload) == length-2
            if marker == 0xe1 and payload.startswith(b'Exif\0\0'):
                return payload[6:]


def identity(tiff):
    assert tiff[:4] == b'II*\0'
    root = entries(tiff, struct.unpack_from('<I',tiff,4)[0])
    exif = entries(tiff, root[34665][2])
    maker = exif.get(37500)
    if not maker:
        return False, 0
    assert maker[0] == 7 and maker[1] >= 6
    m = entries(tiff, maker[2])
    model = m.get(0x12)
    return True, model[2] if model and model[:2] == (4,1) else 0


def original_validity(has_maker, model):
    u=machine()
    obj,tiff,table=M+0x4000,M+0x5000,M+0x6000
    callee=M+0x7000
    u.mem_write(obj,struct.pack('<Q',table))
    u.mem_write(obj+0x50,struct.pack('<Q',tiff))
    u.mem_write(tiff+0x20,struct.pack('<Q',M+0x8000 if has_maker else 0))
    u.mem_write(table+0x138,struct.pack('<Q',callee))
    called=[]
    def hook(uc,address,size,user):
        if address==callee:
            called.append(True)
            uc.reg_write(UC_ARM64_REG_W0,model)
            uc.reg_write(UC_ARM64_REG_PC,uc.reg_read(UC_ARM64_REG_LR))
    u.hook_add(UC_HOOK_CODE,hook)
    u.reg_write(UC_ARM64_REG_X19,obj)
    u.emu_start(0x3688288,0x36882b4,count=32)
    assert u.reg_read(UC_ARM64_REG_PC)==0x36882b4
    return int(u.mem_read(obj+0x110,1)[0]),bool(called)


def main():
    assert hashlib.sha256(DATA).hexdigest()=='c9de0142fb2146c2da50affa48e37e35833a96a15ee698090688039655aa7ca4'
    old=identity(exif_from_jpeg(BASE/'latest.jpg'))
    app1=(BASE/'jpeg-mobile.app1').read_bytes()
    assert app1[:4]==b'\xff\xe1'+struct.pack('>H',len(app1)-2)
    new=identity(app1[10:])
    assert old==(False,0) and new[0] and new[1]>0
    results=[]
    for label,args,wanted in [('original',old,0),('fixed',new,1),('empty_model',(True,0),0)]:
        actual,called=original_validity(*args)
        assert actual==wanted
        results.append(dict(case=label,has_makernote=args[0],has_nonzero_model=bool(args[1]),
                            native_valid=actual,model_getter_called=called))
    print(json.dumps(dict(scope='Original ARM64 constructor validity block; host TIFF parser and substituted GetBackModel; not full APK decode',
                          cases=results),indent=2))


if __name__=='__main__':main()
