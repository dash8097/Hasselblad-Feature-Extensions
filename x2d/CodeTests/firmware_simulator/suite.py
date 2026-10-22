"""全类别离线回归编排；只运行经审阅的组件场景，缺项不视为整机通过。"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess
import sys
import tempfile
import time
from verify_inputs import verify as verify_inputs

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
OUT = ROOT / 'x2d/outputs/4.2.0/firmware-simulator-r2'
CASES = {
    'runtime': ['runtime', '400mp', '1600mp', '25mp', 'raw_stack'],
    'memory': ['400mp'], 'sixteen': ['1600mp'], 'quarter': ['25mp'],
    'stack': ['raw_stack'], 'precapture': ['precapture'], 'af': ['afc_afs'],
    'face': ['face_eye'], 'eye': ['face_eye'], 'power': ['ui_power_settings'],
    'capture_ui': ['ui_power_settings'], 'installer': ['install_uninstall_recovery'],
    'mobile': ['raw_jpeg_mobile'],
}


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for data in iter(lambda: f.read(1024 * 1024), b''):
            h.update(data)
    return h.hexdigest()


def sources():
    # 包括被其它测试间接引用的工程脚本；不纳入旧测试结果。
    return {str(p.relative_to(ROOT)): digest(p)
            for folder in ('x2d/CodeTests', 'x2d/tools')
            for p in (ROOT / folder).rglob('*')
            if p.is_file() and p.suffix in ('.py', '.c', '.h', '.cpp', '.hpp', '.inc', '.qml', '.js', '.json')
            and '__pycache__' not in p.parts}


def write(path, data):
    tmp = path.with_suffix(path.suffix + '.tmp')
    tmp.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    tmp.replace(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cases', nargs='+', choices=CASES, default=list(CASES))
    parser.add_argument('--installer-revision', type=Path, help='显式使用冻结的原生事务revision；不声明当前安装器已验收')
    opt = parser.parse_args()
    if Path.cwd().resolve() != ROOT or not __debug__:
        raise RuntimeError('Use the current workspace without Python -O')
    OUT.mkdir(parents=True, exist_ok=True)
    attempt = Path(tempfile.mkdtemp(prefix='attempt-', dir=OUT))
    latest = dict(path=str(attempt.relative_to(ROOT)), passed=False, stage='preflight')
    write(OUT / 'latest-attempt.json', latest)
    report = dict(firmware='X2D 100C 4.2.0', passed=False, cases={}, requested=opt.cases,
                  full_suite_requested=set(opt.cases) == set(CASES), all_features_passed=False,
                  full_firmware_boot=False, real_camera=False, device_operations=0,
                  hardware_acceptance=False, installed=False, scope='offline_component_adapters')
    write(attempt / 'report.json', report)
    try:
        before = sources()
        write(attempt / 'source-hashes.json', before)
        drivers = {n:h for n,h in before.items() if (ROOT / n).is_relative_to(HERE)}
        # Pillow 使用随附3.12；相机同版本Qt6.4.1需要已安装的3.11，不能导入3.14。
        for name in opt.cases:
            target = attempt / name
            target.mkdir()
            env = dict(os.environ, PYTHONDONTWRITEBYTECODE='1', PYTHONUTF8='1',
                       X2D_SIM_CASE_OUT=str(target), TEMP=str(target), TMP=str(target), TMPDIR=str(target),
                       ZIG_LOCAL_CACHE_DIR=str(attempt / 'zig-local'), ZIG_GLOBAL_CACHE_DIR=str(attempt / 'zig-global'))
            env.pop('X2D_SIM_INSTALLER_REVISION', None)
            if opt.installer_revision is not None:
                revision = opt.installer_revision.resolve()
                if not revision.is_relative_to(ROOT / 'x2d/outputs') or not (revision / 'build-proof.json').is_file():
                    raise ValueError('Installer revision must be an existing local build snapshot')
                env['X2D_SIM_INSTALLER_REVISION'] = str(revision)
            interpreter = Path(sys.executable)
            if name == 'quarter':
                interpreter = Path.home() / '.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
            elif name == 'capture_ui':
                interpreter = Path(os.environ['LOCALAPPDATA']) / 'Programs/Python/Python311/python.exe'
            command = [str(interpreter), '-X', 'utf8', '-X', 'faulthandler', '-B', str(HERE / 'suite_case.py'), name, str(target)]
            start = time.monotonic()
            print('SIM_SUITE_START ' + name, flush=True)
            try:
                result = subprocess.run(command, env=env, capture_output=True, text=True,
                                        encoding='utf-8', errors='replace', timeout=2400,
                                        creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
                (target / 'console.txt').write_text(result.stdout + result.stderr, encoding='utf-8')
                report['cases'][name] = dict(passed=False, categories=CASES[name], command=command,
                    returncode=result.returncode, console_sha256=digest(target / 'console.txt'),
                    elapsed_host_seconds=time.monotonic() - start)
                child = json.loads((target / 'result.json').read_text('utf-8'))
                ok = result.returncode == 0 and child['passed'] is True and child['case'] == name
                report['cases'][name] = dict(passed=ok, categories=CASES[name],
                    result_sha256=digest(target / 'result.json'), returncode=result.returncode,
                    console_sha256=digest(target / 'console.txt'),
                    interpreter_sha256=digest(interpreter),
                    elapsed_host_seconds=time.monotonic() - start, is_camera_performance=False)
            except subprocess.TimeoutExpired as error:
                def decoded(value):
                    return value.decode('utf-8', 'replace') if isinstance(value, bytes) else value or ''
                (target / 'console.txt').write_text(decoded(error.stdout) + decoded(error.stderr), encoding='utf-8')
                report['cases'][name] = dict(passed=False, categories=CASES[name], error=str(error), timeout=True)
            except Exception as error:
                report['cases'].setdefault(name, dict(passed=False, categories=CASES[name]))['error'] = str(error)
            write(attempt / 'report.json', report)
            print('SIM_SUITE_END ' + name + ' passed=' + str(report['cases'][name]['passed']), flush=True)
        after = sources()
        report['changed_sources'] = sorted(n for n in before.keys() | after.keys() if before.get(n) != after.get(n))
        report['source_unchanged'] = not report['changed_sources']
        after_drivers = {n:h for n,h in after.items() if (ROOT / n).is_relative_to(HERE)}
        report['drivers_unchanged'] = drivers == after_drivers
        evidence = verify_inputs(attempt, report['cases'], ROOT, digest)
        write(attempt / 'input-consistency.json', evidence)
        report['tested_inputs_unchanged'] = evidence['passed']
        report['input_consistency_sha256'] = digest(attempt / 'input-consistency.json')
        report['passed'] = (report['drivers_unchanged'] and evidence['passed'] and
                           set(report['cases']) == set(opt.cases) and all(v['passed'] for v in report['cases'].values()))
        report['categories_executed'] = sorted({category for name in report['cases'] for category in CASES[name]})
        report['coverage_registry_sha256'] = digest(HERE / 'coverage.json')
        report['software_category_adapters_passed'] = report['passed'] and report['full_suite_requested']
        report['limitations'] = [
            '这是共同证据入口下的组件适配器；不同进程之间没有模拟完整 camera-service/D-Bus 服务拓扑',
            'ISP、曝光、图像池、镜头、DMA、相册原厂代理使用显式替身；物理效果/吞吐/功耗不由此验收',
            '十六亿十六帧合成使用缩小尺寸解析图样，显影25块为全尺寸描述与故障注入',
            '手机仅验证原厂有界有效性分支及软件解码，不代表完整 Phocus 打开/校正/Wi-Fi 传输通过']
    except BaseException as error:
        report['error'] = str(error)
        raise
    finally:
        write(attempt / 'report.json', report)
        write(OUT / 'latest-attempt.json', dict(latest, stage='finished', passed=report['passed']))
        print('REPORT=' + str(attempt / 'report.json'), flush=True)
    if not report['passed']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
