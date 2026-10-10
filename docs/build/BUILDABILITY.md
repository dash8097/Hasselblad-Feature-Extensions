# 离线检查与构建

Python 3.11+。只读检查当前已跟踪文件：

    python -B scripts/audit_publication.py

运行三项 Python 合成测试，再使用显式 Zig 工具链编译、运行队列、灰度跟踪和抽象音频路由三组主机测试：

    python -B scripts/build_pixel_research.py --compiler zig --build-dir ./outputs/offline

仅构建当前保留组件，不调用已撤下的固件、文件封装或机内适配。输出位置必须明确。Zig 0.13.0 是旧组件使用的版本；本轮未重新编译 C/C++。

独立 BSD 检测库及空白合成图调用：

    python -B scripts/build_x1dii_detector.py --compiler zig --build-dir ./outputs/detector

不构建设备发布器。可选 QML 预览需要自行安装 PySide6，见[界面说明](../../x2d/flash-ui/README.md)。离线入口不需要连接相机。
