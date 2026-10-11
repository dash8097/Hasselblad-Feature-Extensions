# Hasselblad Feature Extensions

相机扩展项目的源码组件：像素合成、图像缓存、目标跟踪、交互状态机和 Qt Quick 界面。
这是独立、非官方项目，与相机厂商没有隶属或认可关系。

| 模块 | 内容 |
| --- | --- |
| [像素处理](x2d/CodeTests/pixel_shift_rgb/README.md) | 四帧与六帧重建、双消费者队列、RAW 分块、预览缩放及输出策略 |
| [缓存与生命周期](x1d/candidates/replay-memory/README.md) | 内存预算、带所有权的缓存、过期结果拒绝 |
| [交互与设置](x1d/wifi-region/temporary-ui/README.md) | 触点所有权、焦点队列、页面池和参数控件 |
| [引闪界面](x2d/flash-ui/README.md) | 本地参数草稿、频道与分组编辑、自绘图标 |
| [跟踪](x2d/subject-tracking/README.md) | 灰度目标匹配和置信度处理 |
| [动画与音频策略](x1d/shutter-effects/README.md) | 曝光动画、抽象音频路由及状态恢复 |
| [开源检测库](X1D2/face-afs/vendor/libfacedetection/SOURCE.md) | 带原始许可的 libfacedetection 源码与模型 |

本仓库提供组件源码及离线测试，当前没有完整相机安装包。目录中的历史版本保留各自接口；使用时以对应源码和测试为准。
机型与固件版本注释说明设计背景，不能替代当前设备的兼容性验证。

## 使用

安装 Python 3.10 或更高版本及 Zig 0.13 工具链，在仓库根目录执行：

```text
python -B scripts/check_sources.py
python -B scripts/test_components.py --compiler <zig路径> --output outputs/tests
```

Qt Quick 组件使用 Qt 5/6 的公开 API；桌面预览测试使用 PySide6，具体依赖见[构建说明](docs/build/BUILDABILITY.md)。
源码导航见[组件目录](docs/SOURCE_INDEX.md)，测试覆盖见[验证说明](docs/VALIDATION.md)。

原创代码采用 [MIT 许可](LICENSE)；第三方组件维持其原有许可与版权声明。

## English

Independent camera-extension source components for pixel processing, bounded caches, tracking, state machines and Qt Quick UI.
Use the explicit offline test entry points above. This repository provides component sources, not a complete installable camera update.
Original code is MIT licensed; third-party code keeps its original license and attribution.
