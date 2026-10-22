# Hasselblad 相机功能扩展

> **风险警告：私自修改相机固件或安装非原厂补丁存在变砖、无法正常使用的风险。请充分了解风险后再操作。**

范围：当前算法、安装器构建、必要依赖、Auto Test 和 UI Test。
以 [当前交付入口](x2d/CodeTests/developer_delivery/README.md) 和其中的版本锁为准。
历史实验、旧候选工程、设备日志、照片和安装包不在本次源码导出范围。

## 功能列表

主要适配第一代 **Hasselblad X2D 100C / 固件 4.2.0**。下表介绍当前源码覆盖的功能方向；源码、离线用例和安装构建入口的存在，不等于每项功能都已完成相机验收，也不代表适用于其他机型或固件。

| 功能 | 说明 | 源码入口 |
| --- | --- | --- |
| RAW 堆栈 | 平均值、最大值、最小值合成；后台累计、缓冲归还、任务进度、取消和失败恢复。 | [堆栈实现](x2d/CodeTests/raw_stack_continuous/) |
| ISO 堆栈 | 按目标 ISO 规划累计帧数，核对曝光一致性，并记录实际累计结果。 | [堆栈任务控制](x2d/CodeTests/raw_stack_continuous/native_stack_runner.cpp) |
| 25MP 输出 | 四分之一尺寸 RAW / JPEG 处理，包含缩小、质量参数与编码路径。 | [图像处理](x2d/CodeTests/pixel_shift_rgb/) |
| 四亿像素 MultiShot | 六帧像素平移合成，包含内存接管、工作线程、显影与输出流程。 | [六帧合成](x2d/CodeTests/pixel_shift_rgb/capture_memory_merge.c) |
| 十六亿像素 MultiShot | 十六帧合成、分块处理及 JPEG 拼接；实际可用性取决于内存、运行环境及对应版本的验收结果。 | [像素合成与输出](x2d/CodeTests/pixel_shift_rgb/) |
| 取景预拍候选 | 以环形队列保留最近一秒、最多 60 帧取景图像，全按快门时冻结有效帧。取景帧与全尺寸 RAW 拍摄不同，真实 DMA、帧率和内存峰值仍需验证。 | [预拍队列](x2d/CodeTests/liveview_precapture/) |
| 对焦控制研究 | AF-S / AF-C / MF 状态与入口、相位评分、稳定等待和反差搜索速度的衔接研究。不能据此保证连续对焦或镜头闭环效果。 | [对焦流程检查](x2d/CodeTests/afc_observer/) |
| 人脸、人眼与目标选择 | 目标锁定、选眼偏好、框显示及边缘处理；真实识别和镜头合焦需单独验收。 | [主体选择](x2d/CodeTests/subject_tracking/) · [人脸边缘处理](x2d/CodeTests/face_edge_fix/) |
| 相机界面 | 拍摄模式、任务进度、对焦图标、参数弹窗、拍摄动画及声音策略的扩展组件。 | [界面组件](x2d/CodeTests/installer_hardening/native_ui/) |
| 显示与省电策略 | 取景超时、参数页和息屏状态相关的显示控制与设置；不把离线计时当作实测功耗。 | [显示控制](x2d/CodeTests/display_controls/) |
| 电子快门引闪研究 | 闪光同步、等待条件和时序策略的候选及测试；不作为通用闪光兼容性承诺。 | [曝光与同步](x2d/CodeTests/sensor_exposure_candidates/) |
| RAW / JPEG 文件兼容性 | 文件头、元数据、预览、质量参数、文件对齐与手机端兼容性检查。完整 RAW 解码、镜头校正及 Wi-Fi 传输仍需对应软件验证。 | [输出与兼容性](x2d/CodeTests/pixel_shift_rgb/) |
| Windows / macOS 安装器构建 | 基础与高级配置、文件核对、安装、统一卸载、失败回退和诊断流程的源码。需要相应工具链及构建输入，macOS 原生 USB 与相机功能须单独验收。 | [安装器](x2d/CodeTests/installer_hardening/) · [构建入口](x2d/CodeTests/developer_delivery/README.md) |
| Auto Test 与 UI Test | 离线检查、21 组界面测试定义，以及前台菜单、快门、模式切换、任务完成和异常恢复的实机测试驱动。清单不代表已执行通过。 | [测试入口](x2d/CodeTests/developer_delivery/README.md) · [实机 UI 驱动](x2d/FunctionalTests/multishot_ui/) |
| 离线调试环境 | 覆盖合成、堆栈、预拍、对焦、界面、安装卸载和文件兼容性等组件场景；不等同于完整相机固件或全部服务启动。 | [场景入口](x2d/CodeTests/firmware_simulator/suite.py) · [覆盖范围](x2d/CodeTests/firmware_simulator/coverage.json) |

## 构建与验证

当前交付版本、依赖和验收范围以 [构建说明](x2d/CodeTests/developer_delivery/README.md) 与 [版本记录](x2d/CodeTests/developer_delivery/release-lock.json) 为准。此仓库提供源码与构建资料，构建所需的外部输入、工具链和设备环境需自行准备。

- [构建与测试入口](x2d/CodeTests/developer_delivery/README.md)
- [21 组 UI 测试矩阵](x2d/CodeTests/developer_delivery/ui_matrix.json)
- [离线场景测试](x2d/CodeTests/firmware_simulator/suite.py)
- [逐文件摘要](source-files.json)

`python scripts/check_sources.py` 检查文件完整性、语法与本地链接。
`python scripts/test_components.py --compiler <zig> --output <output>` 运行独立算法测试。
本轮执行范围和结果在候选外的验证报告中；不代表整包重建或机内功能已经通过。

## 开发交流群

本群用于相机功能开发、技术研究、问题反馈与交流。扫描下方二维码加入开发交流群。

<a href="docs/assets/developer-group-20261022.jpg"><img src="docs/assets/developer-group-20261022.jpg" alt="哈苏开发交流群二维码" width="360"></a>

二维码有效期至 2026 年 10 月 29 日；失效后需更新。

[源码与使用范围](PUBLIC_DISTRIBUTION_POLICY.md)

---

## English

### Hasselblad Camera Feature Extensions

Independent, unofficial camera-feature components, algorithms, build scripts, and automated tests. The current source primarily targets the **first-generation Hasselblad X2D 100C with firmware 4.2.0**.

> **Warning: modifying camera firmware or installing unofficial patches can brick your camera or prevent normal operation. Understand the risks before making changes.**

### Feature overview

This table describes the capabilities covered by the source. Available code, build entry points, and offline checks do not establish that every feature has passed camera acceptance testing or works with other models, firmware versions, or lenses.

| Feature | Description | Source |
| --- | --- | --- |
| RAW stacking | Average, maximum, and minimum accumulation; background processing, buffer return, progress, cancellation, and failure recovery. | [Stack implementation](x2d/CodeTests/raw_stack_continuous/) |
| ISO stacking | Plan frame counts for a target ISO, check exposure consistency, and record the actual accumulation result. | [Stack task control](x2d/CodeTests/raw_stack_continuous/native_stack_runner.cpp) |
| 25 MP output | Quarter-resolution RAW / JPEG processing, including downscaling, quality settings, and encoding paths. | [Image processing](x2d/CodeTests/pixel_shift_rgb/) |
| 400 MP MultiShot | Six-frame pixel-shift composition with in-memory capture handling, worker threads, rendering, and output paths. | [Six-frame composition](x2d/CodeTests/pixel_shift_rgb/capture_memory_merge.c) |
| 1,600 MP MultiShot | Sixteen-frame composition, tiled processing, and JPEG stitching. Practical availability depends on memory, runtime inputs, and acceptance results for the corresponding version. | [Composition and output](x2d/CodeTests/pixel_shift_rgb/) |
| Live-view precapture candidate | A ring buffer retains up to 60 live-view frames from the previous second and freezes valid frames on a full shutter press. These are live-view images rather than full-resolution RAW captures; actual DMA, frame rate, and peak memory require device validation. | [Precapture buffer](x2d/CodeTests/liveview_precapture/) |
| Autofocus control research | AF-S / AF-C / MF state and entry points, phase-detection scores, stability waiting, and contrast-search speed transitions. This does not establish continuous-focus or lens-control performance. | [Autofocus checks](x2d/CodeTests/afc_observer/) |
| Face, eye, and target selection | Target locking, eye preference, focus-box display, and edge handling. Actual recognition and optical focus require separate acceptance testing. | [Target selection](x2d/CodeTests/subject_tracking/) / [Face edge handling](x2d/CodeTests/face_edge_fix/) |
| Camera interface | Components for drive modes, task progress, focus icons, parameter dialogs, shutter animations, and sound policies. | [Interface components](x2d/CodeTests/installer_hardening/native_ui/) |
| Display and power policies | Live-view timeout, parameter-page behavior, screen-off handling, and related settings. Offline timing does not measure physical power consumption. | [Display controls](x2d/CodeTests/display_controls/) |
| Electronic-shutter flash research | Candidates and tests for synchronization, waiting conditions, and timing policies. Universal flash compatibility is not established. | [Exposure and synchronization](x2d/CodeTests/sensor_exposure_candidates/) |
| RAW / JPEG interoperability | File headers, metadata, previews, quality settings, alignment, and mobile compatibility checks. Complete RAW decoding, lens correction, and Wi-Fi transfer still require validation with the relevant software. | [Output and interoperability](x2d/CodeTests/pixel_shift_rgb/) |
| Windows / macOS installer builds | Source for basic and advanced configurations, file verification, installation, a shared uninstall flow, rollback, and diagnostics. External build inputs and toolchains are required; native macOS USB and camera behavior need separate acceptance testing. | [Installer](x2d/CodeTests/installer_hardening/) / [Build entry](x2d/CodeTests/developer_delivery/README.md) |
| Auto Test and UI Test | Offline checks, 21 UI test groups, and device drivers for foreground menus, shutter actions, mode transitions, completion, and error recovery. Test definitions are not passing execution results. | [Test entry](x2d/CodeTests/developer_delivery/README.md) / [Device UI drivers](x2d/FunctionalTests/multishot_ui/) |
| Offline debugging environment | Component scenarios for composition, stacking, precapture, autofocus, UI, installation, removal, and file interoperability. It does not boot complete camera firmware or all camera services. | [Scenario suite](x2d/CodeTests/firmware_simulator/suite.py) / [Coverage](x2d/CodeTests/firmware_simulator/coverage.json) |

### Building and validation

Start with the [build and test entry](x2d/CodeTests/developer_delivery/README.md) and [version record](x2d/CodeTests/developer_delivery/release-lock.json). Prepare the required external inputs, toolchains, and runtime environments. This repository is a source distribution rather than a ready-made camera installer.

- `python scripts/check_sources.py`: check source integrity, Python syntax, and local links.
- `python scripts/test_components.py --compiler <zig> --output <output>`: run standalone algorithm tests.
- [UI test matrix](x2d/CodeTests/developer_delivery/ui_matrix.json): review the defined test groups and their recorded status.
- [Source manifest](source-files.json): inspect per-file checksums.

Offline tests, successful compilation, and device acceptance are separate results. Hardware UI tests require an authorized camera and an appropriate environment; macOS device testing and complete package acceptance must be checked for the exact version in use.

### Developer discussion group

The group is for camera-feature development, research, issue feedback, and technical discussion. Use the [group QR code](docs/assets/developer-group-20261022.jpg) shown above to join. It is valid until **October 29, 2026** and needs replacement after expiration.

[Source and usage scope](PUBLIC_DISTRIBUTION_POLICY.md)
