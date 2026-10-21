# -*- coding: utf-8 -*-
"""Record run40..run44 in docs/PLAN.md (appended, never replacing)."""
import io
import os

HERE = os.path.dirname(os.path.abspath(__file__))
PATH = os.path.join(os.path.dirname(os.path.dirname(HERE)), "docs", "PLAN.md")

t = io.open(PATH, encoding="utf-8", newline="").read()

SEC = """

### run40：让转储说出值，并把方法表也打出来

run39i 只回答了"谁空的"。剩下"谁该填"，那是**代码**问题，不是数据问题。

两处改动：
- `fp2_dump_fields_cb` 从打印 `ok` 改成打印**指针**。`ok` 只回答
  "是不是空的"，把地址扔了，而地址才是能跨轮比较的东西。
- 字段转储之后追加**方法表**（`il2_methods_foreach`）。方法名在元数据里，
  地址在指针表里，`nm` 两个都拿不全；反射两个都有。

结果：`RecordStore` 的方法名 + RVA 全出来了，其中

```
.ctor            0xa49e5c / 0xa49ed0
SetPreferenceMode 0xa4ad38   <- 0 个调用点，从未被调用
Setup            0xa4adf8
GetNumRecords    0xa4a58c
OpenRecordStore  0xa4e63c / 0xa4e6a4
```

**抛出点 `0xa4a674` 落在 `GetNumRecords`（0xa4a58c）与
`SetPreferenceMode`（0xa4ad38）之间** —— 抛异常的是
`RecordStore.GetNumRecords()`，不是之前的任何猜测。

### run41：全局对象 `T`

扫描 RecordStore 代码区（0xa49e5c..0xa4ef40）里所有写 `[xN,#0x18]`
的指令，得四处；其中 `.ctor#1` 与 `Setup` 打的是**同一个槽**：

```
.ctor   adrp x8,#0x1644000; ldr x8,[x8,#0xf60]; ldr x8,[x8]
        ldr x8,[x8,#0xb8];  ldr x8,[x8,#0x18];  str x8,[x19,#0x18]
Setup   同上取到 +0xb8；str <0xa4a3fc 的返回值>,[x9,#0x18]
```

即：存在全局 `T`，**`Setup` 是唯一的写入者**，`.ctor` 只是拷贝。

run41 第一版**少解一级引用**（漏了 `ldr x8,[x8]`），读到 `(p+0xb8)`
那个恰好为 0 的字节，报出"全局对象从未构造"——错误结论来自读错对象。
run41b 修正后：

```
raiz: p=0x7facf4e0c0  s=*p=0x55ab650080  T=(s+0xb8)=0x7fab609cc0
raiz: T+0x13=154  T+0x18=(nil)  T+0x50=0x7f9a0d9c80
```

`T` 存在，`T+0x18` 是 NULL。`T+0x50` 已被赋值，而写 `+0x50` 的那条
指令（`0x772f14`）紧挨在 `bl Setup` **之前** —— 初始化流程走到了那儿。

> 教训：转储代码里的解引用级数必须逐条对照汇编数。
> 少一级不会报错，只会给你一个"看起来很合理"的假结论。

### run42：主动调用 `Setup()`

既然 `Setup` 是唯一写入者，自己调它既验证又修复：

```
setup: classe kairo.unity.io.RecordStore
setup: Setup() retornou (nil)
raiz: T+0x18=(nil)          <- 没填上
```

`Setup` **能被调用**（反射找到了静态 `Setup/0`），但执行后槽仍是空的。

### run43：探针装在写入之后，一次都没触发

hook `0xa4ae74`（`str x8,[x9,#0x18]` 的下一条，此时 `x8` 还是 factory
返回值）。探针装上，但**从不触发** —— `Setup` 根本没走到那一行。

这排除了"factory 返回 NULL"，因为读码说它对 `w0=4` 走跳转表 case 3、
分配后返回指针，只有 `w0=0` 或 `w0>4` 才到 `mov x22,xzr`。

`Setup` 提前离开的最可能位置是 `0xa4ae44 bl 0x5e7c3c`（类初始化）。

### run44：`usePreference_` —— 两条独立线索指向同一个字节

静态字段的 slot 列表（run40）给出：

```
rootStorage_     slot 0x18
usePreference_   slot 0x20
```

而 `GetNumRecords` 读的正是这两个偏移：

```
0xa4a62c  ldr  x8, [x8, #0xb8]    ; T（静态块）
0xa4a630  ldrb w8, [x8, #0x20]    ; usePreference_
0xa4a634  cbz  w8, #0xa4a664      ; 为 0 -> 走 storage_ 那条路
0xa4a664  ldr  x20, [x19, #0x18]  ; storage_（rootStorage_ 的拷贝）
0xa4a668  cbnz x20, ...
0xa4a670  bl   0x600888           ; NullReferenceException
```

**`usePreference_` 非 0 时，代码根本不碰 `storage_`。**

这同时解释了此前的"preference 方法一次都没被调用"：那从来不是证据，
游戏只是**没走到会去问的那一支**。

Kairosoft 的插件带了完整的 SharedPreferences 契约
（`existPreference` / `getPreference` / `putPreference` /
`removePreference`），本 port 也实现了 —— 走 preference 分支才是
Android 上的本意。

于是 run44 的实验最小：**把静态块 `T+0x20` 这一字节置 1**，打印前后值。
改的是一个静态字节，不是代码，前后值都落日志，可逆。
"""

if "### run40：让转储说出值" not in t:
    t = t.rstrip("\n") + "\n" + SEC

io.open(PATH, "w", encoding="utf-8", newline="").write(t)
print("ok: PLAN.md updated to", len(t), "chars")
