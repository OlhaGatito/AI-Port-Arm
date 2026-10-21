[English](../../../examples/first-port/README.en.md) · [Português](../../../examples/first-port/README.md) · [中文](README.md)

# 首个移植：NextOS 原创迷你游戏

本练习把一个 Android AArch64 访客接入真实的 V5 `nxloader`。方形、靶标与音效均由代码生成，无需任何商业游戏。CPU 执行与提取流程已测试；图形/音频/物理手柄仍未验证。

## 1. 准备并编译

请先构建[公共 SDK](../../../toolchains/sdk/README.md)。在合集根目录执行以下命令，使用一个新建的输出目录：

```sh
docker run --rm --network none --user "$(id -u):$(id -g)"   -v "$PWD:/src:ro" -v "$PWD/work:/src/work"   nextos-public-sdk:1 python3 examples/first-port/build.py   --output work/my-first-port
```

该脚本物化合集的 pin、把 `guest.c` 编译为 Android ARM64 freestanding 目标，并将 `adapter.c` 与静态 nxloader 及系统的 SDL2/GLES2 链接。它会审计 GLIBC、生成原创输入，并通过 nxgenerator 生成项目。若出错，请保留日志与产物。不要为了把重复出现的结果当作新结果而抹掉证据。

这不是一个可在 Android 上安装的 APK：`training-input.apk` 是一个练习容器，包含 manifest、原创库与 seed。访客不使用完整的 Bionic、C++ 或 Java 服务。对于普通的 Android 项目，请按 [ARM 编译指南](../../../docs/zh/BUILD-ARM.md) 使用 NDK。

## 2. 理解代码与顺序

| 文件 | 契约 |
| --- | --- |
| [guest.c](../../first-port/guest.c) | 构造函数、JNI_OnLoad、create/resume/step/render/audio/pause/save/destroy |
| [demo.h](../../first-port/demo.h) | 原创 ABI，且只保留真正用到的 JNI GetEnv/GetVersion 槽位 |
| [adapter.c](../../first-port/adapter.c) | 显式注册表、SDL2/GLES2、手柄、PCM 队列与持久化 |
| [prepare_seed.py](../../first-port/prepare_seed.py) | 事务型钩子，把 `seed=7` 转换为 `7` |
| [build.py](../../first-port/build.py) | 构建、字节标识与生成 manifest |
| [test_pipeline.py](../../first-port/test_pipeline.py) | 真实流程的正向与负向测试 |

顺序为：map → 重定位 → 导入 → 保护 → 构造函数 → JNI_OnLoad → create → resume → 帧/音频/输入 → pause → save → destroy。缺少必填导入会阻止构造函数。`GetEnv` 只接受所属线程与声明的版本；其他 JNI 函数均未实现。这里没有 JVM。

注册表导出了 `__android_log_write` 和 `__errno`，两者都有显式实现。渲染产出 RGBA；adapter 负责上传、用 GLES2 呈现，并在 present 前拒绝纯黑中心像素。该检查是为本练习的非黑画面选取的，并非通用检测器。音频为 48 kHz 单声道 S16 PCM。暂停/焦点与热插拔由 adapter 处理，但需要物理验证。

## 3. 测试安装与 CPU 执行

```sh
docker run --rm --network none --user "$(id -u):$(id -g)"   -v "$PWD:/src:ro" -v "$PWD/work:/src/work"   nextos-public-sdk:1 python3 examples/first-port/test_pipeline.py   work/my-first-port
```

预期结果：`PASS: 11 integrated checks`。该测试执行干净提取、钩子、输出校验、QEMU 下的 AArch64 执行、构造函数之前对缺失导入的拒绝、重新打包的接受，以及对错误的 package/payload/ABI 或缺失 payload 的拒绝。钩子失败会保留已提交的原有数据与标记。

测试在宿主机实验室中显式使用 `--ui none`。这不等于图形界面的认证，也不表示通过发布评审。请阅读 `pipeline-tests/RESULT.json` 与各独立日志。`COMPILED.json` 固定原生输入与哈希，当只有文档准备工作变动时避免重新编译。

## 4. 检查结果

```sh
python3 tools/inventory_apk.py work/my-first-port/training-input.apk   --output work/training-inventory.json
```

`generated/` 包含规范启动器、NXSplash、图形化 NXExtract，以及由生成器固定的运行时成员。访客与 seed 只存在于原创输入中。生成后不要修改该目录树。`runtime/` 是与 CPU 测试相关的独立开发安装，带有双语 INSTALLATION 与精确的输入标识。`project/` 保存完整 manifest 与配方。

生成的 adapter 仍标记为 scaffold/非发布，因为既未提升也未做物理测试。[教学实现契约](../../first-port/adapter-contract.json) 解释了现有代码。示例直接进入游戏玩法；schema 要求的 `menu` 段是保留声明，没有菜单测试。映射是静态的，示例不承诺运行期编辑 GPTK。

## 5. 转到真机与你自己的游戏

在明确授权真机的会话中，通过生成的启动器验证图形安装效果：五秒的 NXSplash、渲染器、操控、音频、存档/读档与退出。不要把 headless 安装当作图形提取的证据。D-pad/左摇杆/方向键移动方块；在本练习中 Start/Back/Escape 用于保存并退出。这些绑定并不是其他游戏的默认设置。

该示例还不是发布用 ZIP，也未通过 PortMaster/firmware 的门槛。要发布一个移植，请按 [测试与交付](../../../docs/zh/TESTING.md) 完成契约、包体原创文档、所需的 framework/runtime 以及最终字节的验证。保留 UI、pin 与冻结的源码。可在 `work/ports/` 或其他仓库中开发你自己的移植。
