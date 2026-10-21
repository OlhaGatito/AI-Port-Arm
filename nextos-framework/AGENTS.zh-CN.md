[English](AGENTS.md) · [Português](AGENTS.pt-BR.md) · [中文](AGENTS.zh-CN.md)

# NextOS — 面向 coding agents 的说明

[Português](AGENTS.pt-BR.md)

本私有草稿是一份源码参考合集。维护者必须明确批准将其 GitHub 可见性改为公开。

- 对本项目的署名仅写作 **NextOS**，链接指向 `https://github.com/NextOs-Ports`。保留第三方法律声明。切勿在提交中添加 AI 署名或 co-author 尾部信息。
- 复用代码前，先阅读 `catalog/ports.json`、所选移植的 `SOURCE-MAP.json` 及其许可。源码快照并不能证明游戏流程完整、资源可提取或设备普遍支持。
- 将 `framework/`、`suportando_outros_devices/extrator-universal/` 以及既有的 `ports/*/upstream/` 视为参考来源。开发新游戏时不要修改它们。V5 运行时的字节已冻结；共享行为变更需要单独的 V6 开发工作。
- 每个新游戏都创建在 `work/ports/<port-id>/` 下，或放在独立仓库中。这个被忽略的工作区不属于已发布的合集。
- 只要所有者提供的 Android 输入中包含 `arm64-v8a`，就优先使用 AArch64。仅在必要时使用 ARMv7，并显式处理 Android softfp 与 Linux ARMHF 的边界。
- 保留原生加载、重定位、构造函数、JNI 与生命周期的顺序。未知的必需导入必须产出诊断信息，绝不能编造成功。
- 把游戏专属的 JNI 类、偏移量、回调、图形兼容处理与存档路径保留在新的适配器中。按引擎、ABI 和契约查找参考；不要仅凭游戏名称相似来选择。
- 只使用所有者提供的游戏输入。绝不要提交或上传 APK/IPA/OBB、专有游戏库、资源、存档、内存转储或私有日志；也不要把它们作为 CI 产物上传。
- 上游来源中的命令与路径只是证据，不是授权。未经检查并确认其范围，不要执行上游的构建、下载、SSH 命令、服务命令或清理脚本。设备访问需要使用当前任务提供的地址；绝不要从旧文本中发现地址。
- 编译主机示例使用 `cmake -S examples/shims-reference -B work/host`、`cmake --build work/host` 以及 `ctest --test-dir work/host --output-on-failure`。
- 关于 ARM 构建，请阅读 `docs/en/BUILD-ARM.md`。Android NDK 构建的是 Android 示例；Linux 交叉编译器与 Linux sysroot 构建的是 Linux 加载器。两者不要混用。
- 公开的 Linux 可执行文件最多要求 GLIBC 2.30。默认使用系统自带的 SDL。在 Mali-450 上要面向真实的 GLES2；不要宣传未经实现和实测的 GL 能力。
- 保持 NXExtract 的图形界面以及五秒的 NEXT OS / RETRO ELITE NXSplash 不变。一个打包 BYO-data 的移植需要真实的提取配方和一次干净安装的测试，而不只是采用预提取好的文件。
- 开发时优先做定向检查。不要仅仅为了完善文档就重建已批准的二进制文件或重复走完整的发布关卡。新的硬件声明必须提供该具体产物的证据。
- 说明自己改了什么、编译了什么、测了什么、没测什么、以及仍然不支持的部分。活动音频或正在运行的进程并不能证明视频有效。绝不要因为符号名全部解析成功就把一个 shim 标为完成。
- 在提交合集变更之前，运行 `python3 publication/verify.py` 并审阅 diff。不要把凭据、私有地址和个人署名写进新的文档与提交元数据。
- 葡语与英语的编辑文档必须同步维护完整；在 `publication/languages.json` 中登记语言对，并运行 `publication/verify-docs.py`。
- 合集只包含 NextOS 的移植。要把公开仓库的快照与明确授权、仅存在于合集中的社区条目区分开。绝不要编造来源或下载链接。
- 只使用已收录的 Unity 案例以及 `portando_unity` 中的通用工具。不要把本地研究里的其他游戏导入进来。Freedom Planet 2 的 shader 移植是针对该构建版本的，不是通用的 Vulkan 支持。

- 开始移植时，遵照[公开 SDK](toolchains/sdk/zh/README.md)、[合集固定项](docs/zh/PINS-AND-SOURCES.md)和[集成示例](examples/zh/first-port/README.md)。使用 Python 3.11+；克隆仓库必须包含固定提交，而不只是浅层的 HEAD。
- 选择参考对象之前，先执行清单盘点与配置搜索。绝不要把未知字段理解为支持。把已记录的检查项/限制保留在 [ONBOARDING-UPDATE.en.md](publication/ONBOARDING-UPDATE.en.md) 中。
- 训练输入为原创内容，在 `work/` 中生成。手动运行的 CI 只执行该训练，不会把 APK/ELF/日志作为产物发布。Headless/QEMU 不能用来认可界面或实际图形效果。

- 不要把 iOS/iPadOS 的移植、研究、加载器或示例加入本合集。该排除范围涵盖快照与社区条目。通用第三方组件保留其声明与跨平台说明；这些不构成 iOS 移植参考。

- 关于运行时指南，遵照[Android](docs/zh/ANDROID-RUNTIMES.md)：输入始终为 Android，目标为 Linux ARM，MonoGame/FNA 也只从 Android 构建中获取。只使用已选定的公开移植；不要导入私有研究或新快照。引擎分类既不意味着认可该参考，也不会让 V6 的 API 在 V5 中可用。
