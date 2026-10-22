"""以本轮交付包绑定现有相机 UI 用例，不调用开发安装入口。"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import runpy
import sys

ROOT = Path(__file__).resolve().parents[4]
R7 = ROOT / "x2d/outputs/4.2.0/installer-hardening-r7"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("package", type=Path)
    parser.add_argument("evidence", type=Path)
    parser.add_argument("tool")
    parser.add_argument("arguments", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    package, evidence = args.package.resolve(), args.evidence.resolve()
    assert Path.cwd().resolve() == ROOT
    assert package.is_relative_to(R7 / "native-host/delivery-candidates")
    assert evidence.is_relative_to(R7) and evidence != R7
    assert args.tool.startswith("package_") and args.tool.replace("_", "").isalnum()
    tool = ROOT / "x2d/FunctionalTests/multishot_ui" / (args.tool + ".py")
    assert tool.is_file()
    proof = json.loads((package.parent / "report.json").read_text(encoding="utf8"))
    assert proof["package"] == package.name
    for name, digest in proof["files"].items():
        path = (package / name).resolve()
        assert path.is_relative_to(package)
        assert hashlib.sha256(path.read_bytes()).hexdigest() == digest, name
    evidence.mkdir(parents=True, exist_ok=True)
    binding = {
        "package_path": str(package), "package": package.parent.name,
        "exe_sha256": proof["files"]["HasselbladTuningPatchInstaller.exe"],
        "installation": "通过该交付包前台安装，安装结果和重启核验另附",
    }
    context = evidence / "package-binding.json"
    if context.exists():
        assert json.loads(context.read_text(encoding="utf8")) == binding
    else:
        context.write_text(json.dumps(binding, ensure_ascii=False, indent=2), encoding="utf8")
    os.environ["X2D_TEST_PACKAGE"] = str(package)
    os.environ["X2D_UI_EVIDENCE_DIR"] = str(evidence)
    sys.path.insert(0, str(tool.parent))
    sys.argv = [str(tool), *args.arguments]
    runpy.run_path(str(tool), run_name="__main__")


if __name__ == "__main__":
    main()
