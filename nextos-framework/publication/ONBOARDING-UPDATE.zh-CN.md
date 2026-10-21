[English](ONBOARDING-UPDATE.en.md) · [Português](ONBOARDING-UPDATE.md) · [中文](ONBOARDING-UPDATE.zh-CN.md)

# 首个移植路径的更新

**历史记录：** 以下数据描述的是排除 iOS 移植之前的这一阶段。当前收录为 40 个代码仓库、44 个有源码来源的标题和两个社区条目；参见[当前目录](../catalog/zh/README.md)。

日期：2026 年 9 月 10 日。本次更新实现了[上一次模拟](ONBOARDING-SIMULATION.zh-CN.md)指出的训练路径。仓库保持私有；未做任何游戏发布或可见性变更。

## 新增内容

- [公开 ARM SDK](../toolchains/sdk/zh/README.md)，基于已固定的公开源码构建，含 Linux 编译器、开发用 SDL2/GLES2、Clang 与 QEMU。
- [集成式首个移植](../examples/zh/first-port/README.md)：原创小游戏，Android AArch64 guest、nxloader V5、两个 shims、JNI/lifecycle、GLES2 渲染器、PCM、控制与存档。NXExtract 配方从零准备你自己的输入。
- [pins 与来源恢复](../docs/zh/PINS-AND-SOURCES.md)：助手所允许的 15 个组件的组合过程、保留历史来源，以及工具所需契约的单独导出。
- [Android 清单](../docs/zh/ANDROID-INVENTORY.md)、[41 个可检索 profile](../catalog/profiles.json) 与[带反证的诊断指南](../docs/zh/DIAGNOSTIC-LAB.md)。
- [Mono、Godot、Unity 与 Cocos2d-x 练习](../examples/zh/engines/README.md)，说明每个家族的边界，以及 [SMOL-V/SPIR-V → ESSL 1.00 实验室](../examples/zh/shader-lab/README.md)，不含商业着色器。
- 面向收录集与该原创示例的[手动 CI 工作流](../.github/workflows/onboarding.yml)。不发布任何产物，也不运行商业游戏；本次评审期间尚未在 GitHub 上执行。

所有新指南均有 PT/EN 版本。未修改任何 V5 运行时、NXExtract 或上游快照的冻结文件。这 47 个标题仍然是目录中的参考对象，不是 47 个已测试的移植。

## 已执行的验证

| 环节 | 结果 | 边界 |
| --- | --- | --- |
| 公开 SDK 构建 | PASS | 宿主为 linux/amd64 Docker；依赖均为公开件 |
| AArch64 / ARMv7 交叉 C 示例 | PASS，GLIBC 2.17 / 2.4 | 构建与审计；无 Android ARMv7 物理验证 |
| 收录集 pins | 15 个组件的 Create/materialize/verify 均 PASS | 导出收录集；被省略的历史测试继续省略 |
| 首个移植 | 11 项集成验证 PASS | 宿主机解包与 QEMU 下的 AArch64 CPU 运行 |
| 清单/目录工具 | 10 个测试 PASS | 含 ELF 长导入、标识、路径与符号类型 |
| 着色器实验室 | 5 项验证 PASS | 宿主机翻译/校验；无物理 GLES2 驱动 |
| Mono | P/Invoke、布局、回调与错误 PASS | 托管 Linux 宿主；非 Mono Android bootstrap |
| Godot 3.5.3 | headless 逻辑 PASS | 移动/拾取；无渲染或 ARM 导出 |
| Unity / Cocos2d-x | 已加入练习源码 | Unity 编辑器与 Cocos 集成尚未编译 |

这 11 项集成验证覆盖干净安装、真实钩子、已核对的输出、guest 运行、构造函数前导入缺失、兼容重打包、package/payload/ABI 不兼容、payload 缺失与钩子回退。流水线把日志与结果记入一个新区域。该测试在宿主机上显式关闭了 UI；它不替代规范图形安装。

这些步骤均在开发过程中验证过。对 manifest/生成的修正保留了已编译的本地字节；没有为了更新文档而重新构建可执行文件。此处描述的任何测试都不能证明真机上的画面、声音或控制可用。

## 开发产物的标识

| 产物 | 标识 |
| --- | --- |
| 本地 SDK 镜像 | `sha256:041f653673aab41aa0c53adc557f31a295a8ac32585357ebfea926383b831094` |
| 导出组合的 pin | `f411e522a861e92486fb307dcfb87d0facbad8c0fde1ebf65c491ffe8cc41832` |
| 教学用 AArch64 可执行文件 | `a6c9142a393cd4d05624840cdc92f186f9354fdc4958e7432bdb6ed3de8aca62` |
| 原创 Android AArch64 guest | `ddab9a2004d1b51f8d7cb86e6e9a1c57f5f643badebf926bfd011a929a5fd35a` |
| 训练用原创输入 | `53b00ecaeadf32058f40848874751efc776aa714fb504ead931085102138cf63` |

这些哈希标识的是已记录的那次本地执行。配方会固定其源码/依赖，但重建出的镜像可能因构建时间戳而有不同的 digest。请记录你自己的结果；不要把这里的哈希拿去声称验证过你自己的字节。构建、输入与日志都保留在被忽略的 `work/` 区域。

## 仍然开放的问题

生成的 launcher/NXExtract/NXSplash 已准备就绪，但本次任务未在真机上做物理测试。生成适配器仍然是 scaffold/非发布状态。图形解包、像素、音频、输入、持久化与退出的验证，必须在明确授权的设备上、在任何移植交付之前完成。

所取回的公开 SMOL-V 与 FP2 所要求的哈希一致。FP2 的历史 SPIRV-Cross 提交在本次评审中无法从 Khronos 上游取得；其确切来源仍然待办。该实验室使用的是另一个已声明的公开 pin，不能证明已批准的转换器可逐字节重建。

Unity 与 Cocos 需要相应的编辑器/引擎构建；历史参考仍可能需要额外的外部文件与各自的适配。不存在“shims 覆盖大多数游戏”的说法。许可、编辑评审与公开授权仍在 [REVIEW.zh-CN.md](REVIEW.zh-CN.md) 中。
