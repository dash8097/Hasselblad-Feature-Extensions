"""双档界面契约：执行 Mac 选择回调，检查 Windows 请求与待恢复锁定。"""
import ast
import ctypes
from pathlib import Path
from types import SimpleNamespace
import unittest
import build_native_client as builder
import profile_ui

class ProfileUiTests(unittest.TestCase):
    def test_windows_selection_and_request(self):
        path=builder.HERE/'build_inputs/windows_ui/RawStackInstaller.cs'
        text=profile_ui.windows(path.read_text(encoding='utf-8-sig'))
        self.assertIn('new RadioButton[2]',text)
        self.assertIn('Checked=i==0',text)
        self.assertIn('feature_mask=operationMask',text)
        self.assertIn('box.Enabled=!update.Pending',text)
        self.assertNotIn('feature_mask=255',text)

    def test_mac_exclusive_selection_and_pending_lock(self):
        text=profile_ui.mac((builder.BASE/'mac_gui.py').read_text(encoding='utf-8-sig'))
        tree=ast.parse(text)
        cls=next(n for n in tree.body if isinstance(n,ast.ClassDef) and n.name=='App')
        fn=next(n for n in cls.body if isinstance(n,ast.FunctionDef) and n.name=='profile_pressed')
        scope={'C':ctypes}
        exec(compile(ast.Module(body=[fn],type_ignores=[]),'<profile-ui>','exec'),scope)
        calls=[]
        app=SimpleNamespace(busy=False,installer=SimpleNamespace(pending=False),
                            feature_indicators=[(10,248),(20,255)],send=lambda *a:calls.append(a))
        scope['profile_pressed'](app,20)
        self.assertEqual([c[-1][1] for c in calls],[0,1])
        for busy,pending in ((True,False),(False,True)):
            calls.clear();app.busy=busy;app.installer.pending=pending
            scope['profile_pressed'](app,10)
            self.assertFalse(calls)

if __name__=='__main__':unittest.main()
