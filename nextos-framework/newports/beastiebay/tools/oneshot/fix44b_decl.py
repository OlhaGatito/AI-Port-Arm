# -*- coding: utf-8 -*-
"""Forward-declare fp2_set_use_preference (definition lands after the call)."""
import io
import os

HERE = os.path.dirname(os.path.abspath(__file__))
PATH = os.path.join(os.path.dirname(os.path.dirname(HERE)),
                    "port", "src", "jni.c")

s = io.open(PATH, encoding="utf-8", newline="").read()

ANCHOR = "static void fp2_try_setup_recordstore(const char *onde)"
assert ANCHOR in s, "anchor not found"
assert "static void fp2_set_use_preference(void);" not in s, "already declared"

s = s.replace(ANCHOR,
              "static void fp2_set_use_preference(void);\n\n" + ANCHOR, 1)

io.open(PATH, "w", encoding="utf-8", newline="").write(s)
print("ok: forward declaration added")
