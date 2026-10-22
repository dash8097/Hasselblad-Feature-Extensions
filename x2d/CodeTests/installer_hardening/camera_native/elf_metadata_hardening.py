"""确定性 ELF 元数据剥离；不重排文件，不修改任何程序段或加载字节。"""
import hashlib
import struct


def _sha(data):
    return hashlib.sha256(data).hexdigest()


def normalize_elf(data):
    """返回 (新 bytes, 审计字典)。不支持、损坏或存在歧义的 ELF 拒绝处理。"""
    if not isinstance(data, bytes):
        raise TypeError('ELF 输入必须为不可变 bytes')
    if len(data) < 64 or data[:7] != b'\x7fELF\x02\x01\x01':
        raise ValueError('仅接受 ELF64 little-endian v1')
    h = struct.unpack_from('<HHIQQQIHHHHHH', data, 16)
    kind, machine, version, entry, phoff, shoff, flags, ehsize, phsize, phnum, shsize, shnum, shstr = h
    if kind not in (2, 3) or machine != 183 or version != 1 or ehsize != 64:
        raise ValueError('仅支持固定 AArch64 可执行文件或共享库')
    if phsize != 56 or not 0 < phnum < 65535 or shsize != 64 or not 0 < shnum < 65280 or not 0 < shstr < shnum:
        raise ValueError('不支持扩展计数、缺失或异常 ELF 表')

    def bounds(offset, size, label):
        if offset < 0 or size < 0 or offset > len(data) or size > len(data) - offset:
            raise ValueError(label + '越界')
        return offset, offset + size

    def overlaps(a, b):
        return a[0] < a[1] and b[0] < b[1] and a[0] < b[1] and b[0] < a[1]

    ph_table = bounds(phoff, phsize * phnum, 'program headers')
    sh_table = bounds(shoff, shsize * shnum, 'section headers')
    if overlaps((0, 64), ph_table) or overlaps((0, 64), sh_table) or overlaps(ph_table, sh_table):
        raise ValueError('ELF 表重叠')
    segments = []
    for i in range(phnum):
        p = struct.unpack_from('<IIQQQQQQ', data, phoff + i * phsize)
        segment = bounds(p[2], p[5], 'program segment')
        if p[0] == 1 and p[5] > p[6]:
            raise ValueError('PT_LOAD 文件长度超过内存长度')
        segments.append((i, p, segment))
    sections = [struct.unpack_from('<IIQQQQIIQQ', data, shoff + i * shsize) for i in range(shnum)]
    if sections[0] != (0,) * 10:
        raise ValueError('仅接受普通空 section 0')
    strings = sections[shstr]
    if strings[1] != 3 or strings[2] & 2:
        raise ValueError('异常 section 名称表')
    a, b = bounds(strings[4], strings[5], 'section 名称表')
    names = data[a:b]
    if not names or names[0] != 0:
        raise ValueError('缺失 section 空名称')
    named = []
    ranges = []
    for i, s in enumerate(sections):
        if s[0] >= len(names):
            raise ValueError('section 名称越界')
        end = names.find(b'\0', s[0])
        if end < 0:
            raise ValueError('section 名称未结束')
        try:
            named.append(names[s[0]:end].decode('ascii'))
        except UnicodeDecodeError as e:
            raise ValueError('非 ASCII section 名称') from e
        if s[6] >= shnum:
            raise ValueError('section link 越界')
        if s[1] in (4, 9) and s[7] >= shnum:
            raise ValueError('relocation 目标 section 越界')
        ranges.append(None if s[1] == 8 else bounds(s[4], s[5], 'section 内容'))

    removed = {i for i, s in enumerate(sections) if i and
               (s[1] == 2 or named[i].startswith(('.debug', '.zdebug')) or
                named[i] in ('.gnu_debuglink', '.gnu_debugaltlink'))}
    # 已剥离 ELF 仅有工具链 comment 时保持原摘要，避免触及 UI/runtime 合同。
    if removed:
        removed |= {i for i in range(1, shnum) if named[i] == '.comment'}
    # 符号名称表只从真实 SYMTAB 的 sh_link 推导，不按名称误删动态字符串表。
    for i in tuple(removed):
        if sections[i][1] == 2:
            target = sections[i][6]
            if not target or sections[target][1] != 3 or target == shstr:
                raise ValueError('SYMTAB 的名称表异常')
            removed.add(target)
    while True:
        more = {i for i, s in enumerate(sections) if i and
                ((s[1] in (4, 9) and s[7] in removed) or
                 (s[1] == 18 and s[6] in removed))}
        if more <= removed:
            break
        removed |= more
    for i, s in enumerate(sections):
        if i in removed:
            continue
        if s[6] in removed or (s[1] in (4, 9) and s[7] in removed):
            raise ValueError('保留 section 引用待剥离 section: ' + named[i])

    changes = []
    for i in sorted(removed):
        s = sections[i]
        if s[2] & 2 or s[1] == 8 or i == shstr:
            raise ValueError('待剥离 section 不是独立非 ALLOC 元数据: ' + named[i])
        content = ranges[i]
        header = shoff + i * shsize, shoff + (i + 1) * shsize
        for region in (content, header):
            if any(overlaps(region, seg) for _, _, seg in segments) or overlaps(region, (0, 64)) or overlaps(region, ph_table):
                raise ValueError('剥离范围接触 ELF header 或 program segment: ' + named[i])
        if overlaps(content, sh_table):
            raise ValueError('元数据内容与 section headers 重叠')
        for j, other in enumerate(ranges):
            if j != i and other and overlaps(content, other):
                raise ValueError('元数据内容与其他 section 重叠: ' + named[j])
        changes.extend((content, header))
    result = bytearray(data)
    for start, end in changes:
        result[start:end] = b'\0' * (end - start)
    result = bytes(result)
    assert len(result) == len(data) and result[:64] == data[:64]
    assert result[ph_table[0]:ph_table[1]] == data[ph_table[0]:ph_table[1]]
    proof = []
    for i, p, (start, end) in segments:
        before, after = _sha(data[start:end]), _sha(result[start:end])
        assert before == after
        proof.append(dict(index=i, type=p[0], offset=p[2], filesz=p[5], memsz=p[6], before_sha256=before, after_sha256=after))
    report = dict(schema='X2D_ELF_METADATA_ZERO_V1', before_sha256=_sha(data), after_sha256=_sha(result), bytes=len(data),
                  removed=[dict(index=i, name=named[i], type=sections[i][1], offset=sections[i][4], size=sections[i][5]) for i in sorted(removed)],
                  all_program_segments_unchanged=True, elf_header_unchanged=True, program_headers_unchanged=True,
                  program_segments=proof, preserved_dynamic_symbols=True, file_layout_unchanged=True)
    return result, report
