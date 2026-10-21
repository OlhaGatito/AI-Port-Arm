# -*- coding: utf-8 -*-
"""Atualiza a secao beastiebay em .workbuddy/memory/MEMORY.md.

O run39h mediu o ponto de lancamento e ele NAO esta em BootForm::Init.
A anotacao antiga dizia o contrario, com um " proximo passo" que apontava
para a tabela de nove `cbnz` -- um caminho que a medicao acabou de
descartar.  Sem esta edicao, a proxima sessao comeca pelo lugar errado.
"""
import io
import os

PATH = os.path.join(
    "D:\\", "ProgramData", "nextos-framework", ".workbuddy", "memory",
    "MEMORY.md")

OLD = """- **run39h \u5df2\u786e\u8ba4 `0x600888` \u5c31\u662f\u9519\u8bef 100 \u7684\u629b\u51fa\u70b9**\uff08\u5b9e\u673a\u88ab\u8c03\u7528 2 \u6b21\uff0c\u7d27\u90bb showDialog(100)\uff09\u3002
  \u4e0b\u4e00\u6b65\uff1a\u4fee `mov x0, x19` \u540e\u91cd\u65b0\u8bfb\u51fa\u771f\u5b9e\u8c03\u7528\u504f\u79fb\uff0c\u5bf9\u7167\u4e5d\u5904\u68c0\u67e5\u8868\u5b9a\u4f4d\u3002"""

NEW = """- **\u9519\u8bef 100 = NullReferenceException**\uff08run39h \u5b9e\u6d4b\u63a8\u7ffb\u65e7\u63a8\u65ad\uff09\uff1a
  \u629b\u51fa\u70b9\u8fd4\u56de\u5730\u5740 = **`0xa4a674`**\uff08**\u4e0d\u662f** `BootForm::Init` \u7684 `0x897148`\uff0c
  \u4e24\u8005\u76f8\u8ddd 0x1B3F2C\uff09\u3002\u4ee3\u7801\u4e3a
  `ldr x20,[x19,#0x18]; cbnz; mov x0,xzr; bl 0x600888`\u3002
  `0x600888` = `il2cpp_codegen_raise_null_reference_exception`\uff08\u7528 "System" \u4e0e
  "NullReferenceException" \u4e24\u4e2a rodata \u5b57\u7b26\u4e32\u6784\u9020\uff09\uff0c\u4e0d\u5728 .dynsym\u3002
  **\u8be5\u51fd\u6570\u4e0d\u662f\u6258\u7ba1\u65b9\u6cd5**\uff1a43373 \u9879\u65b9\u6cd5\u6307\u9488\u8868\uff08.data.rel.ro\uff0c
  \u5de5\u5177 `tools/find_method_index.py`\uff09\u91cc\u65e0\u6b64\u5730\u5740\uff0c\u5168\u91cf `bl` \u626b\u63cf\u4e5f\u65e0\u8c03\u7528\u70b9
  \uff08\u5bf9\u7167\uff1a`0x897068` \u547d\u4e2d slot 27066\uff0c\u90bb\u5c45\u6b63\u662f run39 \u6253\u5370\u7684\u65b9\u6cd5\u8868\uff09
  \u21d2 \u5c5e **IL2CPP \u8fd0\u884c\u65f6\u4ee3\u7801**\uff0c`x19`(=this) \u672a\u5fc5\u662f BootForm\u3002
- **\u4e0e run25 \u7ed3\u8bba\u7684\u8c03\u548c**\uff1arun25 \u8bf4\u201c\u9519\u8bef 100 \u4e0d\u662f\u5f02\u5e38\uff0c\u662f\u6e38\u620f\u81ea\u68c0\u5931\u8d25\u540e\u4e3b\u52a8\u8c03
  showDialog\u201d\uff1b\u4e24\u8005\u90fd\u6210\u7acb \u2014\u2014 **\u6e38\u620f\u81ea\u68c0\u662f try/catch**\uff0c\u5f02\u5e38\u88ab\u6e38\u620f\u81a7\u5df1
  \u6355\u83b7\u540e\u7531\u5b83\u8c03 showDialog\u3002\u21d2 \u4fee\u597d\u7a7a\u5f15\u7528\u540e**\u5bf9\u8bdd\u6846\u4e0d\u4f1a\u6d88\u5931\uff0c
  \u800c\u662f\u6e38\u620f\u7ee7\u7eed\u5f80\u4e0b\u8dd1**\uff1b\u9a8c\u6536\u8981\u770b\u540e\u7eed\u6d41\u7a0b\u800c\u4e0d\u662f\u5bf9\u8bdd\u6846\u3002
- run39i\uff08\u8fdb\u884c\u4e2d\uff09\uff1astub \u5df2\u6539\u4e3a\u540c\u65f6\u4f20 x30 \u4e0e x19\uff0c`fp2_dump_bootform_fields()`
  \u6539\u7528 `il2_class_of()` \u53d6**\u771f\u5b9e\u7c7b\u540d**\uff08\u65b0\u589e `il2_class_label()`\uff09\uff0c\u4e0d\u518d\u786c\u7f16\u7801 BootForm\u3002"""


def main():
    with io.open(PATH, encoding="utf-8", newline="") as f:
        s = f.read()
    if OLD not in s:
        raise SystemExit("aborted: anchor not found")
    with io.open(PATH, "w", encoding="utf-8", newline="") as f:
        f.write(s.replace(OLD, NEW))
    print("memory updated")


if __name__ == "__main__":
    main()
