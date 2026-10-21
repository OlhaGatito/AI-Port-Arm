[English](VALIDATION.en.md) · [Português](VALIDATION.md) · [中文](VALIDATION.zh-CN.md)

# 双语版本的验证

**历史记录：** 以下数据描述的是排除 iOS 移植之前的这一阶段。当前收录为 40 个代码仓库、44 个有源码来源的标题和两个社区条目；参见[当前目录](../catalog/zh/README.md)。

日期：2026 年 9 月 10 日。范围：文档、目录以及选定的 Unity 导入。

- 原始收录集：7.715 个来源/辅助文件哈希，41 个仓库/45 个标题，以及 8 个已 pin 的 V5 辅助 ELF。
- 扩充后的目录：47 个 NextOS 标题，含两份没有代码快照的社区条目。
- shims 示例：上一轮准备的宿主机 C99 构建与 CTest 1/1 通过；源码与二进制均保留，未做文档性重建。
- 当前完整性：核对了 7.725 个哈希，含 10 个通用文件/Unity fixture；未改动任何 V5 运行时或上游来源。
- 文档：86 组 PT/EN 对照、1.344 个本地链接、32 个语法有效的 shell 代码块；两种语言下的可执行示例一致。结构性核对不能替代对译文的语义评审。
- Unity 工具：文本图集规划的 15 个测试与触摸校验器的 20 个测试通过；合成 CLI 示例以状态 0 执行，且未声称提供物理证据。
- nxgenerator、nxrelease 与 NXExtract recipe-check 的 CLI 界面通过 `--help` 核对；未启动任何生成、安装或发布测试。
- 隐私/包识别与 `git diff --check`：PASS。15 个 Unity 用例均绑定到提交以及公开仓库中被选中的文件，其可见性已在本轮评审中查证。
- 本轮未执行：ARM/NDK/Godot 构建、商业游戏、真机解包、物理测试、各移植重建或完整的 V5 历史套件。

来源完整性、文档一致性与合成测试都不能证明游戏可玩，也不能证明每个文件的许可。参见[待办项](REVIEW.zh-CN.md)。

后续首次移植的模拟演练在一份干净导出中运行了宿主示例、生成了骨架并核对了配方/pins。结果与边界见[该报告](ONBOARDING-SIMULATION.zh-CN.md)；上述数字描述的是更早的那轮编辑评审。

公开 SDK 与后续练习的实施有各自的成果，见 [ONBOARDING-UPDATE.zh-CN.md](ONBOARDING-UPDATE.zh-CN.md)，含 ARM 构建、QEMU 下的 Android guest、干净解包、着色器与 Godot 逻辑。上述排除项仅描述这一轮更早的编辑评审。
