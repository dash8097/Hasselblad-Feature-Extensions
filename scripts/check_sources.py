"""核对源码清单、文件摘要、Python 语法和文档内部链接。"""
import ast
import hashlib
import json
from pathlib import Path
import re
import sys
from urllib.parse import unquote

ROOT=Path(__file__).resolve().parents[1]
IGNORED={'outputs','.git','__pycache__','.venv','node_modules','.zig-cache','zig-cache'}

def files(root):
    return {p.relative_to(root).as_posix():p for p in root.rglob('*')
            if p.is_file() and not any(part in IGNORED for part in p.relative_to(root).parts)
            and p.relative_to(root).as_posix()!='source-files.json'}

def check(root):
    errors=[]
    try:manifest=json.loads((root/'source-files.json').read_text(encoding='utf-8'))
    except (OSError,ValueError):return ['source-files.json: missing or invalid']
    actual=files(root)
    for p in sorted(set(manifest)-set(actual)):errors.append('missing: '+p)
    for p in sorted(set(actual)-set(manifest)):errors.append('unlisted: '+p)
    for name,path in actual.items():
        data=path.read_bytes()
        if name in manifest and hashlib.sha256(data).hexdigest()!=manifest[name]:errors.append('content differs: '+name)
        if path.is_symlink():errors.append('symbolic link: '+name)
        try:text=data.decode('utf-8-sig')
        except UnicodeDecodeError:errors.append('not UTF-8 source: '+name);continue
        if '\0' in text:errors.append('NUL in source: '+name)
        if path.suffix=='.py':
            try:ast.parse(text,filename=name)
            except SyntaxError as e:errors.append('syntax: '+name+':'+str(e.lineno))
        if path.suffix=='.json':
            try:json.loads(text)
            except ValueError:errors.append('JSON: '+name)
        if path.suffix=='.md':
            for link in re.findall(r'\]\(([^\s)]+)\)',text):
                if re.match(r'^[a-zA-Z][a-zA-Z0-9+.-]*:|^#',link):continue
                target=(path.parent/unquote(link.split('#')[0])).resolve()
                if root.resolve()!=target and root.resolve() not in target.parents:errors.append('outside link: '+name)
                elif not target.exists():errors.append('broken link: '+name+' -> '+link)
    return errors

if __name__=='__main__':
    errors=check(ROOT)
    for error in errors:print(error)
    print('FAIL' if errors else 'PASS: source manifest, syntax and local documentation links')
    sys.exit(bool(errors))
