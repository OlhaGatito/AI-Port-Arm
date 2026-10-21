# -*- coding: utf-8 -*-
import io
p = r"D:/ProgramData/nextos-framework/.workbuddy/memory/MEMORY.md"
s = io.open(p, encoding="utf-8", newline="").read()
anchor = "- run39i\uff08\u8fdb\u884c\u4e2d\uff09\uff1astub"
i = s.find(anchor)
if i < 0:
    raise SystemExit("anchor not found")
NEW = (
    "- **\u9519\u8bef 100 \u6839\u56e0\u5df2\u9501\u5b9a\uff08run39i\uff0c\u5168\u94fe\u5b9e\u6d4b\uff09\uff1a"
    "`kairo.unity.io.RecordStore.storage_`\uff08`+0x18`\uff09\u4e3a NULL\u3002\u955c\u50cf\u76f4\u63a5\u7ed9\u51fa\u771f\u5b9e\u7c7b\u540d\uff1a\n"
    "  ```\n"
    "  [fp2] throw: #1 ... self=0x7f75a272a0  self+0x18=(nil)\n"
    "  [fp2] campo: ---- kairo.unity.io.RecordStore @ 0x7f75a272a0 ----\n"
    "  [fp2] campo:  this+0x10  rsId_           ok\n"
    "  [fp2] campo:  this+0x18  storage_        NULL  <<<\n"
    "  [fp2] campo:  this+0x20  encryptionKey_  NULL  <<<\n"
    "  ```\n"
    "  \u56e0\u679c\uff1a`Utility.existPreference/getPreference/putPreference/removePreference`\n"
    "  \u5168\u90e8\u7ed1\u5b9a\u5230 `j_Utility_preference`\uff08exist \u6052 false\u3001get \u6052 null\u3001put \u7a7a\u5b9e\u73b0\uff09\n"
    "  \u2192 RecordStore \u5efa\u4e0d\u51fa storage_ \u2192 \u7a7a\u5f15\u7528 \u2192 \u81ea\u68c0 try/catch \u2192 showDialog(100)\u3002\n"
    "  \u65e5\u5fd7\u4f50\u8bc1\uff1a\u6e38\u620f\u89e3\u6790\u4e86\u8fd9\u4e9b MethodID\uff0c\u4f46**\u4e00\u6b21\u90fd\u6ca1\u771f\u6b63\u8c03\u7528**\u3002\n"
    "  **\u4fee\u597d\u540e\u9519\u8bef\u5bf9\u8bdd\u6846\u4e0d\u4f1a\u6d88\u5931\uff08\u5b83\u672c\u6765\u5c31\u4e0d\u662f\u6545\u969c\u6e90\uff09\uff0c\u6e38\u620f\u4f1a\u7ee7\u7eed\u5f80\u4e0b\u8dd1**\uff1b\n"
    "  \u9a8c\u6536\u6807\u51c6\u5fc5\u987b\u662f\u201c\u8fdb\u5165\u4e3b\u6d41\u7a0b\u201d\uff0c\u4e0d\u662f\u201c\u770b\u4e0d\u5230 100\u201d\u3002\n"
    "- **\u8bca\u65ad\u4ee3\u7801\u7ea2\u7ebf**\uff1a\u4e0d\u8981\u5047\u5b9a\u5bf9\u8c61\u7c7b\u578b\u3002run39g \u5199\u6b7b `il2_class_by_name(\"BootForm\")`\uff0c\n"
    "  \u82e5\u4e0d\u6539\u6210 `il2_class_of(self)`\uff0c\u672c\u6b21\u4f1a\u6253\u51fa BootForm \u7684\u5b57\u6bb5\u8868\u3001`+0x18` \u663e\u793a\u975e\u7a7a\uff0c\n"
    "  **\u628a\u201c\u8bfb\u9519\u4e86\u5bf9\u8c61\u201d\u8bef\u62a5\u6210\u201c\u5bf9\u8c61\u662f\u597d\u7684\u201d** \u2014\u2014 \u65b9\u5411\u53cd\u4f46\u4e00\u6837\u9519\u3002\n"
)
io.open(p, "w", encoding="utf-8", newline="").write(s[:i] + NEW + s[i:])
print("memory updated for run39i")
