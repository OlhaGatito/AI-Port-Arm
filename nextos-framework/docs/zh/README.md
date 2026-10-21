[English](../en/README.md) · [Português](../pt-BR/README.md) · [仓库中文版](../../README.zh-CN.md)

# NextOS 中文指南

[Português](../pt-BR/README.md)

[仓库](../../README.zh-CN.md) · [46 款作品目录](../../catalog/zh/README.md) · [移植 Unity](../../portando_unity/README.en.md)

从主机 C 示例入手，进阶到[综合首个移植示例](../../examples/zh/first-port/README.md)，过程中使用[公开 SDK](../../toolchains/sdk/zh/README.md)。随后选择引擎。指南为 AI 主导实施提供命令、契约与判定标准；所有者提供的兼容数据保持私有。

| 指南 | 内容 |
| --- | --- |
| [入门](GETTING-STARTED.md) | 克隆、工具与首次测试 |
| [架构](ARCHITECTURE.md) | 组件、职责与 pins |
| [AI 移植](AI-PORTING.md) | 任务、清单登记与自主实施 |
| [编译 ARM](BUILD-ARM.md) | 主机、AArch64、ARMv7、NDK 与 ELF |
| [pins 与来源](PINS-AND-SOURCES.md) | 导出组合与公开文件找回 |
| [Android 清单登记](ANDROID-INVENTORY.md) | 清单、导入、ABI、阻塞项与参考检索 |
| [诊断实验室](DIAGNOSTIC-LAB.md) | 好坏日志与可执行的反例 |
| [选择 Android runtime](ANDROID-RUNTIMES.md) | 八条技术路线、证据与族检索 |
| [Shims](SHIMS.md) | ABI、JNI、归属与测试 |
| [Mono Android](MONO-ANDROID.md) | Mono/.NET、MonoGame/FNA 与 bootstrap |
| [Godot](GODOT.md) | 引擎、导出、渲染器、C# 与输入 |
| [Cocos2d-x](COCOS2D-X.md) | JNI、资源、文本、渲染与音频 |
| [GameMaker Android](GAMEMAKER-ANDROID.md) | YoYo 运行器、VM/YYC、ABI 与数据 |
| [Ren’Py Android](RENPY-ANDROID.md) | Python、SDL/JNI、资源与持久化 |
| [Haxe/hxcpp/Lime Android](HAXE-LIME-ANDROID.md) | bootstrap、线程、TLS/GC 与限制 |
| [C/C++ Android](NATIVE-ANDROID.md) | JNI、NativeActivity、Android SDL 与 Bionic |
| [NXExtract](NXEXTRACT.md) | 配方、所有者数据与干净安装 |
| [测试与交付](TESTING.md) | 证据、候选版本与发布 |
| [故障排查](TROUBLESHOOTING.md) | 症状与定向诊断 |

## 专项技术路线

- [Unity](../../portando_unity/README.en.md)：15 个公开来源案例、两条社区记录与综合工具。
- [Freedom Planet 2 — Vulkan 转 GLES2](../../portando_unity/en/FP2-VULKAN-GLES2.md)：逐着色器程序转换与数据保留。
- [引擎练习](../../examples/zh/engines/README.md)：Mono、Godot、Unity 与 Cocos2d-x，并明确标注构建状态。
- [着色器实验室](../../examples/zh/shader-lab/README.md)：原创着色器、公开 pins 与一个 compute 反例。
- [Shim 示例](../../examples/zh/shims-reference/README.md)：一个小型可编译的契约。
- [许可与致谢](../../LICENSING.zh-CN.md)、[贡献指南](../../CONTRIBUTING.zh-CN.md)、[验证流程](../../publication/VALIDATION.en.md)。

## 语言与历史来源

本出版物中的所有编辑类文档均有等效的 PT/EN 版本。顶部链接用于切换语言。标识符、路径、schema 与命令保持稳定；提示词与解释则被翻译。

本目录（`docs/zh/`）目前译为 **20 篇**，涵盖上述表格中的全部指南；表格下方的专项技术路线（SDK、首个移植示例、引擎练习、着色器实验室、各移植条目目录等）尚无中文版，仍指向英文原文。根目录 README 的[中文版](../../README.zh-CN.md)已一并调整为中文链接。

`framework/` 下的历史文件、NXExtract 树以及 `ports/*/upstream/` 保留原始字节、许可与语言。双语卡片与指南位于这些不可变树之外。代码注释、旧工具提示与规范性法律文本并未被当作新许可那样翻译。
