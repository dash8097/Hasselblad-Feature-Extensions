"""固定官方 4.2.0 的私有帧写入接入审计；不加载候选或连接设备。"""
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT/'x2d/tools'))
from firmware_image import system_elf

SHA = '8c086e9c2a340c2a388e92ae3de39d6d0e5ce19ac3d582626112a24fd3b97aef'
OUT = ROOT/'x2d/outputs/4.2.0/pixel-shift-rgb'


def load():
    binary=system_elf('/bin/camera-storage', SHA)
    rel=binary.elf.get_section_by_name('.rela.plt')
    symbols=binary.elf.get_section(rel['sh_link'])
    plt=binary.elf.get_section_by_name('.plt')['sh_addr']
    names={plt+32+16*i:symbols.get_symbol(r['r_info_sym']).name
           for i,r in enumerate(rel.iter_relocations())}
    names.update(binary.names)
    return binary,names


def main():
    binary,names=load()
    wanted=['StorageService::handleExposureItem','StorageIO::onItemWritten',
            'StorageIO::flushItemFile','Cache::addWriteWork',
            'WriteFileContent::setIncomingImage','WriteFileContent::setIncomingPreview',
            'WriteCacheItem::WriteCacheItem','CacheItemInfo::CacheItemInfo',
            'WriteCacheItem::addFile','Cache::Cache','StorageService::StorageService']
    chosen={}
    for symbol in binary.symbols:
        name=symbol.name
        if symbol['st_size'] and any(w in name for w in wanted):
            chosen[symbol['st_value']]=symbol
    # Debug symbols are mangled; demangled names are provided by c++filt separately.
    for address,size in [(0xb0568,1288),(0xf1d70,1592),(0xf41f8,1688),
                         (0x72460,2140),(0xa9e00,2840),(0xa7ab0,2152),
                         (0x95558,1428),(0xa5208,2012),(0x98af0,2624)]:
        chosen[address]=dict(st_value=address,st_size=size,name=names.get(address,''))
    records=[]
    lines=[]
    for address,symbol in sorted(chosen.items()):
        instructions=list(binary.cs.disasm(binary.read(address,symbol['st_size']),address))
        lines.append(f'FUNCTION {address:x} {symbol["name"]}')
        for i in instructions:
            extra=''
            if i.mnemonic in ('bl','b') and i.op_str.startswith('#'):
                extra=' '+names.get(int(i.op_str[1:],0),'')
            lines.append(f'{i.address:08x} {i.mnemonic:8s} {i.op_str}{extra}')
        records.append(dict(address=hex(address),size=symbol['st_size'],name=symbol['name']))
    (OUT/'private-frame-writer-disassembly.txt').write_text('\n'.join(lines),encoding='utf-8')
    (OUT/'private-frame-writer-audit.json').write_text(json.dumps(dict(
        firmware='X2D 100C 4.2.0',storage_sha256=SHA,functions=records,
        candidate_loaded=False,capture_verified=False),indent=2),encoding='utf-8')
    print('PRIVATE_FRAME_WRITER_STATIC_AUDIT_SAVED')


if __name__=='__main__':main()
