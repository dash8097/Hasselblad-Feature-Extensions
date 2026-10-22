# X2D 当前源码候选

> **风险警告：私自修改相机固件或安装非原厂补丁存在变砖、无法正常使用的风险。请充分了解风险后再操作。**

范围：当前算法、安装器构建、必要依赖、Auto Test 和 UI Test。
以 [当前交付入口](x2d/CodeTests/developer_delivery/README.md) 和其中的版本锁为准。
历史实验、旧候选工程、设备日志、照片和安装包不在本次源码导出范围。

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

[Public distribution policy](PUBLIC_DISTRIBUTION_POLICY.md)
