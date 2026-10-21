[English](../en/GAMEMAKER-ANDROID.md) · [Português](../pt-BR/GAMEMAKER-ANDROID.md) · [中文](../../README.zh-CN.md)

# GameMaker Android 与 YoYo runner

[Português](../pt-BR/GAMEMAKER-ANDROID.md)

这条路线把 Android 构建适配到 Linux ARM，同时保留其 runner 与所有者提供的数据。请从 [runtime 选择](ANDROID-RUNTIMES.md)开始。参考对象是本资料集中已收录的公开 [Forager](../../ports/forager-nextos/README.en.md) 移植。

## 1. 清点 Android 构建

```sh
python3 tools/inventory_apk.py /private/owner-input/game.apk --output work/gamemaker-inventory.json
python3 tools/find_reference.py --runtime gamemaker
```

确认包名、游戏版本、ABI、`libyoyo.so`、C++ 依赖、数据、纹理页、音频以及 Java/JNI 扩展。runner 版本要单独记录。GameMaker 可以用 VM 或 YYC 编译 Android；在开始实现之前先确认所有者那一份的变体。[官方 Android 编译文档](https://gamemaker.io/en/help/articles/android-compiling-your-app)。

Forager 1.0.13 是参考对象的 Android ARMv7 profile。这个数字就是游戏版本。不要把它外推到其他 runner，也不要为另一个提供 `arm64-v8a` 的游戏选择 ARMv7。桌面版 `data.win` 并不是这条 Android 配方的替代输入。

## 2. 固定来源与许可

[SOURCE-MAP](../../ports/forager-nextos/SOURCE-MAP.json) 固定了 `89e5c7107889f6b19a705f006f9f492bb0eb80da`。请阅读：

- [引导与限制](../../ports/forager-nextos/upstream/README.md)。
- [runner 边界](../../ports/forager-nextos/upstream/source/overlay/gmloader/libyoyo.cpp) 与 [RunnerJNILib](../../ports/forager-nextos/upstream/source/overlay/gmloader/classes/RunnerJNILib.cpp)。
- [历史构建](../../ports/forager-nextos/upstream/source/build-release.sh)、[数据准备](../../ports/forager-nextos/upstream/tools/build_forager_port.py) 与 [NXExtract 配方](../../ports/forager-nextos/upstream/extractor.json)。

[NOTICE](../../ports/forager-nextos/upstream/NOTICE.md) 声明该 loader 派生自 gmloader-next，引用点为 `c2fca354df73761887c15f44a0b28ec823581cd5`，许可为 GPL-2.0-only。本资料集包含 GPL-3.0-only 的代码：必须明确这条边界，不要把该 loader 当成同许可而直接贴进 V5 组件。请保留声明，并在一个独立项目中依据[资料集许可说明](../../LICENSING.zh-CN.md)评估其组合关系。原始 runner 与资源均来自所有者。

## 3. 保留 runner 流程

在参考对象中，NXExtract 准备 `runtime/forager.port`；loader 在 `libyoyo.so` 之前加载 `libc++_shared.so`，解析导入项并保留初始化器、JNI 与原生循环。映射新构建的确切顺序：Java 类、RunnerJNILib 调用、surface、尺寸、语言、音频、控制器、暂停与关闭。

`.port` 文件是历史 adapter 的一种选择，并非 GameMaker 或 V5 的通用 API。不要通过调用一个房间、游戏函数或中间入口来绕过初始化。可选扩展需要明确的缺失契约和真实的报错；假装某个必需服务成功只会掩盖后续故障。

## 4. 构建 adapter 并验证 ABI

使用 [ARM 编译指南](BUILD-ARM.md)在自己的工程中生成 Linux 可执行文件。执行前先阅读历史构建：恢复出来的源码、镜像/sysroot、库与被省略的文件都需要固定点。它过去使用 nxbootstrap/NXExtract 的事实并不能把参考对象迁移到 V5。不要为了写指南而重新构建已批准的二进制文件。

对于 ARMv7，请检查 Android softfp/Linux ARMHF 下的 ARM/Thumb、调用约定、双精度/单精度、回调与结构体。翻译必须覆盖返回路径，而不只是导入项。C++ 需要所有权、异常和标准库版本检查；不要在没有契约的情况下跨 ABI 传递对象。未知导入项必须产生可验证的报错，绝不能只是一张通用的 `return 0` 表。

## 5. 诊断游戏边界

| 症状 | 测量方式与否证 |
| --- | --- |
| 第一帧之前就失败 | 库顺序、构造函数、JNI 与首次未解析的调用；拒绝错误的库/ABI |
| 精灵缺失或 surface 为黑 | present 之前的真实着色器、纹理、alpha、FBO 与像素；没有画面时音频必须失败 |
| 部分按钮无响应 | 遮罩、按下/抬起与 runner 实际消费的设备；重连时不重复事件 |
| 重启后语言/存档消失 | 使用独立提取数据之外的持久路径；保存、关闭与重载时不进行破坏性提取 |
| 暂停后音频故障 | 格式、队列、回调与恢复；在目标设备上确认确实被消费并有声音 |

使用系统 SDL 与真实的 GLES provider。Forager 的扩展、纹理或输入调整在另一个移植中得到验证之前仍然只适用于它本身。像素、存档与音频的判定标准见[测试与交付](TESTING.md)。

## 6. 准备数据与交付

参考对象生成了一个包含 24 个成员的确定性 STORE 归档。这些成员及其哈希属于那个构建；新的移植需要自己的清单。[NXExtract](NXEXTRACT.md) 必须校验 Android 身份与关键负载，准备完整的目录结构，并在全新安装时执行钩子。参考 APK 的 SHA 可用于标识本次测试，但不作为容器兼容性的唯一门槛。

包含 PT/EN 双语 `INSTALLATION.md`，在技术字段中标识输入来源，并把存档放在数据封印之外。审计每一个公开的 Linux ELF，确保 GLIBC ≤ 2.30；保留 V5 launcher、图形界面与五秒 NXSplash。Forager 的历史证据并不等于批准另一个 runner、设备或重建后的字节。

## AI mission

```text
Analyze only the supplied Android build. Identify the GameMaker runner,
VM/YYC, ABI, assets and extensions. Read pinned Forager sources and licensing.
Document native flow and implement a separate adapter respecting V5.
Prove ABI, graphics, audio, input and persistence with targeted checks.
Prepare a complete NXExtract recipe and record incompatible dependencies.
Import no commercial data or historical loader as a V5 default.
```
