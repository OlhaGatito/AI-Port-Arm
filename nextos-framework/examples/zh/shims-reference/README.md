[English](../../../examples/shims-reference/README.en.md) · [Português](../../../examples/shims-reference/README.md) · [中文](README.md)

# NextOS shim 示例

本项目为 C99，用于执行小型且明确的契约测试。它不加载 Android ELF，也不实现完整的运行时。目的是在学习集成真实代码之前，先掌握如何组织一个 shim。

## 文件与流程

| 文件 | 作用 |
| --- | --- |
| [shims.h](../../shims-reference/shims.h) | 函数签名、类型化注册表与所有权契约 |
| [shims.c](../../shims-reference/shims.c) | 已知实现；缺失或不兼容时报错 |
| [main.c](../../shims-reference/main.c) | 演示 TLS、签名、缓冲区与日志 |
| [CMakeLists.txt](../../shims-reference/CMakeLists.txt) | C99 构建、pthread 与主机测试 |

`nx_demo_resolve` 要求同时提供名称与枚举签名。该枚举由调用方提供；它既不读取也不校验某个 Android 库的 ABI。联合体中对应的成员必须按其精确类型调用。

## 编译与测试

在仓库根目录执行，且已安装 C 编译器与 CMake：

```sh
cmake -S examples/shims-reference -B work/host -DCMAKE_BUILD_TYPE=Release
cmake --build work/host --parallel 2
ctest --test-dir work/host --output-on-failure
```

预期结果：`explicit-shim-contracts` 通过，且程序以状态零退出。这些用例检查未知导入、签名不符、NULL、两条线程之间独立的 `errno`、容量不足、缺失所有权以及非法日志标签。

## 契约与边界

`__errno` 转发到主机 TLS；它不实现完整的 Bionic TLS。日志使用 `__android_log_write` 的签名，写入 stderr，成功时返回 1，失败时返回负错误码；Android 日志过滤未实现。`nx_demo_property` 只认识 `demo.name`，且绝不应被注册为 `__system_property_get` 的替代品。

不要添加返回成功的兜底分支。扩展必须声明语义、输入输出、所有权、错误以及能够证明该行为的测试。请阅读 [shim 指南](../../../docs/zh/SHIMS.md)。

如需 ARM，请遵循 [编译指南](../../../docs/zh/BUILD-ARM.md)。CMake 不会在 cross 模式下注册测试；ARM 构建与 ARM 执行是两个不同的步骤。完整的 Android loader、图形/音频与解包示例仍属于独立工作。

代码：GPL-3.0-only。作者：**NextOS**。
