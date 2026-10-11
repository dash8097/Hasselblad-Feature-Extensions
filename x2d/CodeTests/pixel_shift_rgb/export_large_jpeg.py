"""将实验线性 DNG 插值为宽高各两倍的 sRGB JPEG；不增加实测细节。"""
import argparse
from pathlib import Path
import sys
import numpy as np
from PIL import Image

import tifffile


def ratios(value):
    a=np.asarray(value,dtype=np.float64).reshape(-1,2)
    return a[:,0]/a[:,1]


def export(source, destination):
    if destination.exists(): raise FileExistsError(destination)
    partial=destination.with_suffix(destination.suffix+'.partial')
    if partial.exists(): raise FileExistsError(partial)
    with tifffile.TiffFile(source) as tf:
        page=tf.pages[0]
        if int(page.photometric)!=34892 or page.shape[-1]!=3:
            raise ValueError('Expected linear RGB DNG')
        matrix=ratios(page.tags[50721].value).reshape(3,3)
        neutral=ratios(page.tags[50728].value)
        if np.any(ratios(page.tags[50714].value)!=0):
            raise ValueError('This exporter requires black-subtracted input')
        white=np.array(page.tags[50717].value,dtype=float)
    # Model matrix + image white balance only; no claim of full factory calibration.
    xyz_from_srgb=np.array([[.4124564,.3575761,.1804375],
                           [.2126729,.7151522,.0721750],
                           [.0193339,.1191920,.9503041]])
    cam_from_rgb=matrix @ xyz_from_srgb
    cam_from_rgb/=cam_from_rgb.sum(axis=1)[:,None]
    rgb_from_cam=np.linalg.inv(cam_from_rgb)
    wb=neutral[1]/neutral
    data=tifffile.memmap(source)
    h,w,_=data.shape
    large=np.empty((h*2,w*2,3),dtype=np.uint8)
    for channel in range(3):
        linear=np.empty((h,w),dtype=np.float32)
        for top in range(0,h,128):
            tile=data[top:top+128].astype(np.float32)
            linear[top:top+128]=np.einsum('...c,c->...',tile,
                rgb_from_cam[channel]*wb/white)
        # Interpolate linear-light data, then apply sRGB transfer and 8-bit coding.
        resized=Image.fromarray(linear).resize((w*2,h*2),Image.Resampling.LANCZOS)
        expanded=np.asarray(resized)
        for top in range(0,h*2,128):
            tile=np.clip(expanded[top:top+128],0,1)
            encoded=np.where(tile<=.0031308,12.92*tile,1.055*tile**(1/2.4)-.055)
            large[top:top+128,:,channel]=np.rint(encoded*255).astype(np.uint8)
        del expanded,resized,linear
        print('INTERPOLATED_CHANNEL',channel,flush=True)
    Image.fromarray(large).save(partial,format='JPEG',quality=95,subsampling=0)
    # Decoder confirmation, not merely checking the JPEG header.
    Image.MAX_IMAGE_PIXELS=None
    with Image.open(partial) as check:
        if check.size!=(w*2,h*2) or check.mode!='RGB':
            raise ValueError('Unexpected JPEG dimensions')
        check.load()
    partial.rename(destination)
    print('JPEG_DECODE_VERIFIED',w*2,h*2,'pixels',w*h*4,flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('destination',type=Path)
    args=p.parse_args();export(args.source,args.destination)
