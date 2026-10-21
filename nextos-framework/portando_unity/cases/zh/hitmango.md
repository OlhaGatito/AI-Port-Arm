# Hitman GO — Unity 2022.3.67f2

[English](../hitmango.en.md) · [Português](../hitmango.md) · [中文](#)

[Unity 索引](../../README.zh-CN.md) · [NextOS 源卡](../../../ports/hitmango-nextos/README.en.md)

选定笔记配置：IL2CPP/AArch64。本文件是关联公开源码的 `portando_unity` 研究资料的编辑性适配，描述历史成因与解法；收录既不重新验证该快照，也不为所有设备背书。

## 症状与分界

原生 GLES2 路径需要对不支持的格式作选择性处理，并与合成器兼容的最终 alpha。

## 待研究的解法

保留受支持的 ETC1；仅在必要时解码 ETC2/EAC。应用后台缓冲校正前，先检查 RGB/alpha 像素。

## 音频与输入

一个原生线程复现 FMODAudioDevice/AudioTrack，但不替换混音器。InControl 控制棋盘移动；光标与快捷键均为语境化。

## 限制与反证

不要把这条手柄通路变成全局触摸模拟。在新产物中重新验证章节、存档与退出。

适配时需为所有者载荷重算签名/偏移。在同一场景内比对修改前后，保留参考并记录新产物哈希。主机测试或可见标题不构成完整玩法的证明。

## 选定的公开源码

来源：[hitmango-nextos](https://github.com/NextOs-Ports/hitmango-nextos)，提交 `3f735165348e17d4f9aa20f1d227a9b1ecb4efb8`。[清单](../../../ports/hitmango-nextos/SOURCE-MAP.json)；[Unity 版来源](../../SOURCE-MAP.json)。

- [README.md](../../../ports/hitmango-nextos/upstream/README.md)
- [hitmango/README.md](../../../ports/hitmango-nextos/upstream/hitmango/README.md)
- [src/main.c](../../../ports/hitmango-nextos/upstream/src/main.c)
- [src/jni.c](../../../ports/hitmango-nextos/upstream/src/jni.c)
- [src/input.c](../../../ports/hitmango-nextos/upstream/src/input.c)
- [src/egl.c](../../../ports/hitmango-nextos/upstream/src/egl.c)

端口/集成作者：**NextOS**。保留第三方许可与署名；游戏数据不随源码附带。
