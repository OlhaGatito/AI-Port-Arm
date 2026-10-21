# Freedom Planet 2 — Unity 2018.4.36f1

[English](../fp2.en.md) · [Português](../fp2.md) · [中文](#)

[Unity 索引](../../README.zh-CN.md) · [NextOS 源卡](../../../ports/fp2-nextos/README.en.md)

选定笔记配置：IL2CPP/AArch64。本文件是关联公开源码的 `portando_unity` 研究资料的编辑性适配，描述历史成因与解法；收录既不重新验证该快照，也不为所有设备背书。

## 症状与分界

该构建使用 Vulkan SMOL-V/SPIR-V 程序。公开路线在安装阶段准备 GLES2/ESSL 1.00 程序，同时保留所需的游戏渲染逻辑。

## 待研究的解法

使用合格的精确翻译与外科式序列化文件写入。StencilDraw/StencilInvert 需要一次窄范围的 ARGB→RGBA swizzle/alpha 校正，并以 SHA 标识。

## 音频与输入

FMOD 经固件 SDL 投递 PCM；输入遵循游戏 Android 契约。检查标题、数字“2”、教学关、后续加载步骤、HUD 与退出。

## 限制与反证

这不是通用的 Vulkan 驱动。无法表达的变体有明确策略；不要用通用模板替换每个着色器。详见本版中的 Vulkan 深入专题。

适配时需为所有者载荷重算签名/偏移。在同一场景内比对修改前后，保留参考并记录新产物哈希。主机测试或可见标题不构成完整玩法的证明。

另请参阅 [FP2：从 Vulkan 到 GLES2](../../zh/FP2-VULKAN-GLES2.md)。

## 选定的公开源码

来源：[fp2-nextos](https://github.com/NextOs-Ports/fp2-nextos)，提交 `f8caab9d946c39041fa5cc25cb52621590f82adc`。[清单](../../../ports/fp2-nextos/SOURCE-MAP.json)；[Unity 版来源](../../SOURCE-MAP.json)。

- [README.md](../../../ports/fp2-nextos/upstream/README.md)
- [src/main.c](../../../ports/fp2-nextos/upstream/src/main.c)
- [src/jni.c](../../../ports/fp2-nextos/upstream/src/jni.c)
- [src/input.c](../../../ports/fp2-nextos/upstream/src/input.c)
- [src/egl.c](../../../ports/fp2-nextos/upstream/src/egl.c)
- [src/gles3.c](../../../ports/fp2-nextos/upstream/src/gles3.c)
- [src/unity6_shader.c](../../../ports/fp2-nextos/upstream/src/unity6_shader.c)

端口/集成作者：**NextOS**。保留第三方许可与署名；游戏数据不随源码附带。
