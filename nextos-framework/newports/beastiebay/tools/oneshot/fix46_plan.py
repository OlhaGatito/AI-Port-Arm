# -*- coding: utf-8 -*-
"""Append run45 and run46 to docs/PLAN.md."""
import io
import os

HERE = os.path.dirname(os.path.abspath(__file__))
PATH = os.path.join(os.path.dirname(os.path.dirname(HERE)), "docs", "PLAN.md")

t = io.open(PATH, encoding="utf-8", newline="").read()

SEC = """
### run45：翻掉一个字节，异常从 2 次降到 1 次

run44 死在 SIGILL（signum 4）。原因是**入口探针本身**：

```
0xa4adf8  str  x19, [sp, #-0x20]!
0xa4adfc  stp  x29, x30, [sp, #0x10]
0xa4ae00  add  x29, sp, #0x10
0xa4ae04  adrp x19, #0x1767000   <- PC 相对
```

trampoline 把这四条搬到别处执行，而 `adrp` 是 PC 相对的 —— 在新地址上
`x19` 拿到的是**错的页**，紧接着的 `strb w8, [x19, #0xe8a]` 就写到了
不该写的地方。

> 铁律：**不要 hook 前四条指令里含 `adrp` / `adr` / 任何分支的地址。**
> 现有五个方法 stub 是碰巧安全，不是设计安全。

run45 撤掉两个探针，把置位挪到 throw 时刻（那时静态块必定已存在）：

```
usepref: usePreference_ 0 -> 1  (rootStorage_ T+0x18=(nil))
```

**旧值确实是 0** —— 假设被验证。而且 `throw:` 从 2 次降到 1 次。

### run46：在第一次 `GetNumRecords` 之前置位

run45 置位在 throw 处理里，也就是**第一次已经崩完**之后。run44 测过
静态块在 Config.init 时还没分配，所以单个早期点不够 —— 改为**沿途反复
尝试**：`getAppWidth/Height` → `getScaleRatio` → throw 兜底。

幂等，且**块为 NULL 时不能记为 done**，否则第一次空转会静默关掉后面
所有机会。

结果：

```
=== throw count ===
2
[fp2] throw: prologo de 0x600888 confere
[fp2] throw: auxiliar de excecao observado em 0x7f74e01888
```

两行**都是 hook 安装信息，没有一条 `throw: #1`**。空引用异常消失了。

#### 但错误 100 还在 —— 这本身就是结论

```
[fp2/sdk] Utility.showDialog(id=0, title="Error", msg="An error has occurred.
```

这印证了 run39i 的推论：对话框**不是故障源**，修掉空引用它也不会消失。
同时也说明自检**不止一个条件**：空引用只是第一个暴露出来的，后面还有
一个。日志里直接给出了下一个嫌疑：

```
[fp2] jni: Utility.putPreference() -> null (no such service on this device)
```

游戏在写存档，而 `putPreference` 是空实现。初始化数据存不下去，自检
自然还是失败。

#### 顺带的进展

- 游戏从"第 2 帧就退出"变成**跑满 45 秒**，加载资源、跑主循环
- `existPreference` / `getPreference` / `putPreference` / `removePreference`
  的 MethodID 全部被解析 —— 说明 preference 分支真的被走到了，
  此前"一次都没调用"是因为根本没走到那一支

### 下一步

实现真实的 preference 存储（内存 + 落盘），让 `existPreference` /
`getPreference` / `putPreference` / `removePreference` 有真实语义，
而不是恒 false / null / 空。
"""

if "### run45：翻掉一个字节" not in t:
    t = t.rstrip("\n") + "\n" + SEC

io.open(PATH, "w", encoding="utf-8", newline="").write(t)
print("ok: PLAN.md now", len(t), "chars")
