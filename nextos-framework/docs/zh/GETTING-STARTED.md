# 快速开始

[English](../en/GETTING-STARTED.md) · [Português](../pt-BR/PRIMEIROS-PASSOS.md) · [中文](../../README.zh-CN.md)

在入门的 C 语言测试之后，请继续阅读[公共 SDK](../../toolchains/sdk/zh/README.md) 与[集成的 ARM64 第一个移植](../../examples/zh/first-port/README.md)：构建、pin、Android 客户端、NXExtract 与 QEMU 测试，全过程不使用商业数据。

本指南从一次克隆开始，以在你的计算机上运行一个 C 示例结束。随后你便具备基础，可以请 AI 助手来实现一个新的移植。目录中收录了 44 个标题的源码参考；每个参考都保留其自身的要求与限制。

## 1. 准备你的计算机

使用 Linux，并安装 Git、Python 3.11 或更高版本、CMake 3.20 或更高版本、C99 编译器、Make 或 Ninja，以及 ELF 工具（`readelf`）。C 项目接受 CMake 3.16，但本指南使用的是更高版本中测试命令所提供的功能。在 Windows 上，请在 Linux 环境中运行这些命令；掌机 GPU 测试仍是独立的步骤。

在安装游戏专用依赖之前，先检查你的环境：

```sh
git --version
python3 --version
cmake --version
cc --version
readelf --version
```

## 2. 克隆并校验集合

在仓库处于私有状态期间，你的 GitHub 账户需要访问权限。请使用你惯常的认证方式，不要把令牌粘贴进脚本或文档。

```sh
git clone https://github.com/NextOs-Ports/nextos-framework.git
cd nextos-framework
git rev-parse HEAD
python3 publication/verify.py
```

记录下集合的 commit。校验器会对照各清单检查导入的文件，并查找被识别的禁止格式与模式。哈希不匹配时必须找回正确的来源；不要为了消除报错就更新清单。

## 3. 构建你的第一个示例

以下命令全部在克隆的根目录执行：

```sh
cmake -S examples/shims-reference -B work/host -DCMAKE_BUILD_TYPE=Release
cmake --build work/host --parallel 2
ctest --test-dir work/host --output-on-failure
```

预期结果：`explicit-shim-contracts` 测试通过。该程序演示了带类型的解析、未知导入的错误、按线程的 `errno`，以及有界缓冲区操作。[阅读示例契约](../../examples/zh/shims-reference/README.md)。

主机测试通过只能证明该示例本身的行为。它还没有加载过 Android 库、没有在 Mali 上渲染过图像，也没有运行过任何游戏。

## 4. 区分参考资料与开发工作

阅读 [AGENTS.md](../../AGENTS.md)，并从[目录](../../catalog/zh/README.md)中选定一个参考。把集合保留为参考资料，把你的移植写在 `work/ports/<port-id>/` 或另一个独立的仓库中。Git 会忽略 `work/`，但这并不能保护通过其它渠道发送出去的文件：请让商业数据远离提交与上传。

在动手编码之前，先确定游戏/构建方式、ABI、引擎、目标系统/GPU，以及你要证明的第一个结果。所有者提供的数据应放在明确私有的位置。旧的主机地址永远不构成建立新连接的授权。

## 5. 选择你的下一条路径

| 目标 | 阅读内容 |
| --- | --- |
| 理解各组件 | [架构](ARCHITECTURE.md) |
| 让 AI 负责实现 | [AI 移植](AI-PORTING.md) |
| 构建 Linux ARM 可执行文件 | [ARM 编译](BUILD-ARM.md) |
| 实现 Android 导入 | [Shims](SHIMS.md) |
| 分析 Unity APK | [移植 Unity](../../portando_unity/README.en.md) |
| 准备所有者的数据 | [NXExtract 与安装](NXEXTRACT.md) |
| 理解验收声明 | [测试与交付](TESTING.md) |

如果第一个命令失败了，请查阅[故障排查](TROUBLESHOOTING.md)。报告命令、错误与架构信息时，不要附带 APK 或完整的私有日志。
