"""固定原生事务编排候选；由已完成完整包校验与路由检查的 Installer 调用。

本模块不提供任意命令接口，不处理旧 stage 归档，不代替完整诊断或重启续接。
不能仅凭此模块将客户端标成可发行。
"""
from dataclasses import dataclass
import hashlib
import re
import secrets
from native_adapter import Ticket, Unconfirmed, check


@dataclass(frozen=True)
class Payload:
    kind: str
    archive: bytes
    archive_sha256: str
    manifest_sha256: str
    feature_mask: int = 255

    def validate(self):
        check(self.kind in ('install-stock', 'uninstall'), 'payload-kind')
        check(type(self.feature_mask) is int and self.feature_mask in (248,255), 'payload-profile')
        check(type(self.archive) is bytes and 0 < len(self.archive) <= 512*1024*1024, 'payload-size')
        for value in (self.archive_sha256, self.manifest_sha256):
            check(isinstance(value, str) and re.fullmatch('[0-9a-f]{64}', value) is not None, 'payload-digest')
        check(hashlib.sha256(self.archive).hexdigest() == self.archive_sha256, 'payload-bytes')


class NativeOperations:
    def __init__(self, installer, adapter, bootstrap, *, attempt=lambda: secrets.token_hex(8),
                 transaction=lambda: secrets.token_hex(4)):
        self.installer = installer
        self.adapter = adapter
        self.bootstrap = bootstrap
        self.attempt = attempt
        self.transaction = transaction

    def _run(self, ticket, *, polls=300, prepare=False):
        ticket.argv()  # 本地检查失败时不引导、不打开传输。
        if prepare:
            self.adapter.prepare_stage(ticket)
        self.adapter.dispatch(ticket)
        result = self.adapter.wait(ticket, polls=polls)
        if not result.success:
            raise Unconfirmed('原生事务失败；原错 '+str(result.primary_rc)+'，恢复 '+str(result.restore_rc)+'；保留现场，未重发')
        return result

    def prepare_payload(self, payload, stage):
        payload.validate()
        ticket = Ticket('payload-unpack', stage, self.attempt(), manifest=payload.archive_sha256,
                        kind=payload.kind)
        ticket.argv()
        check(self.bootstrap.ensure() is True, 'bootstrap-not-confirmed')
        self.adapter.prepare_stage(ticket)
        token = 'native-install' if payload.kind == 'install-stock' else 'native-uninstall'
        self.adapter.upload(ticket, payload.archive, token, payload.archive_sha256)
        result = self._run(ticket)
        check(result.completed and result.fields.get('payload_ready') == '1', 'payload-not-committed')
        return ticket

    def install_files(self, payload):
        """调用者先完成原厂路由、旧 stage 的可证明终止/归档和空间检查。"""
        check(payload.kind == 'install-stock', 'install-payload-kind')
        self.prepare_payload(payload, '/data/local/tmp/x2d-rs1')
        ticket = Ticket('install-stock', '/data/local/tmp/x2d-rs1', self.attempt(),
                        manifest=payload.manifest_sha256,feature_mask=payload.feature_mask)
        result = self._run(ticket)
        check(result.completed and result.fields.get('files_complete') == '1'
              and result.fields.get('manual_reboot_required') == '1', 'install-incomplete')
        self.installer.progress('PATCH_PROGRESS=install|5|5')
        return result

    def uninstall_files(self, payload):
        check(payload.kind == 'uninstall', 'uninstall-payload-kind')
        suffix = self.transaction()
        check(isinstance(suffix, str) and re.fullmatch('[0-9a-f]{8}', suffix) is not None, 'transaction')
        stage = '/data/local/tmp/x2d-un-'+suffix
        self.prepare_payload(payload, stage)
        result = self._run(Ticket('uninstall', stage, self.attempt()))
        check(result.completed and result.fields.get('manual_reboot_required') == '1', 'uninstall-incomplete')
        self.installer.progress('PATCH_PROGRESS=install|5|5')
        return result

    def data_cleanup(self, mode='before'):
        check(mode in ('before', 'installed', 'uninstalled'), 'cleanup-mode')
        stage = '/data/local/tmp/x2d-tidy-'+self.transaction()
        ticket = Ticket('data-cleanup', stage, self.attempt(), mode=mode)
        ticket.argv()
        check(self.bootstrap.ensure() is True, 'bootstrap-not-confirmed')
        result = self._run(ticket, prepare=True)
        if mode == 'before' and result.deferred:
            raise Unconfirmed('清理延期，安装未开始；没有重发清理')
        # 回执与日志保留，不再调用 rm -rf 删除诊断现场。
        return result

    def retained_cleanup(self, stage):
        ticket = Ticket('retained-cleanup', stage, self.attempt())
        ticket.argv()
        check(self.bootstrap.ensure() is True, 'bootstrap-not-confirmed')
        result = self._run(ticket)
        check(result.completed, 'retained-cleanup-incomplete')
        return result
