"""固定相机服务的普通子进程启动；保留原有身份和安全策略。"""
FLAG=b'    setenv X2D_ESFLASH_CAMERA_CHILD 1\n'
HELPER=b'/system/lib64/libx2d_esflash_hotshoe.so'
def patch_camera_service(data):
    assert isinstance(data,bytes) and b'\0' not in data and b'\r' not in data
    first=b'service camera-service /system/bin/camera-service\n'
    assert data.startswith(first) and data.count(b'\nservice ')==0
    assert b'X2D_ESFLASH_CAMERA_CHILD' not in data and HELPER not in data
    rows=data.splitlines(keepends=True)
    preload=[row for row in rows if row.startswith(b'    setenv LD_PRELOAD ')]
    assert len(preload)==1 and data.count(b'LD_PRELOAD')==1
    old=preload[0];new=old.replace(b'LD_PRELOAD ',b'LD_PRELOAD '+HELPER+b':',1)
    result=data.replace(old,new).replace(first,first+FLAG,1)
    assert result.count(FLAG)==1 and result.count(HELPER)==1
    assert result.replace(FLAG,b'').replace(new,old)==data
    return result
