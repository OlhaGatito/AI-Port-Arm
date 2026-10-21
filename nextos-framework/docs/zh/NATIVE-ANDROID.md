[English](../en/NATIVE-ANDROID.md) · [Português](../pt-BR/NATIVE-ANDROID.md) · [中文](../../README.zh-CN.md)

# 原生 Android C/C++：JNI、NativeActivity 与 SDL

本通道覆盖原生 **Android** 游戏库（含自研引擎），其适配器将运行在 Linux ARM 上。先用 [运行时选择](ANDROID-RUNTIMES.md) 确认是否属于某个专门通道。没有 Unity、Godot 或 Mono，并不能说明该游戏其余部分不需要 Java。

## 1. 清点库与 bootstrap

```sh
python3 tools/inventory_apk.py /private/owner-input/game.apk --output work/native-inventory.json
python3 tools/find_reference.py --runtime native-android --abi arm64-v8a
python3 tools/find_reference.py --runtime native-android --abi armeabi-v7a
```

记录包/版本、客机 ABI、ELF 与哈希、`DT_NEEDED`、导入、重定位、TLS、构造函数、所用 Java 类与 Android 服务。[清单](ANDROID-INVENTORY.md) 不会执行 DEX，也无法发现每一处 JNI/dlsym 调用。请到获授权的本地副本中阅读 Activity 及其回调，以补全这张地图。

若可用，优先选择 AArch64；64 位的 Linux 包装器并不会把 ARMv7/x86 客机变成 AArch64。即使原生家族已确认，未知的引擎名在目录中仍记为 `unknown`。混合型引擎（例如以 Cocos2d-x 为壳、其上是 xGen/Horde3D+bgfx 的 Off The Road）需要把初始化与渲染分开处理。

## 2. 按契约选择参考对象

以下参考均已是本集合中选定的公开来源。复用前请先打开清单、README 与许可证；历史 pin 并不构成 V5 迁移。

| 来源 | 选定 commit | 待研究的边界 |
| --- | --- | --- |
| [Action Squad](../../ports/actionsquad-nextos/README.en.md) · [SOURCE-MAP](../../ports/actionsquad-nextos/SOURCE-MAP.json) | `842efc61a47192cd3ac4d3062dbe9c32e808c791` | AArch64、NativeActivity 与输入队列 |
| [KOTOR](../../ports/kotor-nextos/README.en.md) · [SOURCE-MAP](../../ports/kotor-nextos/SOURCE-MAP.json) | `9d8dc0cdd3cf164a39f480200ab20a9a1117b91b` | ARMv7、Odyssey/SDL2、OBB 与引擎上下文 |
| [Angry Birds](../../ports/angrybirds-nextos/README.en.md) · [SOURCE-MAP](../../ports/angrybirds-nextos/SOURCE-MAP.json) | `74af40a86d09126e5e2c3b220102411c44407fd5` | ARMv7、Rovio Fusion、JNI 与混音器 |
| [RCRDX](../../ports/rcrdx-nextos/README.en.md) · [SOURCE-MAP](../../ports/rcrdx-nextos/SOURCE-MAP.json) | `7d79b145f9c271ac5bbf959fb254880482ac99a1` | AArch64、surface 回调、`nativeInit` 与 `SDL_main` |

请从 [Action Squad 的 main](../../ports/actionsquad-nextos/upstream/src/main.c)、[KOTOR 的 main](../../ports/kotor-nextos/upstream/src/main.c)、[Angry Birds 的 main](../../ports/angrybirds-nextos/upstream/src/main_angrybirds.c) 或 [RCRDX 的 main](../../ports/rcrdx-nextos/upstream/src/main.c) 入手。它们代表不同的流程；并不存在能覆盖这四者的通用 entrypoint 列表。

## 3. 区分入口路径

| 观察到的路径 | 所需契约 |
| --- | --- |
| 带 JNI 方法的 Java Activity | 类、签名、`JNI_OnLoad`/注册以及真实被调用的回调 |
| NativeActivity | `ANativeActivity` 结构体/回调、生命周期、窗口、输入与保存的状态 |
| `android_native_app_glue` | 应用线程、命令、looper、同步以及该构建中的 `android_main` 入口 |
| Android SDL | Activity/音频/手柄 JNI、surface/尺寸、`nativeRunMain` 线程与 SDL 循环 |

NDK 文档描述了 `ANativeActivity_onCreate` 与 Activity 回调的注册。在提供结构体或派发事件之前，先还原观察到的契约。[NDK 概念](https://developer.android.com/ndk/guides/concepts)。不要用直接进入游戏的调用来替换原生顺序。

JNI 还要求处理局部/全局引用、异常、字符串/数组、签名与线程绑定。`JNIEnv` 是线程相关的；跨线程共享它会破坏该契约。[官方 JNI 指引](https://developer.android.com/ndk/guides/jni-tips)。后期回调、暂停、恢复与关停同样需要建模。

## 4. 实现 Bionic/Linux 桥

使用 [shims](SHIMS.md)、V5 契约与[集成的原创示例](../../examples/zh/first-port/README.md)。Linux 加载器与 Android 库属于不同的 ABI 环境。不要不加辨别地用 glibc `dlopen` 去加载 Bionic ELF。

逐一对每条边界记录签名、类型、大小/对齐、所有权、线程、错误值与测试项：libc/libm、C++、pthread/TLS、`errno`、assets、JNI/NDK、音频以及 EGL/GLES。对于 ARMv7，要验证 softfp/hard-float 调用方式（含回调）。未知的必要导入、TLS 符号或 V5 不支持的重定位，需要显式诊断；不要用空函数地址把一切都“解析”掉。

在独立项目中使用[公开 SDK](../../toolchains/sdk/zh/README.md) 构建。使用历史脚本前请检查依赖与被省略的文件。仅在 V6 中才可用的 API 属于需要记录的不兼容项；不要改动 V5 组件，也不要自动迁移参考。

## 5. 验证图形、音频与输入

| 边界 | 证据与反例 |
| --- | --- |
| EGL/GLES | 单一窗口/上下文/present 所有者，present 之前的真实尺寸与像素；死上下文或黑屏视为失败 |
| 较旧的 GLES | 在适配 GLES2 之前先确认 GLES1/1.1 与固定功能矩阵；重命名导入并不会让 Swordigo/FF4 变成 GLES2 |
| 音频 | OpenSL、AudioTrack 或原生混音器的格式、声道、队列、回调与消费情况；暂停/恢复与实际试听 |
| 输入 | 游戏实际消费的队列/回调、按下/释放、坐标轴、焦点与生命周期；孤立的 SDL 事件并不充分 |
| 退出与存档 | 原生的暂停、flush/保存与关停顺序；重载进度而不残留进程 |

使用系统自带的 SDL。不要内嵌私有 SDL 来照搬历史方案。触摸光标需要正确的坐标、打磨过的指针以及菜单/玩法行为的区分；不要在游戏按钮上凭空发明全局动作。

## 6. 准备解包并冻结字节

为原作者的完整 APK/splits/OBB 创建 [NXExtract](NXEXTRACT.md) 配方。验证 Android 身份与关键负载，保留兼容的重打包方式，并在真正全新的安装中执行钩子。不要分发原始库或资源。配以 PT/EN 双语的 `INSTALLATION.md`，说明技术性参考副本的身份、布局与说明。

对每一个公开的 Linux ELF 审计 GLIBC ≤ 2.30；保留 V5 加载器、NXExtract 界面与 NXSplash。按照[测试与交付](TESTING.md)冻结已验证的可执行文件，并把交付绑定到同样的字节。本更新未对这些游戏重新验证，每个新目标都需要属于它自己的授权证据。

## AI 任务

```text
Analyze only the supplied Android game and choose already selected public
references by bootstrap, ABI, renderer, audio and input contracts.
Map constructors, JNI/NativeActivity/SDL and threads without skipping stages.
Implement a separate adapter with real errors and ABI/ownership tests.
Prepare complete NXExtract extraction, persistence and byte-linked evidence.
Preserve V5 and historical sources; record remaining incompatible features.
```
