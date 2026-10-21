# 纹理离线规划器

[English](../README.en.md) · [Português](../../../README.md) · [中文](#)

`portando_unity` 的通用工具，在 [SOURCE-MAP.json](../../../SOURCE-MAP.json) 中连同哈希一并保留。它读取主机上的显式 JSON 并向 stdout 输出报告。不打开 APK、不转换图像、不连接设备，也不测量 RSS/FPS。

## 运行合成示例

```sh
python3 portando_unity/diagnostico/texturas/planejar_etc1.py \
  portando_unity/diagnostico/texturas/exemplo.json
python3 -m unittest discover -s portando_unity/diagnostico/texturas -p 'test_*.py'
```

第一条命令应以 status 0 结束，并输出 `action: PLAN_ONLY_NO_CONVERSION`。清单为合成数据；`runtime_validated` 保持为假，真实内存/FPS 仍为 `NOT_MEASURED`。

## 准备清单

把 [exemplo.json](../exemplo.json) 复制到工作区，保留 `schema: unity-texture-plan/1`，并用 `kind: inventory` 记录真实观察。每张纹理需提供 `id`、`width`、`height`、`levels`、`source_format`、`role`、`alpha`、`dynamic`；`levels` 接受有效整数或 `full`。ID 是简单别名，不是文件路径。

接受的格式：RGBA8888、RGB888、RGB565、RGBA4444、A8、RGBA16F、ETC1、ETC1_DUAL。用途：color、normal、data、depth、render_target、font_sdf、video、unknown。Alpha：opaque、blend、cutout、premultiplied、unknown。不要把 PNG 当作常驻格式。

JSON 最多接受 10,000 张纹理，输入至多 8 MiB，尺寸介于 1 与 65,536，并拒绝重复键/ID。遵守这些限制并不能证明 GPU 接受该尺寸。

## 解读结果

| 字段/代码 | 含义 |
| --- | --- |
| `potential_saving_bytes` | 载荷节省的数学差值 |
| `CANDIDATE_REQUIRES_SHADER_AND_VISUAL_PROOF` | 候选仍需要着色器/上传与视觉证据 |
| `NO_PAYLOAD_SAVING` | 无可计算的节省 |
| `REQUIRES_*_ANALYSIS` | 用途、alpha、通道、HDR 或更新需专门研究 |
| `KEEP_EXISTING_COMPRESSED_CONTRACT` | 保留现有压缩表示 |

合计假定所列全部图像同时常驻一份。计算出的 RGBA 缓冲只是暂存场景，不是真实峰值。状态 2 表示输入无效，不是游戏错误。原始诊断消息为葡语；代码/字段稳定，并有两种语言解释。

执行任何真实转换前，请先阅读[图形与 ETC1](../../../zh/GRAPHICS-AND-TEXTURES.md)。通用代码遵循本集合的适用条款；不授予任何游戏数据许可。
