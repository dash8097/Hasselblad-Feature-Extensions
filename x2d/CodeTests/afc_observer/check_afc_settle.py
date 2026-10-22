"""执行候选 AArch64 与官方 4.2.0 函数；硬件接口使用替身，无相机请求。"""
from pathlib import Path
import hashlib, io, json, struct, sys, os
sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
one_frame='--one-frame' in sys.argv
OUT = ROOT/('x2d/outputs/4.2.0/afc-type1-3-r1/settle' if one_frame else 'x2d/outputs/4.2.0/afc-settle-33ms-r2')
if '--startup-order' in sys.argv: OUT = ROOT/'x2d/outputs/4.2.0/afc-startup-order-r1/settle'
if os.environ.get('X2D_SIM_CASE_OUT'):
    OUT = Path(os.environ['X2D_SIM_CASE_OUT'])
sys.path[:0] = [str(ROOT/'x2d/CodeTests/subject_tracking/device_hybrid_test/af_probe'),
               str(ROOT/'.research-cache/x1d-1.25.0/python')]
import inspect_control_paths as paths
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm64_const import *
b = paths.load('aaa')
e = ELFFile(io.BytesIO((OUT/'libx2d_afc_settle.so').read_bytes()))
symbols = {s.name: s['st_value'] for sec in ('.symtab', '.dynsym')
           for s in e.get_section_by_name(sec).iter_symbols() if s['st_value']}
native = {n: a for a, (n, _) in paths.functions(b).items()}
dyn = b.elf.get_section_by_name('.dynsym')
got = {dyn.get_symbol(r['r_info_sym']).name: r['r_offset']
       for r in b.elf.get_section_by_name('.rela.plt').iter_relocations()}
A, L, D, S, T, H = 0x10000000, 0x40000000, 0x30000000, 0x70000000, 0x60000000, 0x6000f000
P, C, AF = D, D+4096, D+8192
PARAMS = b.read(0x1e22b0+0xcb0, 0x178)
QUICK = A+native['cdaf_scene_change_quick']
SLOT = A+got['cdaf_scene_change_quick']
def machine(case='success', fps=60, mode=2, zoom=0):
    u = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
    for addr, size in ((A, 0xa40000), (L, 0x200000), (D, 0x10000), (S, 0x20000), (T, 0x10000)):
        u.mem_map(addr, size)
    for elf, base in ((b.elf, A), (e, L)):
        for seg in elf.iter_segments():
            if seg['p_type'] == 'PT_LOAD': u.mem_write(base+seg['p_vaddr'], seg.data())
    imports = {}
    for sec in e.iter_sections():
        if sec['sh_type'] != 'SHT_RELA': continue
        table = e.get_section(sec['sh_link'])
        for r in sec.iter_relocations():
            if r['r_info_type'] == 1027: value = L+r['r_addend']
            else:
                symbol = table.get_symbol(r['r_info_sym'])
                if symbol['st_shndx'] != 'SHN_UNDEF': value = L+symbol['st_value']+r['r_addend']
                else:
                    value = T+0x100+len(imports)*16
                    imports[value] = symbol.name
            u.mem_write(L+r['r_offset'], struct.pack('<Q', value))
    u.mem_write(L+symbols['base'], struct.pack('<Q', A))
    u.mem_write(SLOT, struct.pack('<Q', QUICK))
    u.mem_write(P, PARAMS)
    u.mem_write(A+0xa290c8, struct.pack('<Q', AF))
    u.mem_write(A+0xa290d0, struct.pack('<Q', AF))
    u.mem_write(AF+9, bytes([mode]))
    u.mem_write(AF+0x74, struct.pack('<f', fps))
    u.mem_write(C+8, struct.pack('<Q', 100))
    u.reg_write(UC_ARM64_REG_TPIDR_EL0, D+0x4000)
    u.reg_write(UC_ARM64_REG_CPACR_EL1, 3 << 20)
    z = dict(fv=200, protects=0, native_calls=0, state_writes=0, library_polls=0, sleeps=0)
    if case == 'mismatch': u.mem_write(QUICK, bytes([b.read(native['cdaf_scene_change_quick'],1)[0]^255]))
    if case == 'conflict': u.mem_write(SLOT, struct.pack('<Q', L))
    def hook(uc, address, size, ignored):
        if address == QUICK: z['native_calls'] += 1
        if address == A+native['cdaf_common_private']:
            uc.reg_write(UC_ARM64_REG_X0, 0 if case == 'no-common' else C)
            uc.reg_write(UC_ARM64_REG_PC, uc.reg_read(UC_ARM64_REG_LR)); return
        name = b.names.get(address-A, '')
        imported = imports.get(address)
        if imported: name = imported
        elif name.endswith('@plt'): name = name[:-4]
        else: return
        x, y, v = [uc.reg_read(k) for k in (UC_ARM64_REG_X0, UC_ARM64_REG_X1, UC_ARM64_REG_X2)]
        result = 0
        if name == 'af_get_cur_fps':
            uc.reg_write(UC_ARM64_REG_PC, A+native[name]); return
        if name == 'memset': uc.mem_write(x, bytes([y&255])*v); result = x
        elif name == 'af_get_afc_param': result = P
        elif name == 'cdaf_common_private': result = C
        elif name == 'cdaf_get_indicator': result = z['fv']
        elif name == 'af_is_zoom_tracking_af_busy': result = zoom
        elif name in ('cdaf_stat_calc','log_mgr_check'): pass
        elif name == 'mprotect':
            z['protects'] += 1
            if case == 'rw-fail' and z['protects'] == 1: result = -1
            if case == 'ro-fail' and z['protects'] == 2: result = -1
            if case == 'ro-persistent' and z['protects'] >= 2: result = -1
        elif name == 'getenv': result = 0  # 未启用时构造函数必须无副作用。
        elif name == 'dl_iterate_phdr':
            z['library_polls'] += 1
            if case != 'startup-timeout' and z['library_polls'] == 3:
                uc.mem_write(L+symbols['base'], struct.pack('<Q', A))
        elif name == 'nanosleep': z['sleeps'] += 1
        elif name in ('snprintf','getpid','write','close'): pass
        elif name == 'open': result = -1
        else: raise AssertionError(name)
        uc.reg_write(UC_ARM64_REG_X0, result&0xffffffffffffffff)
        uc.reg_write(UC_ARM64_REG_PC, uc.reg_read(UC_ARM64_REG_LR))
    u.hook_add(UC_HOOK_CODE, hook)
    return u, z
def call(u, name, channel=0):
    u.reg_write(UC_ARM64_REG_SP, S+0x10000)
    u.reg_write(UC_ARM64_REG_LR, H)
    u.reg_write(UC_ARM64_REG_X0, channel)
    u.emu_start(L+symbols[name], H, count=50000)
    assert u.reg_read(UC_ARM64_REG_PC) == H
    assert u.reg_read(UC_ARM64_REG_SP) == S+0x10000
    return u.reg_read(UC_ARM64_REG_W0)
results = []
for case in ('startup-delayed','startup-timeout'):
    u,z = machine(case)
    u.mem_write(L+symbols['base'],struct.pack('<Q',0))
    call(u,'startup')
    target=struct.unpack('<Q',u.mem_read(SLOT,8))[0]
    assert target==(L+symbols['quick'] if case=='startup-delayed' else QUICK)
    assert z['library_polls']==(3 if case=='startup-delayed' else 120)
    assert z['sleeps']==(2 if case=='startup-delayed' else 120)
    results.append(case)
for case, expected in [('success',0),('mismatch',2),('conflict',3),('rw-fail',4),('ro-fail',5),('ro-persistent',6)]:
    u,z = machine(case)
    assert call(u,'attach') == expected
    target = struct.unpack('<Q',u.mem_read(SLOT,8))[0]
    assert target == (L+symbols['quick'] if case=='success' else L if case=='conflict' else QUICK)
    results.append('attach-'+case)
u,z = machine(); before = bytes(u.mem_read(SLOT,8)); call(u,'install')
assert bytes(u.mem_read(SLOT,8)) == before and z['protects'] == 0
results.append('disabled-constructor-no-side-effects')
for fps in (24,30,60,120):
    for mode,zoom,channel in ((2,0,0),(0,0,0),(1,0,0),(2,1,0),(2,0,1)):
        u,z = machine(fps=fps,mode=mode,zoom=zoom)
        target = (1 if one_frame else max(1,int(fps/30+.5))) if (mode,zoom,channel)==(2,0,0) else int(fps*.1+.5)
        count = z['native_calls']; assert call(u,'quick',channel)==1
        sequence = [call(u,'quick',channel) for _ in range(target)]
        assert sequence == [1]*(target-1)+[0], (fps,mode,zoom,channel,sequence)
        assert z['native_calls']-count == target+1
        # 连续快速变化仍拒绝；重新稳定后才计算新的等待。
        for frame in range(20):
            z['fv']=100 if frame%2==0 else 200
            assert call(u,'quick',channel)==1
        sequence = [call(u,'quick',channel) for _ in range(target)]
        assert sequence == [1]*(target-1)+[0]
        assert bytes(u.mem_read(P,len(PARAMS))) == PARAMS
        results.append(f'native-{fps}fps-mode{mode}-zoom{zoom}-channel{channel}')
# 无有效帧率、异常帧率、无上下文时不提前放行。
for fps,case in ((0,'success'),(300,'success'),(60,'no-common')):
    u,z=machine(case=case,fps=fps)
    if fps==0:
        assert call(u,'quick')==1
        assert call(u,'quick')==0  # 原厂本来已经放行，候选原样返回。
    else:
        assert call(u,'quick')==1
        assert call(u,'quick')==1
        assert call(u,'quick')==1
    results.append(f'fallback-{fps}-{case}')
record = dict(passed=True,cases=results,case_count=len(results),device_requests=0,
              installed=False,native_function_executed=True,parameters_unchanged=True,
              sha256=hashlib.sha256((OUT/'libx2d_afc_settle.so').read_bytes()).hexdigest())
(OUT/'validation.json').write_text(json.dumps(record,indent=2),encoding='utf-8')
print('AFC_SETTLE_33MS_NATIVE_AND_CANDIDATE_PASSED',len(results))
