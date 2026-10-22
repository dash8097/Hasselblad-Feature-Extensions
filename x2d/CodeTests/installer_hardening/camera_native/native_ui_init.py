"""固定全功能原生界面的 init 配置；不生成或执行 shell 脚本。"""
LEGACY=('X2D_NATIVE_MENU','X2D_AFC_ENTRY','X2D_HD_PLAYBACK','QML_XHR_ALLOW_FILE_READ','QML_XHR_ALLOW_FILE_WRITE')
PRELOAD=b'    setenv LD_PRELOAD /system/lib64/libx2d_native_menu.so\n'
FEATURES=b'    setenv X2D_NATIVE_UI_FEATURES 255\n'
def native_init(data):
    assert isinstance(data,bytes) and b'\0' not in data and b'\r' not in data
    assert data.splitlines(keepends=True).count(PRELOAD)==1 and data.count(b'LD_PRELOAD')==1 and b'X2D_NATIVE_UI_FEATURES' not in data,'native UI init scope changed'
    for name in LEGACY:
        token=name.encode();line=b'    setenv '+token+b' 1\n'
        # Reject duplicate, commented and malformed declarations as a changed
        # source instead of silently choosing a different effective value.
        assert data.count(token)==1 and data.splitlines(keepends=True).count(line)==1,'legacy UI init changed: '+name
        data=data.replace(line,b'')
    data=data.replace(PRELOAD,PRELOAD+FEATURES)
    verify_native_init(data)
    return data
def verify_native_init(data):
    assert isinstance(data,bytes) and b'\0' not in data and b'\r' not in data
    assert data.splitlines(keepends=True).count(PRELOAD)==1 and data.splitlines(keepends=True).count(FEATURES)==1
    assert data.count(b'LD_PRELOAD')==1
    assert data.count(b'X2D_NATIVE_UI_FEATURES')==1
    assert all(name.encode() not in data for name in LEGACY)
    return True
