# Sally Face — Unity 2022.3.62f3

[English](../sallyface.en.md) · [Português](../sallyface.md) · [中文](#)

[Unity 索引](../../README.zh-CN.md) · [NextOS 源卡](../../../ports/sallyface-nextos/README.en.md)

选定笔记配置：IL2CPP/AArch64。本文件是关联公开源码的 `portando_unity` 研究资料的编辑性适配，描述历史成因与解法；收录既不重新验证该快照，也不为所有设备背书。

## 症状与分界

品红雨来自场景中使用了超出全局集合的着色器；场景/角色被裁切则源于图集被缩小而 Sprite 矩形未同步。

## 待研究的解法

转换实际使用的程序集合，并保留完整的图集/矩形。缩小尺寸并非 ETC1 的必然结果。

## 音频与输入

音频需要执行 FMODAudioDevice.run 线程函数并向 PCM 队列投喂数据。照搬导航方案之前，请先读 input.c 了解真实映射。

## 限制与反证

笔记记录的是第 1 章的证据，不是贯穿每一章的完整战役。请保留这一限制。

适配时需为所有者载荷重算签名/偏移。在同一场景内比对修改前后，保留参考并记录新产物哈希。主机测试或可见标题不构成完整玩法的证明。

## 选定的公开源码

来源：[sallyface-nextos](https://github.com/NextOs-Ports/sallyface-nextos)，提交 `39f3d3bd7fed4f160ae20af0f8824ab6b71b79bf`。[清单](../../../ports/sallyface-nextos/SOURCE-MAP.json)；[Unity 版来源](../../SOURCE-MAP.json)。

- [README.md](../../../ports/sallyface-nextos/upstream/README.md)
- [src/main.c](../../../ports/sallyface-nextos/upstream/src/main.c)
- [src/jni.c](../../../ports/sallyface-nextos/upstream/src/jni.c)
- [src/input.c](../../../ports/sallyface-nextos/upstream/src/input.c)
- [src/egl.c](../../../ports/sallyface-nextos/upstream/src/egl.c)

端口/集成作者：**NextOS**。保留第三方许可与署名；游戏数据不随源码附带。
