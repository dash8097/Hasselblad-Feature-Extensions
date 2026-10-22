# 开发依赖来源

`jpeg-9f` 来自 IJG 官方 `jpegsrc.v9f.tar.gz`，SHA-256 为 `04705c110cb2469caa79fb71fba3d7bf834914706e9641a4589485c1f832565b`，与本机原构建记录一致。文件清单见 `jpeg-9f-source.json`，分发许可见 `jpeg-9f/README` 的 LEGAL ISSUES。

此目录保存原始 C 源码、头文件和配置模板，未放入主机生成的 `jconfig.h` 或编译缓存。目标平台的 `jconfig.h` 和静态库需按相应构建脚本生成。这里不是另一开发者所引用的完整 Mac vendor 原件。

Windows Zig 0.13 工具链和忽略目录中的开发支持快照放在私有库开发材料附件中，参见 本轮交接（本地引用未随候选收录：`../research/4.2.0/DEVELOPMENT_SOURCE_HANDOFF_20261006.md`）。
