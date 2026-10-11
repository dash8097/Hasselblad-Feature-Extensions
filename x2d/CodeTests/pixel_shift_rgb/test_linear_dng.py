"""独立解码验证；测试文件只写获授权的输出目录。"""
from pathlib import Path
import tempfile
import unittest
from linear_dng import np, tifffile, write_linear_dng
import rawpy

OUTPUT = Path(__file__).resolve().parents[2]/'outputs/4.2.0/pixel-shift-rgb'


class DngTests(unittest.TestCase):
    def test_14bit_samples_independent_decode_and_overwrite_rejection(self):
        # 白电平为 14 位，但容器每通道 16 位，不声称这验证了相机读出位深。
        with tempfile.TemporaryDirectory(dir=OUTPUT,prefix='dng-test-') as d:
            p=Path(d)/'result.dng'
            rgb=(np.arange(96*128*3,dtype=np.uint32)%16384).astype(np.uint16).reshape(96,128,3)
            args=dict(color_matrix=np.eye(3),neutral=[1,1,1],white_level=16383,
                      model='Synthetic 14bit RGB Test')
            write_linear_dng(p,rgb,**args)
            with rawpy.imread(str(p)) as raw:
                np.testing.assert_array_equal(raw.raw_image_visible[:,:,:3],rgb)
                self.assertEqual(raw.white_level,16383)
            with tifffile.TiffFile(p) as t:
                self.assertEqual(t.pages[0].bitspersample,16)
                self.assertEqual(int(t.pages[0].photometric),34892)
            with self.assertRaises(FileExistsError):write_linear_dng(p,rgb,**args)

    def test_invalid_calibration_rejected_before_file_creation(self):
        with tempfile.TemporaryDirectory(dir=OUTPUT,prefix='dng-test-') as d:
            p=Path(d)/'invalid.dng'; rgb=np.zeros((16,16,3),dtype=np.uint16)
            with self.assertRaises(ValueError):
                write_linear_dng(p,rgb,color_matrix=np.zeros((3,3)),neutral=[1,1,1],
                                 white_level=16383,model='Synthetic Test')
            self.assertFalse(p.exists())


if __name__=='__main__':unittest.main()
