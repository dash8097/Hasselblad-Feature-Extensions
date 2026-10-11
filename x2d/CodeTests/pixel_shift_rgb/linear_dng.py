"""实验性 LinearRaw DNG 写入；要求调用者提供真实标定，禁止默认冒充 X2D。"""
from pathlib import Path
import os
import sys

import numpy as np
import tifffile


def rational(values):
    return tuple(n for v in values for n in (int(round(float(v)*1000000)),1000000))


def write_linear_dng(path, rgb, *, color_matrix, neutral, white_level, model):
    """无损、未应用白平衡/色调曲线的相机线性 RGB16。

    当前为整幅数组写入，不适合作为机内内存预算的证明。
    14 位读出可存于 16 位容器，white_level 必须采用实际校正后尺度。
    """
    rgb = np.asarray(rgb)
    matrix = np.asarray(color_matrix,dtype=float)
    neutral = np.asarray(neutral,dtype=float)
    if rgb.dtype != np.uint16 or rgb.ndim != 3 or rgb.shape[2] != 3 or min(rgb.shape[:2])<2:
        raise ValueError('Expected HxWx3 uint16 linear samples')
    if matrix.shape != (3,3) or not np.isfinite(matrix).all() or abs(np.linalg.det(matrix))<1e-9:
        raise ValueError('Valid XYZ-to-camera color matrix required')
    if neutral.shape != (3,) or not np.isfinite(neutral).all() or np.any(neutral<=0):
        raise ValueError('Positive camera-space neutral required')
    if not 0<white_level<=65535 or np.max(rgb)>white_level:
        raise ValueError('Invalid calibrated white level')
    if not model or not model.isascii(): raise ValueError('ASCII model identifier required')
    path = Path(path)
    # 失败文件不冒充有效成品，且拒绝覆盖已有照片。
    if path.exists(): raise FileExistsError(path)
    tmp = path.with_name(path.name+'.partial')
    h,w,_=rgb.shape
    tags = [(274,'H',1,1,False), (50706,'B',4,(1,4,0,0),False),
            (50707,'B',4,(1,1,0,0),False), (50708,'s',0,model,False),
            (50713,'H',2,(1,1),False), (50714,'2I',3,rational([0,0,0]),False),
            (50717,'I',3,(white_level,)*3,False),
            (50718,'2I',2,rational([1,1]),False),
            (50719,'I',2,(0,0),False), (50720,'I',2,(w,h),False),
            (50721,'2i',9,rational(matrix.ravel()),False),
            (50728,'2I',3,rational(neutral),False), (50778,'H',1,21,False)]
    try:
        with tmp.open('xb') as f:
            tifffile.imwrite(f,rgb,photometric=34892,planarconfig='contig',
                             compression=None,metadata=None,byteorder='<',
                             rowsperstrip=min(h,32),extratags=tags,
                             software='Experimental pixel-shift RGB prototype')
            f.flush(); os.fsync(f.fileno())
        with tifffile.TiffFile(tmp) as t:
            if t.pages[0].photometric != 34892 or not np.array_equal(t.asarray(),rgb):
                raise ValueError('Written pixels failed exact readback')
        # Windows rename refuses existing destinations.
        tmp.rename(path)
    except Exception:
        # Retain a partial file for diagnosis; never delete source captures here.
        raise


def verify_synthetic(output):
    import json
    import rawpy
    from prototype import simulate,merge_rows,synthetic_scene
    output=Path(output); output.mkdir(parents=True,exist_ok=True)
    rgb=np.array(list(merge_rows(simulate(129,97,'RGGB'))),dtype=np.uint16).reshape(96,128,3)
    # Synthetic image's declared camera channels are XYZ; this is not an X2D calibration.
    path=output/'synthetic-linear-rgb.dng'
    write_linear_dng(path,rgb,color_matrix=np.eye(3),neutral=[1,1,1],
                     white_level=65535,model='Synthetic RGB XYZ Test')
    with rawpy.imread(str(path)) as raw:
        decoded=raw.raw_image_visible
        if decoded is None or decoded.shape[:2]!=rgb.shape[:2]:
            raise AssertionError('Independent LibRaw decode missing RGB pixels')
        if decoded.ndim!=3 or not np.array_equal(decoded[:,:,:3],rgb):
            raise AssertionError('Independent LibRaw RGB differs')
        report={'source':'synthetic_only','format':'DNG LinearRaw RGB16',
                'independent_reader':'LibRaw through rawpy',
                'pixel_exact':True,'shape':list(rgb.shape),
                'camera_capture':False,'camera_calibration':False,
                'phocus_compatibility':'not tested','in_camera_processing':False}
    (output/'dng-report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps(report))


if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser();parser.add_argument('--synthetic-output',required=True)
    verify_synthetic(parser.parse_args().synthetic_output)
