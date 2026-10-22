"""X2D 离线固件文件与 AArch64 ELF 读取；不访问设备。"""
import hashlib
import io
import lzma
import os
from pathlib import Path, PurePosixPath
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / ".research-cache/python"))

from capstone import Cs, CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN
from elftools.elf.elffile import ELFFile


def system_file(path):
    parts = PurePosixPath(path).parts
    if not path.startswith('/') or '..' in parts:
        raise ValueError('Expected an absolute firmware-internal path')
    directory = os.environ.get('X2D_SYSTEM_ROOT')
    if directory:
        root = Path(directory).resolve(strict=True)
        target = (root / path.lstrip('/')).resolve(strict=True)
        if not target.is_relative_to(root):
            raise ValueError('Firmware path escapes the supplied directory')
        return target.read_bytes()
    from dissect.extfs import ExtFS
    default = Path(__file__).resolve().parents[2] / '.research-cache/system.img'
    image = Path(os.environ.get('X2D_SYSTEM_IMAGE', str(default)))
    with image.open('rb') as stream:
        return ExtFS(stream).get(path).open().read()


class FirmwareElf:
    def __init__(self, data):
        self.data = data
        self.elf = ELFFile(io.BytesIO(data))
        if self.elf['e_machine'] != 'EM_AARCH64':
            raise ValueError('Expected AArch64 ELF')
        symelf = self.elf
        debug = self.elf.get_section_by_name('.gnu_debugdata')
        if not symelf.get_section_by_name('.symtab') and debug:
            symelf = ELFFile(io.BytesIO(lzma.decompress(debug.data())))
        self.symbols = []
        for table in [symelf.get_section_by_name('.symtab'), self.elf.get_section_by_name('.dynsym')]:
            if table:
                self.symbols.extend(table.iter_symbols())
        self.names = {s['st_value']: s.name for s in self.symbols if s['st_value']}
        self.cs = Cs(CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN)
        self.cs.detail = True

    def read(self, address, size):
        if address < 0 or size < 0:
            raise ValueError('Negative ELF range')
        for segment in self.elf.iter_segments():
            if segment['p_type'] != 'PT_LOAD':
                continue
            start = segment['p_vaddr']
            if start <= address and address + size <= start + segment['p_filesz']:
                offset = segment['p_offset'] + address - start
                return self.data[offset:offset + size]
        raise ValueError('ELF range is not fully backed by file bytes')


def system_elf(path, sha256):
    data = system_file(path)
    if hashlib.sha256(data).hexdigest() != sha256:
        raise ValueError('Firmware SHA-256 mismatch')
    return FirmwareElf(data)


def instruction(binary, address):
    decoded = list(binary.cs.disasm(binary.read(address, 4), address))
    if len(decoded) != 1:
        raise ValueError('Expected one AArch64 instruction')
    return decoded[0]
