"""离线执行 Android Phocus 1.0.2 的少量原厂指令，验证坐标尺度。

不是 APK 完整运行或照片解码：TIFF 读取与径向校正以明确的替身隔离。
不连接设备、不修改照片。标准输出为证据 JSON。
"""
import hashlib
import io
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path[:0] = [str(ROOT / '.research-cache/python'),
               str(ROOT / '.research-cache/x1d-1.25.0/python')]
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm64_const import *

BASE = ROOT / 'x2d/outputs/4.2.0/phocus-mobile-analysis'
DATA = (BASE / 'native/libcrosssdk.so').read_bytes()
ELF = ELFFile(io.BytesIO(DATA))
M = 0x10000000
STOP = M + 0x1000


def machine():
    u = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
    segments = [s for s in ELF.iter_segments() if s['p_type'] == 'PT_LOAD']
    # One mapping avoids quadratic overhead of thousands of Unicorn regions.
    end = max(s['p_vaddr'] + s['p_memsz'] for s in segments)
    assert end < M
    u.mem_map(0, (end + 4095) & ~4095)
    for seg in segments:
        u.mem_write(seg['p_vaddr'], seg.data())
    u.mem_map(M, 0x40000)
    u.reg_write(UC_ARM64_REG_SP, M + 0x30000)
    u.reg_write(UC_ARM64_REG_TPIDR_EL0, M + 0x2000)
    u.reg_write(UC_ARM64_REG_LR, STOP)
    # Enable FP/Advanced SIMD in the emulated CPU.
    u.reg_write(UC_ARM64_REG_CPACR_EL1, 3 << 20)
    return u


def run_case(tag):
    u = machine()
    file_obj, desc, correction = M + 0x4000, M + 0x5000, M + 0x6000
    point, result = M + 0x7000, M + 0x7100
    seen = []
    u.mem_write(file_obj + 0x50, struct.pack('<Q', M + 0x8000))

    def hook(uc, address, size, user):
        if address == 0x3746c80:  # GetMultiIntegerTag substitute
            assert uc.reg_read(UC_ARM64_REG_W1) == 3
            assert uc.reg_read(UC_ARM64_REG_W2) == 0x21
            if tag is not None:
                uc.mem_write(uc.reg_read(UC_ARM64_REG_X3), struct.pack('<I', tag))
            uc.reg_write(UC_ARM64_REG_W0, int(tag is not None))
        elif address == 0x3738b10:  # CStreamable destructor substitute
            pass
        elif address == 0x37411d0:  # GetMove: capture input, return synthetic delta
            p = uc.reg_read(UC_ARM64_REG_X1)
            seen.append(struct.unpack('<2d', uc.mem_read(p + 0x10, 16)))
            out = uc.reg_read(UC_ARM64_REG_X2)
            uc.mem_write(out + 0x10, struct.pack('<2d', 3.0, -5.0))
            uc.reg_write(UC_ARM64_REG_W0, 1)
        else:
            return
        uc.reg_write(UC_ARM64_REG_PC, uc.reg_read(UC_ARM64_REG_LR))

    u.hook_add(UC_HOOK_CODE, hook)
    u.reg_write(UC_ARM64_REG_X0, file_obj)
    u.emu_start(0x3544344, STOP, count=1000)
    assert u.reg_read(UC_ARM64_REG_PC) == STOP
    shots = u.reg_read(UC_ARM64_REG_W0)
    assert shots == (1 if tag is None else tag)
    u.mem_write(desc + 0xe0, struct.pack('<I', shots))
    u.reg_write(UC_ARM64_REG_X21, desc)
    u.reg_write(UC_ARM64_REG_X19, correction)
    # Execute the original constructor's scale-selection block only.
    u.emu_start(0x3391bd0, 0x3391be8, count=20)
    scale, = struct.unpack('<I', u.mem_read(correction + 0x30, 4))
    assert scale == (2 if shots > 4 else 1)
    u.mem_write(point + 0x10, struct.pack('<2d', 16000.0, 12000.0))
    for reg, val in [(UC_ARM64_REG_X0, correction), (UC_ARM64_REG_X1, point),
                     (UC_ARM64_REG_X2, result), (UC_ARM64_REG_LR, STOP)]:
        u.reg_write(reg, val)
    u.emu_start(0x3393404, STOP, count=2000)
    assert u.reg_read(UC_ARM64_REG_PC) == STOP
    out = struct.unpack('<2d', u.mem_read(result + 0x10, 16))
    assert seen == [(16000.0 / scale, 12000.0 / scale)]
    assert out == (16000.0 + 3.0 * scale, 12000.0 - 5.0 * scale)
    return dict(tag_0x21_first_value=tag, parsed_shots=shots, scale=scale,
                input_to_radial_model=seen[0], synthetic_corrected_output=out)


if __name__ == '__main__':
    expected = 'acd01663adb117bc1f8e89060c81c64858e11019e49f8cc80ad6b7dd082a3813'
    assert hashlib.sha256((BASE / 'phocus-mobile-1.0.2.apk').read_bytes()).hexdigest() == expected
    assert hashlib.sha256(DATA).hexdigest() == 'c9de0142fb2146c2da50affa48e37e35833a96a15ee698090688039655aa7ca4'
    print(json.dumps(dict(apk_sha256=expected,
        library_sha256=hashlib.sha256(DATA).hexdigest(),
        scope='original ARM64 GetShots, scale-selection block and coordinate mapping; TIFF and radial model stubbed; no full decode or device access',
        cases=[run_case(t) for t in [None, 1, 4, 6]]), indent=2))
