[English](../en/HAXE-LIME-ANDROID.md) · [Português](../pt-BR/HAXE-LIME-ANDROID.md) · [中文](../../README.zh-CN.md)

# Haxe、hxcpp 与 Lime 在 Android 上的移植

[Português](../pt-BR/HAXE-LIME-ANDROID.md)

这条路线覆盖为 Android 构建、并通过 Linux ARM adapter 运行的库。所选的 [Tightrope Theatre](../../ports/tightrope-nextos/README.en.md) 源码可以确定这一技术族并记录其限制。同时请查阅 [runtime 选择](ANDROID-RUNTIMES.md)。

## 1. 认清真实的组成

```sh
python3 tools/inventory_apk.py /private/owner-input/game.apk --output work/haxe-inventory.json
python3 tools/find_reference.py --runtime haxe-lime --abi arm64-v8a
```

在同一个 ABI 中查找 `liblime.so` 与 `libApplicationMain.so`，以及导入项、入口点与 Java bootstrap。按名称做的检测只是一种假设。只有在有证据时才确认 hxcpp 并记录 Haxe、hxcpp、Lime 与 SDL 的版本；`unknown` 也比编造的版本更好。OpenFL 可能是应用的一部分，但光有 Lime 并不能证明这一点。这条路线不是给 HashLink、Neko 或桌面可执行文件的配方。

[Haxe/C++ 手册](https://haxe.org/manual/target-cpp-getting-started.html)描述了 C++ 目标与 hxcpp。这有助于区分生成的原生代码与初始化它的 Java/Android runtime；它并不能保证与另一个库版本可以互换。

## 2. 尊重参考对象的状态

[SOURCE-MAP](../../ports/tightrope-nextos/SOURCE-MAP.json) 固定了 `75eb5da65994995e38057c758440faafd63d87e8`。[README](../../ports/tightrope-nextos/upstream/README.md) 标明为 **1.0.5-test.1**，是一个 ROCKNIX 输入候选，其精确的物理验收仍待完成。其中记录了 1.0.4 时期之前的证据，这并不会自动批准所选的这些字节。

把这份快照用于清点与诊断。处于测试中的行为属于反面参考，而不是可以照抄的已批准实现。代码复用要求先恢复并核对同一边界上已批准的公开参考，确认其 commit、哈希与证据一致。[来源指南](PINS-AND-SOURCES.md)说明了如何在不替换快照的情况下进行恢复。请阅读 [NOTICE](../../ports/tightrope-nextos/upstream/NOTICE.md) 与 [LICENSE](../../ports/tightrope-nextos/upstream/LICENSE)。

## 3. 映射回调与线程

选定的 [src/main.c](../../ports/tightrope-nextos/upstream/src/main.c) 记录了所观察到的 Android 链路：Application/Activity；加载 Lime 与 ApplicationMain；为 SDL、音频与控制器设置 JNI；创建/调整 surface 尺寸；在 SDLMain 线程上通过 `nativeRunMain` 进入 `hxcpp_main`。UI 线程单独投递事件。

分析新构建时，把这条链路当作一个问题来用：它实际需要哪些回调、签名、参数与线程？在必需阶段完成之前不要直接调用 `hxcpp_main`，不要强制进入某个场景/关卡，也不要另外编造一条游戏启动路径。源码清单并不授权照搬该构建特有的转换方式或受保护构建的机制。

## 4. 验证 TLS、GC 与 SDL 边界

[hxcpp 线程/栈手册](https://haxe.org/manual/target-cpp-ThreadsAndStacks.html)描述了线程、栈与垃圾回收之间的关系。针对真实版本检查线程注册、根集、阻塞与恢复。一个始终返回 `NULL` 的 TLS 键或一个空的互斥量都可能在运行初始化之后破坏 runtime。

选定的 [bionic.c](../../ports/tightrope-nextos/upstream/src/bionic.c) 与 [pthread_bridge.c](../../ports/tightrope-nextos/upstream/src/pthread_bridge.c) 有助于定位这些契约；[sdl_java.c](../../ports/tightrope-nextos/upstream/src/sdl_java.c) 展示了 Java/SDL 边界。要区分包含在 Android 客体中的 SDL 与固件的 Linux SDL。结构体、事件和回调不能只因为名称相同就跨越版本/ABI 使用。

明确窗口、上下文、surface 与 present 的归属。保留引擎所需的流程与线程；不要为了掩盖缺失的回调而另建一个窗口。不受支持的导入项必须产生可验证的诊断结论。

## 5. 准备构建与定向诊断

使用 [ARM 编译指南](BUILD-ARM.md)、[shims](SHIMS.md)与一个独立工程来实现经过验证的契约。执行历史 README 中的任何命令之前，先核对清单中已包含/省略的文件；被引用的脚本可能并不属于这份选集。固定 toolchain 与恢复出来的源码。在存在 AArch64 时优先选择，并对每一个公开的 Linux ELF 审计 GLIBC ≤ 2.30。

| 故障 | 用于区分原因的测试 |
| --- | --- |
| 分配/加载场景后卡死 | 每线程 TLS、GC、条件/互斥量以及栈的归属；压测工作线程与关闭流程 |
| surface 就绪但无画面 | 回调顺序、尺寸、当前上下文与 present 之前的像素 |
| 事件已记录但无响应 | Java/SDL 回调与 Lime 消费端；按下/抬起、菜单与玩法分开测试 |
| 恢复后音频故障 | OpenSL/SDL 边界、格式、队列与回调线程；测量消费情况并实际试听 |
| 重启后进度丢失 | 持久目录、flush、存档/重载与关闭，且不残留进程 |

## 6. 带证据的提取与交付

[选定的配方](../../ports/tightrope-nextos/upstream/extractor.json)标识了那个 profile 所需的库与资源。对于另一个移植，要清点完整的 Android 输入与关键负载。[NXExtract](NXEXTRACT.md) 必须用真实的钩子准备完整的副本，接受兼容的重新打包并保留存档。不要把另一个游戏准备好的数据当作成功样例。

包含双语的 `INSTALLATION.md`，保留 UI/NXSplash 与 V5 pins，并遵循[测试与交付](TESTING.md)。宿主机上的 TLS/队列测试并不能批准这个游戏或物理控制。不要把已有移植迁移过来，也不要把 Tightrope 待定的调整并入 V5。

## AI mission

```text
Analyze only the Android build. Confirm hxcpp/Lime, ABI and bootstrap.
Read pinned Tightrope sources for diagnostics, keeping pending acceptance
explicit; do not use the candidate as an approved implementation.
Map threads, TLS/GC, SDL/JNI, surface, audio and the input consumer.
Implement in a separate project with approved public references and pins.
Prepare complete extraction and targeted tests without changing frozen V5.
```
