# Prizefighters 2 — Unity 2022.3.62f2

[English](../pf2.en.md) · [Português](../pf2.md) · [中文](#)

[Unity 索引](../../README.zh-CN.md) · [NextOS 源卡](../../../ports/prizefighters2-nextos/README.en.md)

选定笔记配置：IL2CPP/AArch64。本文件是关联公开源码的 `portando_unity` 研究资料的编辑性适配，描述历史成因与解法；收录既不重新验证该快照，也不为所有设备背书。

## 症状与分界

该构建需要兼容的 GLES2 着色器变体，其菜单会消费托管鼠标输入。

## 待研究的解法

转换必需的程序并保留 Built-in/Gamma。把光标 Y 位置与 Y 增量唯一一次地转换到实际的落点空间；不要在可变分辨率端口上硬编码 720。

## 音频与输入

Input System 使用特定的 StateEvent/GamepadState 布局；鼠标输入为语境化。fmodGetInfo/fmodProcess 与 PCM 队列必须保留混音器。

## 限制与反证

这不是一份 URP.renderScale 配方。载荷标识与钩子需要各自独立验证。

适配时需为所有者载荷重算签名/偏移。在同一场景内比对修改前后，保留参考并记录新产物哈希。主机测试或可见标题不构成完整玩法的证明。

## 选定的公开源码

来源：[prizefighters2-nextos](https://github.com/NextOs-Ports/prizefighters2-nextos)，提交 `53371b37dc5d01e0a8c09d683536ee1c48f35d6c`。[清单](../../../ports/prizefighters2-nextos/SOURCE-MAP.json)；[Unity 版来源](../../SOURCE-MAP.json)。

- [README.md](../../../ports/prizefighters2-nextos/upstream/README.md)
- [src/main.c](../../../ports/prizefighters2-nextos/upstream/src/main.c)
- [src/jni.c](../../../ports/prizefighters2-nextos/upstream/src/jni.c)
- [src/input.c](../../../ports/prizefighters2-nextos/upstream/src/input.c)
- [src/egl.c](../../../ports/prizefighters2-nextos/upstream/src/egl.c)

端口/集成作者：**NextOS**。保留第三方许可与署名；游戏数据不随源码附带。
