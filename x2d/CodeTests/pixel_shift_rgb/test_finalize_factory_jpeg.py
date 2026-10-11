"""合成元数据和JPEG头替身：不读取照片、不连接设备。"""
import os
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch
from finalize_factory_jpeg import app1,finalize,jpeg_size,read_ifd

OUT=Path(__file__).resolve().parents[3]/'x2d/outputs/4.2.0/pixel-shift-rgb/host-temp'


def fixture():
    data=bytearray(1024);data[:8]=b'II*\0'+struct.pack('<I',8)
    struct.pack_into('<H',data,8,2)
    struct.pack_into('<HHI4s',data,10,274,3,1,struct.pack('<H',1)+b'\0\0')
    struct.pack_into('<HHII',data,22,34665,4,1,38)
    struct.pack_into('<H',data,38,4)
    for i,(tag,typ,value) in enumerate(((33434,5,(1,1)),(33437,5,(4,1)),(34855,3,(64,)),(37386,5,(55,1)))):
        pos=40+i*12;struct.pack_into('<HHI',data,pos,tag,typ,1)
        if typ==3:struct.pack_into('<H',data,pos+8,value[0])
        else:
            off=128+i*8;struct.pack_into('<I',data,pos+8,off);struct.pack_into('<II',data,off,*value)
    return bytes(data)


def jpeg():
    sof=b'\x08'+struct.pack('>HH',17498,23326)+b'\x03'+bytes(9)
    return b'\xff\xd8\xff\xc0'+struct.pack('>H',len(sof)+2)+sof+b'\xff\xda\x00\x02SYNTHETIC_SCAN\xff\xd9'


class FinalizeTests(unittest.TestCase):
    def setUp(self):
        OUT.mkdir(parents=True,exist_ok=True)
        self.files=[]
        for data in (fixture(),jpeg()):
            fd,path=tempfile.mkstemp(dir=OUT)
            with os.fdopen(fd,'wb') as f:f.write(data)
            self.files.append(Path(path))
        self.reference,self.source=self.files
        self.destination=self.source.with_suffix('.final.jpg')
        self.files.append(self.destination)

    def tearDown(self):
        for path in self.files:
            assert path.resolve().is_relative_to(OUT.resolve())
            path.unlink(missing_ok=True)

    def test_payload_and_dimensions(self):
        report=finalize(self.source,self.reference,self.destination)
        self.assertTrue(report['encoded_payload_identical'])
        data=self.destination.read_bytes();n=struct.unpack_from('>H',data,4)[0]
        self.assertEqual(data[:2]+data[4+n:],self.source.read_bytes())
        tiff=data[12:4+n];root=read_ifd(tiff,8);exif=read_ifd(tiff,38)
        self.assertEqual(set(root),{274,34665})
        self.assertEqual(struct.unpack('<I',exif[40962][2])[0],23326)
        self.assertEqual(struct.unpack('<I',exif[40963][2])[0],17498)

    def test_existing_output_preserved(self):
        self.destination.write_bytes(b'KEEP')
        with self.assertRaises(FileExistsError):finalize(self.source,self.reference,self.destination)
        self.assertEqual(self.destination.read_bytes(),b'KEEP')

    def test_publish_failure_removes_partial(self):
        before=set(OUT.glob('.jpeg-finalize-*'))
        with patch('finalize_factory_jpeg.os.link',side_effect=OSError('injected')):
            with self.assertRaises(OSError):finalize(self.source,self.reference,self.destination)
        self.assertFalse(self.destination.exists())
        self.assertEqual(before,set(OUT.glob('.jpeg-finalize-*')))

    def test_duplicate_metadata_rejected(self):
        finalize(self.source,self.reference,self.destination)
        with self.assertRaises(ValueError):jpeg_size(self.destination)

    def test_truncated_source_rejected(self):
        self.reference.write_bytes(fixture()[:50])
        with self.assertRaises(ValueError):app1(self.reference,23326,17498)

    def test_metadata_after_frame_rejected(self):
        data=self.source.read_bytes();pos=data.index(b'\xff\xda')
        self.source.write_bytes(data[:pos]+b'\xff\xe1\x00\x08Exif\0\0'+data[pos:])
        with self.assertRaises(ValueError):jpeg_size(self.source)

    def test_two_outputs_are_repeatable(self):
        other=self.destination.with_suffix('.second.jpg');self.files.append(other)
        finalize(self.source,self.reference,self.destination)
        finalize(self.source,self.reference,other)
        self.assertEqual(self.destination.read_bytes(),other.read_bytes())

    def test_zero_denominator_rejected(self):
        data=bytearray(fixture());struct.pack_into('<I',data,132,0)
        self.reference.write_bytes(data)
        with self.assertRaises(ValueError):app1(self.reference,23326,17498)


if __name__=='__main__':unittest.main()
