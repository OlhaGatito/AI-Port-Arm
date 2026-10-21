# -*- coding: utf-8 -*-
"""Restaura a secao perdida do run39h e ordena as secoes.

`fix39i_plan.py` truncou em vez de inserir: usei head[:i] + NEW, o que
descartou tudo entre o marcador e o fim do arquivo.  O resultado foi um
PLAN.md mais curto, com a secao "run39h 实测结果" (que e' a que diz que o
ponto de lancamento NAO esta em BootForm::Init) fora do documento.

Aqui a secao e' reescrita e inserida ANTES da run39i, preservando o resto.
"""
import io
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PATH = os.path.join(os.path.dirname(os.path.dirname(HERE)), "docs", "PLAN.md")

MARK = "### run39i：根因锁定"

SECTION = """### run39h 实测结果：抛出点**不在** `BootForm::Init`

修好上面那个 `mov x0, x21` 之后，偏移读出来了：

```
[fp2] throw: prologo de 0x600888 confere (sub sp/stp/add/mov x0,sp)
[fp2] throw: auxiliar de excecao observado em 0x7fa4162888
[fp2] throw: #1 chamada em libil2cpp.so+0xa4a674
[fp2] throw: #2 chamada em libil2cpp.so+0xa4a674
```

**`0xa4a674`，不是预测的 `0x897148`。**

`BootForm::Init` 在 `0x897068`，两者相距 **0x1B3F2C**。
**此前"错误 100 来自 `Init` 的九处 `cbnz`"这个推断，被测量推翻。**

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

#### 剩下的两种可能

`Init` 入口的字段转储（run39g）里 `+0x18` 是基类的 `leftSoftLabel_`，
当时**非 NULL**。但抛出发生在 `Init` **之后**。

1. `self` 就是 `BootForm`，`leftSoftLabel_` 在 `Init` 过程中被置空
2. `self` 是另一个对象，`+0x18` 一直是 NULL

**离线无法区分**（方法表里没有这个地址）。
⇒ run39i：把 `x19` 一起传出来，问反射。**结果见下一节。**

"""


def main():
    with io.open(PATH, encoding="utf-8", newline="") as f:
        text = f.read()

    i = text.find(MARK)
    if i < 0:
        sys.exit("aborted: marker not found")

    head = text[:i]
    rest = text[i:]
    with io.open(PATH, "w", encoding="utf-8", newline="") as f:
        f.write(head + SECTION + rest)
    print("inserted run39h section; %d -> %d chars"
          % (len(text), len(head) + len(SECTION) + len(rest)))


if __name__ == "__main__":
    main()
