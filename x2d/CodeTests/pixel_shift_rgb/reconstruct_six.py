"""六张整数/半像素采样的实验性 GRBG 重建，不是 Phocus 算法。"""
from contextlib import ExitStack
from pathlib import Path
import argparse,json,os
from linear_dng import np
import rawpy,tifffile


def reconstruct(folder):
    out=folder/'experimental-six-shot-408mp.dng'
    if out.exists() or out.with_suffix('.dng.partial').exists():raise FileExistsError(out)
    offsets=[(0,0),(0,1),(-1,0),(-1,1)]
    with ExitStack() as stack:
        raws=[stack.enter_context(rawpy.imread(str(folder/f'frame-{i}.3fr'))) for i in range(6)]
        arrays=[r.raw_image_visible for r in raws]
        h,w=arrays[0].shape;h-=1;w-=1
        if any(a.shape!=(h+1,w+1) for a in arrays):raise ValueError('Dimensions differ')
        if any(not np.array_equal(r.raw_pattern,[[0,1],[3,2]]) for r in raws):raise ValueError('CFA differs')
        full=np.empty((h,w,4),np.uint16)
        half=np.zeros((h,w,3),np.uint16);valid=np.zeros((h,w,3),np.uint8)
        def normalized(r,t,c):
            black=r.black_level_per_channel[c]
            return np.rint(np.clip((t.astype(float)-black)*65535/(r.white_level-black),0,65535)).astype(np.uint16)
        for top in range(0,h,128):
            rows=min(128,h-top);counts=np.zeros((rows,w,4),np.uint8)
            for r,a,(dx,dy) in zip(raws[:4],arrays[:4],offsets):
                sy,sx=top+1-dy,-dx;t=a[sy:sy+rows,sx:sx+w]
                for py in range(2):
                    for px in range(2):
                        c=int(r.raw_pattern[(sy+py)%2,(sx+px)%2])
                        full[top+py:top+rows:2,px::2,c]=normalized(r,t[py::2,px::2],c)
                        counts[py::2,px::2,c]+=1
            if not np.all(counts==1):raise ValueError('Incomplete integer-grid RGBG')
            # 输出 odd/odd 位置为场景 (x+.5, y+1.5)。
            for i,sy in ((4,top+1),(5,top)):
                r=raws[i];t=arrays[i][sy:sy+rows,1:1+w]
                for py in range(2):
                    for px in range(2):
                        c=int(r.raw_pattern[(sy+py)%2,(1+px)%2]);ch=1 if c==3 else c
                        half[top+py:top+rows:2,px::2,ch]=normalized(r,t[py::2,px::2],c)
                        valid[top+py:top+rows:2,px::2,ch]+=1
        if not np.all(valid[:,:,1]==1) or not np.all(valid[:,:,0]+valid[:,:,2]==1):
            raise ValueError('Half-grid should contain green and exactly one red/blue sample')
    print('SIX_MEASURED_LATTICES_VERIFIED',flush=True)
    mosaic=np.empty((2*h,2*w),np.uint16)
    # GRBG 保留整数格点与半像素对角格点上的实测绿色。
    for y in range(h):
        yp=max(y-1,0);yn=min(y+1,h-1)
        mosaic[2*y,0::2]=((full[y,:,1].astype(np.uint32)+full[y,:,3]+1)//2).astype(np.uint16)
        mosaic[2*y+1,1::2]=half[y,:,1]
        r=full[y,:,0].astype(np.float64)
        num=r+np.r_[r[1:],r[-1]]+half[yp,:,0]*valid[yp,:,0]+half[y,:,0]*valid[y,:,0]
        den=2.+valid[yp,:,0]+valid[y,:,0]
        mosaic[2*y,1::2]=np.rint(num/den).astype(np.uint16)
        b=full[y,:,2].astype(np.float64)+full[yn,:,2]
        ex=half[y,:,2].astype(float)*valid[y,:,2]
        num=b+ex+np.r_[ex[0],ex[:-1]]
        den=2.+valid[y,:,2]+np.r_[valid[y,0,2],valid[y,:-1,2]]
        mosaic[2*y+1,0::2]=np.rint(num/den).astype(np.uint16)
    del full,half,valid
    with tifffile.TiffFile(folder/'frame-0.3fr') as t:
        matrix=t.pages[0].tags[50721].value;neutral=t.pages[0].tags[50728].value
    height,width=mosaic.shape
    tags=[(274,'H',1,1,False),(33421,'H',2,(2,2),False),(33422,'B',4,(1,0,2,1),False),
          (50706,'B',4,(1,4,0,0),False),(50707,'B',4,(1,1,0,0),False),
          (50708,'s',0,'Experimental X2D six shot reconstruction',False),
          (50710,'B',3,(0,1,2),False),(50711,'H',1,1,False),(50713,'H',2,(1,1),False),
          (50714,'I',1,0,False),(50717,'I',1,65535,False),(50719,'I',2,(0,0),False),
          (50720,'I',2,(width,height),False),(50721,'2i',9,matrix,False),
          (50728,'2I',3,neutral,False),(50778,'H',1,0,False)]
    tmp=out.with_suffix('.dng.partial')
    with tmp.open('xb') as f:
        tifffile.imwrite(f,mosaic,photometric=32803,metadata=None,compression=None,
                        rowsperstrip=32,extratags=tags,software='Experimental six shot weighted sampling')
        f.flush();os.fsync(f.fileno())
    with rawpy.imread(str(tmp)) as r:
        if not np.array_equal(r.raw_image_visible,mosaic):raise ValueError('Raw readback differs')
        colors=r.color_desc
        if [[chr(colors[int(c)]) for c in row] for row in r.raw_pattern]!=[['G','R'],['B','G']]:
            raise ValueError('Decoded CFA differs')
    tmp.rename(out);del mosaic
    print('SIX_DNG_EXACT_READBACK',width,height,flush=True)
    with rawpy.imread(str(out)) as r:
        rgb=r.postprocess(use_camera_wb=True,no_auto_bright=True,output_bps=8,
                          output_color=rawpy.ColorSpace.sRGB,demosaic_algorithm=rawpy.DemosaicAlgorithm.AHD)
    with (folder/'six-shot-render.npy').open('xb') as f:np.save(f,rgb)
    report={'experimental':True,'frames_used':6,'dimensions':[width,height],
            'green':'measured integer lattice mean and measured half-pixel diagonal lattice',
            'red_blue':'nearest axial measured samples averaged at remaining CFA sites',
            'boundary':'replicated nearest boundary','raw_readback_exact':True,
            'color':'source 3FR matrix and neutral','phocus_equivalence':False,
            'subpixel_registration_correction':False,'motion_correction':False}
    (folder/'six-reconstruction-report.json').write_text(json.dumps(report,indent=2))
    print('SIX_RENDER_COMPLETE',flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('folder',type=Path);reconstruct(p.parse_args().folder)
