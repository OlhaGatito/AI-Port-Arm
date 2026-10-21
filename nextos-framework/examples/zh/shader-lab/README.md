[English](../../../examples/shader-lab/README.en.md) · [Português](../../../examples/shader-lab/README.md) · [中文](README.md)

# Vulkan → GLES2 着色器实验台

一堂可复现的课程，使用原创着色器，灵感来自为 Freedom Planet 2 记录的方法。其中不含从游戏中提取的着色器、Unity 文件或商业偏移量。

## 1. 准备工具

在 Linux 主机上，使用 Git、Python 3.11+、C++17 编译器和 `glslangValidator`。已验证的运行使用了 glslang 16.4.0。脚本会记录实际使用的版本；若使用其他版本，则需要重新检查结果。SPIRV-Cross 与 SMOL-V 的源码在 [sources.json](../../shader-lab/sources.json) 中按 commit 固定。

```sh
python3 examples/shader-lab/run.py --fetch   --deps work/shader-dependencies --output work/shader-results
```

`--fetch` 授权从两个已声明的公共仓库下载。缺少该选项时，缺失的源码会导致报错。已存在的目录必须处于指定 commit 且工作区干净。构建不会改动 FP2 的翻译器，也不会全局安装依赖。

## 2. 了解转换流程

`color.vert` 和 `color.frag` 生成 Vulkan 1.0 的 SPIR-V。[原创翻译器](../../shader-lab/translate.cpp)先压缩为 SMOL-V，再解压并逐字节比对，之后才翻译为 ESSL 1.00。片元着色器保留 alpha，并显式预乘 RGB。glslang 对生成的结果文本做校验。

`unsupported.comp` 是反向验证用例：compute 着色器不属于本课程 GLES2 流程的一部分。翻译器会返回错误，且不生成任何输出。失败的着色器绝不会被通用着色器替代。`RESULT.json` 记录五项检查、所用工具与 commit 固定值。

## 3. 区分 FP2 的固定版本

SMOL-V 的 commit `4b52c165c13763051a18e80ffbc2ee436314ceb2` 可精确复现已历史 FP2 脚本所要求的 `smolv.cpp`/`.h` 哈希。

历史版 SPIRV-Cross 的 commit `eb32b288ea553e938005fcfd819a2290b1c8032d` 在本轮复核中无法从 Khronos 上游获取（`not our ref`）。本课程改用了另一个显式固定的公开 commit。**这既不替代 FP2 的固定版本，也不能证明其翻译器可被完全相同地重建。** 若要重建已通过的 FP2，请找回该固定版本对应的确切来源；不要移除它的检查项。历史版脚本还要求 SPIRV-Tools 2026.3.1，而本课程这一独立实现并不使用。

## 4. 将该方法用于移植

主机端翻译并不能证明驱动编译、Unity 绑定、stencil、像素、变体或玩法逻辑的正确性。请在授权设备上的目标机器上补充一次真实的 GLES2 测试。[FP2 研究](../../../portando_unity/pt-BR/FP2-VULKAN-GLES2.md) 描述了其余的边界。参考：[SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross)、[SMOL-V](https://github.com/aras-p/smol-v)。
