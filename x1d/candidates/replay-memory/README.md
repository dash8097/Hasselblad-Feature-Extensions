# 缓存与内存预算

`byte_budget.h` 按类别预留内存；`latest_cache.h` 与 `role_cache.h` 使用持有引用的租约管理最新图像和前后窗口。过期结果、容量不足和重复发布有明确拒绝路径。
