[English](ONBOARDING-SIMULATION.en.md) · [Português](ONBOARDING-SIMULATION.md) · [中文](ONBOARDING-SIMULATION.zh-CN.md)

# 首个移植的模拟演练

**历史记录：** 以下数据描述的是排除 iOS 移植之前的这一阶段。当前收录为 40 个代码仓库、44 个有源码来源的标题和两个社区条目；参见[当前目录](../catalog/zh/README.md)。

**历史记录：** 以下缺失项是在上述提交中观察到的。[后续更新](ONBOARDING-UPDATE.zh-CN.md)记录已实现的 SDK、示例与工具、它们的测试以及当前的待办事项。

2026 年 9 月 10 日对提交 `2e7234265f8602d8a31117f65a23874769e13e59` 的评估。场景：一个人或 AI 只拿到本收录集 plus 一份自己的 Android 游戏副本，完全不了解 NextOS 内部环境。

**结论：本收录集可以开始着手调研并编译宿主示例。从 clone 到可演示的 Android 移植，仍缺一条完整可复现的路径。** 这些材料作为参考有价值，但 AI 仍需自行发现并实现各个环节之间的衔接。“完整指南”一词当时过度自信地描述了编辑目标。

## 1. 到底模拟了什么

用 `git archive` 在 `work/` 内生成的一份干净版本文件导出，来测试初始命令。它不含原始工作区中的构建产物或被忽略的文件。使用的是现成的 Linux 计算机，GCC 16.2.1；不是新装的容器，也不是 ARM 机器。pins 测试用的是本收录集仓库自身的 Git 对象。

| 环节 | 观察到的结果 | 覆盖范围 |
| --- | --- | --- |
| `publication/verify.py` | PASS：7.725 个哈希 | 收录集完整性 |
| `examples/shims-reference` 的 CMake/构建/CTest | PASS：1/1 | 宿主 C 示例 |
| 用 `nxproject-aarch64.example.json` 生成 | 成功生成目录 | 项目骨架 |
| 用 `recipe-minimal.json` 做 `recipe-check` | OK | 配方结构 |
| 用 V5 历史提交 pin `nxloader` | 失败：`Git object query failed: fatal: Needed a single revision` | 该收录集中不存在此历史对象 |
| 用上述收录集提交 pin `nxloader` | Create/materialize/verify 均 PASS | 单组件的一种组合，无构建也无发布 |

生成的 adapter 含 `status: unimplemented_nonrelease`、`release_ready: false`、空 lifecycle，没有任何 JNI 方法、音频回调或输入动作。所声明的游戏可执行文件并未创建。这符合生成器的既定契约；缺的是接下来那个把这些骨架落实的练习。

未执行任何商业 APK、游戏、真机、ARM/NDK 构建、完整解包、历史测试套件或发布打包。未改动任何 V5 运行时或移植快照。

## 2. 优先级阻塞项

| 优先级 | 初学者卡在哪里 | 所需证据与材料 |
| --- | --- | --- |
| P0 | 获得可复现的 ARM 环境 | [编译 ARM](../docs/zh/BUILD-ARM.md) 需要外部编译器/sysroot，并使用了 `/opt/...` 形式的示例路径。缺一份公开配方，须写明版本、来源、哈希、C/C++/SDL/EGL 依赖及低 glibc 测试。 |
| P0 | 用这个新仓库固定组件 | [pins 助手](../framework/nxgenerator/framework_pin.py)要求本地 Git 对象。V5 原始 SHA 可标识来源，但不存在于新历史中。缺的是从导出提交出发完整记录并验证组合过程，同时保留原始来源信息。 |
| P0 | 从 C 示例走到 Android guest | [shims-reference](../examples/zh/shims-reference/README.md) 不加载 Android ELF。NDK 练习只编译了一个求和函数，没有展示 Linux 加载器如何加载、解析导入并调用它。缺一个集成的演示项目。 |
| P0 | 用 NXExtract 安装示例数据 | 最小配方能通过解析器，但没有配套的原创输入复现教程，以及解包、钩子、校验与回退。缺的是把该配方接到同一个集成示例上。 |
| P1 | 构建所选的参考对象 | 收录集不是每个移植的完整 checkout。[Chrono 的构建脚本](../ports/chrono-nextos/upstream/build_universal.sh)要求本地镜像 `playfetch-builder:buster`、某个 NextOS 构建的头文件以及 `fonts/NotoSans-Regular.ttf`，收录集中没有。[FP2 的历史 README](../ports/fp2-nextos/upstream/README.md) 要求执行 `build_universal.sh`，同样缺失。缺一份明确的依赖/被省略文件清单，以及如何获取每个被允许的公开来源。 |

不应重建或移动 V5 历史标签来修复 pin。用导出提交获得的成功经验证明在当前助手下存在可行路径，但本研究中只 materialize 了 `nxloader`。测试组件有遗漏；不要声称导出的组合与完整历史树完全相同。

## 3. 仍然缺失的指南与示例

1. **可执行的 Android 清单。** AI 指南列出了 APK 中的库，但把如何解读二进制 manifest 留给读者自行决定。应补上已固定版本的命令/工具，用于 package ID、版本、splits、ABI、引擎、导入、符号类型与重定位。生成私有报告以及一份不含个人路径的可发布输出。尽早说明 [nxloader V5](../framework/nxloader/README.md) 不支持 TLS/TLSDESC、RELR、IFUNC 与 packed relocations，避免写完 adapter 之后才发现这处不兼容。
2. **按契约实现 shims 集成。** 示例注册了两个符号（`__errno`、`__android_log_write`）和一个教学属性，覆盖不了大多数游戏。应展示与真实 registry 的衔接、一个缺失的必需导入、所有权与错误路径。补上资源/文件读取、Bionic 结构体、线程、JNI 与线程引用、生命周期、音频与输入的独立练习。只在现有实现的契约确实吻合时复用。
3. **每个家族的入门项目。** Unity、Mono Android、Godot 与 Cocos2d-x 已有调研指南。仍缺带完整文件、依赖、构建命令与预期输出的小型原创练习。Mono 需要区分托管 Linux 宿主与 Android bootstrap；Godot 需要区分导出自己的项目与适配作者提供的 Android 构建。任何一个都不该声称照抄参考即可。
4. **可按 profile 检索的目录。** 41 条来源记录都有来源/哈希，但没有一条具备 `engine`、`abi`、`renderer`、`build_status` 或 `tested_devices` 等结构化字段。部分数据在社区条目和 15 个 Unity 用例中。整合已证实的数据，其余用 `unknown`，让 AI 按契约而非按标题来选一个基底。
5. **带演示结果的诊断。** 已有常见问题表格。补上好坏输出的示例：重定位被拒、导入缺失、ABI 错误、JNI 未注册、上下文无效、有声音无图像、存档与退出。每个用例都需指明下一步测量项与成功条件。
6. **示例的完整交付配方。** 提供 `nxproject.json`、已实现的契约、NXExtract 配方、PT/EN 双语 `INSTALLATION.md`，以及协调最终测试的命令。物理验证始终因目标而异；合成项目不能证明商业游戏可用。

教学示例与文档可以放在冻结树之外。若某处缺失要求修改 V5 的共享代码或行为，那属于独立的 V6 开发，而不是基线上的热修。

## 4. 针对 Freedom Planet 2 的专门说明

[Vulkan → GLES2 研究](../portando_unity/pt-BR/FP2-VULKAN-GLES2.md)说明了转换方法与限制。要让它作为课程可复现，还缺一个与商业数据独立的实验室：

- 演示如何获取并构建[转换器](../ports/fp2-nextos/upstream/tools/build_exact_shader_tool.sh)的依赖，遵守脚本已要求的 SPIRV-Cross 提交、SPIRV-Tools 版本与 SMOL-V 哈希。这些并非全部由收录集提供。
- 用简单的原创着色器练习 SMOL-V/SPIR-V → ESSL 1.00，涵盖 bindings、alpha，以及拒绝一个不兼容操作，附命令与预期结果。
- 把宿主机翻译测试与 GLES2 编译/链接、真机出图分开。一个原创样例不能证明整个 FP2 构建的转换有效。
- 说明如何取回被排除在收录集之外的公开构建文件，以及如何在作者自己的副本上套用配方，同时不分发商业着色器或数据。

该实验室应当教会转换方法，而不是把 FP2 的偏移量、变体计数或特例当作其他游戏的规则照搬。

## 5. 建议的实施顺序

1. 确定公开的 ARM 环境与收录集自身的 pins 流程。
2. 做一个**原创的移植训练用小游戏**，图形与声音由代码生成：Android AArch64 guest、Linux 加载器、少量显式 shims、JNI/lifecycle、控制、存档与退出。发布全部原创源码。
3. 从零教它的 NXExtract 安装与启动器生成，保留规范 UI 与 NXSplash。
4. 把该项目可复现的宿主与交叉测试自动化，并把物理验证单独记录在文档中。ARMv7 作为后续练习列入，须明确 softfp 边界。
5. 扩展到引擎示例、FP2 着色器实验室与目录覆盖矩阵。

完成标准：某个独立环境下的人照 PT 或 EN 文档一路走通，能编译、准备原创输入、生成项目，并在不查阅私有文件的前提下定位任何缺失依赖。每一步都有可验证的结果与明确的边界。在评审进行期间，收录集保持私有。
