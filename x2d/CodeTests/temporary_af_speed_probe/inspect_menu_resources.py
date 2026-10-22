"""固定 4.2.0 原厂 GUI 资源只读核对；不连接相机。"""
import struct
import sys
import zlib
from pathlib import Path

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from firmware_image import system_elf

GUI_SHA = '16391452abdc69de9e0807e065c0f4ab3f1ccb5fc288f6fc4e6f5cb3bdca12e0'


def load_gui():
    return system_elf('/bin/camera-gui', GUI_SHA)


def resources(binary):
    groups = []
    for suffix in ('struct', 'name', 'data'):
        found = sorted((s['st_value'], s['st_size']) for s in binary.symbols
                       if s.name == '_ZL' + str(len('qt_resource_' + suffix)) + 'qt_resource_' + suffix
                       and s['st_value'])
        groups.append(found)
    assert len(groups[0]) == len(groups[1]) == len(groups[2])
    for (tree, size), (names, _), (data, _) in zip(*groups):
        if size % 22:
            raise ValueError('Unexpected Qt resource structure')

        def walk(index, parent):
            row = binary.read(tree + index * 22, 22)
            name_offset, flags = struct.unpack('>IH', row[:6])
            length = struct.unpack('>H', binary.read(names + name_offset, 2))[0]
            name = binary.read(names + name_offset + 6, length * 2).decode('utf-16-be')
            path = parent + '/' + name if index else ''
            if flags & 2:
                count, first = struct.unpack('>II', row[6:14])
                for child in range(first, first + count):
                    yield from walk(child, path)
            else:
                offset = struct.unpack('>I', row[10:14])[0]
                length = struct.unpack('>I', binary.read(data + offset, 4))[0]
                content = binary.read(data + offset + 4, length)
                if flags & 1:
                    expected = struct.unpack('>I', content[:4])[0]
                    content = zlib.decompress(content[4:])
                    assert len(content) == expected
                elif flags & 4:
                    return  # Unneeded zstd resources are not misrepresented as plain text.
                yield ':' + path, content
        yield from walk(0, '')


if __name__ == '__main__':
    gui = load_gui()
    for path, content in resources(gui):
        if any(part.lower() in path.lower() for part in sys.argv[1:]):
            print(path, len(content))
            if path.endswith(('.qml', '.json', '.xml')):
                print(content.decode('utf-8'))
