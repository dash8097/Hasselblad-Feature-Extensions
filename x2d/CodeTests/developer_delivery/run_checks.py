"""开发者离线回归入口；显式白名单，不枚举或连接相机。"""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[3]
CASES = {
    "profiles": "x2d/CodeTests/installer_hardening/test_package_profiles.py",
    "transactions": "x2d/CodeTests/installer_hardening/test_native_transaction_contract.py",
    "quarter-bindings": "x2d/CodeTests/installer_hardening/camera_native/test_quarter_direct_contract.py",
    "host-profile": "x2d/CodeTests/installer_hardening/native_host/test_profile_ui.py",
    "network-upload": "x2d/CodeTests/installer_hardening/native_host/test_native_network_upload.py",
    "stack-admission": "x2d/CodeTests/raw_stack_continuous/check_runner.py",
    "stack-publication": "x2d/CodeTests/raw_stack_continuous/check_publication.py",
    "stack-startup": "x2d/CodeTests/raw_stack_continuous/check_publish_startup.py",
    "ui-menu": "x2d/CodeTests/installer_hardening/native_ui/build_menu_discovery.py",
}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--cases", nargs="+", choices=CASES, default=list(CASES))
    args = ap.parse_args()
    if args.list:
        print(json.dumps(CASES, ensure_ascii=False, indent=2))
        return
    assert Path.cwd().resolve() == ROOT and sys.version_info[:2] == (3, 11)
    out = ROOT / "x2d/outputs/developer-checks" / str(time.time_ns())
    out.mkdir(parents=True)
    results = []
    # 各测试可能写同一模块缓存，顺序运行，失败仍收集其余独立用例。
    for name in args.cases:
        start = time.monotonic()
        command = [sys.executable, "-X", "utf8", "-B", str(ROOT / CASES[name])]
        try:
            p = subprocess.run(command, cwd=ROOT, capture_output=True, timeout=900)
            (out / (name + ".log")).write_bytes(p.stdout + p.stderr)
            result = {"case": name, "exit": p.returncode, "passed": p.returncode == 0}
        except subprocess.TimeoutExpired:
            result = {"case": name, "passed": False, "error": "TIMEOUT"}
        result["elapsed_seconds"] = time.monotonic() - start
        results.append(result)
        print(json.dumps(result), flush=True)
    report = {"camera_operations": 0, "results": results,
              "passed": all(r["passed"] for r in results)}
    (out / "report.json").write_text(json.dumps(report, indent=2), encoding="utf8")
    print(out)
    raise SystemExit(0 if report["passed"] else 1)


if __name__ == "__main__":
    main()
