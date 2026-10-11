"""只解析 GFP3 两点软件记录；不访问设备、不推断物理积分时刻。"""
import struct


def parse_snapshot(data, *, sequence_before, sequence_after):
    if type(data) is not bytes or len(data) != 116:
        raise ValueError("expected 116 bytes")
    if any(type(v) is not int or not 0 <= v <= 0xFFFFFFFF for v in (sequence_before, sequence_after)):
        raise ValueError("sequence must be uint32")
    words = struct.unpack("<29I", data)
    if (words[0] != 0x33504647 or words[1] != 0 or words[3] != 1 or
            words[2] & 1 or sequence_before != words[2] or sequence_after != words[2]):
        raise ValueError("incomplete or mismatched GFP3 record")

    def stamp(index):
        low, high, control, valid = words[index:index + 4]
        ok = valid == 1 and bool(control & 1)
        return {"counter": ((high << 32) | low) if ok else None, "control": control, "valid": ok}

    before, after = stamp(4), stamp(18)
    interval = None
    if before["valid"] and after["valid"] and before["control"] == after["control"] and after["counter"] >= before["counter"]:
        interval = after["counter"] - before["counter"]
    pending = struct.pack("<2I", *words[24:26])
    encoded_n = pending[2] | ((pending[3] & 15) << 8)
    encoded_r = pending[5] | ((pending[6] & 63) << 8)
    encoding_valid = words[22] == 1 and words[28] >= 1 and encoded_n == (words[23] & 4095)
    single = encoding_valid and words[28] == 1
    return {"record_version": 3, "sequence": words[2],
            "event": "sensorif_control_write_returned", "timer_before": before, "timer_after": after,
            "read_interval_raw_ticks": interval, "timer_frequency_hz": None,
            "start_snapshot": {"status_raw": words[8], "shadow_raw": list(words[9:14]),
                               "config_n": words[14], "mode_flags_raw": words[15],
                               "register_words_raw": list(words[16:18]),
                               "register_buffer_is_last_applied_exposure": False},
            "encoding_snapshot": {"valid": encoding_valid, "matching_call_count": words[28],
                                  "single_matching_call_before_start": single,
                                  "input_exposure_us": ((words[27] << 32) | words[26]) if encoding_valid else None,
                                  "config_n": words[23], "encoded_n": encoded_n, "encoded_r": encoded_r,
                                  "register_words_raw": list(words[24:26]),
                                  "config_n_changed_before_start": words[23] != words[14] if encoding_valid else None},
            "physical_integration_event_verified": False,
            "spi_write_completion_observed": False}
