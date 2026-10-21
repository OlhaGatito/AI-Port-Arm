# 检查短按记录

[English](../README.en.md) · [Português](../../../README.md) · [中文](#)

分析主机上的有限 JSON。不注入输入、不读取设备，也不对端口作任何验收。代码与合成夹具是 [SOURCE-MAP.json](../../../SOURCE-MAP.json) 中标识的副本。

## 示例

```sh
python3 portando_unity/diagnostico/toque/verificar_toque_curto.py \
  portando_unity/diagnostico/toque/fixtures/preservado.json
python3 -m unittest discover -s portando_unity/diagnostico/toque -p 'test_*.py'
```

预期输出：`TRACE_CONSISTENT`、status 0、`evidence_kind: synthetic`，以及 `physical_validation: NOT_ESTABLISHED_BY_THIS_TOOL`。其余夹具分别演示丢失、重复与覆盖不足。

## 记录契约

使用 `schema: unity-short-press/1`、`contract: stateful-press`，以及 `run_id`、`action`、`evidence_kind`、`executable_sha256`、`native_render_enter_us`、`coverage`、`events`。Kind 可为 synthetic、transcription 或 runtime；非合成的观察必须提供可执行文件真实哈希。

记录 `origin`、`delivery`、`consumer` 三个 stage；边为 DOWN/MOVE/UP/CANCEL；以及 `gesture`、`route`、`owner_context` 与单调递增的 `t_us`。覆盖窗口需要起始/结束、丢弃计数、完整性与所观测的分界。消费方必须观察到 `game_action_state` 或 `game_press_release_callback`；仅有输入队列是不够的。

格式请参照 [preservado.json](../fixtures/preservado.json)。输入限 4 MiB、20,000 个事件，且 nativeRender 中至少需两个递增的帧入口。记录若有丢失，不要声明覆盖完整。

## 结果与输出

| 状态 | 退出码 | 含义 |
| --- | --- | --- |
| TRACE_CONSISTENT | 0 | 记录覆盖一次自洽的短按；不证明硬件行为 |
| TRACE_DEFECT | 1 | 观察到的序列含丢失/重复/无效释放 |
| INVALID_INPUT | 2 | JSON/契约无效 |
| INCONCLUSIVE | 3 | 覆盖或因果顺序不足 |
| REVIEW_REQUIRED | 3 | 路由/所有者变更，需另行分析 |
| NO_SHORT_PRESS_OBSERVED | 3 | 记录未覆盖短按场景 |

以 UP 结束的日志可能覆盖不到消费方紧随的那一帧。没有所需窗口时不要诊断“释放丢失”。原始错误消息予以保留；上述代码在葡语与英语中的含义一致。

要在**新**适配器中给正确的分界加插桩，请先阅读[输入与音频](../../../zh/INPUT-AND-AUDIO.md)。
