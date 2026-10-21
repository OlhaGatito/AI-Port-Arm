[English](../en/GODOT.md) · [Português](../pt-BR/GODOT.md) · [中文](../../README.zh-CN.md)

# 将 Godot 游戏从 Android 移植到 Linux ARM

[Português](../pt-BR/GODOT.md)

[原始 Godot 工程](../../examples/zh/engines/README.md)包含完整源码与一个 headless 逻辑测试。渲染与 ARM 导出需要各自的模板与验证；移植一个 Android 构建仍然沿用下面的调查步骤。

首先要确认 runtime 所需的版本、工程格式与扩展。Godot 的 APK 可能允许走一条兼容的 Linux 引擎路线，但一个孤立的 PCK 并不能证明它可以摆脱 Android 插件、C# 或原生库。

## 1. 清点工程

记录 Godot 的主/次/修订版本号以及任何自定义构建；GDScript 或 C#；PCK/导出工程；已导入的资源；Android 插件；GDExtension/GDNative；渲染器；以及音频、视频与输入依赖。要通过数据/runtime 而非目录名来确认版本。

| 调查结论 | 后果 |
| --- | --- |
| Godot 3 且使用 GLES2 | 从那一代版本中寻找兼容的模板 |
| Godot 4 Compatibility | 不要假设默认驱动中存在物理 GLES2 |
| Forward+/Mobile | 其功能可能超出目标的 GL 路线 |
| C# | 固定引擎、程序集与兼容的 .NET runtime |
| Android 插件 | 调整/实现它的契约，或记录为阻断项 |
| 原生扩展 | 需要兼容的 Linux/ABI 构建，或获得授权的等价方案 |

Godot 把渲染方式与图形驱动分开；Compatibility 使用 OpenGL 系列，而 Forward+/Mobile 使用 RenderingDevice。切换方式可能改变功能与画面。[官方渲染器概览](https://docs.godotengine.org/en/stable/tutorials/rendering/renderers.html)。

## 2. 选择兼容的路线

如果你拥有源码工程，则为正确的架构准备 Linux 导出，并使用匹配的模板与资源。导出系统会把工程内容打包成 PCK 再与可执行文件结合；架构必须显式选择。[Godot Linux 导出](https://docs.godotengine.org/en/stable/tutorials/export/exporting_for_linux.html)。

如果你只有所有者提供的 Android 副本，清点其中导出的内容，并判断一个完全对应的 Linux 引擎能否消费它。不要凭空编造源码、密钥、插件或格式兼容性。已编译的 GDScript、纹理导入与程序集都可能把该构建锁定在某一特定版本上。

## 3. 阅读公开参考

[Tearscape](../../ports/tearscape-nextos/README.en.md) 包含 Godot 4.6.1/.NET 的集成源码、视频 provider、输入与数据准备。在复用任何内容之前，先阅读这份快照的 README、`build_low_glibc.sh`、`src/shim/` 以及清单中列出的文件。

这份快照记录了对近期修订版仍然待办的物理验证。将其收录此处并不等于把它提升为已批准的通用基线。应当研究其架构，并在你正在做的移植中保留任何已经批准的产物。一条经过物理验证的 GLES2 路线必须进入官方构建与 pin；不要把它留在使用者不予打包的实验目录中。

## 4. 准备引擎与构建

区分 Linux 引擎、Linux 库、需要的 .NET runtime 以及所有者数据。固定引擎 commit 与补丁、SDK/sysroot、构建配置与依赖哈希。用 [ARM 指南](BUILD-ARM.md)检查每一个 Linux ELF 的架构与 GLIBC。

不要只把 Tearscape 的构建当作实验来执行：它的配方输入已冻结，且遵循单一构建规则。对于新的移植，先阅读该版本确切的构建系统选项，再创建一份独立的配方。这套资料中并不包含现成的通用 ARM Godot SDK，也不包含商业演示 PCK。

对于一个已经准备好的**原始工程**和一套兼容的编辑器安装，下面是查看与使用 CLI 的方式；`Linux ARM64` 必须作为一个预设存在，且已安装正确的模板：

```sh
export NEXTOS_GODOT=/opt/godot/godot
"$NEXTOS_GODOT" --version
"$NEXTOS_GODOT" --help
mkdir -p work/godot-export
"$NEXTOS_GODOT" --headless --path "$PWD/work/godot-project" \
  --export-release "Linux ARM64" "$PWD/work/godot-export/demo-nextos"
```

该命令依赖这个工程/预设：它无法直接从未经准备的 APK 运行。CLI 导出与编译引擎是不同的两件事。[官方命令行参考](https://docs.godotengine.org/en/stable/tutorials/editor/command_line_tutorial.html)。

## 5. 保留窗口、视口与画面

分别测量面板/drawable 尺寸、窗口、逻辑视口与内容矩形。改变逻辑高度可能改变游戏的相机缩放。letterbox 或 stretch 必须遵循已批准的行为以及所有者明确的设置。

在 Mali-450 上保留物理 GLES2。一个翻译壳层必须真正覆盖着色器、纹理、FBO、混合与拷贝操作；改写一个 GL 字符串并不等于实现了渲染器。对比标题画面、玩法、转场、透明与特效。有声音但黑屏属于终极图形失败。

## 6. 输入、音频、C# 与存档

输入动作必须到达真实的 `InputMap` 接收端。如果一个映射被宣传为可编辑，就要证明编辑某个动作会改变运行时行为；仅仅附带文件是不够的。保留控制器标识、合作模式与上下文切换。

测试音频 provider 与关闭流程。对于 C#，验证 .NET 版本、BCL、程序集与 P/Invoke；参见 [Mono/.NET](MONO-ANDROID.md) 以区分不同 runtime。保留 `user://` 与存档迁移；已准备好的数据与存档的生命周期不同。

## 7. 可验证的交付

为导出工程、程序集与严格必要的转换创建 [NXExtract 配方](NXEXTRACT.md)。测试全新安装时的完整输入，然后测试初始场景、进程、转场、音频、输入、存档/重载与退出。记录引擎版本、可执行文件 SHA、数据画像以及实际测试过的设备。

## AI mission

```text
Identify Godot and the exported format, renderer, C#/extensions and Android
plugins. Choose a compatible Linux engine and pin sources, patches and runtime.
Implement providers/adaptations in the new port while preserving the approved
GLES2 route, viewport and InputMap. Document inputs, build, NXExtract and real proof.
```
