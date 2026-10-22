"""重验本次场景实际输入与工件；不把无关工作区文件当作被测依赖。"""
from pathlib import Path
import json
import re


def verify(attempt, cases, root, digest):
    expected, problems = {}, []
    artifact_count = 0
    for name, record in cases.items():
        directory = attempt / name
        try:
            result_path = directory / 'result.json'
            if digest(result_path) != record.get('result_sha256'):
                raise ValueError('子报告与本轮回执不一致')
            result = json.loads(result_path.read_text('utf-8'))
            if result['case'] != name or result['passed'] is not True or result['inputs_unchanged'] is not True:
                raise ValueError('子场景未通过或场景名不匹配')
            for field in ('input_sha256', 'external_runtime_sha256', 'native_module_sha256'):
                values = result[field]
                if not isinstance(values, dict) or (field != 'external_runtime_sha256' and not values):
                    raise ValueError('缺少实际输入清单: ' + field)
                for value, sha in values.items():
                    if not isinstance(sha, str) or not re.fullmatch('[0-9a-f]{64}', sha):
                        raise ValueError('非法输入摘要: ' + value)
                    raw = Path(value)
                    path = (root / raw).resolve() if field == 'input_sha256' else raw.resolve()
                    if field == 'input_sha256':
                        if raw.is_absolute() or not path.is_relative_to(root):
                            raise ValueError('输入相对路径越界: ' + value)
                    elif not raw.is_absolute() or str(raw).startswith('\\\\.\\'):
                        raise ValueError('非法原生依赖路径: ' + value)
                    if path.is_relative_to(attempt):
                        raise ValueError('本轮工件不能冒充外部输入: ' + value)
                    prior = expected.get(path)
                    if prior is not None and prior != sha:
                        problems.append(dict(case=name, kind='cross_stage_conflict', path=str(path)))
                    expected[path] = sha
            for value, sha in result['artifact_sha256'].items():
                path = (directory / value).resolve()
                if not path.is_relative_to(directory) or not path.is_file() or digest(path) != sha:
                    problems.append(dict(case=name, kind='artifact_changed', path=str(path)))
                artifact_count += 1
            if digest(directory / 'console.txt') != record.get('console_sha256'):
                problems.append(dict(case=name, kind='console_changed'))
        except Exception as error:
            problems.append(dict(case=name, kind='invalid_evidence', error=str(error)))
    for path, sha in expected.items():
        if not path.is_file() or digest(path) != sha:
            problems.append(dict(kind='input_changed', path=str(path)))
    return dict(passed=not problems and bool(cases), input_count=len(expected),
                artifact_count=artifact_count, problems=problems,
                input_sha256={str(path): sha for path, sha in sorted(expected.items())})
