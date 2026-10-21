[English](../en/ANDROID-INVENTORY.md) · [Português](../pt-BR/INVENTARIO-ANDROID.md) · [中文](../../README.zh-CN.md)

# Android 清单登记与参考检索

[Português](../pt-BR/INVENTARIO-ANDROID.md)

从所有者提供的兼容输入开始。该工具会生成技术信息，但不执行访客代码，并在其 JSON 中排除外部 APK 文件名与计算机路径。

## 1. 执行清单登记

使用 Python 3.11+、`readelf` 与 `aapt`，以便同样能从二进制清单中获取版本。替换路径；集合中只提供实际存在的 APK，单 APK 的游戏省略第二个：

```sh
python3 tools/inventory_apk.py /private/owner-input/base.apk   /private/owner-input/config.arm64_v8a.apk   --output work/inventory.json
python3 tools/find_reference.py --engine Unity --abi arm64-v8a --renderer GLES2
```

公开 SDK 已包含这些工具。若要在容器中使用它们，需将输入以只读方式显式挂载到 `/private/owner-input`，同时挂载集合目录与 `work/`。不要将输入发送给 CI 或外部服务。输出必须是新文件。

请使用 [Android runtime 选择](ANDROID-RUNTIMES.md)。`find_reference.py` 默认筛选 Android 来源，并接受 `--runtime` 以按族检索。每条结果都包含 PT/EN 指南；`--platform all` 用于显式查询其余历史参考。

清单登记同样能识别 GameMaker（`libyoyo.so`）、Ren’Py（`librenpython.so`）以及 Haxe/hxcpp/Lime（同一 ABI 中的成对库）等提示。它检查被检视的原生库名称，而不会依据名称相似的资源文件进行分类。提示是按 APK 逐个给出的：请比对分包报告，不要把缺失当作不可能。

## 2. 解读各字段

报告包含包名/分包名、可用时的版本、容器大小/哈希、ABI 库、哈希、带类型/绑定/版本的导入、依赖与引擎提示。缺失的版本保持为 `null`；引擎识别仍属假设。目录清单包含 40 个来源档案、明确的未知字段与证据链接。ABI 指的是某一历史配方的声明，而非本版本已验证的执行。

`arm64-v8a` 偏好只适用于包含该 ABI 的那个 APK，并要求该目录下每一个库都具备一致的 64 位 ELF class 与 AArch64 machine。`abi_consistent` 记录这项检查；头部不一致会产生明确的阻塞项，无法据此选择 AArch64。这并不证明其余 loader 契约同样可用。请检查整个分包集合；资源分包并不证明架构正确。ARMv7 构建需要复查 softfp 边界。

## 3. 尊重各项限制

清单登记拒绝不安全或非规范的路径、冲突、符号链接、加密成员、超过 512 MiB 的成员，以及所有 APK 合计解压超过 8 GiB 的集合。2 MiB 的清单上限在解压前即行检查。重复的 base/split 身份与相互冲突的已知版本会被拒绝；未知版本保持为 `null`。嵌套的 APKM/APKS/XAPK 不会自动展开：请在一个私有区域内于本地准备好该集合。该工具不保证分包完整，也不负责格式转换。

TLS、TLSDESC、IFUNC、IRELATIVE、RELR、TEXTREL、RPATH/RUNPATH 以及打包的 Android 重定位，在选择 V5 nxloader 之前都需留意。清单登记还会标记 ARM/AArch64 之外的 ABI、非 `ET_DYN` 库以及不带 EABI5 的 ARM。这些标记提供的是静态分诊，而不是全面的重定位认证。经由 dlsym/JNI 的导入与缓冲区/线程契约仍需进一步调查。

## 4. 给 AI 一个明确的下一步

按档案选择参考实现，打开 `SOURCE-MAP.json` 并阅读与所识别边界对应的实现。在契约表中记录每个导入的签名、类型、归属、错误与测试。[综合示例](../../examples/zh/first-port/README.md)提供了一个可用于练习的原始输入，且不含商业数据。
