# Dead Trigger — Unity 2019.4（补丁待确认 / patch to confirm）

[English](../deadtrigger.en.md) · [Português](../deadtrigger.md) · [中文](#)

[Unity 索引](../../README.zh-CN.md) · [NextOS 源卡](../../../ports/deadtrigger-nextos/README.en.md)

选定笔记配置：IL2CPP/AArch64。本文件是关联公开源码的 `portando_unity` 研究资料的编辑性适配，描述历史成因与解法；收录既不重新验证该快照，也不为所有设备背书。

## 症状与分界

任务后黑屏源于一个过时的控制器弹窗异常打断了 UI 更新。

## 待研究的解法

修复仅限已识别的 Init/Show 函数体，且必须拒绝被 IL2CPP 折叠共享的地址。保留 GLES2 上下文与生命周期。

## 音频与输入

OpenSL/BufferQueue 必须保留 FMOD 期望的对象、虚表与 PCM。检查首次重载、菜单、移动、音频与持久化。

## 限制与反证

本地笔记标识 Unity 2019.4，但缺少完整补丁。不要臆造补丁，也不要把历史基线混同为仓库后续修订。

适配时需为所有者载荷重算签名/偏移。在同一场景内比对修改前后，保留参考并记录新产物哈希。主机测试或可见标题不构成完整玩法的证明。

## 选定的公开源码

来源：[deadtrigger-nextos](https://github.com/NextOs-Ports/deadtrigger-nextos)，提交 `8fe021e4188db177a074547473d7723e060748fe`。[清单](../../../ports/deadtrigger-nextos/SOURCE-MAP.json)；[Unity 版来源](../../SOURCE-MAP.json)。

- [README.md](../../../ports/deadtrigger-nextos/upstream/README.md)
- [src/main.c](../../../ports/deadtrigger-nextos/upstream/src/main.c)

端口/集成作者：**NextOS**。保留第三方许可与署名；游戏数据不随源码附带。
