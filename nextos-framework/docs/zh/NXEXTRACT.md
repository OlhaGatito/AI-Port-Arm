# NXExtract：所有者数据与干净安装

[English](../en/NXEXTRACT.md) · [Português](../pt-BR/NXEXTRACT.md) · [中文](../../README.zh-CN.md)

[集成课程](../../examples/zh/first-port/README.md)提供了原始输入、事务性钩子、干净安装、正向重新打包与真实回滚。这些测试在主机上禁用了 UI；设备端移植验收仍然强制要求使用规范的图形界面。

一个 BYO-data 的移植分发的是被允许的代码/运行时、完整的框架、一份配方，以及带规范图形界面的 NXExtract。数据来自所有者提供的兼容副本。私有仓库中的测试 ZIP 也适用同样规则。

## 1. 定义输入标识

记录游戏名称/版本、包 ID、ABI，以及被测试参考 APK 的大小与 SHA-256。识别关键库、元数据、bundle 与附属文件。分卷输入在测试之前必须完整；缺少必要载荷的 fixture APK 不能证明提取成功。

整个容器的 SHA 标识出被测试的那份副本。配方兼容性还必须校验包 ID、版本契约、ABI、结构以及关键载荷。当容器内的兼容内容保持不变时，不要仅仅因为容器的签名、成员顺序或外部文件名发生变化就拒绝它。

## 2. 编写移植配方

阅读[规范的最小示例](../../suportando_outros_devices/extrator-universal/examples/recipe-minimal.json)以及 `nxextract.py` 中的 `Recipe` 类。最小示例只是结构性的；真实游戏所需的标识还需要额外的校验。

| 字段/区域 | 需要记录的决定 |
| --- | --- |
| `id`、`version`、`title` | 稳定的配方标识与修订 |
| `abi_order` | 仅写已实现的 ABI；AArch64 排在最前 |
| `input.search_dirs` | `gamedata` 排在首位，遵循生成器的策略 |
| `extract` | 内部路径、目标位置以及按载荷的校验 |
| Hooks | 必需的转换、来源、工具与哈希 |
| `validate` | 运行时将消费的数据的最终检查 |
| `commit` 与 marker | 事务性发布与安装标识 |

不要凭这张表臆造键名：请使用所固定版本对应的 schema/校验器以及示例。Hooks 必须是经过定义和审查的可执行文件，不能在 JSON 里藏任意 shell 命令。着色器/纹理转换必须保留未被改动的对象，并产出可校验的输出。

## 3. 在使用设备之前检查结构

用你移植的实际信息创建 `work/ports/demo/extractor.json` 之后，查看可用的命令并校验配方：

```sh
python3 framework/nxgenerator/nxgenerator.py --help
python3 framework/nxrelease/nxrelease.py --help
python3 suportando_outros_devices/extrator-universal/nxextract.py recipe-check \
  --recipe work/ports/demo/extractor.json
```

`recipe-check` 会校验配方；它不安装数据，也不生成图形证据。在清单、运行时和 pin 完成之后，用 `nxgenerator.py <nxproject.json> --source-root <port-root> --output <new-directory>` 生成新的移植目录树。输出必须是全新的；绝不要在已批准的移植之上生成。

## 4. 用两种语言描述安装

每个移植的 ZIP 都包含 `<port-id>/INSTALLATION.md`，同一个文件内同时提供葡萄牙语和英语。请说明启动器与移植目录应放在何处、所有者的副本如何放置、兼容的版本/ABI、参考大小/SHA、首次启动时出现的界面，以及如何查看到错误。

不要发布会暴露下载来源的原始 APK 文件名，也不要发布下载站点、群组或分发方。应通过技术字段来标识内容。ZIP 中不得包含 APK/IPA/OBB、原始库、资源、存档或转换后的游戏数据。

## 5. 用最终候选从零测试

使用将要交付的同一个不可变 ZIP，并在没有已准备好数据和旧 marker 的隔离环境中安装。请保留已有的安装与存档。提供完整的输入，打开规范的启动器，并在运行时之前观察发现资源、图形界面、提取、钩子、校验、回执与 NXSplash 的全过程。

把最终哈希与已批准的参考版本进行对比。记录 ZIP/SHA、配方、完整输入、UI 版本、可见的渲染器与结果。采用已有数据、旧回执、活跃的 PID 或孤立的 `ui.ready` 文件都不能证明该流程。适用的证明必须保留 640×480 与 1280×720 下的 UI 布局/配色。

临时的 UI 控件应放在私有的运行时会话中，而不是游戏的 FAT/exFAT 卡上。不要为了接受一次不安全的握手而削弱权限。配方不得重新构建已批准的可执行文件。

## 6. 把诊断与验收区分开

分别记录标识、空间/结构、钩子、渲染器和校验方面的错误。成功的回执必须属于那一次尝试。参见[测试与交付](TESTING.md)，以把证据绑定到最终的字节上。本文档并不声称已对新测试过的商业安装作出说明。
