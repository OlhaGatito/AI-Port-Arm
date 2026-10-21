[English](../en/AI-PORTING.md) · [Português](../pt-BR/PORTAR-COM-IA.md) · [中文](../../README.zh-CN.md)

# 由 AI 主导完成移植工作

[Português](../pt-BR/PORTAR-COM-IA.md)

在发明新工具之前，请先使用 [Android 清单登记](ANDROID-INVENTORY.md)、[pins 与来源](PINS-AND-SOURCES.md) 以及[诊断实验室](DIAGNOSTIC-LAB.md)。[首个移植示例](../../examples/zh/first-port/README.md)为助手提供了一次已实跑的演示，可用于逐项边界比对。

助手应当负责调查、编写代码、编译、执行授权范围内的检查，并留下可复现的结果。所有者负责提供兼容的输入、选定目标、提供必要的实体反馈并批准发布。每一个可逆的本地微调都无需提问。

## 1. 给出具体任务

复制下面的模板，并填入你已知的内容。未知字段应当变为调查任务，而不是编造数据的许可：

```text
Read AGENTS.md and docs/en/README.md in this clone.
Game and version: [provide]
Local Android copy supplied by me: [private path]
Target for this stage: [system, CPU/GPU and userland ABI]
New repository/directory: [destination]
First objective: identify the build and reach its native flow on the target.

Autonomously handle inventory, public-reference selection, adapter
implementation, build and relevant local tests.
Prefer AArch64. Preserve V5 and every reference port.
For Unity, read portando_unity/README.en.md and use only its public cases.
Read docs/en/ANDROID-RUNTIMES.md and choose one of the eight Android tracks.
Use only already selected public ports, with pinned commit and evidence.

Keep sources, pins, contracts, private logs and results organized.
Do not invent offsets, signatures, support, licenses or successful missing APIs.
Implement and test what you can; describe any blocker precisely.
Do not upload game data to GitHub. Do not access a device without an address
authorized for this task. Prepare reviewable material before requesting publication.
```

## 2. 在选择 loader 之前完成清单登记

创建一份私有报告，内容包含游戏/版本、包名、参考容器大小/SHA、ABI、关键库及哈希、实际引擎/runtime、依赖、资源路径、着色器/贴图、音频与输入。对于分包（split）包，需识别每一个必需的容器。不要混淆游戏版本与引擎版本。

以下命令仅列出显式提供的 APK 内的库；它既不解包也不执行其内容。包名与版本需要一个合适的 Android 二进制清单读取器；仅靠 `strings` 无法确认这两个字段。

```sh
export NEXTOS_OWNER_APK=/private/owner-input/game.apk
python3 - <<'PY'
import os
from zipfile import ZipFile
with ZipFile(os.environ['NEXTOS_OWNER_APK']) as apk:
    for item in apk.infolist():
        if item.filename.startswith('lib/') and item.filename.endswith('.so'):
            print(item.filename, item.file_size)
PY
```

若原始输出含有私有信息，不要提交它。商用数据只在授权环境中读取，不要将其作为附件发送给外部 AI 服务。

## 3. 按契约选择参考实现

打开目录清单与 `SOURCE-MAP.json`。记录参考实现为何匹配：引擎/构建、ABI、JNI、音频、渲染、输入与许可是否兼容。同一个函数在其他构建中可能具有不同的签名。检查被引用实现是否位于所选文件中，以及其证据是否指向同一份产物。

| 发现项 | 对应技术路线 |
| --- | --- |
| Unity（Mono 或 IL2CPP） | [移植 Unity](../../portando_unity/README.en.md) |
| Mono/.NET Android，含来自 Android 构建的 MonoGame/FNA | [Mono Android](MONO-ANDROID.md) |
| Godot 项目/runtime | [Godot](GODOT.md) |
| Cocos2d-x 与 Android 回调 | [Cocos2d-x](COCOS2D-X.md) |
| GameMaker Android | [GameMaker Android](GAMEMAKER-ANDROID.md) |
| Ren’Py Android | [Ren’Py Android](RENPY-ANDROID.md) |
| Haxe/hxcpp/Lime Android | [Haxe/hxcpp/Lime Android](HAXE-LIME-ANDROID.md) |
| C/C++ Android | [C/C++ Android](NATIVE-ANDROID.md) |

## 4. 组织小而可验证的交付物

在**新的**移植仓库中，维护清单登记、契约表、adapter 源码、构建配方、NXExtract 配方与测试记录。建议使用的名称：`docs/inventory.md`、`docs/contracts.md`、`src/`、`recipes/`、`docs/validation.md`。这些名称仅为组织性建议；生成器的实际 schema 仍然具有权威性。

对于每个契约，记录导入/签名、来源出处、许可证、内存/线程归属、错误行为与测试。对于每次运行，记录 commit、ELF SHA、数据画像、目标与结果。私有信息不进入可发布的文档。

## 5. 保持原生顺序，一次只修一处边界

从库/重定位与构造函数开始，然后是初始化 JNI/回调与生命周期，接着是上下文/surface、帧、音频、输入、持久化与退出。确切顺序来自被调查的引擎，而不是一份通用的函数名清单。

出错时，记录假设、可区分的度量、最小修复与反向验证。例如，有声音但黑屏，说明需要测量图形边界，而不是视频的部分成功。遇到未知导入，需要实现其契约；一个笼统的 `return 0` 只会掩盖问题。

## 6. 自动化但不丢失证据

AI 可以生成清单、比对哈希、定位符号、编写针对性测试并准备包。保留已批准的可执行文件；文档化或翻译并不授权重建它。在最终测试批次之前完成所有改动，不要每出一个开发错误就新建一个 ZIP。

一个有用的检查点应当记录：改了什么、执行的命令、结果、剩余工作与下一个独立步骤。恢复工作时，助手应读取该检查点并比对文件，而不是重新发现已被证明的修正。

## 7. 以准确的适用范畴收尾

交付源码与配方、出处、可复现构建、缺失契约的诊断、[干净安装](NXEXTRACT.md) 与[最终字节证据](TESTING.md)。“构建成功”、“打开了菜单”与“完整游戏流程”是不同的结果。只报告实际观察到的内容。

如果缺少输入或设备，应完成独立的工作，并明确指认无法测试的准确边界。在 NextOS 明确批准发布之前，本仓库保持私有。
