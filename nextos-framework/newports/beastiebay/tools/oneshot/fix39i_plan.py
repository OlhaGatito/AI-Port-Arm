# -*- coding: utf-8 -*-
"""Acrescenta ao PLAN.md a identificacao do root cause (run39i).

run39h mediu ONDE a excecao e' lancada.  run39i mediu QUEM esta' vazio, e a
resposta veio da reflexao, nao de outra leitura de codigo.  Esta e' a
conclusao que organiza todo o trabalho restante.
"""
import io
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PATH = os.path.join(os.path.dirname(os.path.dirname(HERE)), "docs", "PLAN.md")

MARK = "### run39h 实测结果：抛出点**不在** `BootForm::Init`"

NEW = """### run39i：根因锁定 —— `kairo.unity.io.RecordStore.storage_` 是 NULL

run39h 回答了"在哪抛"。剩下的问题是"谁空的"，而**离线读码回答不了**：
抛出点不在托管方法表里，没有类名可查。

于是把 stub 改成同时传 `x30`（返回地址）与 `x19`（`this`），
再用 `il2_class_of()` 问反射。答案一行就出来了：

```
[fp2] throw: #1 chamada em libil2cpp.so+0xa4a674  self=0x7f75a272a0  self+0x18=(nil)
[fp2] campo: ---- kairo.unity.io.RecordStore @ 0x7f75a272a0 ----
[fp2] campo:  this+0x10  rsId_           ok
[fp2] campo:  this+0x18  storage_        NULL  <<<   ← 就是它
[fp2] campo:  this+0x20  encryptionKey_  NULL  <<<
```

**`RecordStore` 是存档后端。** `rsId_`（存档 ID）已就位，
但真正干活的 `storage_` 和 `encryptionKey_` 都是 NULL。

#### 完整因果链（全部实测，无推断环节）

```
Utility.existPreference / getPreference / putPreference / removePreference
        │  全部绑定到 j_Utility_rest —— existPreference 恒 false，
        │  getPreference 恒 null，putPreference 空实现
        ▼
RecordStore 建不出 storage_（和 encryptionKey_）
        ▼
storage_ == NULL
        ▼
0xa4a664  ldr x20, [x19, #0x18]   ; storage_
0xa4a668  cbnz x20, ...
0xa4a670  bl 0x600888             ; NullReferenceException
        ▼
游戏启动自检的 try/catch 捕获 → Utility.showDialog(0, "Error",
                                        "An error has occurred.\\n100", null)
```

日志里的直接佐证：游戏解析了这些 MethodID
（`getPreferenceKeys` / `existPreference` / `getPreference` /
`putPreference` / `removePreference` / `saveToSd`），
但**一次都没有真正调用**——因为它们返回的是空实现，
游戏走到 `RecordStore` 就已经拿不到东西了。

#### 这一条同时解释了 run25 与 run39h 的冲突

`tools/meta_dump.py` 记录的 run25 结论是"错误 100 不是异常，
是游戏自检失败后主动调 `showDialog`"。run39h 测到
`NullReferenceException`。**两者都对**：

游戏的启动自检是 `try/catch`。空引用被游戏自己捕获，
再由它调用 `showDialog` 显示错误 100。

**推论（对验收很重要）**：修好 `Preference` 之后，
错误对话框**不会**消失——它本来就不是故障源。
**游戏会继续往下跑**。验收标准必须是"进入游戏主流程"，
而不是"看不到 100"。

#### 硬编码类名的教训（同一个错误，第二次犯）

run39g 的 `fp2_dump_bootform_fields()` 里写死了 `il2_class_by_name("BootForm")`。
run39i 把它改成 `il2_class_of(self)` 拿**对象自己的类**。

如果当时没改，这次日志会打出一份 `BootForm` 的字段表，
`+0x18` 显示 `leftSoftLabel_` 非空，看起来"这个对象没问题"——
**把"读错了对象"误报成"对象是好的"**。
方向反了，但一样是错的。

> 规则：**诊断代码永远不要假定对象的类型**。
> 拿得到就问反射；拿不到就明说"不知道"，不要用假设填空。
"""


def main():
    with io.open(PATH, encoding="utf-8", newline="") as f:
        text = f.read()

    i = text.find(MARK)
    if i < 0:
        sys.exit("aborted: marker not found")

    head = text[:i]
    with io.open(PATH, "w", encoding="utf-8", newline="") as f:
        f.write(head + NEW)
    print("appended run39i root cause; %d -> %d chars"
          % (len(text), len(head) + len(NEW)))


if __name__ == "__main__":
    main()
