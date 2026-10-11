"""验证跨多次读取的一致性和计时边界，全部输入为合成字节。"""
from pathlib import Path
import runpy
import struct
import unittest

parse_snapshot = runpy.run_path(str(
    Path(__file__).resolve().parents[1] / "research/farm_probe_record.py"
))["parse_snapshot"]


def record(**changes):
    words = [0x32504647, 0, 2, 1, 100, 7, 1, 1, 4,
             0x100, 40, 6320, 2582, 0, 3, 0xA5000006,
             0x50032404, 0x00510E00, 125, 7, 1, 1]
    for index, value in changes.items():
        words[int(index)] = value
    return struct.pack("<22I", *words)


class FarmProbeRecordTests(unittest.TestCase):
    def parse(self, data=None, **sequences):
        return parse_snapshot(record() if data is None else data,
                              sequence_before=sequences.get("before", 2),
                              sequence_after=sequences.get("after", 2))

    def test_complete_record_preserves_units_and_raw_bits(self):
        result = self.parse()
        self.assertEqual((result["h_period"], result["v_period"]), (2582, 6320))
        self.assertEqual(result["exposure_line_code"], 4366)
        self.assertEqual(result["exposure_frames_encoded"], 3)
        self.assertTrue(result["exposure_frames_low12_match"])
        self.assertEqual(result["mode_flags_raw"], 0xA5000006)
        self.assertEqual(result["read_interval_raw_ticks"], 25)
        self.assertIsNone(result["timer_frequency_hz"])
        self.assertFalse(result["physical_integration_event_verified"])

    def test_torn_read_and_odd_sequence_are_rejected(self):
        for options in ({"before": 0}, {"after": 4}, {"before": 4, "after": 4}):
            with self.subTest(options=options), self.assertRaises(ValueError):
                self.parse(**options)
        with self.assertRaises(ValueError):
            self.parse(record(**{"2": 3}), before=3, after=3)

    def test_sequence_wrap_to_zero_is_valid(self):
        self.assertEqual(self.parse(record(**{"2": 0}), before=0, after=0)["sequence"], 0)

    def test_invalid_header_and_size_are_rejected(self):
        for data in (record()[:-1], record() + b"\0", bytearray(record()),
                     record(**{"0": 0x53504647}), record(**{"1": 1}), record(**{"3": 0})):
            with self.subTest(length=len(data)), self.assertRaises(ValueError):
                self.parse(data)
        for value in (-1, 0x100000000, True, 2.0):
            with self.subTest(value=value), self.assertRaises(ValueError):
                self.parse(before=value)

    def test_invalid_or_changed_timer_does_not_produce_interval(self):
        for changes in ({"7": 0}, {"21": 0}, {"20": 3}, {"18": 99}):
            self.assertIsNone(self.parse(record(**changes))["read_interval_raw_ticks"])
        for changes in ({"7": 2}, {"6": 0}, {"20": 0}):
            with self.assertRaises(ValueError):
                self.parse(record(**changes))

    def test_cross_word_timer_rollover(self):
        result = self.parse(record(**{"4": 0xFFFFFFF0, "18": 0x10, "19": 8}))
        self.assertEqual(result["read_interval_raw_ticks"], 32)

    def test_config_mismatch_is_reported_without_fabricating_exposure(self):
        result = self.parse(record(**{"14": 4}))
        self.assertFalse(result["exposure_frames_low12_match"])
        self.assertFalse(result["physical_integration_event_verified"])


if __name__ == "__main__":
    unittest.main()
