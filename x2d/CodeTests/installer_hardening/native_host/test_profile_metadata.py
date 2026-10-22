"""离线执行包档位路由；卸载必须使用覆盖两档的基础清理表。"""
import ast
from pathlib import Path
import unittest

class ProfileMetadataTests(unittest.TestCase):
    def setUp(self):
        path=Path(__file__).with_name('native_client_support.py')
        fn=next(n for n in ast.parse(path.read_text(encoding='utf8')).body if isinstance(n,ast.FunctionDef) and n.name=='select_profile')
        def check(ok,reason):
            if not ok:raise ValueError(reason)
        scope={'check':check};exec(compile(ast.Module(body=[fn],type_ignores=[]),str(path),'exec'),scope)
        self.select=scope['select_profile']
        self.profiles={'profiles':{str(n):{'schema':'X2D_NATIVE_CLIENT_V1','firmware':'4.2.0','stock':{'feature_mask':n}} for n in (248,255)}}
    def test_install_uses_exact_selection(self):
        for n in (248,255):self.assertEqual(self.select(self.profiles,n,'Install')['stock']['feature_mask'],n)
        for n in (True,0,7,'248'):
            with self.assertRaises(ValueError):self.select(self.profiles,n,'Install')
    def test_uninstall_always_full_removal_profile(self):
        for n in (248,255):self.assertEqual(self.select(self.profiles,n,'Uninstall')['stock']['feature_mask'],248)
    def test_missing_or_mismatched_profile_refused(self):
        self.profiles['profiles']['248']['stock']['feature_mask']=255
        with self.assertRaises(ValueError):self.select(self.profiles,248,'Install')
        del self.profiles['profiles']['248']
        with self.assertRaises(ValueError):self.select(self.profiles,255,'Install')

if __name__=='__main__':unittest.main()
