# 当前 X2D 构建与验收入口

以 `release-lock.json` 记录的完整包为范围；目标固件为第一代 X2D 4.2.0。
源码不等于安装包，也不表示本轮已经完成相机验收。

- `build_complete_package.py`：Windows / macOS 完整包构建编排。
- `run_checks.py --list`：列出 9 个离线 Auto Test 入口。
- `ui_matrix.json`：保留 21 组 UI 验收定义及原始运行状态。
- `../../FunctionalTests/multishot_ui/`：前台菜单、快门、任务完成和异常检查驱动。

在项目根目录、Python 3.11 环境中执行测试。先准备合法取得的原厂固件、
ARM / Qt / Windows / macOS 工具链及构建器绑定的运行时输入。
`run_checks.py --cases profiles transactions host-profile network-upload` 可选择离线检查。
其余用例还依赖指定工具链或 ARM Linux 测试环境。
UI 硬件驱动需另行授权与实际相机，不能把静态清单当作已通过验收。

当前构建脚本保留对冻结构建输入和升级清理摘要的核验；外部产物未随源码导出，
缺少它们时应停止并报告，不用历史候选包或虚假回执替代。
当前代码依赖的兼容助手按依赖边逐文件保留，不携带旧安装包和实验结果。
