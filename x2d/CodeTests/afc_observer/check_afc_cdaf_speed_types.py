"""官方 4.2.0 AFC 反差选速：执行原厂选速及距离分类分支，离线不连接相机。"""
import json
import os
import struct
import sys
from pathlib import Path

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
sys.path[:0] = [str(ROOT/'x2d/CodeTests/subject_tracking/device_hybrid_test/af_probe'),
               str(ROOT/'.research-cache/x1d-1.25.0/python')]
import inspect_control_paths as paths
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm64_const import *

b = paths.load('aaa')
native = {name: addr for addr, (name, _) in paths.functions(b).items()}
A, D, S, STOP = 0x10000000, 0x30000000, 0x70000000, 0x60000000

def check(distance, values, enabled=True, following=None):
    u = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
    for addr, size in ((A, 0xa40000), (D, 0x10000), (S, 0x20000), (STOP, 0x1000)):
        u.mem_map(addr, size)
    for seg in b.elf.iter_segments():
        if seg['p_type'] == 'PT_LOAD':
            u.mem_write(A+seg['p_vaddr'], seg.data())
    # 固定固件参数模板的 AFC 子表；并非实机运行参数快照。
    params = bytearray(b.read(0x1e22b0+0x868+0x38, 0x38))
    assert params[0x32] == 1
    params[0x32] = int(enabled)
    u.mem_write(D, bytes(params))
    # 原厂模式记录：普通拍摄而非内部 recording 分类。
    u.mem_write(A+0xa290c8, struct.pack('<Q', D+0x1000))
    u.reg_write(UC_ARM64_REG_TPIDR_EL0, D+0x4000)
    u.reg_write(UC_ARM64_REG_CPACR_EL1, 3 << 20)
    selected = []
    def hook(uc, address, size, unused):
        name = b.names.get(address-A, '')
        if not name.endswith('@plt'):
            return
        name = name[:-4]
        x1 = uc.reg_read(UC_ARM64_REG_X1)
        if name == 'get_speed_type_from_peak_distance':
            uc.reg_write(UC_ARM64_REG_PC, A+native[name]); return
        if name == 'cdaf_get_cur_buf_indx': result = len(values)
        elif name == 'af_get_cdaf_special_param': result = D
        elif name == 'is_stand_recording_af_mode': result = 0
        elif name == 'alg_get_distance_to_peak': result = distance
        elif name == 'cdaf_getp_cur_buf_cfa_fv': result = D+0x2000+x1*8
        elif name == 'cdaf_get_indicator': result = values[(x1-D-0x2000)//8]
        elif name == 'cdaf_set_current_speed_type': selected.append(x1); result = 0
        else: raise AssertionError(name)
        uc.reg_write(UC_ARM64_REG_X0, result)
        uc.reg_write(UC_ARM64_REG_PC, uc.reg_read(UC_ARM64_REG_LR))
    u.hook_add(UC_HOOK_CODE, hook)
    sequence = []
    for next_values in [values]+(following or []):
        values = next_values
        u.reg_write(UC_ARM64_REG_SP, S+0x10000)
        u.reg_write(UC_ARM64_REG_LR, STOP)
        u.reg_write(UC_ARM64_REG_X0, 0)
        u.emu_start(A+native['cdaf_get_scan_speed_type'], STOP, count=10000)
        assert u.reg_read(UC_ARM64_REG_PC) == STOP
        assert u.reg_read(UC_ARM64_REG_SP) == S+0x10000
        sequence.append(u.reg_read(UC_ARM64_REG_W0))
    return sequence if following is not None else sequence[0]

results = []
for distance, values, enabled, expected in (
    (0, [100], True, 0),
    (0, [100, 110, 120], True, 0),
    (0, [100, 200, 250], True, 1),
    (1, [100], True, 1),
    (2, [100], True, 2),
    (3, [100], True, 0),
    (2, [100], False, 0),
    (2, [100, 200, 250], False, 1),
):
    got = check(distance, values, enabled)
    assert got == expected, (distance, values, got, expected)
    results.append(dict(distance_class=distance, fv=values, distance_selector=enabled, result=got))

# 同一选速实例：无有效距离分类时，仅 FV 变化减小便能由慢升快。
sequence = check(0, [100, 200, 250], following=[[200, 220, 230], [220, 230, 235]])
assert sequence == [1, 0, 0]
results.append(dict(case='slow-to-fast-without-confirmed-far-distance', result=sequence))

recorded_types = None
if not os.environ.get('X2D_SIM_CASE_OUT'):
    events = json.loads((ROOT/'x2d/outputs/4.2.0/afc-center-analysis/roi200-hunting-4280820b.json').read_text())['events']
    cdaf = [e for e in events if e[1] == 10 and e[7] == 0x64998 and e[11]]
    recorded_types = sorted({e[11]-1 for e in cdaf})
    assert recorded_types == [0, 1]
out = ROOT/'x2d/outputs/4.2.0/afc-low-info-analysis/afc-cdaf-speed-types.json'
if os.environ.get('X2D_SIM_CASE_OUT'):
    out = Path(os.environ['X2D_SIM_CASE_OUT'])/'afc-cdaf-speed-types.json'
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(dict(firmware='X2D 100C 4.2.0', passed=len(results), cases=results,
    frozen_cdaf_speed_types=recorded_types,
    caveat='原厂选速与距离映射真实执行；参数模板和统计输入为离线替身。二档未在本轮实机反差发送记录中出现。',
    device_requests=0), ensure_ascii=False, indent=2), encoding='utf-8')
print(f'AFC_CDAF_SPEED_TYPES: {len(results)} native cases passed; recorded types {recorded_types}')
