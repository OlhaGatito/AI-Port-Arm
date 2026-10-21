# Horizon Chase — Unity 2022.3.33f1

[English](../horizonchase.en.md) · [Português](../horizonchase.md) · [中文](#)

[Unity 索引](../../README.zh-CN.md) · [NextOS 源卡](../../../ports/horizonchase-nextos/README.en.md)

选定笔记配置：IL2CPP/AArch64。本文件是关联公开源码的 `portando_unity` 研究资料的编辑性适配，描述历史成因与解法；收录既不重新验证该快照，也不为所有设备背书。

## 症状与分界

精灵与扫描输出需要区分纹理 alpha、UV 精度与最终后台缓冲的 alpha。

## 待研究的解法

保留既有的 ETC1 RGB/Alpha8。修复受影响的材质/精度；当 RGB 正确但最终 alpha 使扫描输出失效时，仅在交换前改动 alpha 并恢复状态。

## 音频与输入

FMOD 保留音乐/音效；输入使用固件映射。Stream→sample 的切换是针对某个特定失败的补救，不是预防性默认。

## 限制与反证

KMS/SDL 必须保持单一的上下文/表面所有者。来自某一个提供者的证据并不为所有后端背书。

适配时需为所有者载荷重算签名/偏移。在同一场景内比对修改前后，保留参考并记录新产物哈希。主机测试或可见标题不构成完整玩法的证明。

## 选定的公开源码

来源：[horizonchase-nextos](https://github.com/NextOs-Ports/horizonchase-nextos)，提交 `59d2d38dc496ae0a71726181d3ccc80923e4144d`。[清单](../../../ports/horizonchase-nextos/SOURCE-MAP.json)；[Unity 版来源](../../SOURCE-MAP.json)。

- [README.md](../../../ports/horizonchase-nextos/upstream/README.md)
- [third_party/NXExtract/README.md](../../../ports/horizonchase-nextos/upstream/third_party/NXExtract/README.md)
- [src/main.c](../../../ports/horizonchase-nextos/upstream/src/main.c)
- [src/jni_shim.c](../../../ports/horizonchase-nextos/upstream/src/jni_shim.c)
- [src/egl_shim.c](../../../ports/horizonchase-nextos/upstream/src/egl_shim.c)

端口/集成作者：**NextOS**。保留第三方许可与署名；游戏数据不随源码附带。
