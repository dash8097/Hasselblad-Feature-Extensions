"""固定 USB 原语引导合法 /system/bin ELF；不生成、保存或执行脚本文件。"""
import base64
import gzip
import hashlib
import json
import os
from pathlib import Path
import re
import secrets
from native_adapter import EXE, Unconfirmed, check, strict_json, record_fields
from native_network_upload import upload as network_upload

STAGE = '/data/local/tmp/x2d-rs1'
CONTROL = '/data/local/tmp/x2d-bootstrap'
LOCK = '/tmp/x2dt'
TOYBOX_SHA = '999a0669ef654efbda54bc585c0f3c00643d1af61c985bfb7968cfd2f9aed840'


def landing_command(seed):
    check(re.fullmatch('/system/bin/x2d-[0-9a-f]{8}', seed or '') is not None, 'bootstrap-seed-name')
    result = ('{ flock -n 9||exit;s=/system;t=$s/bin/' + seed.rsplit('/',1)[1] +
              ";trap 'r=$?;mount -oremount,ro $s||r=90;exit $r' 0;trap 'exit 143' 1 2 15;set -Ce;" +
              'mount -oremount,rw $s;zcat ' + STAGE + '/z' + seed[-8:] + '.gz>$t;chmod 755 $t;exit;} 9<' + LOCK)
    check(len(result.encode('ascii')) <= 231, 'bootstrap-landing-command-limit')
    return result


class NativeBootstrap:
    def __init__(self, installer, journal_directory, setup_bytes, setup_sha256, allowed_previous_sha256=()):
        check(re.fullmatch('[0-9a-f]{64}', setup_sha256 or '') is not None, 'bootstrap-package-sha')
        check(hashlib.sha256(setup_bytes).hexdigest() == setup_sha256 and setup_bytes[:4] == b'\x7fELF', 'bootstrap-package-content')
        check(all(re.fullmatch('[0-9a-f]{64}', h or '') for h in allowed_previous_sha256), 'bootstrap-previous-list')
        self.installer, self.data, self.digest = installer, setup_bytes, setup_sha256
        self.wire_data = gzip.compress(setup_bytes, compresslevel=9, mtime=0)
        self.wire_digest = hashlib.sha256(self.wire_data).hexdigest()
        self.allowed = frozenset(allowed_previous_sha256)
        self.seed = '/system/bin/x2d-' + setup_sha256[:8]
        self.token = 'z' + setup_sha256[:8]
        self.source = STAGE + '/' + self.token + '.gz'
        self.directory = Path(journal_directory)
        self.directory.mkdir(parents=True, exist_ok=True)
        check(not self.directory.is_symlink(), 'bootstrap-journal-link')
        self.path = self.directory / ('bootstrap-' + setup_sha256 + '.json')

    def _one(self, command):
        check(isinstance(command, str) and len(command.encode('ascii')) <= 231, 'bootstrap-usb-command-limit')
        return self.installer.one(command)

    def _readonly(self):
        # 固定只读查询；不依赖 grep 的首个匹配行。
        text = self._one('cat /proc/mounts')
        check(len(text.encode('ascii')) <= 1048576, 'bootstrap-mount-reply-limit')
        rows = [line.split() for line in text.splitlines()]
        hits = [row for row in rows if len(row) >= 4 and row[1] == '/system']
        check(len(hits) == 1 and 'ro' in hits[0][3].split(',') and 'rw' not in hits[0][3].split(','), 'bootstrap-system-not-readonly')
        return True

    def _file(self, path):
        check(path in (EXE, self.seed, self.source), 'bootstrap-file-path')
        reply = self._one('p=' + path + ';if test -e "$p" || test -L "$p";then test -f "$p" && test ! -L "$p" && echo FILE;else echo ABSENT;fi')
        if reply == 'ABSENT':
            return None
        check(reply == 'FILE', 'bootstrap-file-type')
        fields = self._one('/system/xbin/busybox stat -c %u:%a:%h:%s ' + path)
        match = re.fullmatch(r'0:(600|644|755):1:([1-9][0-9]{0,8})', fields)
        check(match is not None and int(match[2]) <= 67108864, 'bootstrap-file-metadata')
        if path != self.source:
            check(match[1] == '755', 'bootstrap-file-mode')
        result = self._one('/system/xbin/busybox sha256sum ' + path)
        match = re.fullmatch('([0-9a-f]{64})  ' + re.escape(path), result)
        check(match is not None, 'bootstrap-file-sha-framing')
        return match[1]

    def verify(self, executable):
        check(executable == EXE, 'bootstrap-verify-path')
        return self._readonly() and self._file(EXE) == self.digest

    def verify_reader(self, executable):
        # 已派发事务的只读观察允许其短暂 RW 阶段；仍验证固定可执行文件身份。
        # 派发和最终成功仍必须通过 verify 的只读检查。
        check(executable == EXE, 'bootstrap-verify-path')
        return self._file(EXE) == self.digest

    def _prepare_lock(self):
        # 原厂4.2.0 /bin/flock 是 toybox 链接；PATH解析和完整文件摘要都要核对。
        check(self._one('command -v flock') in ('/system/bin/flock', '/bin/flock'), 'bootstrap-flock-resolution')
        check(self._one('readlink -f /system/bin/flock') == '/system/bin/toybox', 'bootstrap-flock-target')
        check(self._one('command -v zcat') in ('/system/bin/zcat', '/bin/zcat'), 'bootstrap-zcat-resolution')
        check(self._one('readlink -f /system/bin/zcat') == '/system/bin/toybox', 'bootstrap-zcat-target')
        check(self._one('/system/xbin/busybox sha256sum /system/bin/toybox') == TOYBOX_SHA + '  /system/bin/toybox', 'bootstrap-flock-binary')
        state = self._one('p=' + LOCK + ';if test -L "$p";then echo LINK;elif test -f "$p";then echo FILE;elif test -e "$p";then echo OTHER;else echo ABSENT;fi')
        check(state in ('ABSENT', 'FILE'), 'bootstrap-lock-type')
        if state == 'ABSENT':
            self._one('(umask 077;set -C;: > ' + LOCK + ')')
        check(self._one('/system/xbin/busybox stat -c %u:%a:%h:%s ' + LOCK) == '0:600:1:0', 'bootstrap-lock-identity')

    def _save_new(self, state):
        fd = os.open(self.path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(fd, 'wb') as stream:
            stream.write(json.dumps(state, sort_keys=True).encode('ascii'))
            stream.flush();os.fsync(stream.fileno())
        if os.name != 'nt':
            fd = os.open(self.directory, os.O_RDONLY | os.O_DIRECTORY)
            try:os.fsync(fd)
            finally:os.close(fd)

    def _sent(self, phase):
        path = self.path.with_suffix('.' + phase + '.sent')
        fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(fd, 'wb') as stream:
            stream.write(b'SENT_BEFORE_USB\n');stream.flush();os.fsync(stream.fileno())
        if os.name != 'nt':
            fd = os.open(self.directory, os.O_RDONLY | os.O_DIRECTORY)
            try:os.fsync(fd)
            finally:os.close(fd)

    def _load(self):
        check(not self.path.is_symlink() and self.path.stat().st_size <= 4096, 'bootstrap-journal-type')
        state = strict_json(self.path.read_text(encoding='ascii'))
        check(set(state) == {'attempt', 'new', 'old'} and re.fullmatch('[0-9a-f]{16}', state['attempt']) is not None
              and state['new'] == self.digest and (state['old'] == 'absent' or state['old'] in self.allowed), 'bootstrap-journal-scope')
        return state

    def _error(self, text):
        self.installer.context.update(reason=text, native_bootstrap='未确认', recovery='保留引导现场；只读检查，不重发', host_exit='1')
        if not self.installer.first_failure:self.installer.first_failure = text
        return Unconfirmed(text)

    def _observe(self, state):
        self._readonly()
        target = self._file(EXE)
        if target == self.digest:
            reader = EXE
        else:
            check(target == (None if state['old'] == 'absent' else state['old']), 'bootstrap-target-unexpected')
            check(self._file(self.seed) == self.digest, 'bootstrap-no-verified-reader')
            reader = self.seed
        raw = self._one(reader + ' bootstrap-read ' + state['attempt'])
        check(len(raw.encode('utf8')) <= 25000, 'bootstrap-read-limit')
        item = strict_json(raw)
        check(set(item) == {'protocol', 'attempt', 'state', 'data'} and item['protocol'] == 'NATIVE_BOOTSTRAP_READ_V1'
              and item['attempt'] == state['attempt'], 'bootstrap-read-scope')
        if item['state'] == 'ABSENT':
            check(item['data'] == '', 'bootstrap-absent-data')
            raise self._error('引导已派发但最终回执尚未取得；没有重复执行')
        check(item['state'] == 'PRESENT' and isinstance(item['data'], str), 'bootstrap-read-state')
        data = base64.b64decode(item['data'], validate=True)
        check(len(data) <= 16384 and base64.b64encode(data).decode('ascii') == item['data'], 'bootstrap-read-base64')
        fields = record_fields(data.decode('ascii'), 'NATIVE_BOOTSTRAP_V1', state,
            ('attempt', 'old', 'new', 'primary_step', 'primary_errno', 'primary_rc', 'restore_rc', 'final_rc', 'setup_ready', 'retained'))
        values = []
        for key in ('primary_rc', 'restore_rc', 'final_rc'):
            check(re.fullmatch('0|[1-9][0-9]{0,2}', fields[key]) is not None and int(fields[key]) <= 255, 'bootstrap-receipt-code')
            values.append(int(fields[key]))
        primary, restore, final = values
        check(restore in (0, 90) and final == (restore or primary), 'bootstrap-receipt-inconsistent')
        check(re.fullmatch('[A-Za-z0-9_.:-]{1,160}', fields['primary_step']) is not None
              and re.fullmatch('0|[1-9][0-9]{0,4}', fields['primary_errno']) is not None, 'bootstrap-receipt-primary')
        check(fields['setup_ready'] in ('0', '1') and fields['retained'] in ('none', 'x2d-native-setup.previous.' + state['attempt']), 'bootstrap-receipt-state')
        self.installer.context.update(native_primary_rc=str(primary), native_restore_rc=str(restore), native_primary_step=fields['primary_step'])
        if final or fields['setup_ready'] != '1':
            raise self._error(f'原生引导失败：{fields["primary_step"]} rc={primary}；恢复 rc={restore}；没有重发')
        check(target == self.digest and self.verify(EXE), 'bootstrap-final-file-mismatch')
        complete = self.path.with_suffix('.complete')
        proof = dict(state=state, final_sha256=hashlib.sha256(data).hexdigest())
        fd = os.open(complete, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(fd, 'wb') as stream:
            stream.write(json.dumps(proof, sort_keys=True).encode('ascii'));stream.flush();os.fsync(stream.fileno())
        if os.name != 'nt':
            fd = os.open(self.directory, os.O_RDONLY | os.O_DIRECTORY)
            try:os.fsync(fd)
            finally:os.close(fd)
        return True

    def ensure(self):
        try:
            check(not self.path.with_suffix('.cleanup.sent').exists(), 'bootstrap-cleanup-already-sent-no-rebootstrap')
            if self.path.exists():
                # 有历史 sent 时，哪怕目标摘要正确也必须读本次最终回执。
                state = self._load()
                complete = self.path.with_suffix('.complete')
                if complete.exists():
                    check(not complete.is_symlink() and complete.stat().st_size <= 4096, 'bootstrap-completed-proof-type')
                    proof = strict_json(complete.read_text(encoding='ascii'))
                    check(set(proof) == {'state', 'final_sha256'} and proof['state'] == state and
                          re.fullmatch('[0-9a-f]{64}', proof['final_sha256']) is not None, 'bootstrap-completed-proof')
                    check(self.verify(EXE), 'bootstrap-completed-current-mismatch')
                    return True
                if self.path.with_suffix('.commit.sent').exists():return self._observe(state)
                if self.path.with_suffix('.seed.sent').exists():
                    raise self._error('之前的引导落地已发送；只允许核对现场，不自动继续或重复落地')
                raise self._error('之前的引导准备记录存在；保留现场，不自动重新开始')
            self._readonly()
            old = self._file(EXE)
            if old == self.digest:return True
            self._prepare_lock()
            check(old is None or old in self.allowed, 'bootstrap-unknown-previous-setup')
            existing_seed = self._file(self.seed)
            check(existing_seed is None or existing_seed == self.digest, 'bootstrap-seed-prefix-collision')
            if existing_seed is None:
                # 原厂 toybox df 的 Free 包含保留块；必须使用 f_bavail。
                available = self._one('/system/xbin/busybox stat -f -c %a:%S /system')
                check(re.fullmatch(r'[0-9]{1,16}:[1-9][0-9]{0,8}', available) is not None, 'bootstrap-space-framing')
                blocks, block_size = map(int, available.split(':'))
                check(blocks * block_size >= len(self.data) + 65536, 'bootstrap-system-space-insufficient')
            self.installer.ensure_stage(STAGE)
            if not getattr(self.installer, 'native_usb_network_upload', False) or not network_upload(self.installer, self.wire_data, STAGE, self.token, self.wire_digest):
                self.installer.upload(self.wire_data, STAGE, self.token, self.wire_digest)
            check(self._file(self.source) == self.wire_digest, 'bootstrap-upload-not-verified')
            existing_seed = self._file(self.seed)
            check(existing_seed is None or existing_seed == self.digest, 'bootstrap-seed-prefix-collision')
            state = dict(attempt=secrets.token_hex(8), new=self.digest, old=old or 'absent')
            self._save_new(state)
            if existing_seed is None:
                self._sent('seed')
                self._one(landing_command(self.seed))
            self._readonly()
            check(self._file(self.seed) == self.digest, 'bootstrap-landed-digest')
            self._sent('commit')
            try:self._one(self.seed + ' bootstrap-commit ' + state['attempt'] + ' ' + state['old'] + ' ' + self.digest)
            except Exception as exc:raise self._error('原生引导回复未确认；下次只读取，不重发：' + str(exc)) from exc
            return self._observe(state)
        except Unconfirmed:raise
        except Exception as exc:raise self._error('引导检查或执行未确认：' + str(exc)) from exc

    def cleanup(self):
        """明确卸载最后一步；中断后只回读，不重新引导、不重复派发。"""
        try:
            journal = self.path.with_suffix('.cleanup.json')
            if journal.exists():
                check(not journal.is_symlink() and journal.stat().st_size <= 4096, 'bootstrap-cleanup-journal-type')
                state = strict_json(journal.read_text(encoding='ascii'))
            else:
                check(self.verify(EXE), 'bootstrap-cleanup-current-unverified')
                old, previous = 'absent', 'none'
                if self.path.exists():
                    prior = self._load()
                    check(self.path.with_suffix('.complete').is_file(), 'bootstrap-cleanup-prior-unconfirmed')
                    old, previous = prior['old'], prior['attempt']
                state = dict(attempt=secrets.token_hex(8), new=self.digest, old=old, previous=previous)
                fd = os.open(journal, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
                with os.fdopen(fd, 'wb') as stream:
                    stream.write(json.dumps(state, sort_keys=True).encode('ascii'));stream.flush();os.fsync(stream.fileno())
                if os.name != 'nt':
                    fd = os.open(self.directory, os.O_RDONLY | os.O_DIRECTORY)
                    try:os.fsync(fd)
                    finally:os.close(fd)
            check(set(state) == {'attempt','new','old','previous'} and state['new'] == self.digest
                  and re.fullmatch('[0-9a-f]{16}', state['attempt']) and
                  (state['old'] == 'absent' or state['old'] in self.allowed) and
                  (re.fullmatch('[0-9a-f]{16}', state['previous']) or state['previous'] == 'none'), 'bootstrap-cleanup-journal-scope')
            if not self.path.with_suffix('.cleanup.sent').exists():
                check(self.verify(EXE), 'bootstrap-cleanup-before-send')
                self._sent('cleanup')
                try:self._one(EXE + ' bootstrap-cleanup ' + state['attempt'] + ' ' + state['new'] + ' ' + state['old'] + ' ' + state['previous'])
                except Exception as exc:raise self._error('引导工具清理回复未确认；只读回执，不重发：' + str(exc)) from exc
            return self._observe_cleanup(state)
        except Unconfirmed:raise
        except Exception as exc:raise self._error('引导工具清理未确认：' + str(exc)) from exc

    def _observe_cleanup(self, state):
        # 当前setup已经自删除：这里只使用原厂只读原语，不能调用ensure/新ELF reader。
        path = CONTROL + '/bootstrap.cleanup.final.' + state['attempt']
        self._readonly()
        seal = path + '.seal'
        self._one('p=' + seal + ';test -f "$p" && test ! -L "$p"')
        seal_stat = '/system/xbin/busybox stat -c %u:%a:%h:%s:%d:%i:%Y:%Z ' + seal
        seal_before = self._one(seal_stat)
        check(re.fullmatch(r'0:600:1:64:[0-9]+:[0-9]+:[0-9]+:[0-9]+', seal_before), 'bootstrap-cleanup-seal-type')
        seal_encoded = self._one('/system/xbin/busybox base64 ' + seal).replace('\n','')
        check(len(seal_encoded) == 88, 'bootstrap-cleanup-seal-size')
        seal_data = base64.b64decode(seal_encoded, validate=True)
        check(re.fullmatch(b'[0-9a-f]{64}', seal_data) and base64.b64encode(seal_data).decode('ascii') == seal_encoded
              and self._one(seal_stat) == seal_before, 'bootstrap-cleanup-seal-unstable')
        self._one('p=' + path + ';test -f "$p" && test ! -L "$p"')
        command = '/system/xbin/busybox stat -c %u:%a:%h:%s:%d:%i:%Y:%Z ' + path
        before = self._one(command)
        m = re.fullmatch(r'0:600:1:([1-9][0-9]{0,4}):[0-9]+:[0-9]+:[0-9]+:[0-9]+', before)
        check(m is not None and int(m[1]) <= 16384, 'bootstrap-cleanup-receipt-type')
        encoded = self._one('/system/xbin/busybox base64 ' + path).replace('\n','')
        check(len(encoded) <= 21848, 'bootstrap-cleanup-reply-limit')
        data = base64.b64decode(encoded, validate=True)
        check(len(data) == int(m[1]) and hashlib.sha256(data).hexdigest() == seal_data.decode('ascii') and base64.b64encode(data).decode('ascii') == encoded
              and self._one(command) == before, 'bootstrap-cleanup-receipt-unstable')
        fields = record_fields(data.decode('ascii'), 'NATIVE_BOOTSTRAP_CLEANUP_V1', state,
            ('attempt','new','old','previous','primary_step','primary_errno','primary_rc','restore_rc','final_rc','removed','expected','tools_removed'))
        for key in ('primary_rc','restore_rc','final_rc','removed','expected'):
            check(re.fullmatch('0|[1-9][0-9]{0,2}', fields[key]) and int(fields[key]) <= 255, 'bootstrap-cleanup-integer')
        primary, restore, final = (int(fields[k]) for k in ('primary_rc','restore_rc','final_rc'))
        check(restore in (0,90) and final == (restore or primary) and fields['tools_removed'] in ('0','1')
              and 1 <= int(fields['expected']) <= 4 and int(fields['removed']) <= int(fields['expected'])
              and (fields['tools_removed'] == '1') == (fields['removed'] == fields['expected'])
              and re.fullmatch('[A-Za-z0-9_.:-]{1,160}', fields['primary_step'])
              and re.fullmatch('0|[1-9][0-9]{0,4}', fields['primary_errno']), 'bootstrap-cleanup-final-inconsistent')
        self.installer.context.update(native_primary_rc=str(primary), native_restore_rc=str(restore), native_primary_step=fields['primary_step'])
        check(final == 0 and fields['tools_removed'] == '1', 'bootstrap-cleanup-failed')
        check(self._file(EXE) is None and self._file(self.seed) is None, 'bootstrap-cleanup-tools-still-present')
        if state['old'] != 'absent':
            previous = '/system/bin/x2d-native-setup.previous.' + state['previous']
            self._one('p=' + previous + ';test ! -e "$p" && test ! -L "$p"')
        self._readonly()
        return True
