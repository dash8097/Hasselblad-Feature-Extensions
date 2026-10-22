"""构建 AFC 约 33 ms 稳定等待候选；仅离线，不改固件文件或连接相机。"""
from pathlib import Path
import hashlib, io, json, os, subprocess, sys
sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
one_frame='--one-frame' in sys.argv
OUT = ROOT / ('x2d/outputs/4.2.0/afc-type1-3-r1/settle' if one_frame else 'x2d/outputs/4.2.0/afc-settle-33ms-r2')
if os.environ.get('X2D_SIM_CASE_OUT'):
    OUT = Path(os.environ['X2D_SIM_CASE_OUT'])
OUT.mkdir(parents=True, exist_ok=True)
assert not (OUT / 'device-apply.txt').exists(), 'Preserve applied evidence'
sys.path.insert(0, str(ROOT / 'x2d/tools'))
from firmware_image import system_elf, system_file
from elftools.elf.elffile import ELFFile
SHA = 'feef8a8dc3a27395e47232c2b25a5da7a7ab335922fbb527e637c439e35bcec7'
b = system_elf('/lib64/libaaa.so', SHA)
syms = {s.name: s for s in b.symbols if s['st_value']}
dyn = b.elf.get_section_by_name('.dynsym')
got = {dyn.get_symbol(r['r_info_sym']).name: r['r_offset']
       for r in b.elf.get_section_by_name('.rela.plt').iter_relocations()}
names = dict(QUICK='cdaf_scene_change_quick', MODE='is_stand_afc_mode',
             COMMON='cdaf_common_private', FPS='af_get_cur_fps')
header = ''.join('#define %s_OFFSET 0x%xUL\n' % (key, syms[n]['st_value'])
                 for key, n in names.items())
header += '#define QUICK_SLOT 0x%xUL\n' % got[names['QUICK']]
checks = list(names.values()) + ['_idle_stage_main']
for i, n in enumerate(checks):
    s = syms[n]
    header += 'static const unsigned char bytes_%d[]={%s};\n' % (
        i, ','.join(str(v) for v in b.read(s['st_value'], s['st_size'])))
header += '#define CHECK_COUNT %d\n' % len(checks)
header += 'static const struct{unsigned long offset;unsigned size;const unsigned char*bytes;}checks[]={\n'
header += ''.join('{0x%x,%d,bytes_%d},\n' % (syms[n]['st_value'], syms[n]['st_size'], i)
                  for i, n in enumerate(checks)) + '};\n'
(OUT / 'settle_layout.h').write_text(header, encoding='ascii')
zig = ROOT / '.research-cache/x1d-1.25.0/toolchain/zig-windows-x86_64-0.13.0/zig.exe'
env = dict(os.environ, ZIG_GLOBAL_CACHE_DIR=str(OUT/'zig-global'),
           ZIG_LOCAL_CACHE_DIR=str(OUT/'zig-local'))
libs, exports = [], set()
for name in ('libc.so', 'libdl.so'):
    data = system_file('/lib64/'+name)
    target = OUT/name
    target.write_bytes(data)
    libs.append(str(target))
    exports.update(s.name for s in ELFFile(io.BytesIO(data)).get_section_by_name('.dynsym').iter_symbols()
                   if s['st_shndx'] != 'SHN_UNDEF')
target = OUT/'libx2d_afc_settle.so'
subprocess.run([str(zig), 'cc', '-target', 'aarch64-linux-gnu', '-std=c11', '-O2', '-g0',
    '-fPIC', '-shared', '-nostdlib', '-fno-stack-protector', '-mno-outline-atomics',
    '-Wl,--no-undefined', '-Wl,-z,noexecstack', '-Wl,-soname,libx2d_afc_settle.so',
    '-I'+str(OUT), *(['-DX2D_AFC_SETTLE_ONE_FRAME'] if one_frame else []), str(HERE/'afc_settle.c'), *libs, '-o', str(target)], env=env, check=True)
e = ELFFile(io.BytesIO(target.read_bytes()))
imports = {s.name for s in e.get_section_by_name('.dynsym').iter_symbols()
           if s.name and s['st_shndx'] == 'SHN_UNDEF'}
assert imports <= exports and e['e_machine'] == 'EM_AARCH64'
assert not any(p['p_flags'] & 3 == 3 for p in e.iter_segments())
(OUT/'build.json').write_text(json.dumps(dict(firmware='X2D 100C 4.2.0',
    source_sha256=SHA, sha256=hashlib.sha256(target.read_bytes()).hexdigest(),
    hooks=[names['QUICK']], wait_seconds=None if one_frame else 1/30, wait_frames=1 if one_frame else None, parameters_modified=False,
    installed=False, device_requests=0), indent=2), encoding='utf-8')
print('AFC_SETTLE_33MS_AARCH64_BUILT_NOT_INSTALLED')
