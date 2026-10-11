"""用自造小型 ICC 验证显示表输入与拒绝路径；只写显式测试输出目录。"""
from __future__ import annotations

import argparse
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
TOOLS = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS))
import generate_display_tables as tables  # noqa: E402


def synthetic_icc(*, version: int = 4, matrix=None, gammas=(563, 563, 563),
                  replace: dict[bytes, bytes] | None = None,
                  extra: dict[bytes, bytes] | None = None) -> bytes:
    """在内存构造 RGB/XYZ 矩阵 ICC，未使用任何厂商配置数据。"""
    if matrix is None:
        matrix = ((0.5, 0.25, 0.1), (0.3, 0.6, 0.1), (0.1, 0.1, 0.8))
    payloads = {}
    for index, tag in enumerate((b"rXYZ", b"gXYZ", b"bXYZ")):
        payloads[tag] = b"XYZ " + bytes(4) + struct.pack(">iii", *(round(value * 65536) for value in matrix[index]))
    for index, tag in enumerate((b"rTRC", b"gTRC", b"bTRC")):
        payloads[tag] = b"curv" + bytes(4) + struct.pack(">IH", 1, gammas[index])
    payloads.update(replace or {})
    payloads.update(extra or {})

    tags = tables.REQUIRED_TAGS + tuple((extra or {}).keys())
    profile = bytearray(132 + 12 * len(tags))
    profile[8] = version
    profile[12:16] = b"mntr"
    profile[16:20] = b"RGB "
    profile[20:24] = b"XYZ "
    profile[36:40] = b"acsp"
    struct.pack_into(">I", profile, 128, len(tags))
    for index, tag in enumerate(tags):
        while len(profile) % 4:
            profile.append(0)
        offset = len(profile)
        data = payloads[tag]
        profile.extend(data)
        struct.pack_into(">4sII", profile, 132 + index * 12, tag, offset, len(data))
    struct.pack_into(">I", profile, 0, len(profile))
    return bytes(profile)


class DisplayTableInputTests(unittest.TestCase):
    output_dir: Path

    def reject(self, profile: bytes) -> None:
        with self.assertRaises(ValueError):
            tables.parse_matrix_rgb_icc(profile)

    def test_valid_matrix_and_table_contract(self) -> None:
        for version in (2, 4):
            with self.subTest(version=version):
                matrix, gamma = tables.parse_matrix_rgb_icc(synthetic_icc(version=version))
                self.assertEqual(gamma, 563 / 256)
                self.assertEqual(len(matrix), 3)
                generated = tables.render_tables(matrix, gamma)
                for declaration in ("adobe_linear[256]", "adobe_to_srgb[9]", "srgb_encoded[4097]"):
                    self.assertIn(declaration, generated)
                self.assertNotIn("synthetic.icc", generated)

    def test_truncated_and_bad_header(self) -> None:
        good = synthetic_icc()
        self.reject(good[:131])
        self.reject(good[:-1])
        damaged = bytearray(good)
        damaged[36:40] = b"xxxx"
        self.reject(damaged)
        damaged = bytearray(good)
        damaged[8] = 5
        self.reject(damaged)
        damaged = bytearray(good)
        damaged[16:20] = b"CMYK"
        self.reject(damaged)

    def test_tag_directory_and_bounds(self) -> None:
        good = synthetic_icc()
        damaged = bytearray(good)
        struct.pack_into(">I", damaged, 128, 999)
        self.reject(damaged)
        damaged = bytearray(good)
        damaged[132:136] = b"xXYZ"
        self.reject(damaged)
        damaged = bytearray(good)
        struct.pack_into(">I", damaged, 132 + 4, len(damaged) - 4)
        self.reject(damaged)
        damaged = bytearray(good)
        first_offset = struct.unpack_from(">I", damaged, 132 + 4)[0]
        struct.pack_into(">I", damaged, 144 + 4, first_offset + 4)
        self.reject(damaged)
        damaged = bytearray(good)
        damaged[144:148] = b"rXYZ"
        self.reject(damaged)

    def test_unsupported_types_and_trc_mismatch(self) -> None:
        self.reject(synthetic_icc(replace={b"rXYZ": b"para" + bytes(16)}))
        self.reject(synthetic_icc(replace={b"rTRC": b"para" + bytes(10)}))
        self.reject(synthetic_icc(replace={b"rTRC": b"curv" + bytes(4) + struct.pack(">IH", 2, 563)}))
        self.reject(synthetic_icc(gammas=(563, 564, 563)))
        self.reject(synthetic_icc(gammas=(0, 0, 0)))
        self.reject(synthetic_icc(extra={b"A2B0": b"mft1" + bytes(12)}))

    def test_singular_matrix(self) -> None:
        self.reject(synthetic_icc(matrix=((0.4, 0.3, 0.2),) * 3))

    def test_cli_requires_explicit_input_and_output(self) -> None:
        with tempfile.TemporaryDirectory(dir=self.output_dir) as directory:
            directory = Path(directory)
            profile = directory / "synthetic.icc"
            header = directory / "display_tables.h"
            profile.write_bytes(synthetic_icc())
            command = [sys.executable, "-B", str(TOOLS / "generate_display_tables.py"),
                       "--icc", str(profile), "--output", str(header)]
            valid = subprocess.run(command, capture_output=True, text=True, check=False)
            self.assertEqual(valid.returncode, 0, valid.stderr)
            self.assertIn("srgb_encoded[4097]", header.read_text(encoding="utf-8"))
            header.unlink()
            profile.write_bytes(synthetic_icc()[:-1])
            invalid = subprocess.run(command, capture_output=True, text=True, check=False)
            self.assertNotEqual(invalid.returncode, 0)
            self.assertFalse(header.exists())
            missing = subprocess.run(command[:3], capture_output=True, text=True, check=False)
            self.assertNotEqual(missing.returncode, 0)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, required=True, help="已经存在的私有测试输出目录")
    options = parser.parse_args()
    if not options.output_dir.is_dir():
        parser.error("测试输出目录必须已存在")
    DisplayTableInputTests.output_dir = options.output_dir.resolve()
    unittest.main(argv=[sys.argv[0]], verbosity=2)
