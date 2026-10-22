"""原厂恢复后的回执收尾。固定目录、先本地归档，不上传辅助程序或递归删除。"""
import base64
import hashlib
import json
import os
from pathlib import Path
import re
import secrets

from native_adapter import check


CONTROL = '/data/local/tmp/x2d-bootstrap'
LOCK = '/tmp/x2dt'


def durable(path, raw):
    """独占落盘；短写、磁盘满或回读不一致均阻止后续相机删除。"""
    check(not path.is_symlink(), 'receipt-archive-link')
    with path.open('xb') as stream:
        check(stream.write(raw) == len(raw), 'receipt-archive-short-write')
        stream.flush()
        os.fsync(stream.fileno())
    check(path.read_bytes() == raw, 'receipt-archive-readback')
    if os.name != 'nt':
        fd = os.open(path.parent, os.O_RDONLY | os.O_DIRECTORY)
        try:
            os.fsync(fd)
        finally:
            os.close(fd)


def valid_file(directory, name):
    if directory == CONTROL:
        return re.fullmatch(
            r'bootstrap\.(?:claim|final|cleanup\.(?:claim|handoff|final))\.[0-9a-f]{16}(?:\.seal)?',
            name) is not None
    return name in ('started', 'result', 'final', 'progress') or re.fullmatch(
        r'dispatch\.[0-9a-f]{16}\.(?:claim|started|exit|log)', name) is not None


class Receipts:
    def __init__(self, installer, directory):
        self.installer = installer
        self.root = Path(directory)
        self.root.mkdir(parents=True, exist_ok=False)
        check(not self.root.is_symlink(), 'receipt-archive-directory-link')

    def one(self, command):
        check(len(command.encode('ascii')) <= 231, 'receipt-command-limit')
        return self.installer.one(command)

    def directories(self):
        text = self.one('for d in /data/local/tmp/x2d-bootstrap /data/local/tmp/x2d-tidy-*;do '
                        'if test -e "$d" || test -L "$d";then echo "$d";fi;done;echo END')
        lines = text.splitlines()
        check(lines and lines[-1] == 'END', 'receipt-directory-list-incomplete')
        paths = sorted(lines[:-1])
        check(len(paths) <= 32 and len(paths) == len(set(paths)), 'receipt-directory-count')
        check(all(p == CONTROL or re.fullmatch(r'/data/local/tmp/x2d-tidy-[0-9a-f]{8}', p)
                  for p in paths), 'receipt-directory-scope')
        return paths

    def listing(self, directory):
        state = self.one('d='+directory+';test -d "$d" && test ! -L "$d" && '
                         'readlink -f "$d";stat -c %d:%i:%u "$d"')
        fields = state.splitlines()
        check(len(fields) == 2 and fields[0] == directory and
              re.fullmatch(r'[0-9]+:[0-9]+:0', fields[1]), 'receipt-directory-identity')
        count = self.one('find '+directory+' -mindepth 1 -maxdepth 1 | wc -l')
        check(count.isdigit() and int(count) <= 256, 'receipt-file-count')
        names = []
        for offset in range(0, int(count), 16):
            page = self.one('find '+directory+' -mindepth 1 -maxdepth 1 | sort | sed -n '+
                            "'"+str(offset+1)+','+str(offset+16)+"p';echo END").splitlines()
            check(page and page[-1] == 'END', 'receipt-file-list-incomplete')
            names.extend(page[:-1])
        check(len(names) == int(count) and len(names) == len(set(names)), 'receipt-file-list-count')
        check(all(p.startswith(directory+'/') and valid_file(directory, p[len(directory)+1:])
                  for p in names), 'receipt-unknown-file')
        return fields[1], names

    def identity(self, path):
        text = self.one('f='+path+';test -f "$f" && test ! -L "$f" && '
                        'stat -c %d:%i:%h:%u:%s:%Y:%Z "$f"')
        match = re.fullmatch(r'([0-9]+):([0-9]+):1:0:([0-9]+):([0-9]+):([0-9]+)', text)
        check(match is not None and int(match[3]) <= 131072, 'receipt-file-identity')
        return text, int(match[3])

    def read(self, path, size):
        raw = bytearray()
        for offset in range(0, size, 3072):
            encoded = self.one('dd if='+path+' bs=3072 skip='+str(offset//3072)+
                               ' count=1 2>/dev/null|/system/xbin/busybox base64')
            part = base64.b64decode(''.join(encoded.split()), validate=True)
            check(len(part) == min(3072, size-offset), 'receipt-short-read')
            raw.extend(part)
        return bytes(raw)

    def archive(self):
        manifest = dict(schema='X2D_LOCAL_RECEIPT_ARCHIVE_V1',
                        action=getattr(self.installer, '_native_action_id', None),
                        metadata_sha256=hashlib.sha256(json.dumps(
                            self.installer.native_metadata, sort_keys=True).encode('ascii')).hexdigest(),
                        directories=[], files=[])
        paths = self.directories()
        for index, directory in enumerate(paths):
            identity, files = self.listing(directory)
            bucket = self.root / str(index)
            bucket.mkdir()
            manifest['directories'].append(dict(path=directory, identity=identity, files=files))
            for path in files:
                before, size = self.identity(path)
                raw = self.read(path, size)
                digest = hashlib.sha256(raw).hexdigest()
                check(self.one('sha256sum '+path) == digest+'  '+path, 'receipt-archive-sha')
                check(self.identity(path)[0] == before, 'receipt-changed-during-read')
                local = bucket/path.rsplit('/', 1)[1]
                durable(local, raw)
                manifest['files'].append(dict(path=path, identity=before, sha256=digest,
                                              local=local.relative_to(self.root).as_posix()))
            check(self.listing(directory) == (identity, files), 'receipt-directory-changed')
        check(self.directories() == paths, 'receipt-directory-set-changed')
        durable(self.root/'manifest.json', json.dumps(manifest, sort_keys=True).encode('ascii'))
        if os.name != 'nt':
            fd = os.open(self.root.parent, os.O_RDONLY | os.O_DIRECTORY)
            try:
                os.fsync(fd)
            finally:
                os.close(fd)
        return manifest

    def prune(self, manifest):
        # 只有调用者已经验过原厂文件、完成 helper 自删除后才调用。
        check(self.one('test ! -e /system/bin/x2d-native-setup && '
                       'test ! -e /tmp/x2d-multishot-active && '
                       'test ! -e /tmp/x2d-data-cleanup-lock && echo IDLE') == 'IDLE',
              'receipt-cleanup-not-idle')
        check(self.one('pidof worker;echo END') == 'END', 'receipt-worker-active')
        check('ro' in self.one("""awk '$2=="/system"{print $4}' /proc/mounts""").split(','),
              'receipt-system-not-readonly')
        check(self.directories() == [d['path'] for d in manifest['directories']],
              'receipt-directory-set-changed')
        for directory in manifest['directories']:
            check(self.listing(directory['path']) == (directory['identity'], directory['files']),
                  'receipt-directory-changed')
        # 所有本地证据和远端身份再核对后，才派发第一条删除。
        for item in manifest['files']:
            check(hashlib.sha256((self.root/item['local']).read_bytes()).hexdigest() == item['sha256'],
                  'receipt-local-archive-changed')
            check(self.identity(item['path'])[0] == item['identity'], 'receipt-identity-changed')
        durable(self.root/'prune-intent.json', json.dumps(manifest, sort_keys=True).encode('ascii'))
        for index, item in enumerate(manifest['files']):
            command = ('f='+item['path']+';{ flock -n 9&&test ! -L "$f"&&sha256sum "$f"|grep -q ^'+
                       item['sha256']+'&&rm "$f"&&echo R;} 9<'+LOCK)
            reply = self.one(command)
            durable(self.root/('removed-'+str(index)+'.txt'), reply.encode('ascii'))
            check(reply == 'R', 'receipt-delete-unconfirmed')
        for directory in manifest['directories']:
            path = directory['path']
            check(self.listing(path) == (directory['identity'], []), 'receipt-extra-file-remains')
            check(self.one('{ flock -n 9&&rmdir '+path+'&&echo R;} 9<'+LOCK) == 'R',
                  'receipt-rmdir-unconfirmed')
        check(self.directories() == [], 'receipt-directories-remain')
        # 事务锁不在回执目录中：只处理固定、空、root 所有且未占用的普通文件。
        state = self.one('f='+LOCK+';if test ! -e "$f" && test ! -L "$f";then echo ABSENT;'
                         'else test -f "$f" && test ! -L "$f" && stat -c %u:%a:%h:%s:%d:%i "$f";fi')
        if state != 'ABSENT':
            check(re.fullmatch(r'0:600:1:0:[0-9]+:[0-9]+', state), 'receipt-lock-identity')
            durable(self.root/'lock-intent.txt', state.encode('ascii'))
            command = ('{ flock -n 9&&[ "$(stat -c %u:%a:%h:%s:%d:%i '+LOCK+')" = '+state+
                       ' ]&&test ! -L '+LOCK+'&&rm '+LOCK+'&&echo REMOVED;} 9<'+LOCK)
            check(self.one(command) == 'REMOVED', 'receipt-lock-delete-unconfirmed')
        check(self.one('test ! -e '+LOCK+' && test ! -L '+LOCK+' && echo ABSENT') == 'ABSENT',
              'receipt-lock-remains')
        durable(self.root/'closed.json', b'{"state":"ARCHIVED_AND_REMOVED"}\n')


def assert_no_pending(journal_root):
    """重开窗口仍拒绝重派发：未决删除只能先诊断，不重新 bootstrap。"""
    root = Path(journal_root)
    if not root.exists():
        return
    for intent in root.glob('*/camera-receipts-*/prune-intent.json'):
        check(not intent.is_symlink() and not intent.parent.is_symlink(), 'receipt-pending-link')
        closed = intent.parent/'closed.json'
        check(not closed.is_symlink() and closed.is_file() and closed.stat().st_size < 128 and
              closed.read_bytes() == b'{"state":"ARCHIVED_AND_REMOVED"}\n',
              '上次卸载回执清理未确认；请先诊断核对，不能重复安装或卸载。证据：'+str(intent.parent))


def run(installer, journal, *, remove=False):
    receipt = Receipts(installer, Path(journal)/('camera-receipts-'+secrets.token_hex(8)))
    manifest = receipt.archive()
    if remove:
        receipt.prune(manifest)
    return receipt.root
