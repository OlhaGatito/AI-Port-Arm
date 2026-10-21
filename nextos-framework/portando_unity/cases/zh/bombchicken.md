# Bomb Chicken — Unity 2022.3.39f1

[English](../bombchicken.en.md) · [Português](../bombchicken.md) · [中文](#)

[Unity 索引](../../README.zh-CN.md) · [NextOS 源卡](../../../ports/bombchicken-gunbrick-nextos/README.en.md)

选定笔记配置：IL2CPP/AArch64。本文件是关联公开源码的 `portando_unity` 研究资料的编辑性适配，描述历史成因与解法；收录既不重新验证该快照，也不为所有设备背书。

## 症状与分界

黑屏可能由 JNI 返回不正确导致的 LevelStart.Awake 异常引起；另一处关卡末失败则源自格式损坏的 Progress。

## 待研究的解法

保留 GLES2/stencil8，只修复已被证实的 JNI/持久化契约。不要用改渲染器来化解逻辑异常。

## 音频与输入

Unity/FMOD 混音器使用 OpenSL/SDL 桥接。IL2CPP 输入调用需要适配本构建正确的重载、typedef 与 MethodInfo。

## 限制与反证

本卡覆盖 Bomb Chicken；同一仓库还包含拥有独立配置的 Gunbrick。不要在两者之间转移偏移量。

适配时需为所有者载荷重算签名/偏移。在同一场景内比对修改前后，保留参考并记录新产物哈希。主机测试或可见标题不构成完整玩法的证明。

## 选定的公开源码

来源：[bombchicken-gunbrick-nextos](https://github.com/NextOs-Ports/bombchicken-gunbrick-nextos)，提交 `53df6041da4ecc4905af5ecd24551034eea21175`。[清单](../../../ports/bombchicken-gunbrick-nextos/SOURCE-MAP.json)；[Unity 版来源](../../SOURCE-MAP.json)。

- [README.md](../../../ports/bombchicken-gunbrick-nextos/upstream/README.md)
- [docs/images/README.md](../../../ports/bombchicken-gunbrick-nextos/upstream/docs/images/README.md)
- [ports/bombchicken/src/main.c](../../../ports/bombchicken-gunbrick-nextos/upstream/ports/bombchicken/src/main.c)
- [ports/gunbrick/src/main.c](../../../ports/bombchicken-gunbrick-nextos/upstream/ports/gunbrick/src/main.c)
- [ports/bombchicken/src/jni.c](../../../ports/bombchicken-gunbrick-nextos/upstream/ports/bombchicken/src/jni.c)
- [ports/gunbrick/src/jni.c](../../../ports/bombchicken-gunbrick-nextos/upstream/ports/gunbrick/src/jni.c)
- [ports/bombchicken/src/input.c](../../../ports/bombchicken-gunbrick-nextos/upstream/ports/bombchicken/src/input.c)
- [ports/gunbrick/src/input.c](../../../ports/bombchicken-gunbrick-nextos/upstream/ports/gunbrick/src/input.c)
- [ports/bombchicken/src/egl.c](../../../ports/bombchicken-gunbrick-nextos/upstream/ports/bombchicken/src/egl.c)
- [ports/gunbrick/src/egl.c](../../../ports/bombchicken-gunbrick-nextos/upstream/ports/gunbrick/src/egl.c)

端口/集成作者：**NextOS**。保留第三方许可与署名；游戏数据不随源码附带。
