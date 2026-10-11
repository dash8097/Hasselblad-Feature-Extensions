"""从使用者显式提供的矩阵 RGB ICC 生成回放显示转换表。"""
from __future__ import annotations

import argparse
import math
import os
from pathlib import Path
import struct
import tempfile


MAX_PROFILE_BYTES = 16 * 1024 * 1024
REQUIRED_TAGS = (b"rXYZ", b"gXYZ", b"bXYZ", b"rTRC", b"gTRC", b"bTRC")
LUT_TAGS = tuple(prefix + str(index).encode("ascii")
                 for prefix in (b"A2B", b"B2A", b"D2B", b"B2D") for index in range(4))
SRGB_D50 = ((0.4360747, 0.3850649, 0.1430804),
            (0.2225045, 0.7168786, 0.0606169),
            (0.0139322, 0.0971045, 0.7141733))


def inverse(matrix: tuple[tuple[float, ...], ...]) -> list[list[float]]:
    """求 3×3 矩阵逆；拒绝零矩阵及数值上接近奇异的输入。"""
    if len(matrix) != 3 or any(len(row) != 3 for row in matrix):
        raise ValueError("需要 3×3 矩阵")
    scale = max(abs(value) for row in matrix for value in row)
    if not math.isfinite(scale) or scale == 0:
        raise ValueError("ICC 色彩矩阵不可逆")
    rows = [[float(matrix[y][x]) for x in range(3)] + [float(x == y) for x in range(3)]
            for y in range(3)]
    for col in range(3):
        pivot = max(range(col, 3), key=lambda row: abs(rows[row][col]))
        if abs(rows[pivot][col]) <= scale * 1e-8:
            raise ValueError("ICC 色彩矩阵不可逆或接近奇异")
        rows[col], rows[pivot] = rows[pivot], rows[col]
        divisor = rows[col][col]
        rows[col] = [value / divisor for value in rows[col]]
        for row in range(3):
            if row != col:
                factor = rows[row][col]
                rows[row] = [x - factor * y for x, y in zip(rows[row], rows[col])]
    return [row[3:] for row in rows]


def parse_matrix_rgb_icc(profile: bytes) -> tuple[tuple[tuple[float, ...], ...], float]:
    """仅接受 RGB/XYZ、单一 gamma 的 ICC v2/v4 矩阵配置。"""
    if not 132 <= len(profile) <= MAX_PROFILE_BYTES:
        raise ValueError("ICC 长度无效")
    declared_size = struct.unpack_from(">I", profile)[0]
    if declared_size != len(profile) or profile[36:40] != b"acsp":
        raise ValueError("ICC 头或声明长度无效")
    if profile[8] not in (2, 4) or profile[12:16] not in (b"mntr", b"scnr", b"prtr"):
        raise ValueError("不支持的 ICC 版本或配置类别")
    if profile[16:20] != b"RGB " or profile[20:24] != b"XYZ ":
        raise ValueError("仅支持 RGB 输入和 XYZ PCS 的矩阵 ICC")

    count = struct.unpack_from(">I", profile, 128)[0]
    directory_end = 132 + count * 12
    if count < len(REQUIRED_TAGS) or count > 256 or directory_end > len(profile):
        raise ValueError("ICC 标签目录无效")
    tags: dict[bytes, bytes] = {}
    spans: set[tuple[int, int]] = set()
    for index in range(count):
        signature, offset, size = struct.unpack_from(">4sII", profile, 132 + index * 12)
        if signature in tags or offset % 4 or size == 0 or offset < directory_end or size > len(profile) - offset:
            raise ValueError("ICC 标签越界、重复或未对齐")
        span = (offset, offset + size)
        if any(span != other and span[0] < other[1] and other[0] < span[1] for other in spans):
            raise ValueError("ICC 标签数据重叠")
        spans.add(span)
        tags[signature] = profile[offset:offset + size]
    if any(tag not in tags for tag in REQUIRED_TAGS):
        raise ValueError("缺少矩阵 RGB 必需标签")
    if any(tag in tags for tag in LUT_TAGS):
        raise ValueError("包含未验证的 LUT 转换路径")

    primaries = []
    for tag in REQUIRED_TAGS[:3]:
        data = tags[tag]
        if len(data) != 20 or data[:8] != b"XYZ " + bytes(4):
            raise ValueError("不支持的 RGB 原色标签类型")
        primaries.append(tuple(value / 65536 for value in struct.unpack_from(">iii", data, 8)))
    matrix = tuple(tuple(primaries[column][row] for column in range(3)) for row in range(3))
    inverse(matrix)

    gammas = []
    for tag in REQUIRED_TAGS[3:]:
        data = tags[tag]
        if len(data) != 14 or data[:8] != b"curv" + bytes(4) or struct.unpack_from(">I", data, 8)[0] != 1:
            raise ValueError("仅支持单值 gamma 的 curv TRC；LUT 和参数曲线未验证")
        gamma = struct.unpack_from(">H", data, 12)[0] / 256
        if gamma <= 0:
            raise ValueError("ICC gamma 无效")
        gammas.append(gamma)
    if len(set(gammas)) != 1:
        raise ValueError("RGB 三通道 TRC 不一致")
    return matrix, gammas[0]


def render_tables(matrix: tuple[tuple[float, ...], ...], gamma: float) -> str:
    transform = inverse(SRGB_D50)
    coefficients = [round(sum(transform[y][k] * matrix[k][x] for k in range(3)) * 1048576)
                    for y in range(3) for x in range(3)]
    if any(value < -(1 << 31) or value >= (1 << 31) for value in coefficients):
        raise ValueError("ICC 转换系数超出 int32 范围")
    linear = [round((index / 255) ** gamma * 1048576) for index in range(256)]

    def encode(value: float) -> float:
        return 12.92 * value if value <= 0.0031308 else 1.055 * value ** (1 / 2.4) - 0.055

    encoded = [round(encode(index / 4096) * 65535) for index in range(4097)]

    def array(name: str, kind: str, values: list[int]) -> str:
        body = ",\n".join("    " + ",".join(str(value) for value in values[start:start + 16])
                            for start in range(0, len(values), 16))
        return f"static const {kind} {name}[{len(values)}] = {{\n{body}\n}};\n"

    return ("/* Generated from a user-supplied matrix RGB ICC; input profile is not embedded. */\n"
            + array("adobe_linear", "uint32_t", linear)
            + array("adobe_to_srgb", "int32_t", coefficients)
            + array("srgb_encoded", "uint16_t", encoded))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--icc", type=Path, required=True, help="使用者自行合法准备的矩阵 RGB ICC")
    parser.add_argument("--output", type=Path, required=True, help="明确指定输出 display_tables.h")
    args = parser.parse_args()
    if args.icc.resolve() == args.output.resolve():
        parser.error("输入与输出不能是同一文件")
    try:
        with args.icc.open("rb") as source:
            profile = source.read(MAX_PROFILE_BYTES + 1)
            if len(profile) > MAX_PROFILE_BYTES:
                raise ValueError("ICC 超出大小上限")
        content = render_tables(*parse_matrix_rgb_icc(profile))
        if not args.output.parent.is_dir():
            raise ValueError("输出目录必须已存在")
        descriptor, temporary = tempfile.mkstemp(prefix=".display-tables-", suffix=".tmp", dir=args.output.parent)
        try:
            with os.fdopen(descriptor, "w", encoding="utf-8", newline="\n") as output:
                output.write(content)
            os.replace(temporary, args.output)
        finally:
            Path(temporary).unlink(missing_ok=True)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(f"已生成 {args.output}")


if __name__ == "__main__":
    main()
