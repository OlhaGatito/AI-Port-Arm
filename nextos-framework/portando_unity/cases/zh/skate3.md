# Skateboard Party 3 — Unity 2022.3.45f1

[English](../skate3.en.md) · [Português](../skate3.md) · [中文](#)

[Unity 索引](../../README.zh-CN.md) · [NextOS 源卡](../../../ports/skate3-nextos/README.en.md)

选定笔记配置：IL2CPP/AArch64。本文件是关联公开源码的 `portando_unity` 研究资料的编辑性适配，描述历史成因与解法；收录既不重新验证该快照，也不为所有设备背书。

## 症状与分界

该构建保留 GLES2 与 ETC1，残余的 ETC2 子集需要选择性回退。

## 待研究的解法

转换前先确认实际的后端选择与每一次格式上传。分析日志并不能证明存在图形卡死。

## 音频与输入

FMOD/OpenSL 向 SDL 投递音频；保留原生控件与固件映射。验证进入、年龄菜单、特技与退出。

## 限制与反证

仓库名为 skate3-nextos，但游戏是 Skateboard Party 3。仓库名并不自动说明用的是 InControl 还是 Rewired。

适配时需为所有者载荷重算签名/偏移。在同一场景内比对修改前后，保留参考并记录新产物哈希。主机测试或可见标题不构成完整玩法的证明。

## 选定的公开源码

来源：[skate3-nextos](https://github.com/NextOs-Ports/skate3-nextos)，提交 `18621e90ce7458c3166aa50d5cc508a13e65ee84`。[清单](../../../ports/skate3-nextos/SOURCE-MAP.json)；[Unity 版来源](../../SOURCE-MAP.json)。

- [README.md](../../../ports/skate3-nextos/upstream/README.md)
- [src/main.c](../../../ports/skate3-nextos/upstream/src/main.c)
- [src/jni.c](../../../ports/skate3-nextos/upstream/src/jni.c)
- [src/input.c](../../../ports/skate3-nextos/upstream/src/input.c)
- [src/egl.c](../../../ports/skate3-nextos/upstream/src/egl.c)

端口/集成作者：**NextOS**。保留第三方许可与署名；游戏数据不随源码附带。
