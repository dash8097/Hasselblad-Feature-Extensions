"""只测试收尾竞态、旧回执和 USB 中断决策；不访问 USB 或执行任何命令。"""
import unittest
from native_transaction_contract import Dispatch, ReceiptError, parse_final, invocation

ATTEMPT='0123456789abcdef'
MANIFEST='1'*64


def receipt(operation='install-stock', primary=0, restore=0, **changes):
    values={'operation':operation,'attempt':ATTEMPT,'primary_step':'none',
            'primary_rc':str(primary),'restore_rc':str(restore),'final_rc':str(restore or primary)}
    if operation=='install-stock':
        values.update(manifest=MANIFEST,firmware='4.2.0',feature_mask='255',files_complete='1',manual_reboot_required='1')
    else:
        values.update(transaction='12345678')
        if operation=='uninstall':values['manual_reboot_required']='1'
    values.update(changes)
    return 'NATIVE_TRANSACTION_V1\n'+''.join(f'{key}={value}\n' for key,value in values.items())


class ContractTests(unittest.TestCase):
    def test_basic_receipt_must_match_requested_profile(self):
        basic=receipt(feature_mask='248')
        self.assertTrue(parse_final(basic,operation='install-stock',attempt=ATTEMPT,
                                   manifest=MANIFEST,feature_mask=248).completed)
        with self.assertRaises(ReceiptError):
            self.parse(basic)
        with self.assertRaises(ReceiptError):
            parse_final(receipt(),operation='install-stock',attempt=ATTEMPT,
                        manifest=MANIFEST,feature_mask=248)
        exe='/system/bin/x2d-native-setup'
        self.assertEqual(invocation(exe,'install-stock','/data/local/tmp/x2d-rs1',
                                    ATTEMPT,feature_mask=248)[2],'248')
        for invalid in (True, '248', 0, 7, 256):
            with self.assertRaises(ReceiptError):
                invocation(exe,'install-stock','/data/local/tmp/x2d-rs1',
                           ATTEMPT,feature_mask=invalid)

    def test_retained_cleanup_is_not_completed(self):
        text=receipt('data-cleanup',mode='uninstalled',outcome='deferred-retained',
                     retained='117',retained_unknown='116',retained_diagnostic='1')
        result=parse_final(text,operation='data-cleanup',attempt=ATTEMPT,
                           transaction='12345678',mode='uninstalled')
        self.assertTrue(result.success)
        self.assertTrue(result.deferred)
        self.assertFalse(result.completed)

    def parse(self, text):
        return parse_final(text,operation='install-stock',attempt=ATTEMPT,manifest=MANIFEST)

    def test_done_alone_never_success(self):
        dispatch=Dispatch('install-stock',ATTEMPT,manifest=MANIFEST)
        dispatch.before_send()
        self.assertIsNone(dispatch.observe(None))
        with self.assertRaises(ReceiptError):dispatch.observe('RAW_STACK_STOCK_FILES_INSTALLED_MANUAL_REBOOT_REQUIRED\n')
        result=dispatch.observe(receipt(restore=90))
        self.assertFalse(result.success)
        self.assertEqual((result.primary_rc,result.restore_rc),(0,90))

    def test_old_attempt_or_other_package_never_success(self):
        for changes in ({'attempt':'fedcba9876543210'},{'manifest':'2'*64},{'firmware':'4.1.0'},{'feature_mask':'7'},
                        {'files_complete':'0'},{'manual_reboot_required':'0'},{'operation':'uninstall'}):
            with self.subTest(changes=changes),self.assertRaises(ReceiptError):self.parse(receipt(**changes))

    def test_incomplete_duplicated_and_corrupt_rejected(self):
        good=receipt()
        for text in (good[:-1],good+'attempt='+ATTEMPT+'\n',good.replace('\n','\r\n'),good+'\0',
                     good.replace('final_rc=0','final_rc=90'),good.replace('primary_rc=0','primary_rc=-1'),
                     good.replace('restore_rc=0','restore_rc=1'),good.replace('final_rc=0','final_rc=256'),
                     good.replace('primary_rc=0','primary_rc=00'),good.replace('primary_step=none\n','')):
            with self.subTest(text=text),self.assertRaises(ReceiptError):self.parse(text)

    def test_usb_loss_does_not_authorize_replay(self):
        dispatch=Dispatch('install-stock',ATTEMPT,manifest=MANIFEST)
        dispatch.before_send()
        self.assertIsNone(dispatch.observe())
        with self.assertRaisesRegex(ReceiptError,'no-replay'):dispatch.before_send()
        self.assertTrue(dispatch.observe(receipt()).success)
        with self.assertRaisesRegex(ReceiptError,'changed'):dispatch.observe(receipt(primary=17))

    def test_primary_preserved_with_restore_failure(self):
        r=self.parse(receipt(primary=17,restore=90,primary_step='copy-write'))
        self.assertEqual((r.primary_rc,r.restore_rc,r.final_rc,r.primary_step),(17,90,90,'copy-write'))

    def test_stage_bound_uninstall_and_cleanup(self):
        for operation in ('uninstall','retained-cleanup'):
            text=receipt(operation)
            r=parse_final(text,operation=operation,attempt=ATTEMPT,transaction='12345678')
            self.assertTrue(r.success)
            with self.assertRaises(ReceiptError):parse_final(text,operation=operation,attempt=ATTEMPT,transaction='87654321')

    def test_fixed_argv(self):
        args=invocation('/system/bin/x2d-native-setup','install-stock','/data/local/tmp/x2d-rs1',ATTEMPT)
        self.assertEqual(args,['/system/bin/x2d-native-setup','install-stock','255',ATTEMPT])
        for stage in ('/tmp/other','/data/local/tmp/x2d-un-12345678;reboot','/data/local/tmp/x2d-un-12345678/../x','/data/local/tmp/x2d-un-ABCDEF01'):
            with self.assertRaises(ReceiptError):invocation('/system/bin/x2d-native-setup','uninstall',stage,ATTEMPT)

    def test_data_cleanup_binds_mode_and_does_not_call_deferred_completed(self):
        for outcome in ('complete','deferred-cleanup','deferred-capture'):
            dispatch=Dispatch('data-cleanup',ATTEMPT,transaction='12345678',mode='before')
            dispatch.before_send()
            result=dispatch.observe(receipt('data-cleanup',mode='before',outcome=outcome))
            self.assertEqual(result.completed,outcome=='complete')
            self.assertEqual(result.deferred,outcome!='complete')
            with self.assertRaises(ReceiptError):
                parse_final(receipt('data-cleanup',mode='uninstalled',outcome=outcome),operation='data-cleanup',attempt=ATTEMPT,transaction='12345678',mode='before')
        args=invocation('/system/bin/x2d-native-setup','data-cleanup','/data/local/tmp/x2d-tidy-12345678',ATTEMPT,mode='uninstalled')
        self.assertEqual(args,['/system/bin/x2d-native-setup','data-cleanup','uninstalled','/data/local/tmp/x2d-tidy-12345678',ATTEMPT])
        with self.assertRaises(ReceiptError):invocation('/system/bin/x2d-native-setup','data-cleanup','/data/local/tmp/x2d-un-12345678',ATTEMPT,mode='before')


if __name__=='__main__':unittest.main()
