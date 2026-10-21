# 对 Unity 构建做三态判定

[English](../en/TRIAGE.md) · [Português](../pt-BR/TRIAGEM.md) · [中文](#)

本版为进入目录的 NextOS 端口甄选 `portando_unity` 资料。从真实的构建身份开始；仅凭 Unity 版本并不能决定渲染器、音频或输入。

## 1. 判定而不臆测

记录游戏名称/版本、包名、完整容器集合、ABI、Unity 库、Mono/IL2CPP 运行时与完整引擎版本。交叉核对资源头与运行时标识。一个名为 `unity6_shader.c` 的文件并不能证明是 Unity 6，“Unity 22” 也不能替代 2022.x.y 及其后缀。

区分静态观察、假设与实测行为。若确切补丁未知，如实记录该缺口。使用关键载荷哈希；外部 APK 的文件名不能标识构建。

## 2. 清点分界

| 领域 | 需采集的字段 |
| --- | --- |
| 启动 | 库、构造函数、JNI_OnLoad、NativeLoader 与 Activity 入口 |
| 运行时 | Mono/IL2CPP、元数据/程序集与版本 |
| 图形 | Built-in/URP、序列化 API、着色器/变体、物理上下文与扩展 |
| 纹理 | 常驻格式、尺寸、mip、alpha、用途与更新 |
| 音频 | FMOD、OpenSL、AudioTrack、AAudio；调用、格式与回调 |
| 输入 | InControl、Rewired、Input System、自定义代码或触摸；消费方 |
| 数据 | StreamingAssets、bundle、拆分/OBB、布局与所需预处理 |
| 状态 | 偏好设置、存档、暂停/恢复与退出 |

一个 APK 可能包含未被选中的变体。区分存储格式与实际上传至 GPU 的格式。每次 Mali-450 判定都要评估 ETC1/双平面，例外须在[纹理指南](GRAPHICS-AND-TEXTURES.md)中写明。

## 3. 按配置选择参考

使用 [Unity 索引](../README.zh-CN.md) 与 [SOURCE-MAP.json](../SOURCE-MAP.json) 中的哈希。比较版本、ABI、问题与契约，再检视公开实现。不要把未入选的案例引入本版。

举例：Suzy Cube 可用于原生 GLES2/ETC1 与 2017 生命周期；Freedom Planet 2 可用于 Vulkan 程序转换；Huntdown 需拆分其两套配置；Party Hard 解释方向权归属；Nameless Cat 展示固件 SDL 集成。

历史解决方案文档并不会自动把某个公开 HEAD 绑定到已通过的二进制。记录差异，仅在适配确需时才索取精确证据。

## 4. 保留 Android 流程

记录加载顺序、重定位、构造函数、JNI、surface/focus/resume 配置与原生帧入口。适配器遵循构建的真实流程。不要调用其他版本的“类似”方法，也不要跳过初始化以进入某个场景。

缺失的库不要返回通用句柄。虚假宣称支持 AAudio 或返回无效的 JNI 对象，会把失败推迟到音频或加载阶段。可选缺失按预期错误处理；必需缺失则成为已识别的阻塞项。

## 5. 产出可执行结论

交付内容：身份、选定的参考、契约表、首个可疑分界、用于证实/证伪的测量，以及下一步的本地改动。“可行”必须说明哪些已存在、哪些仍需实现；它不等于“可玩”。

Mali-450 使用的是物理 GLES2。`-force-gles20` 不会凭空生成缺失的变体、着色器或操作。缺乏像素、声音、操作、存档与退出的证据时，相应方面仍属部分完成。请改用[分界诊断](DIAGNOSTICS.md)。

## AI 任务

```text
确认 Unity 构建并逐项清点各分界。使用已入选目录的 NextOS 参考，固定哈希与许可。
保留原生流程、固件 SDL 与物理 GLES2。按用途规划 ETC1/双平面纹理，只实现新端口适配器，
并记录测量、修复与反证。
```
