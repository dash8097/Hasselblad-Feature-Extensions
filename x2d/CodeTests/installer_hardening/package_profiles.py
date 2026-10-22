"""R7 两档安装配置；构建期与编译后的主机客户端共用，不访问设备。

沿用旧功能位以识别历史安装记录：248 为基础，255 为高级。
电子快门闪光是共同基础功能，不增加第三个可选项。
"""
from copy import deepcopy
from pathlib import PurePosixPath

BASIC = 248
ADVANCED = 255
DEFAULT = BASIC
PROFILES = (
    {"id": "basic", "mask": BASIC, "label": "基础", "default": True,
     "description": "电子快门闪光、对焦增强、人眼与 AFC、高清回放、显示及快门效果"},
    {"id": "advanced", "mask": ADVANCED, "label": "高级", "default": False,
     "description": "包含全部基础功能，并加入像素超频、25MP、RAW／ISO 堆栈"},
)

_ADVANCED_DIRS = (
    "/system/bin/x2d-raw-stack/", "/system/etc/x2d-raw-stack/",
    "/system/bin/x2d-pixel-overclock/", "/system/etc/x2d-pixel-overclock/",
    "/system/bin/x2d-memory-manual-r17/", "/system/bin/x2d-native-disk400/",
    "/system/bin/x2d-multi-resolution/", "/system/etc/x2d-multi-resolution/",
)
_ADVANCED_FILES = frozenset((
    "/system/lib64/libx2d_af_probe.so",
    "/system/lib64/libx2d_quarter_native.so",
    "/system/lib64/libx2d_quarter_upstream.so",
    "/system/bin/x2d-quarter-native-check",
))
_BASIC_FILES = frozenset((
    "/system/lib64/libx2d_af_base0.so",
    "/system/lib64/libx2d_afc_phase.so",
    "/system/lib64/libx2d_afc_roi.so",
    "/system/lib64/libx2d_afc_settle.so",
    "/system/lib64/libx2d_face_lock.so",
    "/system/lib64/libx2d_face_continuity.so",
    "/system/lib64/libx2d_face_publication.so",
    "/system/lib64/libx2d_native_menu.so",
    "/system/lib64/libx2d_audio_preview.so",
    "/system/etc/x2d-ciallo.wav",
    "/system/etc/init/x2d-shutter-audio.rc",
    "/system/etc/init/camera-service.rc",
    "/system/etc/init/camera-gui.rc",
    "/system/etc/init/camera-storage.rc",
    "/system/etc/x2d-feature-mask",
    "/system/bin/x2d-native-flow",
    "/system/bin/x2d-esflash",
    "/system/lib64/libx2d_esflash_hotshoe.so",
    "/system/etc/init/x2d-esflash.rc",
))


def profile(mask):
    if type(mask) is not int or mask not in (BASIC, ADVANCED):
        raise ValueError("请选择基础或高级安装。")
    return deepcopy(next(p for p in PROFILES if p["mask"] == mask))


def required_profile(path):
    """未知文件须显式分类，避免新捕获后台误入基础版。"""
    if (not isinstance(path, str) or not path.startswith('/system/')
            or '\\' in path or '\0' in path
            or str(PurePosixPath(path)) != path
            or any(p in ('', '.', '..') for p in path.split('/')[1:])):
        raise ValueError("安装目标路径无效")
    if path in _BASIC_FILES:
        return BASIC
    if path in _ADVANCED_FILES or path.startswith(_ADVANCED_DIRS):
        return ADVANCED
    raise ValueError("安装目标尚未分类：" + path)


def selected_targets(targets, mask):
    profile(mask)
    paths = [r['path'] for r in targets]
    if len(paths) != len(set(paths)):
        raise ValueError("安装目标重复")
    return [deepcopy(r) for r in targets
            if required_profile(r['path']) == BASIC or mask == ADVANCED]


def service_init(data, mask):
    """基础版直接加载已调优的速度库，不装捕获桥或改变两套速度。"""
    profile(mask)
    if not isinstance(data, bytes) or b'\r' in data or b'\0' in data:
        raise ValueError("camera-service init 格式变化")
    wrapper = b'/system/lib64/libx2d_af_probe.so'
    base = b'/system/lib64/libx2d_af_base0.so'
    if data.count(wrapper) != 1 or base in data:
        raise ValueError("camera-service preload 基线变化")
    return data.replace(wrapper, base) if mask == BASIC else data
