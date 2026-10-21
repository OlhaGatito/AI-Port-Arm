# Suzy Cube — Unity 2017.4.40f1

[English](../suzycube.en.md) · [Português](../suzycube.md) · [中文](#)

[Unity 索引](../../README.zh-CN.md) · [NextOS 源卡](../../../ports/suzycube-nextos/README.en.md)

选定笔记配置：IL2CPP/AArch64。本文件是关联公开源码的 `portando_unity` 研究资料的编辑性适配，描述历史成因与解法；收录既不重新验证该快照，也不为所有设备背书。

## 症状与分界

该构建已包含 GLES2 与 ETC1。预渲染阶段的阻塞项在启动/JNI 契约，包括真实的 String.length 与把 __stack_chk_guard 作为对象使用。

## 待研究的解法

在 initJni 期间保留 Unity 的加载 libil2cpp。文档记载的顺序为 nativeRecreateGfxState、focus、resume 与 nativeRender；不要套用 Unity 2022 的回调。

## 音频与输入

FMOD 保留混音器；InControl 接收已归一化手柄。检查已被 SDL 变换过的轴，并关注焦点与热插拔下的方向卡死。

## 限制与反证

该构建的历史证据不覆盖每一种数据变体。当原生路径本身已提供所需格式时，不要启动着色器/纹理转换。

适配时需为所有者载荷重算签名/偏移。在同一场景内比对修改前后，保留参考并记录新产物哈希。主机测试或可见标题不构成完整玩法的证明。

## 选定的公开源码

来源：[suzycube-nextos](https://github.com/NextOs-Ports/suzycube-nextos)，提交 `c3db13c749173e08d33d57839aca961d0a2ab319`。[清单](../../../ports/suzycube-nextos/SOURCE-MAP.json)；[Unity 版来源](../../SOURCE-MAP.json)。

- [README.md](../../../ports/suzycube-nextos/upstream/README.md)
- [src/main.c](../../../ports/suzycube-nextos/upstream/src/main.c)
- [src/jni.c](../../../ports/suzycube-nextos/upstream/src/jni.c)
- [src/input.c](../../../ports/suzycube-nextos/upstream/src/input.c)
- [src/egl.c](../../../ports/suzycube-nextos/upstream/src/egl.c)

端口/集成作者：**NextOS**。保留第三方许可与署名；游戏数据不随源码附带。
