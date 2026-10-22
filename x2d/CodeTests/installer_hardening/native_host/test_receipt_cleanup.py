"""回执收尾故障门禁；使用内存设备模型，不访问 USB。"""
import hashlib
import ast
import json
import os
from pathlib import Path
import re
import tempfile
import sys
from types import SimpleNamespace
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import native_receipt_cleanup as module


OUTPUT = Path(__file__).resolve().parents[3]/'outputs/4.2.0/installer-hardening-r7/native-host/receipt-tests'


class MemoryDevice(module.Receipts):
    """目录/文件读取替身；归档、删除编排和本地持久化使用生产方法。"""
    def __init__(self, directory):
        super().__init__(SimpleNamespace(_native_action_id='1'*16, native_metadata={'test': 1}), directory)
        self.folder = module.CONTROL
        self.files = {self.folder+'/bootstrap.final.'+'2'*16: b'final\n',
                      self.folder+'/bootstrap.cleanup.final.'+'3'*16+'.seal': b'4'*64}
        self.folders = [self.folder]
        self.commands = []
        self.failure = None
        self.changed = False
        self.denied = False
        self.lock_busy = False

    def directories(self):
        return list(self.folders)

    def listing(self, path):
        return '20:7:0', sorted(p for p in self.files if p.startswith(path+'/'))

    def identity(self, path):
        return '20:9:1:0:'+str(len(self.files[path]))+':1:'+('2' if self.changed else '1'), len(self.files[path])

    def read(self, path, size):
        return self.files[path]

    def one(self, command):
        # 此模型只消费被生产代码实际派发的固定命令；未知命令不猜测成功。
        if len(command.encode('ascii')) > 231:
            raise AssertionError('command too long')
        self.commands.append(command)
        if command.startswith('sha256sum '):
            path = command[len('sha256sum '):]
            return hashlib.sha256(self.files[path]).hexdigest()+'  '+path
        if command.endswith('echo IDLE'):
            return '' if self.denied else 'IDLE'
        if command == 'pidof worker;echo END':
            return 'END'
        if '/proc/mounts' in command:
            return 'ro,nosuid'
        if command.startswith('f=') and '&&rm "$f"' in command:
            if self.lock_busy:
                return ''
            if self.failure == 'delete':
                raise RuntimeError('USB_UNCONFIRMED')
            path = command[2:].split(';', 1)[0]
            digest = command.split('grep -q ^', 1)[1].split('&&', 1)[0]
            if hashlib.sha256(self.files[path]).hexdigest() != digest:
                raise RuntimeError('HASH_CHANGED')
            del self.files[path]
            if self.failure == 'after-delete':
                raise RuntimeError('USB_RESULT_LOST')
            return 'R'
        if command.startswith('{ flock -n 9&&rmdir '):
            directory = command.split('rmdir ', 1)[1].split('&&', 1)[0]
            if self.files:
                raise RuntimeError('NOT_EMPTY')
            self.folders.remove(directory)
            return 'R'
        if command.startswith('f=/tmp/x2dt;'):
            return 'ABSENT'
        if command == 'test ! -e /tmp/x2dt && test ! -L /tmp/x2dt && echo ABSENT':
            return 'ABSENT'
        raise AssertionError(command)


class ReceiptTests(unittest.TestCase):
    def setUp(self):
        OUTPUT.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=OUTPUT)
        assert Path(self.temp.name).resolve().is_relative_to(OUTPUT.resolve())
        self.addCleanup(self.temp.cleanup)
        self.device = MemoryDevice(Path(self.temp.name)/'archive')

    def deletions(self):
        return [c for c in self.device.commands if '&&rm "$f"' in c]

    def test_complete_archive_precedes_prune(self):
        initial = dict(self.device.files)
        manifest = self.device.archive()
        self.assertFalse(self.deletions())
        for item in manifest['files']:
            self.assertEqual((self.device.root/item['local']).read_bytes(), initial[item['path']])
        self.device.prune(manifest)
        self.assertFalse(self.device.files)
        self.assertFalse(self.device.folders)
        self.assertEqual(json.loads((self.device.root/'closed.json').read_text())['state'],
                         'ARCHIVED_AND_REMOVED')

    def test_archive_write_failure_never_deletes(self):
        with patch.object(module, 'durable', side_effect=OSError('disk full')):
            with self.assertRaises(OSError):
                self.device.archive()
        self.assertFalse(self.deletions())

    def test_changed_identity_stops_all_deletion(self):
        manifest = self.device.archive()
        self.device.changed = True
        with self.assertRaises(Exception):
            self.device.prune(manifest)
        self.assertFalse(self.deletions())

    def test_changed_local_copy_stops_all_deletion(self):
        manifest = self.device.archive()
        (self.device.root/manifest['files'][0]['local']).write_bytes(b'changed')
        with self.assertRaises(Exception):
            self.device.prune(manifest)
        self.assertFalse(self.deletions())

    def test_added_remote_file_stops_all_deletion(self):
        manifest = self.device.archive()
        self.device.files[self.device.folder+'/extra'] = b'new'
        with self.assertRaises(Exception):
            self.device.prune(manifest)
        self.assertFalse(self.deletions())

    def test_active_task_stops_all_deletion(self):
        manifest = self.device.archive()
        self.device.denied = True
        with self.assertRaises(Exception):
            self.device.prune(manifest)
        self.assertFalse(self.deletions())

    def test_transport_failure_has_no_retry_or_closed_record(self):
        for fault in ('delete', 'after-delete'):
            with self.subTest(fault=fault):
                other = MemoryDevice(Path(self.temp.name)/fault)
                manifest = other.archive()
                other.failure = fault
                with self.assertRaisesRegex(RuntimeError, 'USB'):
                    other.prune(manifest)
                self.assertEqual(sum('&&rm "$f"' in c for c in other.commands), 1)
                self.assertTrue((other.root/'prune-intent.json').is_file())
                self.assertFalse((other.root/'closed.json').exists())

    def test_file_scope(self):
        self.assertTrue(module.valid_file(module.CONTROL, 'bootstrap.cleanup.final.'+'f'*16+'.seal'))
        self.assertFalse(module.valid_file(module.CONTROL, '../other'))
        self.assertFalse(module.valid_file(module.CONTROL, 'photo.jpg'))
        tidy = '/data/local/tmp/x2d-tidy-12345678'
        self.assertTrue(module.valid_file(tidy, 'dispatch.'+'e'*16+'.log'))
        self.assertFalse(module.valid_file(tidy, 'unknown'))

    def test_busy_lock_never_deletes(self):
        manifest = self.device.archive()
        before = dict(self.device.files)
        self.device.lock_busy = True
        with self.assertRaises(Exception):
            self.device.prune(manifest)
        self.assertEqual(self.device.files, before)
        self.assertFalse((self.device.root/'closed.json').exists())

    def test_pending_survives_new_action(self):
        root = Path(self.temp.name)/'journal'
        pending = root/'old-action'/'camera-receipts-123'
        pending.mkdir(parents=True)
        (pending/'prune-intent.json').write_text('{}')
        with self.assertRaisesRegex(Exception, '请先诊断'):
            module.assert_no_pending(root)
        for bad in (b'', b'{', b'{"state":"OTHER"}'):
            (pending/'closed.json').write_bytes(bad)
            with self.assertRaises(Exception):
                module.assert_no_pending(root)
        (pending/'closed.json').write_bytes(b'{"state":"ARCHIVED_AND_REMOVED"}\n')
        module.assert_no_pending(root)

    def test_journal_root_is_shared_on_both_platforms(self):
        source = Path(__file__).with_name('native_client_support.py')
        tree = ast.parse(source.read_text(encoding='utf8'))
        fn = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == 'journal_root')
        for platform, environment, suffix in (
            ('windows', {'LOCALAPPDATA': str(Path(self.temp.name)/'local')}, 'local'),
            ('windows', {}, 'AppData/Local'),
            ('mac', {}, 'Library/Application Support'),
        ):
            scope = dict(Path=Path, os=os, PLATFORM=platform)
            exec(compile(ast.Module(body=[fn], type_ignores=[]), str(source), 'exec'), scope)
            with patch.dict(os.environ, environment, clear=True), patch.object(Path, 'home', return_value=Path(self.temp.name)):
                self.assertEqual(scope['journal_root'](),
                                 Path(self.temp.name)/suffix/'X2DInstaller/native-transactions')
        for name in ('operations', 'begin_action'):
            body = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == name)
            self.assertTrue(any(isinstance(n, ast.Call) and isinstance(n.func, ast.Name) and n.func.id == 'journal_root'
                                for n in ast.walk(body)))


if __name__ == '__main__':
    unittest.main()
