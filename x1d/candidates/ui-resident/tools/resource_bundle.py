"""独立生成 Qt RCC v1 文本覆盖包；格式沿用本项目已使用的 Qt 5.5 资源布局。"""
import struct
import zlib

def qhash(name):
    result = 0
    for char in name:
        result = (result << 4) + ord(char)
        result ^= (result & 0xf0000000) >> 23
        result &= 0x0fffffff
    return result

def rcc(files):
    root = {'name':'', 'children':{}}
    for path, text in files.items():
        if not path.startswith('/') or any(p in ('', '.', '..') for p in path[1:].split('/')):
            raise ValueError('非法 QRC 路径')
        node = root
        for name in path[1:].split('/'):
            node = node.setdefault('children',{}).setdefault(name,{'name':name})
        node['text'] = text
    nodes = [root]
    for node in nodes:
        if 'children' in node:
            node['first'] = len(nodes)
            nodes.extend(sorted(node['children'].values(), key=lambda n:(qhash(n['name']),n['name'])))
    names, payload, tree = bytearray(), bytearray(), bytearray()
    for node in nodes:
        name = node['name']; offset = len(names)
        encoded = name.encode('utf-16-be')
        names += struct.pack('>HI',len(encoded)//2,qhash(name)) + encoded
        if 'children' in node:
            tree += struct.pack('>IHII',offset,2,len(node['children']),node['first'])
        else:
            text = node['text'].encode('utf-8')
            compressed = struct.pack('>I',len(text)) + zlib.compress(text,9)
            tree += struct.pack('>IHHHI',offset,1,0,1,len(payload))
            payload += struct.pack('>I',len(compressed)) + compressed
    return b'qres' + struct.pack('>IIII',1,20+len(payload)+len(names),20,20+len(payload)) + payload + names + tree
