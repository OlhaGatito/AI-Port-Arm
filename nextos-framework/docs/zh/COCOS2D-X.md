[English](../en/COCOS2D-X.md) · [Português](../pt-BR/COCOS2D-X.md) · [中文](../../README.zh-CN.md)

# Cocos2d-x 在 Linux ARM 上的 Android 版本

[Português](../pt-BR/COCOS2D-X.md)

[原始 Cocos 场景](../../examples/zh/engines/README.md)演示了绘制、输入与持久化，并提供了 Cocos2d-x 3.17.2 模板的集成说明。该工程在本修订版中尚未构建过；仅凭这个场景本身并不能实现 Android loader。

Cocos2d-x 通常把引擎和游戏逻辑集中在同一个 C++ 库中，但同时也依赖 Java/JNI 来处理生命周期、资源、文本、音频、服务与输入。在保留游戏原生循环与执行顺序的前提下，重建这些边界。

## 1. 确认引擎与 ABI

查找 `cocos2d` 符号、`org.cocos2dx.lib` 类、渲染器 JNI 方法以及 C++ 依赖。确认版本/fork、ABI、主库、`libc++_shared.so`、资源与音频。Cocos Creator 游戏或基于 JavaScript 的分支可能采用不同的架构；不要将它们自动归类为同一条原生 Cocos2d-x 移植路线。

对本地库使用 `readelf -h`、`readelf -d` 和 `readelf -Ws`。要一并记录动态注册的导入项与回调。在存在 AArch64 时优先选择；ARMv7 需要复核 ABI、结构体与浮点调用。

## 2. 研究一份范围明确的公开参考

[Chrono Trigger](../../ports/chrono-nextos/README.en.md) 记录了 Cocos2d-x 3.14.1/GLES2、AArch64 loader、`Cocos2dxBitmap`、OpenSL ES 与原生输入。[Geometry Dash/SubZero](../../ports/geometrydash-nextos/README.en.md) 提供了另一套资料选取方式，两者的配置互不相同。

阅读 Chrono 的 `SOURCE-MAP.json`、README、`src/main.c`、`src/jni_shim.c`、`src/text_render.c`、`src/opensles_shim.c` 和 `src/ct_framework.c`。其中具体的名称、挂钩与固定点都属于那个移植工程。列入目录并不会自动使这一组合成为 V5。

## 3. 重建引导顺序

在文档记载的 Chrono 配置中：加载并初始化 C++ 依赖；加载/重定位游戏库；安装经过验证的挂钩；只执行一次构造函数；调用 `JNI_OnLoad`；发布上下文/资源；`nativeInit`；resume；事件/渲染循环；pause/存档与退出。

检查目标工程的 Java/JNI 顺序。上游的 [Cocos2dxRenderer](https://github.com/cocos2d/cocos2d-x/blob/v3/cocos/platform/android/java/src/org/cocos2dx/lib/Cocos2dxRenderer.java) 说明了 Android 端如何驱动回调；以游戏内置的那份版本为准。在提供它所需的上下文之前不要调用 `nativeInit`，也不要执行两次构造函数。

## 4. 按边界构建 adapter

| 边界 | 需要验证的实现 |
| --- | --- |
| 资源 | 正确的路径/大小写查找、压缩文件、读取/定位/关闭 |
| JNI | 真实的类、签名、字符串/数组与回调；对缺失项给出明确的报错 |
| 文本 | 字体、字号、对齐、Unicode、位图的跨度/格式与 alpha |
| GLES | 渲染线程上使用正确的上下文，状态管理与单次 present |
| 音频 | OpenSL 对象/vtable、BufferQueue 与缓冲区生命周期 |
| 输入 | 游戏实际消费的手柄/触摸/键盘 |
| 持久化 | 可写目录、偏好设置、暂停、存档与退出 |

`Cocos2dxBitmap` 应当单独测试：即使其他精灵正常显示，文本不可见也可能源于尺寸、跨度、alpha 通道或上传回调。不要用空字符串或固定位图来掩盖缺失的方法。

## 5. 编译时不混用 C++ 运行时

loader 运行在 Linux 上，遵循 [ARM 指南](BUILD-ARM.md)；原始库仍然是 Android 版本。作为客体的 C++ 依赖必须遵循该构建的契约，并以受控的解析方式避免与主机运行时冲突。不要任意用 Linux 的 `libstdc++` 替换 `libc++_shared.so`。

把 Chrono 的 `build_universal.sh` 当作一份特定配方来读，执行前先检查所需的镜像/SDK 与缺失的源文件。在新的移植中，保留经过审定的编译选项、架构、重定位、可见性与 GLIBC。在还在识别导入项时不要打包。

## 6. 诊断第一幅画面

如果引擎没有绘制，确认是否已进入原生渲染，且数据完整、上下文有效。如果绘制了但呈黑色，请对比 present 之前紧邻的着色器/程序链接、纹理/alpha、FBO 与像素。轮廓正确却呈黑色的物体可能是采样器/wrap 状态有误；不要对所有材质统一施加全局 CLAMP。

如果 RGB 正确但在合成器中最终画面变成透明/黑色，请在不破坏 RGB 或 scissor/clear 状态的前提下测试最终 alpha。修复措施应基于实测的能力/缺陷，而不是仅凭设备名称。

## 7. 测试完整行为

在可用的语言中测试文本、菜单、关卡加载、玩法、转场、音频、暂停/恢复、原生输入、热插拔、存档/重载与退出。不要叠加会接管原生输入的键盘映射器。对于触摸，按内容矩形换算坐标，并保证按下/移动/抬起与上下文一致。

为关键库和来自所有者副本的资源准备 [NXExtract](NXEXTRACT.md)，要有真实校验且不含随包发布的游戏数据。记录 [最终产物凭证](TESTING.md)。翻译过的菜单或第一帧并不能证明所有场景。

## AI mission

```text
Identify the Cocos2d-x fork, ABI, C++ library, JNI and renderer lifecycle.
Use Chrono and Geometry Dash as public references by contract. Implement
assets, text, audio and input in the new adapter, preserving constructors and
lifecycle. Build for Linux ARM, run targeted tests and create a data-free NXExtract recipe.
```
