"""同包卸载复用 helper、完整档位绑定和更新选择保持。无 USB。"""
import ast
import copy
import hashlib
import json
from pathlib import Path
import re
from types import SimpleNamespace
import unittest

HERE = Path(__file__).resolve().parent


def load_function(file, name, scope):
    tree = ast.parse((HERE/file).read_text(encoding='utf8'))
    node = next(n for n in ast.walk(tree) if isinstance(n, ast.FunctionDef) and n.name == name)
    exec(compile(ast.Module(body=[node], type_ignores=[]), file, 'exec'), scope)
    return scope[name]


def check(value, reason):
    if not value:
        raise ValueError(reason)


class Tests(unittest.TestCase):
    def setUp(self):
        self.profiles = {}
        for mask in (248, 255):
            self.profiles[str(mask)] = dict(
                stock=dict(feature_mask=mask, required_guards={'/system/guard': 'original'}),
                removal=dict(targets=[dict(path='/system/test', original='original',
                                         mode='644', allowed=['a', 'b'] if mask == 248 else ['b', 'a'])]),
                setup_resource='native/'+str(mask), payload_prefix=str(mask)+'/')
        self.raw = json.dumps(dict(profiles=self.profiles)).encode()
        self.anchors = {'native/client-metadata.json': hashlib.sha256(self.raw).hexdigest(),
                        'native/248': 'a'*64, 'native/255': 'b'*64}
        self.calls = []
        self.reply = 'ABSENT'
        def one(cmd):
            self.calls.append(cmd)
            return self.reply
        self.installer = SimpleNamespace(root=HERE, _native_action='Uninstall', one=one,
                                        log=[], stock=self.profiles['248']['stock'])
        scope = dict(check=check, ordinary=lambda *args:SimpleNamespace(read_bytes=lambda:self.raw),
                     digest=lambda raw:hashlib.sha256(raw).hexdigest(), RESOURCE_HASHES=self.anchors,
                     strict_json=json.loads, re=re)
        self.select = load_function('native_client_support.py', 'select_uninstall_helper', scope)

    def test_exact_same_package_profile_and_payload(self):
        for digest, mask in (('a',248), ('b',255)):
            self.reply = digest*64+'  /system/bin/x2d-native-setup'
            self.select(self.installer)
            self.assertEqual(self.installer.stock['feature_mask'], mask)
            self.assertEqual(self.installer.native_metadata['payload_prefix'], str(mask)+'/')
            self.assertEqual(self.installer.removal, self.profiles[str(mask)]['removal'])
        self.assertEqual(len(self.calls), 2)
        self.assertTrue(all('sha256sum' in c and len(c.encode()) <= 231 for c in self.calls))

    def test_absent_and_other_versions_use_default_without_allowing_them(self):
        # 未登记摘要不会被加入任何 allowlist，后续 NativeBootstrap.ensure 仍拒绝。
        for reply in ('ABSENT', 'c'*64+'  /system/bin/x2d-native-setup'):
            self.reply = reply
            self.select(self.installer)
            self.assertEqual(self.installer.native_metadata['setup_resource'], 'native/248')
            self.assertNotIn('bootstrap_previous_sha256', self.installer.native_metadata)

    def test_bad_file_reply_rejected(self):
        for reply in ('LINK', 'OTHER', '', 'a'*64, 'a'*64+'  /system/wrong'):
            self.reply = reply
            with self.assertRaises(ValueError):
                self.select(self.installer)

    def test_resource_tampering_rejected_before_usb(self):
        self.raw += b' '
        with self.assertRaisesRegex(ValueError, 'metadata-anchor'):
            self.select(self.installer)
        self.assertFalse(self.calls)

    def test_scope_mismatch_rejected_before_usb(self):
        for key in ('original', 'mode', 'allowed'):
            changed = copy.deepcopy(self.profiles)
            changed['255']['removal']['targets'][0][key] = ['c'] if key == 'allowed' else 'changed'
            self.raw = json.dumps(dict(profiles=changed)).encode()
            self.anchors['native/client-metadata.json'] = hashlib.sha256(self.raw).hexdigest()
            with self.assertRaisesRegex(ValueError, 'scope-mismatch'):
                self.select(self.installer)
        self.assertFalse(self.calls)

    def test_ambiguous_helpers_rejected(self):
        self.anchors['native/255'] = 'a'*64
        self.reply = 'a'*64+'  /system/bin/x2d-native-setup'
        with self.assertRaisesRegex(ValueError, 'ambiguous'):
            self.select(self.installer)

    def test_update_selection_restored_after_success_or_failure(self):
        for action in ('Install', 'Uninstall'):
            for route in ('Stock', 'KnownProject'):
                for failure in (False, True):
                    inst = self.installer
                    inst._native_action = action
                    self.reply = 'b'*64+'  /system/bin/x2d-native-setup'
                    requested = dict(native_metadata=self.profiles['248'], stock=self.profiles['248']['stock'],
                                     full_stock=self.profiles['248']['stock'], removal=self.profiles['248']['removal'])
                    inst.__dict__.update(requested)
                    inst.context = {}
                    inst.status = lambda _: None
                    def operation(*args, **kwargs):
                        self.assertEqual(inst.native_metadata['payload_prefix'], '255/')
                        if failure:
                            raise OSError('simulated disconnect')
                    inst.cleanup = operation
                    inst.data_cleanup = operation
                    support = SimpleNamespace(select_uninstall_helper=self.select,
                        prepare_uninstall=operation, cleanup_tool=operation, archive_uninstall_receipts=operation,
                        operations=lambda _:SimpleNamespace(uninstall_files=operation),
                        payload=lambda *_:None, verify_targets=operation)
                    fn = load_function('native_client_methods.py', 'uninstall',
                                       dict(native_client_support=support, removal_route=lambda *_:None))
                    if failure:
                        with self.assertRaises(OSError):
                            fn(inst, {}, route)
                    else:
                        fn(inst, {}, route)
                    for key, value in requested.items():
                        self.assertIs(getattr(inst, key), value)


if __name__ == '__main__':
    unittest.main()
