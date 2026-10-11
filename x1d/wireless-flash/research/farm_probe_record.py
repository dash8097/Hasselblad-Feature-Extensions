"""解析 GFP2 合成或已提供记录；本模块没有设备读取、安装或引闪入口。"""
import struct

MAGIC = 0x32504647
RECORD_BYTES = 88


def _sequence(value):
    if type(value) is not int or not 0 <= value <= 0xFFFFFFFF:
        raise ValueError("序号必须是 uint32")
    return value


def _timer(words):
    low, high, control, valid = words
    if valid not in (0, 1) or (valid and not control & 1):
        raise ValueError("计时器有效标记矛盾")
    return {
        "control": control,
        "valid": bool(valid),
        "counter": (high << 32 | low) if valid else None,
    }


def parse_snapshot(raw, *, sequence_before, sequence_after):
    """调用方须提供包围整份记录读取的两次序号；不能拿包序号替代。

    此检查只确认记录完整性。没有确定本次物理感光帧、计数频率或延时。
    """
    if type(raw) is not bytes or len(raw) != RECORD_BYTES:
        raise ValueError("必须提供完整的 88 字节 GFP2 记录")
    before_sequence = _sequence(sequence_before)
    after_sequence = _sequence(sequence_after)
    w = struct.unpack("<22I", raw)
    if w[0] != MAGIC or w[1] != 0 or w[3] != 1:
        raise ValueError("记录标识、一次性准备状态或事件类型不符")
    if w[2] & 1 or not before_sequence == w[2] == after_sequence:
        raise ValueError("记录仍在写入或读取期间已变化")
    before, after = _timer(w[4:8]), _timer(w[18:22])
    interval = None
    if (before["valid"] and after["valid"] and
            before["control"] == after["control"] and
            after["counter"] >= before["counter"]):
        interval = after["counter"] - before["counter"]
    encoded_frames = (w[16] >> 16) & 0xFFF
    return {
        "record_version": 2,
        "sequence": w[2],
        "event": "sensorif_control_write_returned",
        "physical_integration_event_verified": False,
        "sensor_status_raw": w[8],
        "sensorif_shadow_raw": list(w[9:14]),
        "v_period": w[11],
        "h_period": w[12],
        "extra_clock_raw": w[13],
        "exposure_frames_config": w[14],
        "mode_flags_raw": w[15],
        "rolling_flag_byte": (w[15] >> 8) & 0xFF,
        "sensor_register_words_raw": list(w[16:18]),
        "exposure_frames_encoded": encoded_frames,
        "exposure_frames_low12_match": encoded_frames == (w[14] & 0xFFF),
        "exposure_line_code": (w[17] >> 8) & 0x3FFF,
        "timer_before": before,
        "timer_after": after,
        "read_interval_raw_ticks": interval,
        "timer_frequency_hz": None,
    }
