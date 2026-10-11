"""十六张半像素 RGBG 重建，再插值为约十六亿采样点 CFA DNG。"""
from pathlib import Path
import argparse,io,json,os,struct
from linear_dng import np
import tifffile,rawpy
from remosaic_trial import remosaic

POSITIONS=[(0,0),(0,1),(0,2),(0,3),(1,3),(1,2),(1,1),(1,0),
           (2,0),(2,1),(2,2),(2,3),(3,3),(3,2),(3,1),(3,0)]


def tags_for(w,h,matrix,neutral):
    return [(274,'H',1,1,False),(33421,'H',2,(2,2),False),(33422,'B',4,(0,1,1,2),False),
            (50706,'B',4,(1,4,0,0),False),(50707,'B',4,(1,1,0,0),False),
            (50708,'s',0,'Experimental X2D sixteen shot resampled CFA',False),
            (50710,'B',3,(0,1,2),False),(50711,'H',1,1,False),(50713,'H',2,(1,1),False),
            (50714,'I',1,0,False),(50717,'I',1,65535,False),(50719,'I',2,(0,0),False),
            (50720,'I',2,(w,h),False),(50721,'2i',9,matrix,False),
            (50728,'2I',3,neutral,False),(50778,'H',1,0,False)]


def run(folder):
    output=folder/'experimental-sixteen-shot-1632mp.dng'
    if output.exists():raise FileExistsError(output)
    with rawpy.imread(str(folder/'frame-0.3fr')) as r:
        sh,sw=r.raw_image_visible.shape
    h,w=2*sh-4,2*sw-4
    pp=folder/'half-grid-rgbg.npy'
    if pp.exists():raise FileExistsError(pp)
    planes=np.lib.format.open_memmap(pp,mode='w+',dtype=np.uint16,shape=(h,w,4))
    coverage=np.zeros((4,4,4),np.uint8)
    matrix=neutral=None
    for i,(dx,dy) in enumerate(POSITIONS):
        src=folder/f'frame-{i}.3fr'
        with src.open('rb') as f:head=f.read(65536)
        e='<' if head[:2]==b'II' else '>';n=struct.unpack_from(e+'H',head,1428)[0];found=False
        for j in range(n):
            tag,typ,count,off=struct.unpack_from(e+'HHII',head,1430+12*j)
            if tag==0x21:
                if struct.unpack_from(e+'ii',head,off)!=(16,i):raise ValueError('Group mismatch')
                found=True
        if not found:raise ValueError('Group tag absent')
        with tifffile.TiffFile(src) as t:
            m=t.pages[0].tags[50721].value;v=t.pages[0].tags[50728].value
            if i==0:matrix,neutral=m,v
            elif m!=matrix or v!=neutral:raise ValueError('Color metadata varies')
        sx=(dx+1)//2;sy=(4-dy)//2;hx=2*sx-dx;hy=2*sy+dy-3
        with rawpy.imread(str(src)) as r:
            if r.raw_image_visible.shape!=(sh,sw) or not np.array_equal(r.raw_pattern,[[0,1],[3,2]]):
                raise ValueError('Source geometry changed')
            a=r.raw_image_visible
            for py in range(2):
                for px in range(2):
                    c=int(r.raw_pattern[(sy+py)%2,(sx+px)%2])
                    coverage[(hy+2*py)%4,(hx+2*px)%4,c]+=1
            for top in range(0,h//2,128):
                nr=min(128,h//2-top);tile=a[sy+top:sy+top+nr,sx:sx+w//2]
                for py in range(2):
                    for px in range(2):
                        c=int(r.raw_pattern[(sy+top+py)%2,(sx+px)%2]);black=r.black_level_per_channel[c]
                        value=np.rint(np.clip((tile[py::2,px::2].astype(float)-black)*65535/(r.white_level-black),0,65535)).astype(np.uint16)
                        planes[hy+2*(top+py):hy+2*(top+nr):4,hx+2*px::4,c]=value
        print('FRAME_INTEGRATED',i+1,flush=True)
    if not np.all(coverage==1):raise ValueError('RGBG coverage incomplete')
    planes.flush()
    mosaic=remosaic(planes);del planes
    height,width=mosaic.shape
    tags=tags_for(width,height,matrix,neutral)
    partial=output.with_suffix('.dng.partial')
    with partial.open('xb') as f:
        tifffile.imwrite(f,mosaic,photometric=32803,metadata=None,compression=None,
                        rowsperstrip=32,extratags=tags,software='Experimental sixteen shot half-grid reconstruction')
        f.flush();os.fsync(f.fileno())
    disk=tifffile.memmap(partial)
    for y in range(0,height,128):
        if not np.array_equal(disk[y:y+128],mosaic[y:y+128]):raise ValueError('TIFF sample readback differs')
    del disk,mosaic
    partial.rename(output)
    print('LARGE_CFA_DNG_VERIFIED',width,height,flush=True)
    source=tifffile.memmap(output)
    dest=folder/'sixteen-shot-render.npy'
    if dest.exists():raise FileExistsError(dest)
    rgb=np.lib.format.open_memmap(dest,mode='w+',dtype=np.uint8,shape=(height,width,3))
    # 分条独立 LibRaw 去马赛克，32 行重叠避免 AHD 条带边缘影响。
    for y in range(0,height,512):
        end=min(y+512,height);a=max(0,y-32);b=min(height,end+32)
        buffer=io.BytesIO()
        tifffile.imwrite(buffer,source[a:b],photometric=32803,metadata=None,compression=None,
                        rowsperstrip=32,extratags=tags_for(width,b-a,matrix,neutral))
        buffer.seek(0)
        with rawpy.imread(buffer) as r:
            if not np.array_equal(r.raw_image_visible,source[a:b]):raise ValueError('Independent tile raw readback differs')
            tile=r.postprocess(use_camera_wb=True,no_auto_bright=True,adjust_maximum_thr=0.0,output_bps=8,
                               output_color=rawpy.ColorSpace.sRGB,demosaic_algorithm=rawpy.DemosaicAlgorithm.AHD)
        rgb[y:end]=tile[y-a:end-a]
        if y%4096==0:print('RENDERED_ROWS',end,height,flush=True)
    rgb.flush();del rgb,source
    report={'frames':16,'half_grid_rgbg':[w,h],'output_cfa':[width,height],
            'raw_readback':'whole TIFF exact; all overlapping strips exact with LibRaw',
            'render':'LibRaw AHD strips with 32-row halo; no auto brightness; fixed maximum threshold',
            'resampled_output':True,'native_1_6_gigapixel_detail':False,
            'factory_pipeline_complete':False,'motion_correction':False}
    (folder/'sixteen-reconstruction-report.json').write_text(json.dumps(report,indent=2))
    print('SIXTEEN_RENDER_COMPLETE',flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('folder',type=Path);run(p.parse_args().folder)
