# Terraria — Unity 2021.3.56f2

[English](../terraria.en.md) · [Português](../terraria.md) · [中文](#)

[Unity 索引](../../README.zh-CN.md) · [NextOS 源卡](../../../ports/terraria-nextos/README.en.md)

选定笔记配置：IL2CPP/AArch64。本文件是关联公开源码的 `portando_unity` 研究资料的编辑性适配，描述历史成因与解法；收录既不重新验证该快照，也不为所有设备背书。

## 症状与分界

当 eglGetProcAddress 未能路由 EGL 函数、且目标设备拒绝了所请求的配置时，引擎可能完全不发出 GL 调用。

## 待研究的解法

保留 EGL 身份与有效的 SDL 上下文配置，由主机驱动 nativeRender。测量 GfxDevice 创建与首次 present。

## 音频与输入

fmodGetInfo 报告的采样率与 DirectByteBuffer 大小必须与实际产出的帧数一致。InControl 需要单一手柄通路，需要时在文本输入期间将其置为无效。

## 限制与反证

不要把历史 job/GC 绕过手段提升为框架能力。该分类来自受检的 Android 构建，而非其他游戏版本。

适配时需为所有者载荷重算签名/偏移。在同一场景内比对修改前后，保留参考并记录新产物哈希。主机测试或可见标题不构成完整玩法的证明。

## 选定的公开源码

来源：[terraria-nextos](https://github.com/NextOs-Ports/terraria-nextos)，提交 `e04f6fe7d3be9591e659f6ecd42355d1b5d2caf6`。[清单](../../../ports/terraria-nextos/SOURCE-MAP.json)；[Unity 版来源](../../SOURCE-MAP.json)。

- [README.md](../../../ports/terraria-nextos/upstream/README.md)
- [package/universal/README.md](../../../ports/terraria-nextos/upstream/package/universal/README.md)
- [src/main.c](../../../ports/terraria-nextos/upstream/src/main.c)
- [src/jni_shim.c](../../../ports/terraria-nextos/upstream/src/jni_shim.c)
- [src/egl_shim.c](../../../ports/terraria-nextos/upstream/src/egl_shim.c)

端口/集成作者：**NextOS**。保留第三方许可与署名；游戏数据不随源码附带。
