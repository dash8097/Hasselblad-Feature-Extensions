# 构建与运行

基础检查使用 Python 3.10+ 标准库。`scripts/test_components.py` 显式接受 Zig 0.13 的路径和输出目录；编译缓存与结果都写入该输出目录。
测试程序处理人工内存数据、模拟回调和本地测试文件。

```text
python -B scripts/check_sources.py
python -B scripts/test_components.py --compiler <zig路径> --output outputs/tests
```

Qt Quick 桌面检查需要 PySide6，单独运行：

```text
python -B x2d/flash-ui/CodeTests/smoke.py
python -B scripts/check_parameter_tile.py
```

可选的图像文件实验使用 numpy、Pillow、tifffile 或 rawpy；音频文件转换使用 numpy 和 soundfile。它们处理调用者提供的本地文件，不属于默认测试入口。

历史修订目录保留原始组件接口，不能视作每个目录都能构建为完整程序。默认回归只覆盖 `scripts/component_tests.json` 中逐项列出的目标，其他源码是组件参考。
本仓库不附带 Qt、系统 SDK 或相机运行环境。各自的许可证与安装方式由对应上游维护。
