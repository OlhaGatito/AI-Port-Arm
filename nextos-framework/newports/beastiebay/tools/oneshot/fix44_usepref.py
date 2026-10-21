# -*- coding: utf-8 -*-
"""run44: set RecordStore.usePreference_ and see whether the game proceeds.

Two independent readings converge on the same byte.

Static-field slots dumped at run40 place `usePreference_` at 0x20, and
`rootStorage_` at 0x18:

    rootStorage_     (slot 0x18)   <- what Setup writes, what .ctor copies
    usePreference_   (slot 0x20)

GetNumRecords reads exactly those two offsets, in this order:

    0xa4a62c  ldr  x8, [x8, #0xb8]   ; T, the static block
    0xa4a630  ldrb w8, [x8, #0x20]   ; usePreference_
    0xa4a634  cbz  w8, #0xa4a664     ; zero -> fall through to storage_
    0xa4a664  ldr  x20, [x19, #0x18] ; storage_  (= copy of rootStorage_)
    0xa4a668  cbnz x20, ...
    0xa4a670  bl   0x600888          ; NullReferenceException

So a non-zero usePreference_ takes the branch that never touches storage_.
That is the branch Kairosoft's own Android build must take, because the
plugin ships a full SharedPreferences contract (existPreference /
getPreference / putPreference / removePreference) and this port implements
it -- which is why "no preference was ever read" was never evidence of
anything: the game never got far enough to ask.

Flipping one byte is the smallest experiment that distinguishes
"the file backend was never built" from "the preference backend was the
intended one".  It is a byte in a static block, not a code edit, and the
logged before/after values make it reversible.
"""
import io
import os

HERE = os.path.dirname(os.path.abspath(__file__))
PATH = os.path.join(os.path.dirname(os.path.dirname(HERE)),
                    "port", "src", "jni.c")

s = io.open(PATH, encoding="utf-8", newline="").read()

OLD = """    Il2CppObject *r = il2_call(m, NULL, NULL, "RecordStore.Setup");
    nx_log("setup: Setup() retornou %p", (void *)r);
    /* A unica prova que importa: o slot que .ctor copia. */
    fp2_dump_storage_root(nx_find_mod("libil2cpp.so"));
}"""

NEW = """    Il2CppObject *r = il2_call(m, NULL, NULL, "RecordStore.Setup");
    nx_log("setup: Setup() retornou %p", (void *)r);
    /* A unica prova que importa: o slot que .ctor copia. */
    fp2_dump_storage_root(nx_find_mod("libil2cpp.so"));
    fp2_set_use_preference();
}

/* run44: usePreference_ (estatico, T+0x20).
 *
 * DUAS leituras independentes convergem no mesmo byte.
 *
 * A lista de campos estaticos do run40 poe `usePreference_` em 0x20 e
 * `rootStorage_` em 0x18.  GetNumRecords le exatamente esses dois:
 *
 *   0xa4a630  ldrb w8, [x8, #0x20]   ; usePreference_
 *   0xa4a634  cbz  w8, #0xa4a664     ; zero -> cai no storage_
 *   0xa4a664  ldr  x20, [x19, #0x18] ; storage_ (copia de rootStorage_)
 *   0xa4a670  bl   0x600888          ; NullReferenceException
 *
 * Um usePreference_ nao-nulo toma o ramo que NAO encosta em storage_.
 * E' o ramo que o build Android da Kairosoft deve tomar: o plugin traz um
 * contrato completo de SharedPreferences (existPreference / getPreference /
 * putPreference / removePreference) e este port o implementa.  Isso tambem
 * explica por que "nenhuma preferenca foi lida" nunca foi evidencia de
 * nada: o jogo nunca chegou ao ponto de perguntar.
 *
 * Virar um byte e' o menor experimento que separa "o backend de arquivo
 * nao foi construido" de "o backend de preferencia era o pretendido".
 * E' um byte num bloco estatico, nao uma mudanca de codigo, e os valores
 * antigo/novo no log o tornam reversivel. */
static void fp2_set_use_preference(void)
{
    nx_mod *m = nx_find_mod("libil2cpp.so");
    if (!m || !m->base) {
        nx_log("usepref: libil2cpp.so ausente");
        return;
    }
    void *p = *(void *const *)((const char *)m->base + 0x1644000u + 0xf60u);
    if (!p)
        return;
    void *sv = *(void *const *)p;
    if (!sv)
        return;
    void *t = *(void *const *)((const char *)sv + 0xb8);
    if (!t) {
        nx_log("usepref: bloco estatico T e' NULL");
        return;
    }
    unsigned char *flag = (unsigned char *)t + 0x20;
    nx_log("usepref: usePreference_ %u -> 1  (rootStorage_ T+0x18=%p)",
           (unsigned)*flag, *(void *const *)((const char *)t + 0x18));
    *flag = 1;
}"""

assert OLD in s, "setup tail anchor not found"
s = s.replace(OLD, NEW, 1)

io.open(PATH, "w", encoding="utf-8", newline="").write(s)
print("ok: run44 usePreference_ setter installed")
