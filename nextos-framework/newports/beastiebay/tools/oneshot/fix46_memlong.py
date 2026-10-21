# -*- coding: utf-8 -*-
"""Replace the run39i 'in progress' bullet in MEMORY.md with the run40..46 outcome."""
import io

P = r"D:\ProgramData\nextos-framework\.workbuddy\memory\MEMORY.md"
lines = io.open(P, encoding="utf-8", newline="").read().split("\n")

start = None
for i, ln in enumerate(lines):
    if ln.startswith("- run39i") and "\u8fdb\u884c\u4e2d" in ln:
        start = i
        break
if start is None:
    raise SystemExit("start not found")

end = start + 1
while end < len(lines) and not lines[end].startswith("- "):
    end += 1

NEW = [
    "- **run40-run46：空引用已消除，错误 100 仍在（自检不止一个条件）**：",
    "  - 抛出点归属 = **`RecordStore.GetNumRecords`**（0xa4a58c，抛在 0xa4a674）；",
    "    `SetPreferenceMode`(0xa4ad38) 有 **0 个 bl 调用点**，游戏从不调它。",
    "  - 全局链：`*( *( *(base+0x1644f60) ) + 0xb8)` = **静态字段块 T**（不是对象）；",
    "    `T+0x18` = `rootStorage_`（Setup 唯一写入者，`.ctor` 只拷贝）；",
    "    `T+0x20` = **`usePreference_`**。`GetNumRecords` 先读 `T+0x20`：非 0 则走",
    "    preference 分支、**根本不碰 storage_**；为 0 则读 `storage_` 崩溃。",
    "  - **修法（run46）**：把静态块 `T+0x20` 置 1（实测旧值 = 0）。置位点必须在",
    "    第一次 GetNumRecords 之前：沿途多点尝试（getAppWidth → getScaleRatio →",
    "    throw 兜底），因为 Config.init 时静态块还未分配。",
    "  - **效果**：`throw: #1` 完全消失；游戏从「第 2 帧退出」变成跑满 45s、",
    "    加载资源、写 SharedPreferences。",
    "  - **但错误 100 对话框仍在** → 印证「对话框不是故障源」，且自检还有第二个",
    "    条件。下一个嫌疑：`Utility.putPreference() -> null`（空实现），存档写不下去。",
    "- **新增两条 hook/诊断铁律**：",
    "  1. **不要 hook 前四条指令含 `adrp`/`adr`/分支的地址** —— trampoline 是",
    "     「搬到别处执行」，PC 相对指令搬过去就算错（run44 因此 SIGILL）。现有五个",
    "     stub 是碰巧安全。",
    "  2. **转储代码的解引用级数必须逐条对照汇编数** —— 少一级不报错，只给一个",
    "     看起来很合理的假结论（run41 第一版就是这样）。",
    "  3. **幂等标志的陷阱**：「块还没分配」时不能记为 done，否则第一次空转会静默",
    "     关掉后面所有机会。",
]

lines[start:end] = NEW
io.open(P, "w", encoding="utf-8", newline="").write("\n".join(lines))
print("ok: MEMORY.md updated, %d lines" % len(lines))
