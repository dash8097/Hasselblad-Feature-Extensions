"""原生相机事务的主机端契约。最终接入时随业务模块编译，不分发源码。"""
from dataclasses import dataclass
import re


class ReceiptError(ValueError):
    pass


@dataclass(frozen=True)
class Receipt:
    operation: str
    attempt: str
    primary_rc: int
    restore_rc: int
    final_rc: int
    primary_step: str
    fields: dict

    @property
    def success(self):
        return self.final_rc == 0

    @property
    def deferred(self):
        return self.operation == 'data-cleanup' and self.fields.get('outcome', '').startswith('deferred-')

    @property
    def completed(self):
        return self.success and not self.deferred


def _check(value, message):
    if not value:
        raise ReceiptError(message)


def _hex(value, width):
    return isinstance(value, str) and re.fullmatch(r'[0-9a-f]{' + str(width) + '}', value) is not None


def parse_final(text, *, operation, attempt, transaction=None, manifest=None, mode=None, feature_mask=255):
    """只接受本次原子最终回执；done/result/历史日志不参与成功判定。"""
    _check(operation in ('install-stock', 'uninstall', 'retained-cleanup', 'data-cleanup'), 'operation')
    _check(_hex(attempt, 16), 'expected-attempt')
    _check(isinstance(text, str) and 0 < len(text) <= 16384, 'receipt-size')
    _check('\r' not in text and '\0' not in text and text.endswith('\n'), 'receipt-framing')
    lines = text.split('\n')
    _check(lines[0] == 'NATIVE_TRANSACTION_V1' and lines[-1] == '', 'receipt-version')
    fields = {}
    for line in lines[1:-1]:
        _check('=' in line, 'receipt-field')
        key, value = line.split('=', 1)
        _check(re.fullmatch('[a-z_]{1,40}', key) is not None and key not in fields, 'receipt-duplicate-or-key')
        _check(len(value) <= 2048 and all(ord(c) >= 32 for c in value), 'receipt-value')
        fields[key] = value
    _check(fields.get('operation') == operation, 'receipt-other-operation')
    _check(fields.get('attempt') == attempt, 'receipt-other-attempt')
    if operation != 'install-stock':
        _check(_hex(transaction, 8) and fields.get('transaction') == transaction, 'receipt-other-transaction')
    else:
        _check(_hex(manifest, 64) and fields.get('manifest') == manifest, 'receipt-other-package')
        _check(type(feature_mask) is int and feature_mask in (248, 255) and fields.get('firmware') == '4.2.0' and fields.get('feature_mask') == str(feature_mask), 'receipt-package-scope')
    codes = []
    for key in ('primary_rc', 'restore_rc', 'final_rc'):
        value = fields.get(key, '')
        _check(re.fullmatch(r'0|[1-9][0-9]{0,2}', value) is not None, 'receipt-exit-code')
        code = int(value)
        _check(code <= 255, 'receipt-exit-range')
        codes.append(code)
    primary, restore, final = codes
    _check(restore in (0, 90) and final == (restore or primary), 'receipt-exit-inconsistent')
    _check('primary_step' in fields, 'receipt-primary-step-missing')
    if operation == 'data-cleanup':
        _check(mode in ('before', 'installed', 'uninstalled') and fields.get('mode') == mode,
               'receipt-other-cleanup-mode')
        _check(fields.get('outcome') in ('complete', 'deferred-cleanup', 'deferred-capture', 'deferred-retained'),
               'receipt-cleanup-outcome')
    if final == 0 and operation == 'install-stock':
        _check(fields.get('files_complete') == '1', 'receipt-files-not-complete')
    if final == 0 and operation in ('install-stock', 'uninstall'):
        _check(fields.get('manual_reboot_required') == '1', 'receipt-reboot-state')
    return Receipt(operation, attempt, primary, restore, final, fields['primary_step'], fields)


class Dispatch:
    """响应中断后仍保留已派发状态；允许读取，禁止自动再次执行写操作。"""
    def __init__(self, operation, attempt, transaction=None, manifest=None, mode=None, feature_mask=255):
        _check(operation in ('install-stock', 'uninstall', 'retained-cleanup', 'data-cleanup'), 'operation')
        _check(_hex(attempt, 16), 'attempt')
        self.expected = dict(operation=operation, attempt=attempt,
                             transaction=transaction, manifest=manifest, mode=mode, feature_mask=feature_mask)
        self.sent = False
        self.receipt = None

    def before_send(self):
        _check(not self.sent, 'write-already-dispatched-no-replay')
        # 在 transport 调用之前置位，发送后连接中断也不能当作未发送。
        self.sent = True

    def observe(self, final_text=None):
        _check(self.sent, 'write-not-dispatched')
        if final_text is None:
            return None
        result = parse_final(final_text, **self.expected)
        if self.receipt is not None:
            _check(self.receipt == result, 'final-receipt-changed')
        self.receipt = result
        return result


def invocation(executable, operation, stage, attempt, mode=None, feature_mask=255):
    """只生成固定程序 argv；不生成 shell 控制流或任意命令入口。"""
    _check(executable == '/system/bin/x2d-native-setup', 'setup-executable')
    _check(_hex(attempt, 16), 'attempt')
    if operation == 'install-stock':
        _check(stage == '/data/local/tmp/x2d-rs1', 'install-stage')
        _check(type(feature_mask) is int and feature_mask in (248, 255), 'install-feature-mask')
        return [executable, operation, str(feature_mask), attempt]
    if operation == 'data-cleanup':
        _check(mode in ('before', 'installed', 'uninstalled'), 'cleanup-mode')
        _check(isinstance(stage, str) and re.fullmatch('/data/local/tmp/x2d-tidy-[0-9a-f]{8}', stage) is not None,
               'cleanup-stage')
        return [executable, operation, mode, stage, attempt]
    _check(operation in ('uninstall', 'retained-cleanup'), 'operation')
    _check(isinstance(stage, str) and re.fullmatch('/data/local/tmp/x2d-un-[0-9a-f]{8}', stage) is not None,
           'uninstall-stage')
    return [executable, operation, stage, attempt]
