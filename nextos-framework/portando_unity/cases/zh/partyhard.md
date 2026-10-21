# Party Hard GO — Unity 6000.3.10f1

[English](../partyhard.en.md) · [Português](../partyhard.md) · [中文](#)

[Unity 索引](../../README.zh-CN.md) · [NextOS 源卡](../../../ports/partyhard-nextos/README.en.md)

选定笔记配置：IL2CPP/AArch64。本文件是关联公开源码的 `portando_unity` 研究资料的编辑性适配，描述历史成因与解法；收录既不重新验证该快照，也不为所有设备背书。

## 症状与分界

数据布局不正确会阻塞场景；此后 KeyEvent 与 HAT 方向键同时投递，导致移动卡死。

## 待研究的解法

保留 assets/bin/Data 与生命周期。使用单一方向权威，并在上下文/焦点变更或断开连接时释放动作。SharedPreferences.apply 可以批量写入，同时保证 commit/暂停/退出的持久性。

## 音频与输入

InControl 控制玩法；右摇杆/R3 服务于操作选择对话框。FMOD/AudioTrack 保留混音器与帧计数。

## 限制与反证

笔记区分了公开版 1.0.6/V5 的结案状态与更早的构建。在把该证据对应到所观察的 HEAD 之前，请先核对快照修订版本。

适配时需为所有者载荷重算签名/偏移。在同一场景内比对修改前后，保留参考并记录新产物哈希。主机测试或可见标题不构成完整玩法的证明。

## 选定的公开源码

来源：[partyhard-nextos](https://github.com/NextOs-Ports/partyhard-nextos)，提交 `0a03b8483ac9c84d3be63c8dd898cf5acd305576`。[清单](../../../ports/partyhard-nextos/SOURCE-MAP.json)；[Unity 版来源](../../SOURCE-MAP.json)。

- [README.md](../../../ports/partyhard-nextos/upstream/README.md)
- [src/main.c](../../../ports/partyhard-nextos/upstream/src/main.c)
- [src/jni.c](../../../ports/partyhard-nextos/upstream/src/jni.c)
- [src/input.c](../../../ports/partyhard-nextos/upstream/src/input.c)
- [src/egl.c](../../../ports/partyhard-nextos/upstream/src/egl.c)
- [src/gles3.c](../../../ports/partyhard-nextos/upstream/src/gles3.c)
- [src/unity6_shader.c](../../../ports/partyhard-nextos/upstream/src/unity6_shader.c)

端口/集成作者：**NextOS**。保留第三方许可与署名；游戏数据不随源码附带。
