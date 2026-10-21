[English](../en/DIAGNOSTIC-LAB.md) · [Português](../pt-BR/DIAGNOSTICO-GUIADO.md) · [中文](../../README.zh-CN.md)

# 通过可复现失败进行分诊

按 [原创练习游戏](../../examples/zh/first-port/README.md) 先跑一遍测试。下列每个案例都包含一个故意植入的失败与一个可验证的条件，无需商业输入。

## 读懂症状并选择测量方式

| 案例/日志 | 预期结果 | 真实移植中的下一步动作 |
| --- | --- | --- |
| `cpu.log` | JNI 之前执行构造函数；契约 PASS | 保持顺序与哈希 |
| `missing-import.log` | `FAIL resolve`；无构造函数 | 识别并实现该导入的签名；绝不用零值继续 |
| `wrong-package.log` | `NXE3001`；无标记 | 检查清单与 splits 集合 |
| `wrong-payload.log` | `NXE3001`；负载被拒 | 先比对内部构建/哈希，再谈偏移 |
| `missing-payload.log` | `NXE3001`；缺少种子 | 找回完整输入；不要削弱配方 |
| `wrong-abi.log` | `NXE3001`；库被拒 | 检查 ELF、ABI 与内部路径 |
| `hook-rollback.log` | `NXE7001`；先前数据未变 | 修正转换，同时保留已批准的状态 |
| shader/compute 实验室 | `TRANSLATION REJECTED`；无输出 | 实现该操作，或声明不兼容 |

## 区分其他边界

未被接受的 JNI 版本需要核对运行时契约；跨线程共享 `JNIEnv` 即使指针非 null 也是错误的。TLS/IFUNC/RELR 应在[清单](ANDROID-INVENTORY.md) 中较早出现。V5 会拒绝其契约之外的操作；不要用宽容的解析器把这个事实藏起来。

在硬件上，只有音频没有图像时，需要在 present 边界测量上下文与像素。实验室的 RAM 帧缓冲测试不是这样的证据。Cocos 中缺失文本需要检查 bitmap/alpha/stride；卡住的 Unity 转场更可能涉及同步而非着色器。请查阅各引擎的指南，并且每次测量只记录一个假设。

## 记录一次有用的修正

请记录：命令、文件/commit、技术输入、到达的边界、错误、假设、最小改动、通过的测试、仍应失败的反向用例，以及可得的物理结果。日志在评审前保持私有。新的源码或二进制文件不会继承另一产物的批准。
