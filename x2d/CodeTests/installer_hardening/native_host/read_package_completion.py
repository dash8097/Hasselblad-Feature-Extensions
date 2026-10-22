"""读取当前交付包任务的发布回执及空闲状态，不安装、不读取照片。"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[4]
R7 = ROOT / "x2d/outputs/4.2.0/installer-hardening-r7"


def main():
    p = argparse.ArgumentParser()
    p.add_argument("package", type=Path)
    p.add_argument("evidence", type=Path)
    p.add_argument("tag")
    a = p.parse_args()
    package, evidence = a.package.resolve(), a.evidence.resolve()
    assert Path.cwd().resolve() == ROOT
    assert package.is_relative_to(R7 / "native-host/delivery-candidates")
    assert evidence.is_relative_to(R7) and evidence.is_dir()
    assert a.tag.replace("-", "").isalnum()
    target = evidence / (a.tag + "-completion.json")
    assert not target.exists()
    proof = json.loads((package.parent / "report.json").read_text(encoding="utf8"))
    for name, digest in proof["files"].items():
        path = (package / name).resolve()
        assert path.is_relative_to(package)
        assert hashlib.sha256(path.read_bytes()).hexdigest() == digest
    os.environ["X2D_TEST_PACKAGE"] = str(package)
    os.environ["X2D_UI_EVIDENCE_DIR"] = str(evidence)
    sys.path.insert(0, str(ROOT / "x2d/FunctionalTests/multishot_ui"))
    sys.path.insert(0, str(package / "engine"))
    import windows_engine
    from package_ui import transport_lock, read_status
    from package_pixel_shot import task_receipt
    with transport_lock():
        w = windows_engine.Transport(package)
        i = windows_engine.mac_patch.Installer(package, run_batch=w.batch)
        one = lambda c: w.batch([c])[0].strip()
        state = read_status(one)
        result = {
            "package": str(package),
            "photo_contents_read": False,
            "state": {k: state.get(k) for k in
                      ("session", "ack", "stage", "busy", "ready", "error")},
            "receipt": task_receipt(one),
            "exposure": i.property("camera", "exposure_status"),
            "storage": i.property("storage", "storage_status"),
            "active_error": i.property("error", "active_error"),
            "lock": one("if test -e /tmp/x2d-multishot-active;then echo PRESENT;else echo ABSENT;fi"),
            "mount": one("awk '$2==\"/system\"{print $4}' /proc/mounts"),
        }
    target.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf8")
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
