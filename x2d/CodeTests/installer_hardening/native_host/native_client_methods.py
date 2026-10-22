"""仅作为构建期 AST 方法模板，不随客户端分发。"""
class Methods:
    def check_package(self):
        native_client_support.verify_package(self)

    def data_cleanup(self, mode='before'):
        return native_client_support.cleanup_data(self, mode)

    def cleanup(self):
        return native_client_support.cleanup(self)

    def install(self):
        self.status('核对原厂状态与原生暂存目录')
        self.cleanup()
        # 原厂选路后只回收有完成回执的旧安装目录；失败现场不删。
        self.data_cleanup('uninstalled')
        self.data_cleanup('before')
        self.ensure_stage('/data/local/tmp/x2d-rs1')
        native_client_support.assert_fresh_install_stage(self)
        free = parse_data_space(self.one(DATA_SPACE_COMMAND))
        self.log.append('STACK_DATA_FREE_KIB='+str(free))
        require(self.property('camera','exposure_status') == '0'
                and self.property('storage','storage_status') == '0'
                and self.property('error','active_error') == 'false', '相机非空闲，未上传安装载荷')
        native_client_support.operations(self).install_files(native_client_support.payload(self, 'install-stock'))
        native_client_support.verify_targets(self, self.native_metadata['install_targets'])
        self.context['native_files_complete'] = 'install'
        try:
            self.data_cleanup('installed')
        except Exception as exc:
            raise PatchError('安装文件已经核对完成；后续清理未确认。请诊断，不要重复安装。原始错误：'+protected(exc)) from exc
        return '安装文件及本次回执已核对。请正常关机再开机；需要查看运行状态请点“诊断”。'

    def uninstall(self, snapshot, selected_route):
        requested = (self.native_metadata, self.stock, self.full_stock, self.removal)
        try:
            native_client_support.select_uninstall_helper(self)
            if selected_route == 'Stock':
                native_client_support.archive_uninstall_receipts(self)
                self.cleanup()
                self.data_cleanup('uninstalled')
                native_client_support.cleanup_tool(self)
                native_client_support.archive_uninstall_receipts(self, remove=True)
                return '原厂文件已核对，保留文件、安装辅助程序和机内回执已清除；回执已保存到电脑。'
            removal_route(snapshot, self.removal)
            native_client_support.prepare_uninstall(self)
            self.status('正在执行原生卸载事务')
            native_client_support.operations(self).uninstall_files(native_client_support.payload(self, 'uninstall'))
            native_client_support.verify_targets(self, self.removal['targets'], original=True)
            self.context['native_files_complete'] = 'uninstall'
            # 原厂文件恢复后，等待正常重启再清理仍被进程映射的旧库。
            self.log.append('原厂文件已核对；保留库和数据清理等待正常重启，尚未宣称全量清理完成。')
            return '原厂文件及本次回执已核对。请正常关机再开机，然后再点“卸载”完成保留文件和辅助程序清理。'
        finally:
            # 更新流程随后用 stock.feature_mask 保存用户选择，不能带回卸载所用档位。
            self.native_metadata, self.stock, self.full_stock, self.removal = requested
