"""执行实际主机收尾函数；不导入 USB、不访问相机。"""
import ast
from pathlib import Path
from types import SimpleNamespace
import unittest


class CleanupCompletionTest(unittest.TestCase):
    def test_uninstall_waits_for_reboot_before_cleanup(self):
        path=Path(__file__).with_name('native_client_methods.py')
        tree=ast.parse(path.read_text(encoding='utf8'))
        cls=next(n for n in tree.body if isinstance(n,ast.ClassDef))
        fn=next(n for n in cls.body if isinstance(n,ast.FunctionDef) and n.name=='uninstall')
        for route in ('KnownProject','Stock'):
            with self.subTest(route=route):
                calls=[]
                support=SimpleNamespace(
                    select_uninstall_helper=lambda _:calls.append('select-helper'),
                    prepare_uninstall=lambda _:calls.append('exit-preparation'),
                    operations=lambda _:SimpleNamespace(uninstall_files=lambda p:calls.append('uninstall')),
                    payload=lambda *args:'payload',
                    verify_targets=lambda *args,**kwargs:calls.append('verify-original'),
                    archive_uninstall_receipts=lambda _,remove=False:calls.append('prune-receipts' if remove else 'archive-receipts'),
                    cleanup_tool=lambda _:calls.append('tool'))
                scope={'native_client_support':support,'removal_route':lambda *args:calls.append('route')}
                exec(compile(ast.Module(body=[fn],type_ignores=[]),str(path),'exec'),scope)
                obj=SimpleNamespace(context={},log=[],native_metadata={},stock={},full_stock={},removal={'targets':[]},status=lambda _:None,
                    cleanup=lambda:calls.append('retained'),data_cleanup=lambda mode:calls.append('data-'+mode))
                text=scope['uninstall'](obj,{},route)
                if route=='KnownProject':
                    self.assertEqual(calls,['select-helper','route','exit-preparation','uninstall','verify-original'])
                    self.assertEqual(obj.context['native_files_complete'],'uninstall')
                    self.assertIn('关机',text)
                else:
                    self.assertEqual(calls,['select-helper','archive-receipts','retained','data-uninstalled','tool','prune-receipts'])

    def test_only_complete_receipt_can_advance(self):
        path=Path(__file__).with_name('native_client_support.py')
        tree=ast.parse(path.read_text(encoding='utf8'))
        fn=next(n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name=='cleanup_data')
        for mode in ('before','installed','uninstalled'):
            for outcome in ('complete','deferred-capture','deferred-cleanup','deferred-retained'):
                with self.subTest(mode=mode,outcome=outcome):
                    calls=[]
                    result=SimpleNamespace(deferred=outcome!='complete',fields={'outcome':outcome})
                    def cleanup(selected):
                        calls.append(selected)
                        return result
                    scope={'operations':lambda _:SimpleNamespace(data_cleanup=cleanup)}
                    exec(compile(ast.Module(body=[fn],type_ignores=[]),str(path),'exec'),scope)
                    installer=SimpleNamespace(log=[])
                    if result.deferred:
                        with self.assertRaisesRegex(RuntimeError,'清理尚未完成'):
                            scope['cleanup_data'](installer,mode)
                    else:
                        self.assertIs(scope['cleanup_data'](installer,mode),result)
                    self.assertEqual(calls,[mode])
                    self.assertEqual(len(installer.log),1)


if __name__=='__main__':unittest.main()
