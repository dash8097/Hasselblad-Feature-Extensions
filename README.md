# Hasselblad Feature Extensions

独立、非官方的离线算法与界面示例项目，与相机厂商没有隶属或认可关系。名称仅用于说明研究背景。

当前公开版只保留项目原创离线组件，以及保留原许可的开源检测库。历史机内候选、安装器、固件处理链和设备接入代码已从当前版本撤下。本仓库没有可安装的相机更新包。

| 内容 | 入口 | 当前范围 |
| --- | --- | --- |
| 像素合成 | [算法说明](x2d/CodeTests/pixel_shift_rgb/README.md) | 四帧重建、六采样逐行内核、有界双消费者队列 |
| 灰度目标跟踪 | [跟踪组件](x2d/subject-tracking/README.md) | 调用方提供图像和目标；不操作镜头 |
| 人眼偏好 | [离线策略](x2d/CodeTests/face_edge_fix/README.md) | 左右选择、丢失延迟与回退 |
| 引闪界面 | [离线 QML](x2d/flash-ui/README.md) | 原创页面与 SVG，无设备后端 |
| 动画与声音路由 | [离线组件](x1d/shutter-effects/README.md) | 程序绘制动画、抽象后端与替身测试 |
| 开源检测库 | [来源与许可](X1D2/face-afs/vendor/libfacedetection/SOURCE.md) | 固定上游 libfacedetection，保留 BSD-3-Clause 声明 |

当前版本不附带厂商固件、提取物、解密工具、密钥、内部地址或补丁装载链，也不提供设备日志、样片、厂商图标、字体或录音。不能将离线算法结果解释为机内验收或画质提升的测量。

运行只读公开范围检查：

    python -B scripts/audit_publication.py

离线组件的显式构建入口：

    python -B scripts/build_pixel_research.py --compiler zig --build-dir ./outputs/offline
    python -B scripts/build_x1dii_detector.py --compiler zig --build-dir ./outputs/detector

本次完成文件清单、哈希、依赖、相对链接及 Python 语法检查，并运行三项纯内存合成测试；C/C++ 编译与界面渲染未在本次清理中重跑。见[公开状态](docs/publication/PUBLICATION_STATUS.md)和[构建说明](docs/build/BUILDABILITY.md)。

原创部分适用 [MIT](LICENSE)，第三方检测库维持原许可。许可声明不覆盖未获授权的第三方内容。清理是发布范围控制，不是法律无风险保证。

**当前清理不改写旧提交，旧内容仍可能经 Git 历史访问。** 详见[清理记录](docs/publication/SANITIZATION.md)。后续公开变更须重新审查并更新[逐文件清单](publication/public-allowlist.json)。

## English

Independent, unofficial offline algorithms and UI examples, not affiliated with or endorsed by the camera manufacturer. No installable camera update is provided.

The current tree contains selected original pixel-processing, tracking, target-policy and UI components, plus the separately licensed libfacedetection library. Firmware processing, decryption, device bindings, installers and private research material are excluded. Original work remains MIT; third-party notices remain intact.

This cleanup does not rewrite Git history. Older content may remain accessible through past commits. Offline checks are not camera acceptance tests or legal clearance.
