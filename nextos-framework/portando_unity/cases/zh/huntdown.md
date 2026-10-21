# Huntdown — Unity 2022.3.47f1 / 6000.2.6f2

[English](../huntdown.en.md) · [Português](../huntdown.md) · [中文](#)

[Unity 索引](../../README.zh-CN.md) · [NextOS 源卡](../../../ports/huntdown-nextos/README.en.md)

选定笔记配置：IL2CPP/AArch64。本文件是关联公开源码的 `portando_unity` 研究资料的编辑性适配，描述历史成因与解法；收录既不重新验证该快照，也不为所有设备背书。

## 症状与分界

存在两套数据配置：200023/Unity 2022 与 200036/Unity 6。第一套配置的玩法证据不能套用到第二套。

## 待研究的解法

2022 配置：在 Swappy 回退前先测量 EGL 时间戳能力，并在打补丁前校验签名。6 配置：代理 JNI、类型化回调与门面/ESSL 在选定笔记中仅有有限的标题/菜单证据。

## 音频与输入

输入遵循按配置区分的 GamePad 方法并使用固件映射。音频在 FMOD/OpenSL 与 AAudio 之间也有差异；声明了两个槽位并不证明物理上支持双人同屏。

## 限制与反证

不要把 2022 的 Swappy 补丁应用到 Unity 6，也不要把基于日志的配置修复当作已通过实机验收的设备。

适配时需为所有者载荷重算签名/偏移。在同一场景内比对修改前后，保留参考并记录新产物哈希。主机测试或可见标题不构成完整玩法的证明。

## 选定的公开源码

来源：[huntdown-nextos](https://github.com/NextOs-Ports/huntdown-nextos)，提交 `7eef080e69a66a09efc005bf3472035eef09248b`。[清单](../../../ports/huntdown-nextos/SOURCE-MAP.json)；[Unity 版来源](../../SOURCE-MAP.json)。

- [README.md](../../../ports/huntdown-nextos/upstream/README.md)
- [huntdown/README.md](../../../ports/huntdown-nextos/upstream/huntdown/README.md)
- [src/main.c](../../../ports/huntdown-nextos/upstream/src/main.c)
- [src/jni_shim.c](../../../ports/huntdown-nextos/upstream/src/jni_shim.c)
- [src/egl_shim.c](../../../ports/huntdown-nextos/upstream/src/egl_shim.c)
- [src/gles3.c](../../../ports/huntdown-nextos/upstream/src/gles3.c)
- [src/unity6_shader.c](../../../ports/huntdown-nextos/upstream/src/unity6_shader.c)

端口/集成作者：**NextOS**。保留第三方许可与署名；游戏数据不随源码附带。
