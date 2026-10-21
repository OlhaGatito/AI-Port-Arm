# -*- coding: utf-8 -*-
"""Append the run40..run46 outcome to the workspace daily log."""
import io
import os

P = r"D:\ProgramData\nextos-framework\.workbuddy\memory\2026-10-05.md"

SEC = """

## run40-run46：从「根因锁定」走到「第一处失败已修复」

承接 run39i（根因 = `RecordStore.storage_` 为 NULL）。本段六轮全部实测，
结论按时间顺序：

1. **run40** —— 转储改打印指针（不再打印 `ok`），并追加**方法表**。
   抛出点 `0xa4a674` 落在 `GetNumRecords`(0xa4a58c) 与
   `SetPreferenceMode`(0xa4ad38) 之间 ⇒ **抛异常的是 GetNumRecords**。
   `SetPreferenceMode` 有 **0 个 bl 调用点**，游戏从不调用它。

2. **run41** —— 扫描 RecordStore 代码区里所有写 `[xN,#0x18]` 的指令，
   发现 `.ctor` 与 `Setup` 打的是**同一个槽**，即全局 `T`；`Setup` 是
   唯一写入者，`.ctor` 只是拷贝。
   **第一版少解一级引用**（漏了 `ldr x8,[x8]`），读出"全局对象从未构造"
   —— 假结论来自读错对象，不报错。run41b 修正。

3. **run42** —— 反射主动调用 `Setup()`：**能调**，但槽仍是空的。

4. **run43** —— 探针装在写入指令之后，**一次都没触发** ⇒ Setup 没走到
   那一行（读码说 factory 对 `w0=4` 不可能返回 NULL）。

5. **run44** —— **SIGILL（signum 4）**。入口探针搬走的前四条含
   `adrp`（PC 相对），在 trampoline 里 `x19` 拿到错页，`strb` 写到野地址。
   **铁律：不要 hook 前四条含 adrp/adr/分支的地址**（现有五个 stub 是碰巧安全）。
   副作用收获：探针 #3 显示 **游戏自己调用 Setup**（lr=+0x7eff54，
   对上调用点 0x7eff50）⇒ "Setup 从未运行"被彻底否证。

6. **run45 / run46** —— 静态字段 slot 列表给了 `usePreference_` = 0x20、
   `rootStorage_` = 0x18，而 `GetNumRecords` 恰好读这两个：
   `ldrb w8,[T,#0x20]; cbz w8, ->storage_路径`。
   实测旧值 = 0，置 1 后：
   - `throw: #1` **完全消失**（只剩 hook 安装日志）
   - 游戏从"第 2 帧退出"变成**跑满 45 秒**、加载资源、写 SharedPreferences
   - **但错误 100 对话框仍在** —— 印证 run39i 推论：对话框不是故障源，
     且自检**不止一个条件**；下一个嫌疑是
     `Utility.putPreference() -> null (no such service)`（空实现）

### 可复用铁律（新增两条）

- **转储代码里的解引用级数必须逐条对照汇编数**。少一级不报错，只给一个
  看起来很合理的假结论。
- **hook 目标的前四条指令不能含 adrp/adr/分支**。trampoline 是"搬到别处
  执行"，PC 相对指令搬过去就算错；分支搬过去跳错地方。
- **幂等标志的经典陷阱**："块还没分配"时不能记为 done，否则第一次空转
  会静默关掉后面所有机会。改成沿途多点尝试（getAppWidth → getScaleRatio
  → throw 兜底）。

### 设备侧新坑

- **ArkOS 重启会清空 `/tmp`**，`r39g.sh` 依赖 `/tmp/bb_launch.sh`，
  重连后必须先重传该脚本（症状是日志 0 字节，看起来像二进制没部署）。
- 设备 SSH 连接**偶发 8 次全超时**（设备在线、uptime 正常），重试通常能通。
"""

if os.path.exists(P):
    t = io.open(P, encoding="utf-8", newline="").read()
else:
    t = "# 2026-10-05\n"

if "## run40-run46" not in t:
    t = t.rstrip("\n") + "\n" + SEC
    io.open(P, "w", encoding="utf-8", newline="").write(t)

print("ok: daily log", len(t), "chars")
