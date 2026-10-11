"""为原厂编码JPG无损加入白名单EXIF；主机步骤，不重编码像素。"""
import argparse
import hashlib
import os
from pathlib import Path
import struct
import tempfile

SIZES={1:1,2:1,3:2,4:4,5:8,7:1,9:4,10:8}


def read_ifd(data,offset):
    if offset<8 or offset+2>len(data):raise ValueError('IFD range')
    count=struct.unpack_from('<H',data,offset)[0]
    if count>128 or offset+2+count*12+4>len(data):raise ValueError('IFD length')
    result={}
    for i in range(count):
        pos=offset+2+i*12;tag,typ,n=struct.unpack_from('<HHI',data,pos)
        if tag in result:raise ValueError('Duplicate tag')
        if typ not in SIZES:continue
        size=n*SIZES[typ]
        start=pos+8 if size<=4 else struct.unpack_from('<I',data,pos+8)[0]
        if size>4096 or start+size>len(data):
            # 不跟随MakerNote、缩略图等非白名单数据。
            result[tag]=None;continue
        result[tag]=(typ,n,data[start:start+size])
    return result


def app1(reference,width,height):
    with Path(reference).open('rb') as f:data=f.read(16384)
    if data[:4]!=b'II*\0':raise ValueError('Source must be little-endian TIFF')
    first=read_ifd(data,struct.unpack_from('<I',data,4)[0])
    ptr=first.get(34665)
    if ptr is None or ptr[:2]!=(4,1):raise ValueError('Missing EXIF pointer')
    source=read_ifd(data,struct.unpack('<I',ptr[2])[0])
    # 必需拍摄参数采用原始有理数，不能从文本或四亿合成总时长推算。
    expected={33434:5,33437:5,34855:3,37386:5}
    for tag,typ in expected.items():
        if source.get(tag) is None or source[tag][:2]!=(typ,1):raise ValueError('Missing required capture parameter')
        if typ==5:
            numerator,denominator=struct.unpack('<II',source[tag][2])
            if not numerator or not denominator:raise ValueError('Invalid capture rational')
        elif not struct.unpack('<H',source[tag][2])[0]:raise ValueError('Invalid sensitivity')
    orientation=first.get(274,(3,1,struct.pack('<H',1)))
    if orientation[:2]!=(3,1) or not 1<=struct.unpack('<H',orientation[2])[0]<=8:raise ValueError('Invalid orientation')
    keep=(33434,33437,34850,34855,36864,36867,36868,37377,37378,37380,37386,41986,41987,42034,42036)
    exif={tag:source[tag] for tag in keep if source.get(tag) is not None}
    exif.update({40961:(3,1,struct.pack('<H',1)),40962:(4,1,struct.pack('<I',width)),40963:(4,1,struct.pack('<I',height))})
    # 不复制设备序列号、GPS、MakerNote、旧缩略图或Software字段。
    root={274:orientation,34665:(4,1,struct.pack('<I',38))}
    payload=bytearray(b'II*\0'+struct.pack('<I',8))
    payload.extend(bytes(30+2+len(exif)*12+4))
    def emit(values,offset):
        struct.pack_into('<H',payload,offset,len(values))
        for i,(tag,(typ,count,value)) in enumerate(sorted(values.items())):
            pos=offset+2+i*12;struct.pack_into('<HHI',payload,pos,tag,typ,count)
            if len(value)<=4:payload[pos+8:pos+12]=value.ljust(4,b'\0')
            else:
                if len(payload)&1:payload.append(0)
                struct.pack_into('<I',payload,pos+8,len(payload));payload.extend(value)
    emit(root,8);emit(exif,38)
    value=b'Exif\0\0'+payload
    if len(value)>65533:raise ValueError('EXIF segment too large')
    return b'\xff\xe1'+struct.pack('>H',len(value)+2)+value


def jpeg_size(source):
    with Path(source).open('rb') as f:
        if f.read(2)!=b'\xff\xd8':raise ValueError('Not JPEG')
        dimensions=None
        while True:
            marker=f.read(2)
            if len(marker)!=2 or marker[0]!=255:raise ValueError('Invalid JPEG marker')
            size=f.read(2)
            if len(size)!=2:raise ValueError('Truncated JPEG')
            n=struct.unpack('>H',size)[0]
            if n<2:raise ValueError('Invalid JPEG segment size')
            data=f.read(n-2)
            if len(data)!=n-2:raise ValueError('Truncated segment')
            if marker==b'\xff\xe1' and data.startswith(b'Exif\0\0'):raise ValueError('Already contains EXIF')
            if marker[1] in (0xc0,0xc1,0xc2):
                if len(data)<6 or data[0]!=8:raise ValueError('Unsupported frame')
                if dimensions is not None:raise ValueError('Duplicate frame header')
                h,w=struct.unpack_from('>HH',data,1);dimensions=(w,h)
            if marker[1] in (0xda,0xd9):
                if dimensions is None:raise ValueError('Missing frame header')
                return dimensions


def finalize(source,reference,destination):
    source,reference,destination=map(Path,(source,reference,destination))
    if destination.exists():raise FileExistsError(destination)
    w,h=jpeg_size(source)
    if (w,h)!=(23326,17498):raise ValueError('Unexpected output geometry')
    segment=app1(reference,w,h)
    fd,tmp=tempfile.mkstemp(prefix='.jpeg-finalize-',suffix='.partial',dir=destination.parent)
    try:
        with os.fdopen(fd,'wb') as out,source.open('rb') as inp:
            out.write(inp.read(2));out.write(segment)
            while block:=inp.read(1024*1024):out.write(block)
            out.flush();os.fsync(out.fileno())
        # link失败不会替换已有输出；只清理本次临时文件。
        os.link(tmp,destination)
    finally:
        os.unlink(tmp)
    with source.open('rb') as a,destination.open('rb') as b:
        assert a.read(2)==b.read(2)==b'\xff\xd8'
        assert b.read(len(segment))==segment
        ha=hashlib.file_digest(a,'sha256').digest();hb=hashlib.file_digest(b,'sha256').digest()
        assert ha==hb,'JPEG payload changed'
    return dict(width=w,height=h,encoded_payload_identical=True,metadata_added=True)


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('source');p.add_argument('reference');p.add_argument('destination');a=p.parse_args()
    print(finalize(a.source,a.reference,a.destination))
