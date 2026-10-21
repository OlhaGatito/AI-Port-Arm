# -*- coding: utf-8 -*-
"""Acrescenta ao PLAN.md a conclusao do run39h medido.

O run39h mediu o ponto de lancamento e ele NAO esta em BootForm::Init.  Tudo
que o PLAN diz antes disso -- "nove pontos cbnz dentro de Init", "o primeiro
falhou" -- e' inferencia agora DESMENTIDA por medida.  Deixar isso no
documento sem aviso faria o proximo leitor comecar pelo lugar errado.
"""
import io
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PATH = os.path.join(os.path.dirname(os.path.dirname(HERE)), "docs", "PLAN.md")

MARK = "### 下一步（run39h 修正版）"

NEW = """### run39h 实测结果：抛出点**不在** `BootForm::Init`

修好 `mov x0, x19`（见上一节的第二个坑）之后，偏移读出来了：

```
[fp2] throw: prologo de 0x600888 confere (sub sp/stp/add/mov x0,sp)
[fp2] throw: auxiliar de excecao observado em 0x7fa4162888
[fp2] throw: #1 chamada em libil2cpp.so+0xa4a674
[fp2] throw: #2 chamada em libil2cpp.so+0xa4a674
```

**`0xa4a674`，不是预测的 `0x897148`。**

`BootForm::Init` 在 `0x897068`，两者相距 **0x1B3F2C**。
**此前"错误 100 来自 `Init` 的九处 `cbnz`"这个推断，被 measurement 推翻。**

#### 抛出点的代码

```
0xa4a664  ldr      x20, [x19, #0x18]     ; 取字段
0xa4a668  cbnz     x20, #0xa4a674        ; 判空
0xa4a66c  mov      x0, xzr
0xa4a670  bl       #0x600888             ; 抛异常
0xa4a674  mov      x0, x20               ; ← x30 读到的就是这个
```

`x30` 读数与 `bl` 的下一条指令**逐字节吻合** ⇒ 测量方法本身成立，
读数可信，只是被测对象和原先的假设不是同一个。

#### `0x600888` 是什么：`NullReferenceException`

它的实现里有两个 `.rodata` 字符串指针：

```
0x1115be5 → "System"
0x11b1efb → "NullReferenceException"
```

即 `il2cpp_codegen_raise_null_reference_exception()`。
它不在 `.dynsym`（`tools/throw_sym.py` 实测窗口内 0 符号），
所以按 RVA + 首 4 指令机器码定位是对的。

**结论：错误 100 是一个空引用。**

#### 这个函数不是托管方法

`0xa4a59c` 处的 `stp x29, x30` 不是函数入口：真实的 `stp x29, x30` 在
`0xa4a408`，而 `0xa4a59c` 是同一函数内部的第二个序言
（`mov x19, x0` 出现在那里，`x19` 就是 `this`）。

两条独立证据说明它**不在托管方法表里**：

| 判据 | 结果 |
|---|---|
| `tools/find_method_index.py` 查 43373 项方法指针表 | **无此地址** |
| 全量扫描所有 `bl` 指令找指向 `0xa4a59c` 的调用 | **0 处** |
| 同工具查已确认的 `BootForm::Init`（`0x897068`） | **命中 slot 27066**，邻居正是 run39 打印的 `.ctor`/`ToString`/`Update` |

第三条是对照实验：工具本身是对的。

⇒ **这是 IL2CPP 运行时代码，不是游戏自己的方法。**
`x19`（`this`）因此**未必是 `BootForm`**。

#### 与 run25 结论的冲突（必须调和，不能忽略）

`tools/meta_dump.py` 的文档记录 run25 的结论：

> error 100 is NOT an exception and NOT a crash: run25 proved the game
> deliberately calls `Utility.showDialog(0, ...)` after its own start-up
> self-check fails.

而 run39h **测量到** `NullReferenceException` 在 `showDialog` 之前抛出两次。

两者可以同时成立，只有一个解释：

> **游戏的启动自检是一个 `try/catch`。** 异常被游戏自己捕获，
> 然后由它主动调用 `showDialog` 显示"错误 100"。
> run25 看到了"主动调 showDialog"（那是真的），但没看到它上游的
> `catch`（那也是真的）。

**这条调和很重要**：它意味着修好空引用**不会**让错误 100 消失，
而是会让游戏**继续往下跑**。这正是想要的结果，
但也意味着"修好之后仍要验证后续流程"，不能只看对话框是否消失。

#### `this+0x18` 是什么：两种可能，尚需实测

`Init` 入口的字段转储（run39g）里，`+0x18` 是基类的 `leftSoftLabel_`，
且当时**非 NULL**（打印为 `ok`）。

但抛出发生在 `Init` **之后**，`+0x18` 那时已是 NULL。
所以有两种可能，**必须实测区分**：

1. `self` 就是 `BootForm`（或它的子类），`leftSoftLabel_` 在 `Init`
   过程中被置空 ⇒ 追"谁写 null"
2. `self` 是另一个对象，`+0x18` 一直是 NULL ⇒ 是别处的初始化缺失

**离线无法区分**，因为方法表里没有这个地址。
⇒ run39i：让 stub 把 `x19`（`this`）一起传出来，
用 `il2_class_of()` 问反射它的**真实类名**，
并按真实类型转储字段（不再硬编码 `BootForm`）。

> 已改：`fp2_dump_bootform_fields()` 现在用
> `il2_class_of(self)` + `il2_class_label()`，
> 新增 `il2_class_label()` 于 `il2.c`。
> **理由**：硬编码类名在对象不是该类时会让转储"看起来是空的"，
> 也就是把"读错了对象"误报成"对象是空的"——同一类错误，
> 只是换了个方向。
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
    print("appended run39h findings; %d -> %d chars" % (len(text),
                                                        len(head) + len(NEW)))


if __name__ == "__main__":
    main()
