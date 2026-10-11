# 验证说明

本次在 Windows x64、Zig 0.13.0、Python 3.14.0 与 PySide6 6.11.2 上运行组件回归。C/C++ 测试使用 `-UNDEBUG`，确保断言生效。

| 项目 | 结果 | 覆盖 |
| --- | --- | --- |
| C/C++ 主机回归 | 19 个目标通过 | 六帧合成、批次边界、双消费者、RAW 读取及分块、预览缩放、网格容量、快门按键、跟踪、音频路由、缓存、持久设置和无线模式状态机 |
| Python 人工输入测试 | 4 个目标、17 个用例通过 | 像素原型、可重建的 AF 曲线、黑屏事件分析、软件时间分段 |
| 参数控件 | 通过 | 调用方颜色、文字适配、按键信号及禁用按键 |
| 引闪页面 | 5 页通过 | 桌面加载、自绘 SVG、未连接状态 |
| libfacedetection | 编译与合成空白图像通过 | 160×120 输入；固定版本文件及许可与上游一致 |

默认入口为 `scripts/test_components.py`，目标清单在 `scripts/component_tests.json`。每次运行生成新的结果文件，单项失败会保留对应日志并使整次运行失败。

```text
python -B scripts/test_components.py --compiler <zig路径> --output outputs/tests
python -B scripts/check_parameter_tile.py
python -B x2d/flash-ui/CodeTests/smoke.py
python -B scripts/build_x1dii_detector.py --compiler <zig路径> --build-dir outputs/detector
python -B scripts/check_sources.py
```

`check_sources.py` 检查文件清单、SHA-256、Python 语法、JSON 和文档本地链接。修改源码后需同步更新 `source-files.json` 的对应摘要。

尚未覆盖：macOS/Linux 主机执行、ARM/相机运行、所有历史 QML 页面的完整加载、使用者图像输入和实际硬件性能。历史目录中未列入默认目标的文件为组件参考，不据此声明完整程序可构建或整机可用。
