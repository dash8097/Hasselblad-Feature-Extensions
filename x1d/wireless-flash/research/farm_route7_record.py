"""第七条有界记录解析；零事件不能单独证明某条路径没有执行。"""
import struct

MAGIC, SIZE = 0x31523747, 96
STAGES = {1: "normal_capture_marker", 2: "route7_request_entry",
          3: "route7_before_pulse", 4: "route7_after_sensor_call_and_wait"}


def parse_snapshot(data, *, sequence_before, sequence_after):
    if type(data) is not bytes or len(data) != SIZE:
        raise ValueError("record size mismatch")
    words = struct.unpack("<24I", data)
    magic, armed, seq, count, overflow, dropped, lock, reserved = words[:8]
    if type(sequence_before) is not int or type(sequence_after) is not int:
        raise ValueError("sequence type")
    if magic != MAGIC or armed not in (0, 1) or count > 16 or overflow not in (0, 1) or dropped not in (0, 1) or lock or reserved:
        raise ValueError("record header invalid or producer busy")
    if sequence_before != seq or seq != sequence_after or seq & 1:
        raise ValueError("inconsistent snapshot")
    events = []
    for word in words[8:8 + count]:
        stage, mode = word & 255, (word >> 8) & 255
        if word >> 16 or stage not in STAGES or (stage != 2 and mode != 255):
            raise ValueError("event encoding")
        events.append({"stage": STAGES[stage], "mode": mode if stage == 2 else None})
    seen = {e["stage"] for e in events}
    return {"armed": bool(armed), "sequence": seq, "count": count, "events": events,
            "complete_buffer": not (overflow or dropped), "overflow": bool(overflow), "dropped": bool(dropped),
            "normal_capture_marker_seen": STAGES[1] in seen, "route7_entry_seen": STAGES[2] in seen,
            "route7_pulse_branch_seen": STAGES[3] in seen, "route7_sensor_return_boundary_seen": STAGES[4] in seen,
            "physical_flash_or_integration_verified": False}
