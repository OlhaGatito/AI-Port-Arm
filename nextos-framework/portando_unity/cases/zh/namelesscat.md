# Nameless Cat — Unity 6000.3.11f1

[English](../namelesscat.en.md) · [Português](../namelesscat.md) · [中文](#)

[Unity 索引](../../README.zh-CN.md) · [NextOS 源卡](../../../ports/namelesscat-nextos/README.en.md)

选定笔记配置：IL2CPP/AArch64。本文件是关联公开源码的 `portando_unity` 研究资料的编辑性适配，描述历史成因与解法；收录既不重新验证该快照，也不为所有设备背书。

## 症状与分界

私有的 SDL 改变了视频集成方式，尽管 GL 上下文存活，画面仍为黑屏。

## 待研究的解法

使用固件 SDL，并在 SDL_Init 之前的规范化分界处准入/配置控制器。保留已有的图形门面与提供者。

## 音频与输入

GameController 使用真实的 GUID/映射；暂停与光标保持语境化。保留固定配置的 FMOD/AudioTrack/OpenSL 后端。

## 限制与反证

本地笔记并未把每个历史来源绑定到某一个已通过的提交。公开快照是被识别的参考，不是该二进制的自动证明。

适配时需为所有者载荷重算签名/偏移。在同一场景内比对修改前后，保留参考并记录新产物哈希。主机测试或可见标题不构成完整玩法的证明。

## 选定的公开源码

来源：[namelesscat-nextos](https://github.com/NextOs-Ports/namelesscat-nextos)，提交 `fb489c406495e30822d50c8250c76810845414d5`。[清单](../../../ports/namelesscat-nextos/SOURCE-MAP.json)；[Unity 版来源](../../SOURCE-MAP.json)。

- [README.md](../../../ports/namelesscat-nextos/upstream/README.md)
- [generated/namelesscat/README.md](../../../ports/namelesscat-nextos/upstream/generated/namelesscat/README.md)
- [src/main.c](../../../ports/namelesscat-nextos/upstream/src/main.c)
- [src/jni.c](../../../ports/namelesscat-nextos/upstream/src/jni.c)
- [src/input.c](../../../ports/namelesscat-nextos/upstream/src/input.c)
- [src/egl.c](../../../ports/namelesscat-nextos/upstream/src/egl.c)
- [src/gles3.c](../../../ports/namelesscat-nextos/upstream/src/gles3.c)
- [src/unity6_shader.c](../../../ports/namelesscat-nextos/upstream/src/unity6_shader.c)

端口/集成作者：**NextOS**。保留第三方许可与署名；游戏数据不随源码附带。
