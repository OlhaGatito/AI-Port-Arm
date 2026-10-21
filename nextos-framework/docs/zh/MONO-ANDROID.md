[English](../en/MONO-ANDROID.md) · [Português](../pt-BR/MONO-ANDROID.md) · [中文](../../README.zh-CN.md)

# Mono Android、MonoGame 与 .NET 运行时

首先练熟 [原创 Mono 演练](../../examples/zh/engines/README.md)：在 Linux 上验证 P/Invoke、布局、回调与错误。它讲授的就是这条边界；下面的 Mono Android 引导属于另一套独立实现。

本指南讲解如何选择带托管代码 Android 游戏的执行路线、如何保持 bootstrap 顺序，以及如何诊断原生桥接。没有任何一条“复制 DLL 然后运行 mono”的通用配方能覆盖所有这些构建。

两条路线输入均仍是 Android；Linux 主机只是运行该构建的一种策略。MonoGame/FNA 并不提供桌面游戏通道。参见 [运行时选择](ANDROID-RUNTIMES.md)。[Blossom Tales](../../ports/blossomtales-nextos/README.en.md) 也是目录中一份公开的 Mono Android/MonoGame 参考。

## 1. 识别真实的运行时

| 本地副本中的证据 | 待确认的假设 | 下一步决策 |
| --- | --- | --- |
| `libmonodroid`、`libmonosgen`、assembly 存储、`mono.android` 类 | Android 版 Mono/.NET | 研究 Android 运行时链 |
| `libunity` 与托管 assembly | 使用 Mono 的 Unity | 参考 Unity 指南及其确切版本 |
| `libunity` + `libil2cpp` + metadata | Unity IL2CPP | 托管代码已转为原生；属于另一条路线 |
| 随附 Android 构建提供的 MonoGame/FNA assembly | 为这些 Android 输入评估一个 Linux 主机 | 验证 entrypoint、BCL 以及每一处 Android/原生依赖 |
| 使用 C# 的 Godot | Godot/.NET | 参照 Godot 的版本与 .NET 宿主 |

这些名称只是线索。请确认版本、ABI、依赖以及 assembly 的存储方式。assembly 可能被打包、压缩，或附带 AOT 镜像；单凭 `.dll` 扩展名不足以判定其中含有可供 JIT 使用的 IL。

## 2. 在保留 Android 运行时与使用 Linux 主机之间选择

**路线 A：Android 兼容。** 载入属于原作者构建的那条原生链，实现 Bionic/JNI/Android，并让原始 bootstrap 初始化托管运行时与 Activity。[Stardew Valley](../../ports/stardewvalley-nextos/README.en.md) 与 [ScourgeBringer](../../ports/scourgebringer-nextos/README.en.md) 提供了用于研究该路线的公开代码。

**路线 B：托管 Linux 主机。** 仅当 entrypoint、assembly、BCL 与原生库在 Android 之外确实可用，并且有文档记载的适配时，才成立。[SOR4](../../ports/sor4-nextos/README.en.md) 及其 MonoGame 补丁展示了另一些边界；不要因为两个游戏都用 C# 就照搬某一游戏的 bootstrap。

[Mono embedding 手册](https://www.mono-project.com/docs/advanced/embedding/) 描述了兼容应用的初始化、assembly 加载与托管/原生调用。该 API 并不会自动替代 `Java_mono_android_Runtime_init`、Java 类注册、Activity 或 Android 构建的打包。

## 3. 在调用 Game.Run 之前梳理初始化流程

记录库的顺序与构造函数/JNI、运行时配置、assembly 存储、类型/assembly 注册、Activity/OnCreate、视图创建与循环启动。所选 Stardew 的 README 描述了 `libmonosgen-2.0.so → libxamarin-app.so → libmonodroid.so` 以及 Android bootstrap；该顺序属于那个特定配置。

过早调用 `Game.Run()` 可能跳过必需的初始化。应通过正确的加载器载入依赖：不要盲把 Bionic ELF 交给 glibc `dlopen`。若某份参考拒绝 AOT 而改用 JIT，请先确认新运行时允许该回退，并且具备所需的 IL，再沿用这一决定。

## 4. 排查常见的失败边界

| 边界 | 测量项 | 诊断示例 |
| --- | --- | --- |
| Bionic/glibc | 常量、结构体、对齐与 errno | 错误的 `sysconf` 枚举返回无效的页大小 |
| 线程/GC | 信号量、TLS、挂起与按 ABI 区分的信号 | `sem_t` 大小不正确会破坏内存 |
| JNI/Java | 签名、引用、类与回调 | 能进入 Activity 并不能证明后期 Java 服务可用 |
| P/Invoke | 每座库的实际名称、ABI 与解析方式 | 错误的 provider 只在打开某个菜单功能时暴露 |
| Assemblies | 标识、版本、完整性与依赖 | 混用 BCL/运行时会导致类型或方法损坏 |
| EGL/GL | 真实上下文与桌面 GL/GLES 归类 | 桌面库被误判会改变 MonoGame 的路线 |

请从 Stardew 的 `src/bionic_shims.c`、`src/pthread_bridge.c`、`src/jni_shim.c` 与 `src/sdv_egl_bridge.c` 入手，以及 ScourgeBringer 的 shims/AAudio 桥。检查清单文件中包含哪些文件。不要用另一台设备的常量整体替换 `sysconf`，也不要把禁用 GC/jobs 当作通用修复手段。

## 5. 进行首次构建

Linux 可执行文件参见 [ARM 编译](BUILD-ARM.md)；随后阅读所选移植的构建脚本，列出其工具链、生成的源文件与缺失的依赖。含有源码的 C# 项目可能有独立的托管构建步骤；这既不会重新编译原作者的专有 assembly，也不授权提交这些 assembly。

固定运行时/BCL 与原生库的版本。对每一个再分发的 Linux 依赖审计 GLIBC。对于公开的 SOR4，参考配方是 `sor4-nextos` 中的 `port/wwise-native/build-glibc230.sh`；来自另一代码树的历史配方并不等价。

## 6. 检查图形、音频与输入

保留运行时实际选择的图形 API，以及唯一的上下文/present 所有者。音频需要其专属后端：XACT/OpenAL、FMOD/AAudio 或其他路线不能互换。记录采样率、格式、声道、回调与被消费的队列，然后在设备上试听。

把手柄输入投递到真实的 Android/MonoGame 路径，并观察其托管消费者。一个入队的 SDL 事件并不能证明产生了实际动作。请覆盖横向/纵向菜单、游戏玩法、暂停、屏幕键盘、热插拔与退出；在没有契约的情况下不要重复实现鼠标/键盘/手柄。

## 7. 准备数据并收口证据

NXExtract 必须从原作者的完整副本中准备 assembly/存储、库与内容。所需的存储迁移也必须在全新安装上发生，并在更新过程中保留存档。[配方与全新安装测试](NXEXTRACT.md)。

记录运行时配置、产物/SHA、Activity 入口、首帧、音频、输入、保存/重载与关停。ScourgeBringer 快照记录了 glibc 2.30 的验证局限：主机测试或另一个固件不能等同于在该目标上的验收。请始终阅读所选 commit 的状态。

## AI 任务

```text
Identify the managed runtime and its complete inputs. Compare Android and
Linux routes by contract, not by the .NET name. Document bootstrap, Bionic,
JNI, P/Invoke, assemblies and AOT/JIT. Implement the adapter in a separate
project, preserve data and references, build for ARM and run targeted tests.
Record unproven contracts and the NXExtract installation flow.
```
