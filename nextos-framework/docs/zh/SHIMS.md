# Shims：从导入到经过验证的契约

[English](../en/SHIMS.md) · [Português](../pt-BR/SHIMS.md) · [中文](../../README.zh-CN.md)

对于与真实 nxloader 相关的导入，请运行[集成的第一个移植](../../examples/zh/first-port/README.md)。它实现了两个 provider、JNI/生命周期顺序，以及构造函数之前导入缺失的反向验证用例。扩展覆盖应依据契约进行，不能靠统计符号名称的数量。

shim 把一个 Android 客户端所期望的接口适配到 Linux 主机上。正确的实现会保留签名、调用约定、内存布局、所有权、生命周期、错误与回调。找到符号名称并不等于证明了兼容性。

## 1. 研究可执行示例

从 [examples/shims-reference](../../examples/zh/shims-reference/README.md) 开始。`shims.h` 定义签名与一个函数指针联合；`shims.c` 只注册显式的实现；`main.c` 验证这些契约。

| 契约 | 演示的行为 | 限制 |
| --- | --- | --- |
| `__errno` | 返回调用线程的 `errno` 地址 | 不能覆盖全部 Bionic TLS |
| `__android_log_write` | 输出到 stderr 的诊断信息；成功投递返回 1，失败返回负的错误码 | 未实现 Android 的过滤/logd |
| `nx_demo_property` | 只提供 `demo.name`，且带显式容量 | 不是 `__system_property_get` |
| Resolver | 符号名缺失或枚举签名错误时返回 NULL | 枚举无法推断 ELF 的 ABI |

教学用的 resolver 不做重定位。在与真实 loader 集成之前，助手必须从库/引擎的契约中识别出 ELF 符号类型与签名。像 `__stack_chk_guard` 这样的对象不能变成函数地址。

## 2. 盘点导入

在私有区域准备好某个所有者的库之后，用 `readelf -Ws` 查看符号、用 `readelf -d` 查看依赖、用 `readelf -r` 查看重定位。区分必需的未定义符号、弱符号、对象与 TLS。请检查在运行时通过 `dlsym`、`eglGetProcAddress` 和 JNI 获得的导入：静态列表可能不包含它们。

一张有用的工作表应包含：名称；符号版本；类型；ABI；签名；调用方；预期结果/错误；缓冲区所有者；线程；选定的实现；来源/哈希/授权；测试；限制。这张表属于新的移植。

## 3. 选择适配类型

| 情形 | 决定 |
| --- | --- |
| ABI 与语义已被证明完全相同 | 以正确的类型转发给主机 |
| 布局/枚举/错误不同 | 显式转换输入与输出 |
| 异步回调或有状态对象 | 实现生命周期与同步 |
| 缺失的可选服务 | 返回契约所定义的“无”，并为该路径提供证据 |
| 未知的必需服务 | 带精确诊断信息失败，并在验收前实现 |

不要盲目强制转换 `pthread_mutex_t`、`FILE`、信号结构、`dirent` 或 JNI 对象。不要强制把所有互斥量都变成递归的。受管指针、JNI 句柄和原生指针不能互相替换。

## 4. 实现一个新的契约

先编写能把正确实现与空壳区分开的用例：合法输入、缓冲区边界、真实错误、允许为 NULL 的情形、并发、所有权与销毁。然后注册带类型的函数，并接入一个已知调用方。不要用通用强制转换把不兼容隐藏起来。

对于 JNI，要按精确签名注册类、方法和字段。保留局部/全局引用、异常、字符串转换与线程附加。`JNIEnv` 属于它所在的线程；不要在多个 worker 之间共享任意一个指针。[官方 JNI 指南](https://developer.android.com/ndk/guides/jni-tips)。

## 5. 寻找更完整的实现

| 边界 | 起始来源 |
| --- | --- |
| ELF 与重定位 | [nxloader](../../framework/nxloader/README.md) |
| Android/生命周期 | [nxandroid](../../framework/nxandroid/README.md) |
| 上下文/present | [nxgl](../../framework/nxgl/README.md) |
| 混音器/输出 | [nxaudio](../../framework/nxaudio/README.md) |
| 输入与上下文 | [nxinput](../../framework/nxinput/README.md) |
| Unity | [公开案例与诊断](../../portando_unity/README.en.md) |
| Mono/.NET、Godot、Cocos | [Mono](MONO-ANDROID.md)、[Godot](GODOT.md)、[Cocos2d-x](COCOS2D-X.md) |

在适配代码之前请先阅读清单和授权信息。上游代码可能包含特定的解决方案，这些方案不应变成框架的默认行为。

## 6. 诚实地衡量覆盖度

把每个契约归类为 `implemented-tested`、`adapter-required`、`optional-absent` 或 `unsupported`。记录所实际使用的数据资料/ABI。“所有导入都已解析”衡量的只是解析；它并不衡量玩法、并发、存档或图形保真度。

覆盖面的扩大必须围绕目录中已有的族系与重复出现的契约来推进。当前的示例只教授结构；它不是一个近乎完整的 Android shim，也不代表能运行大多数 Android 游戏。共享的 V5 改动需要在 V6 分支上单独开发。
