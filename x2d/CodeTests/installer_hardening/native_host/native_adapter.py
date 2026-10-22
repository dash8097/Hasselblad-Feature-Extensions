"""原生机内事务的电脑端候选。须随业务模块编译；不包含自动 bootstrap 或重发。

Installer 提供 one/upload/context/status/diagnose/verify_current。固定 transaction-read
入口由同目录 C++ 候选提供，尚需正式安装清单及路由接入。
"""
from dataclasses import asdict, dataclass
import base64
import hashlib
import json
import os
from pathlib import Path
import re
import time

from native_transaction_contract import Receipt, ReceiptError, invocation, parse_final

EXE = '/system/bin/x2d-native-setup'
OPERATIONS = ('install-stock', 'uninstall', 'retained-cleanup', 'data-cleanup', 'payload-unpack')


class Unconfirmed(RuntimeError):
    """保留现场，仅允许继续读取；不表示写操作未发生。"""


def check(ok, message):
    if not ok:
        raise ReceiptError(message)


def strict_json(text):
    def pairs(items):
        result = {}
        for key, value in items:
            check(key not in result, 'duplicate-json-key')
            result[key] = value
        return result
    return json.loads(text, object_pairs_hook=pairs)


@dataclass(frozen=True)
class Ticket:
    operation: str
    stage: str
    attempt: str
    manifest: str | None = None
    mode: str | None = None
    kind: str | None = None
    feature_mask: int = 255

    def argv(self):
        if self.operation == 'payload-unpack':
            check(self.kind in ('install-stock', 'uninstall'), 'unpack-kind')
            check(re.fullmatch('[0-9a-f]{16}', self.attempt or '') is not None, 'attempt')
            check(re.fullmatch('[0-9a-f]{64}', self.manifest or '') is not None, 'manifest')
            check(self.mode is None, 'unexpected-mode')
            check(self.stage == '/data/local/tmp/x2d-rs1' if self.kind == 'install-stock' else
                  re.fullmatch('/data/local/tmp/x2d-un-[0-9a-f]{8}', self.stage or '') is not None, 'unpack-stage')
            return [EXE, 'dispatch', 'payload-unpack', self.kind, self.stage, self.attempt]
        check(self.kind is None, 'unexpected-kind')
        direct = invocation(EXE, self.operation, self.stage, self.attempt, self.mode, self.feature_mask)
        if self.operation == 'install-stock':
            check(bool(re.fullmatch('[0-9a-f]{64}', self.manifest or '')), 'manifest')
        else:
            check(self.manifest is None, 'unexpected-manifest')
        check(self.operation == 'data-cleanup' or self.mode is None, 'unexpected-mode')
        return [direct[0], 'dispatch', *direct[1:]]

    def expected(self):
        self.argv()
        return dict(operation=self.operation, attempt=self.attempt, manifest=self.manifest,
                    mode=self.mode, transaction=None if self.operation == 'install-stock' else self.stage[-8:],
                    feature_mask=self.feature_mask)


class Journal:
    """独占创建 sent 回执；即使进程在 USB 调用前后崩溃，同 attempt 都不能再派发。"""
    def __init__(self, directory):
        self.root = Path(directory)
        self.root.mkdir(parents=True, exist_ok=True)
        check(not self.root.is_symlink(), 'journal-directory-link')

    def _path(self, ticket):
        ticket.argv()
        return self.root / (ticket.attempt + '.json')

    def begin(self, ticket):
        path = self._path(ticket)
        data = json.dumps(dict(ticket=asdict(ticket), sent=True, observed={}, quarantined=False), sort_keys=True).encode()
        fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(fd, 'wb') as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        self._sync_directory()

    def _sync_directory(self):
        if os.name != 'nt':
            fd = os.open(self.root, os.O_RDONLY | os.O_DIRECTORY)
            try:
                os.fsync(fd)
            finally:
                os.close(fd)

    def _read(self, path, limit):
        check(not path.is_symlink(), 'journal-file-link')
        with path.open('rb') as stream:
            data = stream.read(limit + 1)
        check(len(data) <= limit, 'journal-file-limit')
        return data.decode('ascii')

    def _exclusive(self, path, text):
        try:
            fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        except FileExistsError:
            check(self._read(path, 256) == text, 'journal-observation-conflict')
            return
        with os.fdopen(fd, 'wb') as stream:
            stream.write(text.encode('ascii'))
            stream.flush()
            os.fsync(stream.fileno())
        self._sync_directory()

    def load(self, ticket):
        path = self._path(ticket)
        check(not path.is_symlink() and path.stat().st_size <= 32768, 'journal-type-or-size')
        value = strict_json(self._read(path, 32768))
        check(value.get('ticket') == asdict(ticket) and value.get('sent') is True, 'journal-scope')
        value['observed'] = {}
        for name in ('claim', 'started', 'exit', 'final'):
            try:
                digest = self._read(self.root / (ticket.attempt + '.' + name + '.sha256'), 65)
            except FileNotFoundError:
                continue
            check(re.fullmatch('[0-9a-f]{64}\n', digest) is not None, 'journal-digest')
            value['observed'][name] = digest[:-1]
        value['quarantined'] = (self.root / (ticket.attempt + '.quarantined')).exists()
        return value

    def save(self, ticket, record):
        check(record['ticket'] == asdict(ticket) and record['sent'] is True, 'journal-update-scope')
        # 不覆盖 sent 或已观察摘要，避免两个只读观察者交错写回撤销隔离状态。
        if record.get('quarantined'):
            self._exclusive(self.root / (ticket.attempt + '.quarantined'), 'RECEIPT_CONFLICT\n')
        for name, digest in record['observed'].items():
            check(name in ('claim', 'started', 'exit', 'final') and re.fullmatch('[0-9a-f]{64}', digest) is not None, 'journal-digest-field')
            self._exclusive(self.root / (ticket.attempt + '.' + name + '.sha256'), digest + '\n')

    def archive(self, ticket, files):
        """将已验证的本次事务回执原文持久化，后续删机内暂存区不丢证据。"""
        ticket.argv()
        for name, text in files.items():
            check(name in ('claim', 'started', 'exit', 'final'), 'archive-receipt-name')
            if text is None:
                continue
            raw = text.encode('utf8')
            check(len(raw) <= 16384, 'archive-receipt-size')
            path = self.root / (ticket.attempt + '.' + name + '.raw')
            check(not path.is_symlink(), 'archive-receipt-link')
            try:
                stream = path.open('xb')
            except FileExistsError:
                check(path.read_bytes() == raw, 'archive-receipt-conflict')
                continue
            with stream:
                check(stream.write(raw) == len(raw), 'archive-receipt-short-write')
                stream.flush()
                os.fsync(stream.fileno())
            check(path.read_bytes() == raw, 'archive-receipt-readback')
            self._sync_directory()

def command(argv):
    # 固定 argv 的无引用安全子集；不会拼接 shell 分支、重定向或后台符号。
    check(argv and argv[0] == EXE, 'illegal-execution-path')
    check(all(isinstance(arg, str) and re.fullmatch('[A-Za-z0-9_./-]+', arg) for arg in argv), 'unsafe-argument')
    result = ' '.join(argv)
    check(len(result.encode('ascii')) <= 231, 'usb-command-too-long')
    return result


def record_fields(text, version, expected, keys):
    check(isinstance(text, str) and text.endswith('\n') and '\r' not in text and '\0' not in text, 'record-framing')
    lines = text.splitlines()
    check(lines[0] == version, 'record-version')
    fields = {}
    for line in lines[1:]:
        check('=' in line, 'record-field')
        key, value = line.split('=', 1)
        check(key not in fields and key in keys, 'record-field-key')
        fields[key] = value
    check(set(fields) == set(keys), 'record-fields')
    check(all(fields.get(k) == v for k, v in expected.items()), 'record-other-transaction')
    return fields


def parse_unpack(text, ticket):
    ticket.argv()
    check(len(text.encode('utf8')) <= 16384, 'unpack-receipt-size')
    fields = record_fields(text, 'NATIVE_TRANSACTION_V1',
        dict(operation='payload-unpack', kind=ticket.kind, stage=ticket.stage,
             attempt=ticket.attempt, manifest=ticket.manifest),
        ('operation', 'kind', 'stage', 'attempt', 'manifest', 'primary_step',
         'primary_errno', 'primary_rc', 'restore_rc', 'final_rc', 'payload_ready'))
    check(re.fullmatch('[A-Za-z0-9_-]{1,120}', fields['primary_step']) is not None, 'unpack-step')
    check(re.fullmatch('0|[1-9][0-9]{0,4}', fields['primary_errno']) is not None, 'unpack-errno')
    values = []
    for key in ('primary_rc', 'restore_rc', 'final_rc'):
        check(re.fullmatch('0|[1-9][0-9]{0,2}', fields[key]) is not None, 'unpack-code')
        value = int(fields[key]);check(value <= 255, 'unpack-code-range');values.append(value)
    primary, restore, final = values
    check(restore in (0, 90) and final == (restore or primary), 'unpack-code-consistency')
    check(fields['payload_ready'] in ('0', '1') and (final != 0 or fields['payload_ready'] == '1'), 'unpack-payload-ready')
    return Receipt(ticket.operation, ticket.attempt, primary, restore, final, fields['primary_step'], fields)


class NativeAdapter:
    def __init__(self, installer, journal, verify_bootstrap, verify_reader=None):
        self.installer, self.journal, self.verify_bootstrap = installer, journal, verify_bootstrap
        self.verify_reader = verify_reader

    def _failure(self, reason, *, terminal=False):
        context = self.installer.context
        context.update(reason=reason, host_exit='1', recovery='状态未确认；保留现场，只允许继续诊断，不重复派发')
        if not self.installer.first_failure:
            self.installer.first_failure = '阶段：' + str(context.get('stage', '原生事务')) + '\n' + reason
        return Unconfirmed(reason)

    def _bootstrap(self):
        # 回调必须核对包内摘要、合法 /system/bin 文件身份与已恢复只读状态。
        # 此模块不执行 /data 路径，也不自行 remount、复制或 chmod 来绕开引导。
        result = self.verify_bootstrap(EXE)
        check(result is True, 'native-system-bootstrap-not-verified')

    def prepare_stage(self, ticket):
        ticket.argv()
        check(self.journal_missing(ticket), 'stage-after-dispatch')
        self._bootstrap()
        self.installer.status('准备原生事务暂存目录')
        reply = self.installer.one(command([EXE, 'stage-init', ticket.stage]))
        check(reply == 'NATIVE_STAGE_DIRECTORY_READY', 'stage-init-unconfirmed-no-replay')

    def upload(self, ticket, data, token, digest):
        ticket.argv()
        check(self.journal_missing(ticket), 'upload-after-dispatch')
        check(hashlib.sha256(data).hexdigest() == digest, 'upload-digest')
        # 复用当前上传器的前缀长度+摘要续传。它当前只支持 rs1/un；tidy 无载荷。
        check(ticket.operation in ('install-stock', 'uninstall', 'payload-unpack'), 'upload-operation')
        if ticket.operation == 'payload-unpack':
            check(token == ('native-install' if ticket.kind == 'install-stock' else 'native-uninstall')
                  and digest == ticket.manifest, 'unpack-upload-binding')
            if ticket.kind == 'install-stock' and getattr(self.installer, 'native_usb_network_upload', False):
                from native_network_upload import upload
                if upload(self.installer, data, ticket.stage, token, digest):
                    return
        return self.installer.upload(data, ticket.stage, token, digest)

    def journal_missing(self, ticket):
        try:
            self.journal.load(ticket)
            return False
        except FileNotFoundError:
            return True

    def dispatch(self, ticket):
        argv = ticket.argv()
        self._bootstrap()
        self.installer.status('派发原生事务；保持 USB 连接')
        self.installer.context.update(target=ticket.stage, native_attempt=ticket.attempt,
                                      native_operation=ticket.operation, camera_exit='未取得')
        # 持久化失败就不发送；持久化成功后绝不因 ACK 丢失撤销 sent。
        self.journal.begin(ticket)
        try:
            reply = self.installer.one(command(argv))
            check(reply == 'NATIVE_DISPATCHED_V1 operation=' + ticket.operation + ' attempt=' + ticket.attempt,
                  'dispatch-reply-unconfirmed')
        except Exception as exc:
            raise self._failure('派发回复未确认；请求可能已执行，没有重发：' + str(exc)) from exc
        self.installer.context['reason'] = '相机已确认派发；尚未取得事务最终回执'

    def observe(self, ticket):
        record = self.journal.load(ticket)
        check(not record.get('quarantined'), 'receipt-conflict-quarantined')
        check((self.verify_reader or self.verify_bootstrap)(EXE) is True, 'native-system-reader-not-verified')
        self.installer.status('读取本次原生事务回执')
        try:
            raw = self.installer.one(command([EXE, 'transaction-read', ticket.operation, ticket.stage, ticket.attempt]))
            check(isinstance(raw, str) and len(raw.encode('utf8')) <= 200000, 'reader-limit')
            envelope = strict_json(raw)
            check(set(envelope) == {'protocol', 'operation', 'stage', 'attempt', 'files'}, 'reader-schema')
            check(envelope['protocol'] == 'NATIVE_TRANSACTION_READ_V1' and envelope['operation'] == ticket.operation
                  and envelope['stage'] == ticket.stage and envelope['attempt'] == ticket.attempt, 'reader-scope')
            check(isinstance(envelope['files'], dict) and set(envelope['files']) == {'claim', 'started', 'exit', 'final'}, 'reader-files')
            files = {}
            for name, item in envelope['files'].items():
                check(isinstance(item, dict) and set(item) == {'state', 'data'}, 'reader-file-schema')
                check(item['state'] in ('ABSENT', 'PRESENT') and isinstance(item['data'], str), 'reader-file-unavailable:' + name + ':' + str(item.get('state')))
                check(item['state'] == 'PRESENT' or item['data'] == '', 'reader-absent-data')
                data = None
                if item['state'] == 'PRESENT':
                    decoded = base64.b64decode(item['data'], validate=True)
                    check(len(decoded) <= 16384, 'reader-record-limit')
                    check(base64.b64encode(decoded).decode('ascii') == item['data'], 'reader-base64-canonical')
                    data = decoded.decode('utf8')
                old = record['observed'].get(name)
                if old is not None:
                    check(data is not None and hashlib.sha256(data.encode()).hexdigest() == old, 'observed-receipt-changed')
                if data is not None and name != 'final':
                    record['observed'][name] = hashlib.sha256(data.encode()).hexdigest()
                files[name] = data
            expected = dict(operation=ticket.operation, attempt=ticket.attempt)
            if files['claim'] is not None:
                record_fields(files['claim'], 'NATIVE_DISPATCH_V1', expected, ('operation', 'attempt'))
            if files['started'] is not None:
                fields = record_fields(files['started'], 'NATIVE_DISPATCH_STARTED_V1', expected, ('operation', 'attempt', 'pid', 'start'))
                check(re.fullmatch('[1-9][0-9]{0,9}', fields['pid']) is not None and int(fields['pid']) <= 2147483647
                      and re.fullmatch('0|[1-9][0-9]{0,19}', fields['start']) is not None
                      and int(fields['start']) <= 18446744073709551615, 'started-process-identity')
            # retained-cleanup 在同一目录内原子替换 final；新子进程启动后，
            # 替换前仍可能读到完整的旧回执。只等待，不把它登记为本次回执。
            if (ticket.operation == 'retained-cleanup' and files['exit'] is None
                    and files['claim'] is not None and files['started'] is not None
                    and files['final'] is not None and record['observed'].get('final') is None):
                prior = re.findall(r'^attempt=([0-9a-f]{16})$', files['final'], re.MULTILINE)
                if len(prior) == 1 and prior[0] != ticket.attempt:
                    old_scope = ticket.expected(); old_scope['attempt'] = prior[0]
                    parse_final(files['final'], **old_scope)
                    self.journal.save(ticket, record)
                    return None
            exit_rc = None
            if files['exit'] is not None:
                check(re.fullmatch(r'(0|[1-9][0-9]{0,2})\n', files['exit']) is not None, 'dispatch-exit-framing')
                exit_rc = int(files['exit'])
                check(exit_rc <= 255, 'dispatch-exit-range')
                self.installer.context['camera_exit'] = 'dispatch=' + str(exit_rc)
            receipt = (parse_unpack(files['final'], ticket) if ticket.operation == 'payload-unpack'
                       else parse_final(files['final'], **ticket.expected())) if files['final'] is not None else None
            if receipt is not None:
                record['observed']['final'] = hashlib.sha256(files['final'].encode()).hexdigest()
            if receipt is not None and exit_rc is not None:
                check(files['claim'] is not None and files['started'] is not None, 'dispatch-identity-missing')
                check(receipt.final_rc == exit_rc, 'final-dispatch-exit-mismatch')
                if receipt.success:
                    self._bootstrap()
                self.journal.archive(ticket, files)
                self.journal.save(ticket, record)
                self.installer.context.update(camera_exit=f'primary={receipt.primary_rc}; restore={receipt.restore_rc}; final={receipt.final_rc}; dispatch={exit_rc}',
                    native_primary_step=receipt.primary_step, native_primary_rc=str(receipt.primary_rc), native_restore_rc=str(receipt.restore_rc),
                    recovery='本次机内恢复回执为0；不等于任务成功' if receipt.restore_rc == 0 else '本次机内恢复未确认，保留现场',
                    reason='原生事务完成' if receipt.completed else '清理已延期' if receipt.deferred else '原生事务失败；原错与恢复错分别保留', host_exit='0' if receipt.success else '1')
                if not receipt.success and not self.installer.first_failure:
                    self.installer.first_failure = f'原生事务原错：{receipt.primary_step} rc={receipt.primary_rc}；恢复 rc={receipt.restore_rc}'
                return receipt
            self.journal.save(ticket, record)
            if exit_rc is not None:
                raise Unconfirmed('派发子进程已退出，但最终回执缺失；不能认定成功或重发')
            self.installer.context['reason'] = '本次最终回执或派发退出回执尚未齐全；没有重复派发'
            return None
        except ReceiptError as exc:
            record['quarantined'] = True
            self.journal.save(ticket, record)
            raise self._failure('回执冲突或不可信：' + str(exc), terminal=True) from exc
        except Exception as exc:
            raise self._failure('原生事务读取未确认：' + str(exc)) from exc

    def wait(self, ticket, *, polls=90, interval=1.0, sleep=time.sleep):
        check(type(polls) is int and 1 <= polls <= 600 and 0 <= interval <= 5, 'wait-policy')
        for index in range(polls):
            result = self.observe(ticket)
            if result is not None:
                return result
            if index + 1 < polls:
                sleep(interval)
        raise self._failure('等待回执超时；继续保留本次 attempt，仅可读取，没有重发')

    def preview_current(self):
        # 沿用完整当前文件/菜单/worker/face/AF检查，不把 receipt0当当前运行验证。
        return self.installer.verify_current()

    def diagnose(self):
        saved = {k: v for k, v in self.installer.context.items() if k.startswith('native_')}
        first = self.installer.first_failure
        try:
            return self.installer.diagnose()
        finally:
            self.installer.context.update(saved)
            if first:
                self.installer.first_failure = first
