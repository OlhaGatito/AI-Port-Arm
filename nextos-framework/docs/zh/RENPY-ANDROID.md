[English](../en/RENPY-ANDROID.md) · [Português](../pt-BR/RENPY-ANDROID.md) · [中文](../../README.zh-CN.md)

# Ren'Py Android：Python、SDL 与 bootstrap

[Português](../pt-BR/RENPY-ANDROID.md)

这条路线从 Android 版本出发，研究资料集中已选定的公开 [Summertime Saga](../../ports/summertimesaga-nextos/README.en.md) 源码。目标是在 Linux ARM 上用一个专用 adapter 运行 Android 流程。请先查阅 [runtime 选择](ANDROID-RUNTIMES.md)。

## 1. 确认引擎与完整数据

```sh
python3 tools/inventory_apk.py /private/owner-input/game.apk --output work/renpy-inventory.json
python3 tools/find_reference.py --runtime renpy --abi arm64-v8a
```

确认 `librenpython.so`、ABI、依赖、嵌入式 Python、Ren'Py 版本、标准库、原生模块、脚本、归档与资源。要区分游戏版本、runtime 脚本版本与 ELF 身份。该快照包含 Ren'Py 8.5.3 的源码，但这并不为任何 `librenpython.so` 或任一 Ren'Py 游戏提供担保。

不要自动混用不同版本的脚本/字节码、扩展与解释器。配方必须从 APK 及其全部必需的分包开始。复制桌面版目录或用宿主 Python 替换解释器，都不能证明 Android 兼容性。

## 2. 阅读准确的参考对象

[SOURCE-MAP](../../ports/summertimesaga-nextos/SOURCE-MAP.json) 固定了 `bf5929bfdcdb03e17a27d053a91c36477b207032`。在着手适配之前，阅读[历史 README](../../ports/summertimesaga-nextos/upstream/README.md)、[许可声明](../../ports/summertimesaga-nextos/upstream/LICENSE)以及下列文件：

| 边界 | 选定的来源 |
| --- | --- |
| loader、SDL/JNI 与入口 | [src/main.c](../../ports/summertimesaga-nextos/upstream/src/main.c)、[jni_shim.c](../../ports/summertimesaga-nextos/upstream/src/jni_shim.c) |
| Python bootstrap | [main.py](../../ports/summertimesaga-nextos/upstream/main.py)、[android/apk.py](../../ports/summertimesaga-nextos/upstream/android/apk.py)、[jnius](../../ports/summertimesaga-nextos/upstream/jnius/__init__.py) |
| 资源与转换 | [prepare_summertime_data.py](../../ports/summertimesaga-nextos/upstream/tools/prepare_summertime_data.py)、[extractor.json](../../ports/summertimesaga-nextos/upstream/extractor.json) |
| 图形 provider 与音频 | [egl_shim.c](../../ports/summertimesaga-nextos/upstream/src/egl_shim.c)、[audio_backend_policy.c](../../ports/summertimesaga-nextos/upstream/src/audio_backend_policy.c) |

保留逐文件的许可声明，包括 Ren'Py 及其依赖。README 按目标设备记录了历史结果；它们既不能替代 profile 的限制，也不能批准新的构建。

## 3. 复现 Android bootstrap

选定的 `src/main.c` 以合适的导入项加载 Android ELF，准备 JavaVM/JNI，注册 SDL 集成，提供环境/尺寸，并通过 `nativeRunMain`/`SDL_main` 进入。`main.py` 驱动 Ren'Py bootstrap。在调用之前，先映射真实构建的构造函数、`JNI_OnLoad`、回调与线程。

Ren'Py Android 允许通过 Pyjnius 调用 Java 并访问 `PythonSDLActivity`。也要清点脚本在初始菜单之后发出的调用。[官方 Android/Pyjnius 文档](https://www.renpy.org/doc/html/android.html#pyjnius)。一个替换用的 `jnius` 模块必须实现真正需要的签名、返回对象与错误；一个接受任意方法的对象并不能证明兼容性。

## 4. 区分目录结构、runtime 与持久化

历史准备步骤会去除资源前缀、建立确定性索引并应用游戏专属的兼容模块。复用前先阅读这些规则：不要不加区分地剥离所有名称的前缀。在提交事务性目录之前，先检测转换后的名称冲突、无效路径、缺失的必需文件与不兼容的负载。

把固定的脚本/runtime 与存档、偏好设置及用户持久化分开测试。测试存档/重载、按需使用的回滚、文本/重音、姓名输入与更新，且不得丢失进度。`runtime-overrides/` 下的两个模块属于 Summertime Saga；它们不是适用于所有 Ren'Py 游戏的通用补丁。

## 5. 构建与诊断

使用 [ARM 编译指南](BUILD-ARM.md)与[公开 SDK](../../toolchains/sdk/zh/README.md) 构建 Linux loader。那份[历史通用构建](../../ports/summertimesaga-nextos/upstream/build_universal.sh)可用于清点源码、sysroot、库与被省略的依赖；在选定的这份克隆中它并不是一条保证可用的命令。检查每一个公开的 Linux ELF 是否满足 GLIBC ≤ 2.30，并使用固件 SDL。打包在所有者 Android ELF 中的 SDL 并不授权重新分发私有的 Linux SDL。

| 症状 | 下一步的证据 |
| --- | --- |
| 导入或字节码错误 | 解释器/模块版本、路径与输入文件；拒绝不兼容的版本组合 |
| 菜单能打开，场景失败 | 完整的资源、索引、延迟导入与真实的 Java/Pyjnius 调用 |
| 有声音但黑屏 | present 之前的上下文/surface、着色器、纹理与像素；不要采信黑屏视频 |
| 暂停后声音消失 | 已协商的后端、格式/队列与恢复；不要固定其他设备的配置 |
| 触摸/光标点不中目标 | 视口、坐标换算与 Ren'Py 的消费端；测试边缘与分辨率变化 |

## 6. 全新安装并记录限制

为完整的 Android 输入、包/版本/ABI 身份与关键负载哈希实现 [NXExtract](NXEXTRACT.md) 配方。接受兼容的重新打包，但不要把整个容器的 SHA 作为唯一条件。PT/EN 双语的 `INSTALLATION.md` 通过技术字段标识参考副本，并说明文件去向。

保留标准的提取界面与 NXSplash。按照[测试与交付](TESTING.md)记录真实的提取过程、最终哈希、画面、音频、输入、持久化与退出。宿主机上的 Python 测试并不能批准 Android 这条链路、Mali-450 渲染或其他设备。具体改动只留在 adapter 中；V5 保持冻结。

## AI mission

```text
Inventory only the supplied Ren'Py Android input. Pin Python, Ren'Py, ELF,
modules and assets; read Summertime Saga at its selected commit.
Map SDL/JNI/Python bootstrap and Pyjnius calls, including late calls.
Implement contracts and transactional preparation in a separate project.
Test errors, persistence and complete extraction; record physical limits.
Do not mix desktop runtimes or include game data in the publication.
```
