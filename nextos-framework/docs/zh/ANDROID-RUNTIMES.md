[English](../en/ANDROID-RUNTIMES.md) · [Português](../pt-BR/ANDROID-RUNTIMES.md) · [中文](../../README.zh-CN.md)

# 为 Android 游戏选择 runtime

[Português](../pt-BR/ANDROID-RUNTIMES.md)

本路线始终从所有者提供的 **Android** 构建出发，目标为 Linux ARM。游戏示例仅来自[目录中已选定的公开来源](../../catalog/ports.json)。这些指南只补充操作步骤；它们既不引入其他端口，也不引入商业数据。

## 1. 先识别，再选择

在克隆根目录中使用 Python 3.11+、`readelf` 运行命令；若需读取二进制清单，还需 `aapt`。[清单登记指南](ANDROID-INVENTORY.md)说明了独立的输入、限制与隐私要求。替换路径并使用新的输出文件：

```sh
python3 tools/inventory_apk.py /private/owner-input/game.apk --output work/android-inventory.json
python3 tools/find_reference.py --runtime gamemaker
python3 tools/find_reference.py --runtime renpy --abi arm64-v8a
python3 tools/find_reference.py --runtime haxe-lime --abi arm64-v8a
python3 tools/find_reference.py --runtime native-android --abi arm64-v8a
```

检索默认返回含 **Android 来源**的档案，其中包括 `guides` 中的 PT/EN 路径。`--engine` 按名称检索；`--runtime` 用于选择族，即使引擎名称仍为 `unknown` 也适用。ABI 与渲染器属于附加过滤条件，绝不能替代证据。结果为空时应着手契约调查，而不是证明其不可能。`--platform all` 表示显式检索完整的历史目录，该检索超出本路线范围。

## 2. 遵循对应的技术路线

| `--runtime` 的族 | Android 输入中的线索 | 指南与选定参考 |
| --- | --- | --- |
| `unity` | `libunity.so`；Mono 或 IL2CPP/metadata | [Unity](../../portando_unity/README.en.md)，按版本区分的具体案例 |
| `mono-android` | Mono/.NET Android、程序集与 Java bootstrap | [Mono Android](MONO-ANDROID.md)、Stardew Valley、ScourgeBringer、Blossom Tales；SOR4 用于宿主适配 |
| `godot` | Godot 库、PCK/导出及匹配版本 | [Godot](GODOT.md)、Tearscape |
| `cocos2d-x` | JNI/Cocos2d-x、调度器与资源 | [Cocos2d-x](COCOS2D-X.md)、Chrono Trigger 与 Geometry Dash |
| `gamemaker` | `libyoyo.so`、运行器及其所消费的文件 | [GameMaker Android](GAMEMAKER-ANDROID.md)、Forager |
| `renpy` | `librenpython.so`、Python/Ren'Py bootstrap 与资源 | [Ren'Py Android](RENPY-ANDROID.md)、Summertime Saga |
| `haxe-lime` | 同一 ABI 中存在 `liblime.so` 与 `libApplicationMain.so`；确认 hxcpp | [Haxe/hxcpp/Lime Android](HAXE-LIME-ANDROID.md)、Tightrope Theatre |
| `native-android` | C/C++ 库、JNI、NativeActivity 或 Android SDL | [Android C/C++](NATIVE-ANDROID.md)、Action Squad、KOTOR、Angry Birds 与 RCRDX |

MonoGame/FNA 仅当它属于被检查的该 Android 构建时才适用。清单登记并不能识别所有 runtime：被重命名的库、内含的程序集与混合引擎都需要进一步阅读。仅凭 `liblime.so` 无法判定其为 hxcpp 应用；缺少已知特征也不能判定为 C/C++。以 Java/Kotlin 为主的 APK 需要 runtime 层面的调查；这些指南并未演示通用的 ART 支持。

## 3. 理解分类所能证明的范围

[档案集](../../catalog/profiles.json) 区分 `platform`、`runtime_family`、引擎、版本、ABI、渲染器与状态。新的分类会提供 `classification_evidence`，其中包含快照路径与 SHA-256。[技术路线登记表](../../catalog/android-runtimes.json)提供双语链接。共有 38 个 Android 来源档案；没有导入来源的社区卡片不参与检索。

打开每条结果的 `SOURCE-MAP.json`，检查 commit、纳入/排除的文件与许可。某个参考实现可能包含早于或晚于 V5 的 pins，以及部分验证。已选定的 Tightrope 来源为 `1.0.5-test.1`，物理验收仍待确定：用它来研究分类并诊断限制，不要作为已批准的基础版本。不要把它所带的变通做法提升为默认方案。

## 4. 在 V5 范围内实施

使用 [SDK](../../toolchains/sdk/zh/README.md)、[pins](PINS-AND-SOURCES.md)、[原始首个移植示例](../../examples/zh/first-port/README.md)以及一个独立的 adapter 项目。当 Android 构建提供 AArch64 时，优先选择 AArch64。不要把 Linux wrapper 的 CPU 与目标 ABI 相混淆。对于 ARMv7，需证明 softfp/ARMHF 桥接可用。

V5 保留在 `framework-v5`，commit `657fb65a23b5c3b20040e76307b27e6470b1d17c`。集成参考实现之前，先检查 loader 契约。某个仅 V6 提供的功能，不会因它的快照出现在目录中就变为可用；应当记录该不兼容的依赖。本文档不再新增任何共享 runtime。

## 5. 完成新的移植

保持原生顺序，并将特定的类、回调与适配保留在 adapter 中。使用系统 SDL，并审计每一个公开的 Linux ELF 的 GLIBC ≤ 2.30。[NXExtract](NXEXTRACT.md) 与[测试与交付](TESTING.md)描述了干净安装、所有者数据、双语 `INSTALLATION.md` 以及指向同一最终产物的证据。

NXExtract/NXSplash 画面保持为规范形式。图形、音频、操作、保存/重载与退出都需要相应的证据；参考检索与宿主测试并不等于设备可用。在获得授权的实体运行之前，保留已批准的字节并收集所需的检查项。
