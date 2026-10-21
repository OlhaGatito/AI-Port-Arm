[English](../en/PINS-AND-SOURCES.md) · [Português](../pt-BR/PINS-E-FONTES.md) · [中文](../../README.zh-CN.md)

# pins、来源出处与公开来源找回

[Português](../pt-BR/PINS-E-FONTES.md)

历史标签标识来源。本新仓库拥有自己的历史，缺少原始的 V5 commit 对象。该流程在不用重建标签的前提下同时保留这两重身份。

## 1. 物化已冻结的选型

使用 Python 3.11+ 与 Git 2.29+，或使用公开 SDK。克隆完整历史，并在根目录运行：

```sh
python3 tools/pin_collection.py --destination work/my-collection-pin
```

该辅助脚本会对 `c5a5c827ed1918572a7cc01fc5a1740341b7a92d` 打 pin，该哈希已在本集合中发布。它不使用会移动的 `HEAD`。结果是包含 commits、哈希与收据的 15 个组件的组合。原始出处仍为 V5 `657fb65a23b5c3b20040e76307b27e6470b1d17c`。

## 2. 理解这两棵树

`source/` 完全遵循 V5 的 `framework_pin.py` 契约。nxloader 编译使用该树。`tool-source/` 包含完整的选定导出，并按 pinned manifest 逐文件校验。生成过程使用第二棵树，因为它还需要 `framework/contracts/apkcompat`，而 V5 pin 的封闭注册表无法纳入该模块。

不要在 `source/` 内新增目录：那样会使校验失效。`TOOL-SOURCE.json` 记录第二棵树的文件；没有任何规范组件被修改。被省略的私有测试保持省略。该组合不承诺提供完整的历史测试集，也不改变既有移植的基准线。

## 3. 找回被省略的公开来源

```sh
python3 tools/recover_reference.py fp2-nextos --path build_universal.sh   --destination work/recovered-fp2
```

该文件在目录清单所对应的 commit 上取回并记录其哈希，且仅写入 `work/` 之下。若它已被包含在选型中，下载内容必须与 manifest 的 SHA-256 一致。路径中的特殊字符会原样保留在 URL 中；文件本身与其出处记录都必须是新的，并使用 `0600` 权限。该命令不会执行该脚本，也不会修改 `ports/*/upstream/`。使用前请检查路径、依赖与许可。公开副本中可能含有不适合新文档的历史路径。

FP2 的公开仓库中就有这个脚本，尽管选型时将其省略了。Chrono 的构建需要一个本地镜像和一个被排除的 TTF；新 SDK 并不会自动使该历史脚本变得可移植。请查阅[档案目录](../../catalog/profiles.json)，并准备一份带明确依赖的自己的配方。

## 4. 检查适用范畴

不要为了接受另一个库而移除哈希。要区分集合 pins、组件 pins、移植 commit 与所有者输入身份。新的构建有其自身的出处，需要独立的测试。[着色器实验室](../../examples/zh/shader-lab/README.md)针对 FP2 的依赖演示了这一区别。
