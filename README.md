# Hasselblad Feature Extensions

An independent, unofficial source repository for camera-feature research. It presents selected, sanitized original algorithms, UI components, and offline tests. Main development remains in a separate research project. This repository does not currently provide a complete installable camera update.

## 四亿像素 JPEG 实拍对比 / 400 MP JPEG detail comparison

![左：四亿像素 JPEG 放大 400%；右：一亿像素 JPEG 放大 800%](docs/showcase/assets/x2d-408mp-vs-100mp-detail.png)

左侧四亿像素放大 **400%**，右侧一亿像素放大 **800%**，以相近的主体显示大小比较。观察空调格栅的细线分离、交叉处和边缘轮廓，可以直观看到这组实拍样张的细节差异。

Left: **400 MP at 400%**. Right: **100 MP at 800%**, at approximately matched subject display size. Compare the grille's fine lines, intersections and edge definition.

[查看完整截图和对比说明 / Full screenshot and comparison notes](docs/showcase/X2D_408MP_DETAIL_COMPARISON.md)。这是开发版 JPEG 的实拍展示，包含锐化处理；不代表受控实验测得的分辨率倍数，也不代表本公开仓库已提供完整安装包。

## Contents and status

| Area | Entry | Public scope |
| --- | --- | --- |
| X2D pixel-shift area-array scan synthesis | [Pixel research](x2d/CodeTests/pixel_shift_rgb/README.md) | Reconstruction from four/six positional samples, batched reads and background writes; the six-sample target is approximately 400 MP |
| X2D autofocus optimization | [AF research](x2d/CodeTests/temporary_af_speed_probe/README.md) | Controlled sweep parameters, speed limits and fine-focus transitions; historical candidates, not a universally accepted AF upgrade |
| X2D face/eye target optimization | [Eye preference](x2d/CodeTests/face_edge_fix/README.md) | Left/right choice, loss-delay switching and face fallback; target policy, not a new detector or camera controller |
| X2D electronic-shutter flash | [Synchronization research](x2d/research/history/ESHUTTER_BRANCH_ANALYSIS.md) | Ordinary-flash synchronization under suitable exposure conditions; no HSS or accepted on-camera synchronization claim |
| X2D subject tracking | [Grayscale tracker](x2d/subject-tracking/README.md) | Template-based region retention and stale-observation checks; no neural identity recognition |
| X1D II face priority | [X1D II](X1D2/README.md) | Original excerpts and a BSD-licensed detector; the complete publisher still has missing dependencies |
| X1D components | [X1D](x1d/README.md) | Flash/settings policies, replay research, audio routing, animation and offline resource tools |
| X2D II | [X2D II](x2d2/README.md) | Topic index, without a completed on-camera adaptation |
| Offline UI | [Flash UI](x2d/flash-ui/README.md) | Original icons and QML pages, without a device backend |

X1D II and X2D II are different models. X1D II face-priority excerpts are in `X1D2/`; historical candidates do not establish current camera installation or acceptance.

## X2D highlights

### Pixel Overclock: approximately 400 MP

Pixel Overclock uses **pixel-shift area-array scanning and synthesis**: samples from different pixel-shift positions are mapped to a denser spatial grid. The six-sample research path reconstructs a **23326 × 17498** image: **408,158,348 pixels**, approximately **408 MP**. Six input images count the positional samples, not a 600 MP output or a change to the sensor's physical pixel count. A four-sample reference path is retained for algorithm comparisons.

The public source provides pixel-shift scan reconstruction, batched parallel reads, compute partitions, background writes, and plain-memory pipeline components. Synthetic-input comparisons verify algorithm consistency; they do not establish an equivalent gain in optical detail. Scene motion, lens resolution, sampling alignment and processing quality still affect the result. Actual camera capture, rendering, final-file saving and album integration are outside this public build. See [pixel research](x2d/CodeTests/pixel_shift_rgb/README.md).

### Autofocus optimization research

This work investigates controlled fast-sweep parameters, speed limits, transitions toward fine focus, and the associated mode/UI behavior. Historical candidates are not a universal autofocus upgrade: a speed-command multiplier does not imply the same improvement in total focus time, and lens compatibility, focus accuracy and on-camera stability require separate verification. This update supplies no firmware addresses, internal bindings or installation procedure. See the [AF research scope](x2d/CodeTests/temporary_af_speed_probe/README.md).

### Face and eye target behavior

The public eye-preference policy covers left/right choice, choosing the eye nearest a supplied position, delayed switching when an eye is lost, returning to a preferred eye after it reappears, and falling back to a valid face target. Missing eyes do not reuse stale coordinates or display a fabricated old eye box. This is target-selection logic, not a new face detector or an accepted camera AF controller. See [eye preference](x2d/CodeTests/face_edge_fix/README.md).

The separate [grayscale tracker](x2d/subject-tracking/README.md) retains a bounded target region and rejects stale observations; it does not claim neural face detection or identity recognition. X1D II's licensed detector is a separate module and does not establish X2D integration.

### Electronic-shutter flash synchronization

This research investigates enabling ordinary flash during electronic-shutter capture under suitable exposure conditions. It is separate from high-speed synchronization (HSS) and does not establish a measured, generally safe shutter-speed threshold. Existing dynamic flash compensation must remain intact; any additional wireless timing adjustment is a separate contribution, with zero adjustment preserving the original behavior. The public scope is a sanitized research summary and an independent offline flash UI, not an installable synchronization implementation or proof of physical flash timing. See [synchronization scope](x2d/research/history/ESHUTTER_BRANCH_ANALYSIS.md) and [offline flash UI](x2d/flash-ui/README.md).

## X1D research highlights

- **Autofocus behavior:** research into sweep/fine-focus stages, bounded parameter choices and settings interaction. Historical candidates require their own dependencies and accuracy checks; they are not a universal focus-speed claim.
- **Replay and image output:** full-size JPEG and embedded-preview separation, loading/memory lifetime, and display-color conversion with explicit ICC inputs. A responsive preview and full-resolution inspection are distinct goals.
- **Flash and exposure timing:** ordinary-flash policies, group/power settings and optional half-press updates, with original compensation responsibilities preserved. Offline policy checks do not establish physical synchronization or radio transmission.
- **UI and state restoration:** original animation, abstract audio routing, settings persistence and failure recovery. No audio assets, manufacturer resources or complete device installer are supplied.

See [first-generation X1D](x1d/README.md) and the [build scope](docs/build/BUILD_INPUTS.md) for available components and remaining dependencies.

## X1D II research highlights

The second-generation X1D work focuses on **face-priority single autofocus (AF-S)**: validating detections, selecting a target, exchanging bounded target state and processing previews. Conditional eye-geometry candidates are part of the source excerpts, without an accepted on-camera eye-AF claim.

The public module includes original selection/exchange/publisher excerpts and a fixed BSD-licensed detector. The detector builds independently and passed a synthetic blank-image call; the complete publisher still lacks four project headers, so full AF integration and detection-quality acceptance are not claimed. See [X1D II](X1D2/README.md). This is separate from X2D II, whose current public content remains a [topic index](x2d2/README.md).

## Offline verification

Use Python 3.11+ and, for these tested examples, Zig 0.13.0. Specify an output directory. These entry points do not connect to a camera.

```sh
python -B scripts/CodeTests/test_build_contract.py
python -B scripts/build_core.py --compiler zig --driver zig --build-dir ./outputs/core
python -B scripts/build_pixel_research.py --compiler zig --build-dir ./outputs/pixel-research
python -B scripts/build_x1dii_detector.py --compiler zig --build-dir ./outputs/x1dii-detector
```

The pixel entry checks plain-memory results against a fixed file algorithm, plus ten C component checks and eleven Python tests. The X1D II entry builds only the detector and checks a synthetic blank image; it does not build the incomplete publisher. See [build methods](docs/build/BUILDABILITY.md) and [inputs](docs/build/BUILD_INPUTS.md).

## Publication notes

Browse the grouped [documentation index](docs/README.md).

- [Updates](docs/publication/PUBLIC_UPDATES.md) and [verification status](docs/publication/PUBLICATION_STATUS.md)
- [Scope](docs/publication/PUBLICATION_SCOPE.md), [source review](docs/publication/SOURCE_REVIEW.md), and [contributing](CONTRIBUTING.md)
- [Layout](docs/project/DIRECTORY_LAYOUT.md) and [stage summary](docs/publication/STAGE_DELIVERY.md)

Current documents use concise topic summaries, without historical firmware addresses, device records or internal work logs. Git history has not been rewritten; documentation cleanup is not a comprehensive rights review of older source.

Original work uses [MIT](LICENSE). The detector in `X1D2/face-afs/vendor/libfacedetection/` retains its original BSD-3-Clause terms, notices and publicly released model parameters. Project licensing does not grant third-party rights. This project is not affiliated with or endorsed by the manufacturer.

---

## 中文

独立、非官方的相机功能研究源码仓库，展示经过筛选和脱敏的原创算法、界面组件及离线测试。主要开发在独立研究项目进行；本仓库目前没有完整可安装的相机更新包。

### 内容与状态

| 方向 | 阅读入口 | 公开内容 |
| --- | --- | --- |
| X2D 像素位移面阵扫描合成 | [像素超频](x2d/CodeTests/pixel_shift_rgb/README.md) | 四／六个位置采样的空间重建、分批读取与后台写入；六合一目标输出约四亿像素 |
| X2D 对焦优化 | [对焦研究](x2d/CodeTests/temporary_af_speed_probe/README.md) | 受控快扫参数、速度限制与精扫衔接；历史候选，不是已普遍验收的对焦升级 |
| X2D 人脸／人眼目标优化 | [人眼偏好](x2d/CodeTests/face_edge_fix/README.md) | 左右眼选择、丢失延迟切换与人脸回退；目标策略，不是新增检测器或机内控制器 |
| X2D 电子快门闪光 | [同步研究](x2d/research/history/ESHUTTER_BRANCH_ANALYSIS.md) | 合适曝光条件下的普通闪光同步研究；不涉及 HSS，未声明机内同步验收通过 |
| X2D 主体跟踪 | [灰度模板](x2d/subject-tracking/README.md) | 目标区域保持与过期观测检查；不宣称神经身份识别 |
| X1D II 人脸优先 | [X1D II](X1D2/README.md) | 自写候选片段及 BSD 开源检测库；完整发布器仍缺依赖 |
| X1D 通用组件 | [X1D](x1d/README.md) | 引闪与设置策略、回放研究、声音路由、动画和离线资源工具 |
| X2D II | [X2D II](x2d2/README.md) | 机型资料索引；不提供已完成的机内适配 |
| 离线界面 | [引闪界面](x2d/flash-ui/README.md) | 原创图标和 QML 页面，无设备后端 |

X1D II 与 X2D II 是不同机型；二代人脸优先源码位于 `X1D2/`。历史候选不代表当前公开源码已经完成相机安装或功能验收。

### X2D 主要研究内容

#### 像素超频：约四亿像素

像素超频采用**像素位移面阵扫描合成**：把不同位移位置取得的采样映射到更密的空间像素阵列。六合一研究路径的目标图像为 **23326 × 17498**，共 **408,158,348 像素**，约 **4.08 亿像素**。六张指位移采样数量，不是六亿像素输出，也不改变传感器的物理像素数量；源码还保留四个位置采样的参考路径供算法对照。

公开部分提供位移扫描重建、分批并行读取、计算分区、后台写入和通用内存流水线。自造输入对照证明算法结果一致，不等于光学细节也提高同样倍数；场景运动、镜头分辨力、采样对齐和处理质量仍会影响结果。实际相机拍摄、显影、成片保存和相册接入不在公开构建范围，见[像素研究](x2d/CodeTests/pixel_shift_rgb/README.md)。

#### 对焦优化研究

研究受控快扫参数、速度限制、转入精扫的阶段衔接，以及相应的模式与界面行为。历史候选不能视为通用对焦升级：速度命令倍数不等于总对焦耗时提升倍数，镜头兼容性、合焦精度和机内稳定性仍需分别验证。本次不提供固件地址、内部绑定或安装流程，见[对焦研究范围](x2d/CodeTests/temporary_af_speed_probe/README.md)。

#### 人脸与人眼目标优化

公开的人眼偏好策略包括左右眼选择、按给定位置选择最近眼睛、丢失后的延迟切换、偏好眼重新出现后的恢复，以及回退到有效的人脸目标。眼睛丢失时不继续使用旧坐标，也不伪画上一帧眼框。这是目标选择逻辑，不是新增的人脸检测器或已验收的机内对焦控制器，见[人眼偏好](x2d/CodeTests/face_edge_fix/README.md)。

独立的[灰度模板跟踪](x2d/subject-tracking/README.md)用于有限时长的目标区域保持和过期观测拒绝，不宣称神经人脸检测或身份识别。X1D II 的开源检测库属于另一个模块，不能据此认定 X2D 已完成接入。

#### 电子快门闪光同步

研究目标是在合适曝光条件下，让电子快门拍摄使用普通闪光同步。它与高速同步 HSS 分开，也没有给出经测量确认、普遍可靠的快门速度阈值。原有动态闪光补偿必须保留；额外无线时序调整应独立叠加，零值保持原有行为。公开内容为脱敏研究摘要与独立离线引闪界面，不是可安装的同步实现，也不证明物理闪光时序已验收，见[同步范围](x2d/research/history/ESHUTTER_BRANCH_ANALYSIS.md)和[离线引闪界面](x2d/flash-ui/README.md)。

### 第一代 X1D 主要研究内容

- **对焦行为**：快扫／精扫阶段、受控参数与设置交互研究。历史候选仍需对应依赖与精度验证，不宣称普遍的对焦提速。
- **回放与图像输出**：全尺寸 JPEG 和内嵌预览的区分、加载与内存生命周期，以及显式 ICC 输入的显示颜色转换。快速预览与全分辨率细节检查是两个目标。
- **引闪与曝光时序**：普通闪光策略、分组／功率设置与可选半按更新，保留原有补偿责任。离线策略检查不证明物理同步或无线发射已完成。
- **界面与状态恢复**：原创动画、抽象声音路由、设置持久化和失败恢复，不附带音频、厂商资源或完整设备安装包。

可公开组件和依赖缺口见[第一代 X1D](x1d/README.md)与[构建范围](docs/build/BUILD_INPUTS.md)。

### X1D 二代主要研究内容

重点为**人脸优先单次对焦 AF-S**：检测结果检查、目标选择、有界目标状态交换和预览处理。源码片段包含受条件约束的眼部几何候选，不宣称机内人眼对焦已验收。

公开模块包含项目自写的选择／交换／发布器片段，以及固定版本的 BSD 开源检测库。检测库已独立编译并通过合成空白图调用；完整发布器仍缺四个项目头文件，不宣称完整对焦接入或检测质量已验收，见 [X1D II](X1D2/README.md)。它与 X2D 二代分开，后者当前公开内容仍为[主题索引](x2d2/README.md)。

### 离线验证

需要 Python 3.11+；编译示例使用已验证的 Zig 0.13.0。输出由使用者显式指定，测试不连接相机。

```sh
python -B scripts/CodeTests/test_build_contract.py
python -B scripts/build_core.py --compiler zig --driver zig --build-dir ./outputs/core
python -B scripts/build_pixel_research.py --compiler zig --build-dir ./outputs/pixel-research
python -B scripts/build_x1dii_detector.py --compiler zig --build-dir ./outputs/x1dii-detector
```

像素入口验证普通内存与固定文件算法的一致性，另运行十个 C 组件检查和十一项 Python 测试。X1D II 入口只编译开源检测库并测试自造空白图，不编译缺少依赖的完整发布器。详细输入见 [构建方法](docs/build/BUILDABILITY.md)和[依赖清单](docs/build/BUILD_INPUTS.md)。

### 公开说明

按类别浏览[文档索引](docs/README.md)。

- [本次更新](docs/publication/PUBLIC_UPDATES.md)与[当前验证状态](docs/publication/PUBLICATION_STATUS.md)
- [公开范围](docs/publication/PUBLICATION_SCOPE.md)、[来源审核](docs/publication/SOURCE_REVIEW.md)与[贡献说明](CONTRIBUTING.md)
- [目录索引](docs/project/DIRECTORY_LAYOUT.md)与[阶段成果](docs/publication/STAGE_DELIVERY.md)

当前文档使用简短主题说明，移除了历史固件地址、设备记录与内部施工过程。Git 历史仍保留；既有旧源码的全面权利审核也未因此自动完成。

原创部分采用 [MIT](LICENSE)。`X1D2/face-afs/vendor/libfacedetection/` 按其原始 BSD-3-Clause 许可发布，保留上游声明及公开模型参数；项目许可不覆盖第三方权利。本项目不代表厂商或获得厂商认可。
