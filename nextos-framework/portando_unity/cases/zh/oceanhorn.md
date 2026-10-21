# Oceanhorn: Chronos Dungeon — Unity 2022.3.61f1

[English](../oceanhorn.en.md) · [Português](../oceanhorn.md) · [中文](#)

[Unity 索引](../../README.zh-CN.md) · [NextOS 源卡](../../../ports/oceanhorn-nextos/README.en.md)

选定笔记配置：IL2CPP/AArch64。本文件是关联公开源码的 `portando_unity` 研究资料的编辑性适配，描述历史成因与解法；收录既不重新验证该快照，也不为所有设备背书。

## 症状与分界

启动与精灵效果取决于：能区分原生 JNIBridge 指针与 GCHandle，并使用正确的图集 alpha 来源。

## 待研究的解法

保留原生 GLES2 变体与帧缓冲取景。不要把这个参考默认项当作实验性的渲染降载来开启。

## 音频与输入

Rewired 消费 KeyEvent/MotionEvent。FMOD 回退到常驻加载必须以异步 worker 失败为前提，并需要实测内存与音频。

## 限制与反证

本作为 Chronos Dungeon。不要把系列中其他游戏的版本或解法与此混淆。

适配时需为所有者载荷重算签名/偏移。在同一场景内比对修改前后，保留参考并记录新产物哈希。主机测试或可见标题不构成完整玩法的证明。

## 选定的公开源码

来源：[oceanhorn-nextos](https://github.com/NextOs-Ports/oceanhorn-nextos)，提交 `7cb05cca86afbb4b8a6e76aa251c8f0f8397bc9e`。[清单](../../../ports/oceanhorn-nextos/SOURCE-MAP.json)；[Unity 版来源](../../SOURCE-MAP.json)。

- [README.md](../../../ports/oceanhorn-nextos/upstream/README.md)
- [src/main.c](../../../ports/oceanhorn-nextos/upstream/src/main.c)
- [src/jni_shim.c](../../../ports/oceanhorn-nextos/upstream/src/jni_shim.c)
- [src/egl_shim.c](../../../ports/oceanhorn-nextos/upstream/src/egl_shim.c)

端口/集成作者：**NextOS**。保留第三方许可与署名；游戏数据不随源码附带。
