# Merchant of the Skies — Unity 6000.4.2f1

[English](../merchantskies.en.md) · [Português](../merchantskies.md) · [中文](#)

[Unity 索引](../../README.zh-CN.md) · [NextOS 源卡](../../../ports/merchantskies-nextos/README.en.md)

选定笔记配置：IL2CPP/AArch64。本文件是关联公开源码的 `portando_unity` 研究资料的编辑性适配，描述历史成因与解法；收录既不重新验证该快照，也不为所有设备背书。

## 症状与分界

TextMeshPro 字形呈方块，暴露出一个通道契约：R8 转为 LUMINANCE 后产生了恒定 alpha。

## 待研究的解法

在已证实的 GLES2 路径中，LUMINANCE_ALPHA 表示必须保留 r/a 读取。在物理 GLES3 上，R8 存储、RED 上传与 swizzle 必须保持一致；不要只改一侧。

## 音频与输入

缺失的 libaaudio 必须按“不可用”失败，以免选中错误的混音器通路。Rewired 使用已归一化的手柄输入；A/B 动作取决于实测契约。

## 限制与反证

不要仅因相似就照抄键码表或音频回退方案。当 nativeDone 未返回时，要在关闭期限前保留暂停/存档。

适配时需为所有者载荷重算签名/偏移。在同一场景内比对修改前后，保留参考并记录新产物哈希。主机测试或可见标题不构成完整玩法的证明。

## 选定的公开源码

来源：[merchantskies-nextos](https://github.com/NextOs-Ports/merchantskies-nextos)，提交 `213a3d9875158164bb532ef031651a052ab8360e`。[清单](../../../ports/merchantskies-nextos/SOURCE-MAP.json)；[Unity 版来源](../../SOURCE-MAP.json)。

- [README.md](../../../ports/merchantskies-nextos/upstream/README.md)
- [src/main.c](../../../ports/merchantskies-nextos/upstream/src/main.c)
- [src/jni.c](../../../ports/merchantskies-nextos/upstream/src/jni.c)
- [src/input.c](../../../ports/merchantskies-nextos/upstream/src/input.c)
- [src/egl.c](../../../ports/merchantskies-nextos/upstream/src/egl.c)
- [src/gles3.c](../../../ports/merchantskies-nextos/upstream/src/gles3.c)
- [src/unity6_shader.c](../../../ports/merchantskies-nextos/upstream/src/unity6_shader.c)

端口/集成作者：**NextOS**。保留第三方许可与署名；游戏数据不随源码附带。
