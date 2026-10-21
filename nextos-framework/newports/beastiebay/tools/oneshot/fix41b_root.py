# -*- coding: utf-8 -*-
"""run41b: the first root dump dereferenced one level too few.

The disassembly is four instructions, and I wired up three:

    adrp x8, #0x1644000
    ldr  x8, [x8, #0xf60]   ; p  = *(slot)
    ldr  x8, [x8]           ; s  = *p        <-- dropped in run41
    ldr  x8, [x8, #0xb8]    ; T  = *(s+0xb8)

Reading *(p+0xb8) instead of *(*p+0xb8) reported "T is NULL -- the global
object was never built".  That conclusion was about the wrong object; the
byte at p+0xb8 happens to be zero.  The game itself walks this chain
successfully at .ctor time (it stores a value rather than faulting), so
following it exactly is both correct and safe.
"""
import io
import os

HERE = os.path.dirname(os.path.abspath(__file__))
PATH = os.path.join(os.path.dirname(os.path.dirname(HERE)),
                    "port", "src", "jni.c")

s = io.open(PATH, encoding="utf-8", newline="").read()

OLD = """    const unsigned char *slot =
        (const unsigned char *)m->base + 0x1644000u + 0xf60u;
    void *s = *(void *const *)slot;
    if (!s) {
        nx_log("raiz: slot 0x1644f60 -> NULL (bloco estatico nao montado)");
        return;
    }
    void *t = *(void *const *)((const char *)s + 0xb8);
    nx_log("raiz: S=%p  T=(S+0xb8)=%p", s, t);"""

NEW = """    const unsigned char *slot =
        (const unsigned char *)m->base + 0x1644000u + 0xf60u;
    void *p = *(void *const *)slot;
    if (!p) {
        nx_log("raiz: slot 0x1644f60 -> NULL (bloco estatico nao montado)");
        return;
    }
    void *s = *(void *const *)p;
    if (!s) {
        nx_log("raiz: *(0x1644f60)=%p mas *p e' NULL", p);
        return;
    }
    void *t = *(void *const *)((const char *)s + 0xb8);
    nx_log("raiz: p=%p  s=*p=%p  T=(s+0xb8)=%p", p, s, t);"""

assert OLD in s, "root anchor not found"
s = s.replace(OLD, NEW, 1)

io.open(PATH, "w", encoding="utf-8", newline="").write(s)
print("ok: run41b dereference fixed")
