"""固定 X2D 4.2.0 离线控制链核对；不连接或执行相机程序。"""
import argparse
import bisect
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[5]
sys.path.insert(0, str(ROOT / 'x2d/tools'))
from firmware_image import system_elf

SOURCES = {
    'pdaf-stream': ('/lib64/camera/plugins/stream_filter/libdcam_pdaf_stream_filter.so', 'e56012e2a60a1d96d998d754b2fcb253382f9205c6f12ec530025af9cad5476f'),
    'aaa': ('/lib64/libaaa.so', 'feef8a8dc3a27395e47232c2b25a5da7a7ab335922fbb527e637c439e35bcec7'),
    'rcam': ('/lib64/librcam.so', '72ebc8deebce4a29047c475e77ab4edbf2860abb1f572fea45260fe17ad0bda5'),
    'service': ('/bin/camera-service', 'fbcf828f73bca13f0c8b95e7dd0b95ac483ae36954ec06179098c8a1a65f9f82'),
}


def load(key):
    b = system_elf(*SOURCES[key])
    dyn = b.elf.get_section_by_name('.dynsym')
    rel = b.elf.get_section_by_name('.rela.plt')
    plt = b.elf.get_section_by_name('.plt')
    if rel and plt:
        slots = {r['r_offset']: dyn.get_symbol(r['r_info_sym']).name for r in rel.iter_relocations()}
        for a in range(plt['sh_addr'], plt['sh_addr'] + plt['sh_size'], 16):
            ins = list(b.cs.disasm(b.read(a, 16), a))
            if len(ins) >= 2 and ins[0].mnemonic == 'adrp' and ins[1].mnemonic == 'ldr':
                slot = ins[0].operands[1].imm + ins[1].operands[1].mem.disp
                if slot in slots:
                    b.names[a] = slots[slot] + '@plt'
    return b


def functions(b):
    return {s['st_value']: (s.name, s['st_size']) for s in b.symbols
            if s['st_value'] and s['st_info']['type'] == 'STT_FUNC' and s['st_size']}


def branches(b):
    sec = b.elf.get_section_by_name('.text')
    for i, (word,) in enumerate(struct.iter_unpack('<I', sec.data())):
        if word & 0xfc000000 not in (0x94000000, 0x14000000):
            continue
        off = word & 0x3ffffff
        if off & (1 << 25):
            off -= 1 << 26
        a = sec['sh_addr'] + i * 4
        yield a, a + off * 4


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('library', choices=SOURCES)
    p.add_argument('action', choices=['symbols', 'dis', 'calls', 'callers'])
    p.add_argument('pattern')
    args = p.parse_args()
    b = load(args.library)
    fs = functions(b)
    selected = {a: v for a, v in fs.items() if re.search(args.pattern, v[0], re.I)}
    if args.action == 'callers':
        targets = {a for a, name in b.names.items() if re.search(args.pattern, name, re.I)}
        starts = sorted(fs)
        for a, dest in branches(b):
            if dest not in targets:
                continue
            idx = bisect.bisect_right(starts, a) - 1
            f = starts[idx] if idx >= 0 else 0
            owner = fs[f][0] if f and a < f + fs[f][1] else '(symbol gap)'
            print(f'{a:#x} -> {b.names[dest]} from {f:#x} {owner}')
        return
    for a, (name, size) in sorted(selected.items()):
        print(f'{a:#x} {size} {name}')
        if args.action == 'symbols':
            continue
        for ins in b.cs.disasm(b.read(a, size), a):
            target = b.names.get(ins.operands[0].imm, '') if ins.mnemonic in ('bl', 'b') else ''
            if args.action == 'dis' or target:
                print(f'  {ins.address:#x}: {ins.mnemonic:8} {ins.op_str:34} {target}')


if __name__ == '__main__':
    main()
