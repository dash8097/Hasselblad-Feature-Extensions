"""四帧线性 RGBG 插值再马赛克实验；主机运行，不调用相机。"""
from pathlib import Path
from contextlib import ExitStack
import argparse
import json
import os
from linear_dng import np, rational
import tifffile
import rawpy


def remosaic(planes):
    """RGGB 在原坐标间隔 0.5 的网格采样；末行末列采用边界复制。"""
    h,w,c=planes.shape
    if c!=4 or planes.dtype!=np.uint16: raise ValueError('Expected uint16 RGBG')
    out=np.empty((h*2,w*2),np.uint16)
    out[0::2,0::2]=planes[:,:,0]
    for y in range(h):
        yn=min(y+1,h-1)
        g=planes[y,:,1].astype(np.uint32)
        out[2*y,1::2]=((g+np.r_[g[1:],g[-1]]+1)//2).astype(np.uint16)
        out[2*y+1,0::2]=((planes[y,:,3].astype(np.uint32)+planes[yn,:,3]+1)//2).astype(np.uint16)
        b=planes[y,:,2].astype(np.uint32)+planes[yn,:,2]
        out[2*y+1,1::2]=((b+np.r_[b[1:],b[-1]]+2)//4).astype(np.uint16)
    return out


def run(folder):
    path=folder/'experimental-408mp-remosaic.dng'
    render=folder/'experimental-408mp-remosaic-render.npy'
    for p in (path,path.with_suffix('.dng.partial'),render):
        if p.exists(): raise FileExistsError(p)
    # 独立的解析斜坡检查：半像素采样与两路绿色保持独立。
    y,x=np.mgrid[:5,:6]
    fixture=np.stack([100+x*4+y*8,200+x*4+y*8,
                      300+x*4+y*8,400+x*4+y*8],axis=2).astype(np.uint16)
    f=remosaic(fixture)
    assert f[2,2]==112 and f[2,3]==214 and f[3,2]==416 and f[3,3]==318
    offsets=[(0,0),(0,1),(-1,0),(-1,1)]
    with ExitStack() as stack:
        raws=[stack.enter_context(rawpy.imread(str(folder/f'frame-{i}.3fr'))) for i in range(4)]
        arrays=[r.raw_image_visible for r in raws]
        h,w=arrays[0].shape;h-=1;w-=1
        if any(a.shape!=(h+1,w+1) for a in arrays):raise ValueError('Dimensions differ')
        if any(not np.array_equal(r.raw_pattern,[[0,1],[3,2]]) for r in raws):raise ValueError('CFA differs')
        planes=np.empty((h,w,4),np.uint16)
        for top in range(0,h,128):
            rows=min(128,h-top);counts=np.zeros((rows,w,4),np.uint8)
            for r,a,(dx,dy) in zip(raws,arrays,offsets):
                sy,sx=top+1-dy,-dx
                tile=a[sy:sy+rows,sx:sx+w]
                for py in range(2):
                    for px in range(2):
                        c=int(r.raw_pattern[(sy+py)%2,(sx+px)%2]);black=r.black_level_per_channel[c]
                        v=np.clip((tile[py::2,px::2].astype(float)-black)*65535/(r.white_level-black),0,65535)
                        planes[top+py:top+rows:2,px::2,c]=np.rint(v).astype(np.uint16)
                        counts[py::2,px::2,c]+=1
            if not np.all(counts==1):raise ValueError('RGBG samples missing or repeated')
        matrix=raws[0].rgb_xyz_matrix[:3].copy()
        neutral=1/np.array(raws[0].camera_whitebalance[:3],float)
    print('RGBG_RECONSTRUCTED',planes.shape,flush=True)
    mosaic=remosaic(planes);del planes
    height,width=mosaic.shape
    tags=[(274,'H',1,1,False),(33421,'H',2,(2,2),False),(33422,'B',4,(0,1,1,2),False),
          (50706,'B',4,(1,4,0,0),False),(50707,'B',4,(1,1,0,0),False),
          (50708,'s',0,'Experimental X2D interpolated remosaic',False),
          (50710,'B',3,(0,1,2),False),(50711,'H',1,1,False),
          (50713,'H',2,(1,1),False),(50714,'I',1,0,False),(50717,'I',1,65535,False),
          (50719,'I',2,(0,0),False),(50720,'I',2,(width,height),False),
          (50721,'2i',9,rational(matrix.ravel()),False),
          (50728,'2I',3,rational(neutral),False),(50778,'H',1,21,False)]
    tmp=path.with_suffix('.dng.partial')
    with tmp.open('xb') as stream:
        tifffile.imwrite(stream,mosaic,photometric=32803,metadata=None,
                         compression=None,rowsperstrip=32,extratags=tags,
                         software='Experimental four-shot bilinear remosaic')
        stream.flush();os.fsync(stream.fileno())
    with rawpy.imread(str(tmp)) as r:
        if not np.array_equal(r.raw_image_visible,mosaic):raise ValueError('Independent sample readback differs')
        if not np.array_equal(r.raw_pattern,[[0,1],[3,2]]):raise ValueError('Independent CFA interpretation differs')
    tmp.rename(path);del mosaic
    print('DNG_EXACT_READBACK_PASSED',width,height,flush=True)
    with rawpy.imread(str(path)) as r:
        rgb=r.postprocess(use_camera_wb=True,no_auto_bright=True,output_bps=8,
                          output_color=rawpy.ColorSpace.sRGB,
                          demosaic_algorithm=rawpy.DemosaicAlgorithm.AHD)
    with render.open('xb') as stream:np.save(stream,rgb)
    report={'experimental':True,'width':width,'height':height,'pixels':width*height,
            'interpolation':'bilinear at half-pixel positions, replicated outer boundary',
            'green_planes':'kept separate before RGGB sampling','independent_raw_readback_exact':True,
            'render':'LibRaw AHD, camera WB, sRGB, no auto brightness',
            'factory_calibration_complete':False,'new_spatial_detail_claimed':False}
    (folder/'remosaic-report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print('FULL_RESOLUTION_DEMOSAIC_COMPLETE',rgb.shape,flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('folder',type=Path);run(p.parse_args().folder)
