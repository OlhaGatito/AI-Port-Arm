# Unity 分界诊断

[English](../en/DIAGNOSTICS.md) · [Português](../pt-BR/DIAGNOSTICO.md) · [中文](#)

记录 **症状 → 测量 → 原因 → 修复 → 反证 → 限制**。一个名为 “fix” 的文件并不授权在所有游戏中启用该行为。

| 症状 | 用于区分的测量 | 选定参考 |
| --- | --- | --- |
| 无 GL 调用 | 引导、nativeRender 与 EGL 解析 | Terraria |
| 数帧后黑屏 | present、时间戳与 Swappy 选择 | Huntdown（2022 配置） |
| RGB 正确但扫描输出为黑 | 最终 alpha 与合成器；修改前后状态 | Horizon Chase、Hitman GO |
| 精灵呈色块矩形 | Swizzle、alpha 与预乘 | Freedom Planet 2 |
| 文字呈方块 | R8/SDF 通道与物理格式 | Merchant of the Skies |
| 品红雨、其余正确 | 场景中超出全局程序的着色器 | Sally Face |
| 场景/角色被裁切 | 完整图集与 Sprite 矩形 | Sally Face |
| 引擎存活但关卡未启动 | JNI/托管异常与完整数据 | Bomb Chicken |
| 上下文存活，换 SDL 后画面丢失 | 实际提供者与库字节 | Nameless Cat |
| 方向卡死 | KeyEvent/HAT 重复、焦点与释放 | Party Hard GO |
| 点击位置偏移 | 托管鼠标坐标空间与 Y 增量 | Prizefighters 2 |
| 声音加快 | fmodGetInfo、SDL 采样率与每帧字节 | Terraria |
| 混音器不启动 | FMODAudioDevice 线程与实际可用库 | Sally Face、Merchant of the Skies |

卡片与源码由[索引](../README.zh-CN.md)关联。本表给出有案例支撑的假设，并不表示该原因也会出现在新游戏中。

## 一次诊断运行

定义比对结果、可执行文件/SHA、输入、场景、设置与观察窗口。只添加必要的插桩，并在每个分界记录观察者。不要把黑屏的 DRM 抓屏当作游戏画面证明，也不要把 PID 当作帧计数器。

尽可能比对呈现前像素与实际屏幕。探针保持在默认发布行为之外。启动下一个实例前先结束前一个，且只用任务授权的那台设备。

## 收录的工具

[纹理规划器](../diagnostico/texturas/zh/README.md)依据清单计算潜在载荷，不转换资源也不测帧率。[触屏检查器](../diagnostico/toque/zh/README.md)分析显式序列；它既不注入事件，也不单独度量消费方。

只收录了通用工具与合成示例。未导入图库、会话绑定采集器、报告以及被排除游戏的源码。

## 证伪假设

若测量与假设相悖，从新适配器移除该实验并记录结果。若假设成立，实现最小修复并反证，然后测试原场景与下一过渡。不要为制造一个通过的结论而删除存档、改动数据或重建已通过的二进制。

部分结论仍然是部分的。标出历史源码与已通过产物之间的任何缺口；本版不制造新的实机认证。
