# NextOS Framework

[English](README.en.md) | [Português](README.md)

iOS 移植、研究与示例不在本合集的范围之内。

V5 框架，用于将 Android 游戏移植到 Linux ARM，配套**扩充版英文与葡文指南**，收录 **44 个游戏**的精选源码，以及**社区目录中的另外 2 个移植**。大部分调查、编码、构建与验证工作可由 AI 主导完成。

**私有审阅仓库。转为公开需 NextOS 明确批准。** 本合集不包含任何商业游戏数据，也不代表 46 个已认证的可安装包。

合集/整合作者：**NextOS** · [官方 GitHub](https://github.com/NextOs-Ports)。

## 从这里开始

1. [入门指引](docs/zh/GETTING-STARTED.md) 与 [全部指南索引](docs/zh/README.md)。
2. [让 AI 主导移植](docs/zh/AI-PORTING.md)。
3. [公开 ARM SDK](toolchains/sdk/zh/README.md) 与 [首个集成移植示例](examples/zh/first-port/README.md)。
4. [Shims](docs/zh/SHIMS.md)、[NXExtract](docs/zh/NXEXTRACT.md) 与 [测试](docs/zh/TESTING.md)。

> 说明：本页已导向 `docs/zh/` 下的中文指南。少数条目（SDK、首个移植示例、目录、贡献与许可等）目前尚无中文版，仍指向英文原文。

## 按引擎分类的指南

[选择 Android 运行时](docs/zh/ANDROID-RUNTIMES.md)。游戏参考仅限合集中已选定的公开移植。

| 路线 | 内容 |
| --- | --- |
| [Unity](portando_unity/README.en.md) | 分诊、生命周期、GLES2、ETC1/双通道、输入、音频及 15 个公开案例 |
| [Mono Android](docs/zh/MONO-ANDROID.md) | Mono/.NET、来自 Android 构建的 MonoGame/FNA、程序集与引导流程 |
| [Godot](docs/zh/GODOT.md) | 引擎/导出、渲染器、C#、视口与 InputMap |
| [Cocos2d-x](docs/zh/COCOS2D-X.md) | C++ 库、JNI、资源、文本、音频与原生主循环 |
| [GameMaker Android](docs/zh/GAMEMAKER-ANDROID.md) | YoYo Runner、VM/YYC、ABI 与数据 |
| [Ren'Py Android](docs/zh/RENPY-ANDROID.md) | Python、SDL/JNI、资源与持久化 |
| [Haxe/hxcpp/Lime Android](docs/zh/HAXE-LIME-ANDROID.md) | 引导、线程、TLS/GC 与限制 |
| [C/C++ Android](docs/zh/NATIVE-ANDROID.md) | JNI、NativeActivity、Android SDL 与 Bionic |
| [Freedom Planet 2：Vulkan → GLES2](portando_unity/en/FP2-VULKAN-GLES2.md) | SMOL-V/SPIR-V、着色程序翻译、精准写入与模板/透明 |

## 第一个示例

```sh
cmake -S examples/shims-reference -B work/host -DCMAKE_BUILD_TYPE=Release
cmake --build work/host --parallel 2
ctest --test-dir work/host --output-on-failure
```

这是在电脑上运行的入门 C 测试。接下来，[集成自研小游戏](examples/zh/first-port/README.md)将教你：ARM64 编译、加载 Android 客体库、通过 NXExtract 安装，以及在 CPU 模拟下执行 11 项检查。[公开 SDK](toolchains/sdk/zh/README.md) 提供构建环境；实际画面、声音与实体手柄仍需真机测试。

要移植其他游戏，请使用[可执行清单](docs/zh/ANDROID-INVENTORY.md)、[固定版本与源码](docs/zh/PINS-AND-SOURCES.md)、[各引擎练习](examples/zh/engines/README.md)与[着色器实验室](examples/zh/shader-lab/README.md)。

## 目录与源码

[46 个 NextOS 游戏](catalog/zh/README.md)：其中 44 个的源码精选自 40 个公开仓库，另 2 个为社区条目——Stranger Things 3 与 AVGN I & II Deluxe，暂未导入代码。Freedom Planet 2 已包含在这 44 个之中。每条参考均保留各自的状态、来源与限制。

| 目录 | 内容 |
| --- | --- |
| `framework/` | 保留的 V5 组件/模板 |
| `suportando_outros_devices/extrator-universal/` | 固定版本的 NXExtract：引擎、Runner 与 UI |
| `ports/` | 代码快照、葡/英双语档案与清单 |
| `portando_unity/` | 精选双语版、案例与通用工具 |
| `docs/pt-BR/`、`docs/en/`、`docs/zh/` | 扩充版对照指南（中文版见 `docs/zh/`，共 20 篇） |
| `examples/` | 集成示例、引擎练习、着色器实验室与 shim 参考（中文版见 `examples/zh/`） |
| `catalog/` | port 目录与 profile（中文版见 `catalog/zh/`） |
| `toolchains/sdk/` | 公开开发 SDK（中文版见 `toolchains/sdk/zh/`） |
| `publication/` | 完整性、校验、语言与待办事项（尚无中文版） |

V5：`framework-v5` @ `657fb65a23b5c3b20040e76307b27e6470b1d17c`。[导出的哈希](publication/v5-export.json)。12 个依赖私有组件的历史测试已被省略；运行时字节保持原样。既有移植保留 V3/V4/V5/V6 固定版本，未做迁移。

## 贡献与再分发

请阅读 [AGENTS.md](AGENTS.md)、[贡献指南](CONTRIBUTING.zh-CN.md) 与 [许可/致谢](LICENSING.zh-CN.md)。仅可使用所有者提供的那份拷贝来准备数据；严禁向 GitHub/CI 上传 APK、IPA、OBB、原始库、资源或存档。ZIP 包遵循各组件的许可并保留 NextOS/第三方致谢。非商业提案仍在审阅中，不取代既有的 GPL/MIT 授权。

编辑类指南为双语；历史源码、注释与规范性许可保留原始语言。[更新与检查](publication/ONBOARDING-UPDATE.en.md) · [发布前待办](publication/REVIEW.en.md)。
