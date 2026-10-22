import unittest
import package_profiles as p


class ProfileTests(unittest.TestCase):
    def test_default_and_exact_choices(self):
        self.assertEqual(p.DEFAULT,248)
        self.assertEqual([x['mask'] for x in p.PROFILES],[248,255])
        for bad in (True,False,'248',None,0,7,247,249,256):
            with self.subTest(bad=bad),self.assertRaises(ValueError):p.profile(bad)

    def test_advanced_contains_basic_and_keeps_separate_speed_library(self):
        rows=[{'path':'/system/lib64/libx2d_af_base0.so'},
              {'path':'/system/lib64/libx2d_af_probe.so'},
              {'path':'/system/bin/x2d-raw-stack/worker'},
              {'path':'/system/lib64/libx2d_native_menu.so'},
              {'path':'/system/bin/x2d-esflash'}]
        basic=p.selected_targets(rows,248)
        self.assertEqual([r['path'] for r in basic],
                         [rows[i]['path'] for i in (0,3,4)])
        self.assertEqual(p.selected_targets(rows,255),rows)
        basic[0]['path']='changed'
        self.assertEqual(rows[0]['path'],'/system/lib64/libx2d_af_base0.so')

    def test_unknown_or_ambiguous_paths_fail_closed(self):
        for path in ('/system/bin/unknown','/system/bin/x2d-raw-stack/../unknown',
                     '/system/bin//x2d-raw-stack/a','/system/bin/x2d-raw-stack/',
                     '/system/bin/x2d-raw-stack-other/a','relative','/system/./a'):
            with self.subTest(path=path),self.assertRaises(ValueError):p.required_profile(path)
        row={'path':'/system/lib64/libx2d_af_base0.so'}
        with self.assertRaises(ValueError):p.selected_targets([row,row],248)

    def test_basic_init_uses_same_tuned_library(self):
        init=b'service camera-service /system/bin/camera-service\n    setenv LD_PRELOAD /system/lib64/libx2d_af_probe.so:other\n'
        self.assertEqual(p.service_init(init,255),init)
        self.assertEqual(p.service_init(init,248),
                         init.replace(b'libx2d_af_probe.so',b'libx2d_af_base0.so'))
        for bad in (init+init,init.replace(b'libx2d_af_probe.so',b'libx2d_af_base0.so'),init+b'\0'):
            with self.assertRaises(ValueError):p.service_init(bad,248)


if __name__=='__main__':unittest.main()
