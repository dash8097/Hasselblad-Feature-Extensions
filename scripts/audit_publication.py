"""只读公开文件清单检查；不修改历史，不证明法律权利。"""
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
MANIFEST = 'publication/public-allowlist.json'
BLOCKED = re.compile(
    r'(?i)\bdecrypt(?:ion)?\b|Crypto\.Cipher|STATIC_KEY|IV_SALT|MAGIC_HASH_SALT|'
    r'com\.hasselblad|cdn\.hasselblad|qrc:///|/system/(?:lib|bin|etc)|'
    r'\bdlopen\b|\bdlsym\b|\bmprotect\b|LD_PRELOAD|'
    r'BEGIN (?:RSA |OPENSSH |EC )?PRIVATE KEY|gh[pousr]_[A-Za-z0-9]{20,}|'
    r'C:[/\\]Users[/\\]|/data/local/tmp|hasselblad-cim-firmware-extractor'
)
def validate(blobs):
    records = json.loads(blobs[MANIFEST].decode('utf-8'))['files']
    expected = {r['path']: r['sha256'] for r in records}
    errors = []
    if len(expected) != len(records): errors.append('duplicate-manifest-path')
    if set(blobs) != set(expected) | {MANIFEST}: errors.append('file-set-mismatch')
    for path, digest in expected.items():
        parts = PurePosixPath(path).parts
        if not parts or path.startswith('/') or '..' in parts or '\\' in path:
            errors.append('invalid-manifest-path'); continue
        content = blobs.get(path)
        if content is None or hashlib.sha256(content).hexdigest() != digest:
            errors.append('content-mismatch:' + path); continue
        try: text = content.decode('utf-8')
        except UnicodeDecodeError:
            errors.append('non-text-file:' + path); continue
        if '\x00' in text: errors.append('embedded-binary:' + path)
        if path.endswith(('.c','.cpp','.h','.py','.js','.qml','.html','.svg')) \
                and path != 'scripts/audit_publication.py' and BLOCKED.search(text):
            errors.append('excluded-material-pattern:' + path)
    return errors
def main():
    root = Path(__file__).resolve().parents[1]
    paths = subprocess.run(['git','ls-files','-z'],cwd=root,check=True,
                           capture_output=True).stdout.decode('utf-8').split('\0')
    blobs = {}
    for path in filter(None, paths):
        file = root / path
        if file.is_symlink(): raise SystemExit('Refusing symlink: ' + path)
        try: file.resolve().relative_to(root)
        except ValueError: raise SystemExit('Outside repository: ' + path)
        if file.is_file(): blobs[path] = file.read_bytes()
    errors = validate(blobs)
    if errors: raise SystemExit('\n'.join(errors))
    print('PASS: tracked files match reviewed text allowlist; history not checked')
if __name__ == '__main__':
    main()
