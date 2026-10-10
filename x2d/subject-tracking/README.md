# 灰度模板跟踪

项目自写的 16 × 16 灰度模板与整数平移搜索。调用方串行提供图像、目标和观测，核心拒绝过期或不匹配的观测，限制保留时间。

没有设备调用、模型加载或镜头操作，不宣称尺度、姿态或身份识别能力。合成测试为 [test_subject_tracker.c](../CodeTests/subject_tracking/test_subject_tracker.c)，入口见[构建说明](../../docs/build/BUILDABILITY.md)。
