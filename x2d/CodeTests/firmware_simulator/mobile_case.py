"""只用自造像素和元数据执行生产封装；不读取用户照片，不运行整个Phocus。"""
from pathlib import Path
import hashlib
import re
import struct

ROOT = Path(__file__).resolve().parents[3]
PIX = ROOT / 'x2d/CodeTests/pixel_shift_rgb'
ZIG = ROOT / '.research-cache/x1d-1.25.0/toolchain/zig-windows-x86_64-0.13.0/zig.exe'


def raw_header():
    data = bytearray(16384); data[:8] = b'II*\0' + struct.pack('<I', 8)
    def ifd(at, tags):
        struct.pack_into('<H', data, at, len(tags))
        for i, tag in enumerate(tags):
            struct.pack_into('<HHII', data, at + 2 + 12*i, *tag)
    ifd(8, [(274,3,1,1), (34665,4,1,256)])
    ifd(256, [(33434,5,1,512), (34855,3,1,100), (37500,7,30,768)])
    struct.pack_into('<II', data, 512, 1, 125)
    ifd(768, [(17,4,1,1), (18,4,1,1)])
    return bytes(data)


def marks(f):
    assert f.read(2) == b'\xff\xd8'
    while True:
        at=f.tell(); h=f.read(4)
        assert len(h)==4 and h[0]==255
        size=int.from_bytes(h[2:],'big'); assert size>=2
        payload=f.read(size-2); assert len(payload)==size-2
        yield at,h[1],h+payload
        if h[1]==218: return


def digest_range(path, start, count):
    h=hashlib.sha256()
    with path.open('rb') as f:
        f.seek(start)
        while count:
            data=f.read(min(count,1048576)); assert data
            h.update(data); count-=len(data)
    return h.hexdigest()


def check(out, call, compile_test):
    import probe_mobile_jpeg_validity as native
    assert hashlib.sha256(native.DATA).hexdigest()=='c9de0142fb2146c2da50affa48e37e35833a96a15ee698090688039655aa7ca4'
    dep=ROOT/'x2d/outputs/4.2.0/pixel-shift-rgb/jpeg-turbo-roi'
    source,config=dep/'source',dep/'host'
    names=re.search(r'set\(JPEG_SOURCES (.*?)\)',(source/'CMakeLists.txt').read_text('utf-8'),re.S)[1].split()
    stream=out/'jpeg-stream.exe'
    call([ZIG,'cc','-O3','-D_DEFAULT_SOURCE','-I',config,'-I',source,
          PIX/'check_mobile_jpeg_stream.c',*[source/n for n in names],source/'jsimd_none.c','-o',stream])
    raw=out/'synthetic.3fr'; raw.write_bytes(raw_header())
    compile_test('exif',['check_compat_mobile_exif.c'],args=[raw,out/'result.app1'],marker='EXIF_IDENTITY_WHITELIST')
    preview=out/'preview.jpg'; call([stream,'encode',preview,3888,2918,80], 'JPEG_STREAM_COMPLETE')
    cases=[]
    for w,h,q in ((23326,17498,80),(46652,34996,60)):
        pack=out/f'pack-{w}.exe'
        call([ZIG,'cc','-O2','-DPACKAGE_MOBILE_MPF=1',f'-DPACKAGE_IMAGE_WIDTH={w}u',
              f'-DPACKAGE_IMAGE_HEIGHT={h}u','-DFACTORY_EXIF_MODEL="X2D 100C"',PIX/'package_native_preview.c','-o',pack])
        main=out/f'main-{w}.jpg'; dst=out/f'packed-{w}.jpg'
        call([stream,'encode',main,w,h,q],'JPEG_STREAM_COMPLETE')
        args=[pack,main,preview,dst,'--capture-raw',raw]; call(args)
        with dst.open('rb') as f: headers=list(marks(f))
        exif=[data for _,m,data in headers if m==225 and data[4:10]==b'Exif\0\0']; assert len(exif)==1
        tiff=exif[0][10:]; root=native.entries(tiff,8); metadata=native.entries(tiff,root[34665][2])
        assert root[272][3].rstrip(b'\0')==b'X2D 100C'
        assert metadata[40962][2]==w and metadata[40963][2]==h
        assert native.original_validity(*native.identity(tiff))[0]==1
        assert set(native.entries(tiff,metadata[37500][2]))=={17,18}
        mpf=[(at,data) for at,m,data in headers if m==226 and data[4:8]==b'MPF\0']; assert len(mpf)==1
        at,data=mpf[0]; table=native.entries(data[8:],8); assert table[0xb001][2]==2
        attr,size,offset,_,_=struct.unpack_from('<IIIHH',table[0xb002][3]); assert attr==0x30000 and offset==0
        attr,length,offset,_,_=struct.unpack_from('<IIIHH',table[0xb002][3],16)
        assert attr==0x10003 and length==preview.stat().st_size
        start=size-main.stat().st_size; pre=at+8+offset
        assert start%4096==pre%4096==dst.stat().st_size%4096==0 and pre>=size
        assert digest_range(dst,start,main.stat().st_size)==digest_range(main,0,main.stat().st_size)
        assert digest_range(dst,pre,length)==digest_range(preview,0,length)
        call([stream,'decode',dst,w,h,0],'JPEG_STREAM_COMPLETE')
        before=digest_range(dst,0,dst.stat().st_size); call(args,ok=False)
        assert digest_range(dst,0,dst.stat().st_size)==before
        reject=out/f'invalid-{w}.jpg'
        call([pack,preview,preview,reject,'--capture-raw',raw],ok=False)
        assert not reject.exists() and not reject.with_suffix('.jpg.partial').exists()
        cases.append(dict(dimensions=[w,h],quality=q,full_software_decode=True,main_scan_unchanged=True,
                          native_validity=1,mpf_alignment=True,overwrite_rejected=True))
    assert raw.read_bytes()==raw_header()
    for args,wanted in [((False,0),0),((True,0),0),((True,1),1)]:
        assert native.original_validity(*args)[0]==wanted
    return dict(cases=cases,synthetic_inputs_only=True,raw_pixels_changed=False,
                phone_library_sha256=hashlib.sha256(native.DATA).hexdigest(),
                mocked='Exif解析与GetBackModel返回由有界替身提供；未执行整个APK',
                raw_lens_correction_verified=False,full_phocus_verified=False,wifi_verified=False)
