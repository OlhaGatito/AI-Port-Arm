# -*- coding: utf-8 -*-
"""run41: read the global object that RecordStore.storage_ comes from.

Offline reading of RecordStore::.ctor (0xa49e5c) and RecordStore::Setup
(0xa4adf8) shows both touching the SAME slot:

    adrp x8, #0x1644000
    ldr  x8, [x8, #0xf60]   ; S  (static block pointer)
    ldr  x8, [x8]           ;
    ldr  x8, [x8, #0xb8]    ; T  <- the global object
    ldr  x8, [x8, #0x18]    ; T+0x18
    str  x8, [x19, #0x18]   ; .ctor: storage_ = T+0x18

and Setup stores the return of Create...(4, 1, null) into T+0x18.

So the whole question "who should have filled storage_" becomes "is Setup
reached, and what is T".  Both are answered by reading the slot directly at
throw time -- no new hook, no new trampoline, one dereference chain.

Naming T is the part offline work cannot do: reflection knows the class, the
disassembly only knows an offset.  So this asks il2_class_of().
"""
import io
import os

HERE = os.path.dirname(os.path.abspath(__file__))
PATH = os.path.join(os.path.dirname(os.path.dirname(HERE)),
                    "port", "src", "jni.c")

s = io.open(PATH, encoding="utf-8", newline="").read()

ANCHOR = """    if (fp2_throw_hits == 1 && self)
        fp2_dump_bootform_fields(self);
}"""

NEW = """    if (fp2_throw_hits == 1 && self) {
        fp2_dump_bootform_fields(self);
        fp2_dump_storage_root(mod);
    }
}

/* run41: a RAIZ de onde `storage_` deveria vir.
 *
 * RecordStore::.ctor (0xa49e5c) e RecordStore::Setup (0xa4adf8) leem e
 * escrevem o MESMO slot, o que fecha a pergunta "quem devia ter preenchido
 * storage_" numa unica cadeia de desreferencias:
 *
 *   .ctor  ldr x8,[x8,#0xb8]; ldr x8,[x8,#0x18]; str x8,[x19,#0x18]
 *   Setup  ldr x9,[x8,#0xb8]; str <retorno de 0xa4a3fc>,[x9,#0x18]
 *
 * Ou seja: existe um objeto global T, Setup e' quem o preenche, e .ctor e'
 * quem o copia.  Se T+0x18 ainda e' NULL no instante da excecao, Setup nao
 * chegou a rodar -- e isso e' MEDICAO, nao leitura de codigo.
 *
 * O nome de T e' o pedaco que nenhuma leitura offline entrega: o disassembly
 * conhece um offset, a reflexao conhece a classe.  Dai o il2_class_of(). */
static void fp2_dump_storage_root(nx_mod *m)
{
    if (!m || !m->base) {
        nx_log("raiz: libil2cpp.so sem base conhecida");
        return;
    }
    /* 0x1644000 e' a pagina que o adrp calcula; 0xf60 e' o deslocamento do
     * ldr.  Nesta imagem vaddr == offset, entao base + (0x1644000+0xf60) e'
     * o endereco vivo.  O slot e' .bss -- nao existe no arquivo, e por isso
     * nenhuma ferramenta offline pode ter a resposta. */
    const unsigned char *slot =
        (const unsigned char *)m->base + 0x1644000u + 0xf60u;
    void *s = *(void *const *)slot;
    if (!s) {
        nx_log("raiz: slot 0x1644f60 -> NULL (bloco estatico nao montado)");
        return;
    }
    void *t = *(void *const *)((const char *)s + 0xb8);
    nx_log("raiz: S=%p  T=(S+0xb8)=%p", s, t);
    if (!t) {
        nx_log("raiz: T e' NULL -- o objeto global nunca foi construido");
        return;
    }
    unsigned char flag13 = *(const unsigned char *)((const char *)t + 0x13);
    void *f18 = *(void *const *)((const char *)t + 0x18);
    void *f50 = *(void *const *)((const char *)t + 0x50);
    nx_log("raiz: T+0x13=%u  T+0x18=%p  T+0x50=%p", (unsigned)flag13, f18,
           f50);
    /* T+0x18 e' exatamente o valor que .ctor copia para storage_.  Se ele
     * ja e' NULL aqui, Setup nao rodou; se nao e' NULL, entao e' o .ctor que
     * esta' lendo outro T -- e as duas hipoteses dao consertos diferentes. */
    nx_log("raiz: storage_ deveria ser T+0x18 = %s",
           f18 ? "nao-nulo (Setup rodou)" : "NULL (Setup nao rodou)");
    Il2CppClass *k = il2_class_of((Il2CppObject *)t);
    nx_log("raiz: T e' %s", k ? il2_class_label(k) : "(classe ilegivel)");
    if (k)
        il2_fields_foreach(k, fp2_dump_fields_cb, t);
}"""

assert ANCHOR in s, "throw event anchor not found"
s = s.replace(ANCHOR, NEW, 1)

# forward declaration so the definition order does not matter
DECL = """__attribute__((used)) void fp2_throw_event(void *x30, void *self)
{"""
assert DECL in s, "throw_event anchor not found"
s = s.replace(DECL, "static void fp2_dump_storage_root(nx_mod *m);\n\n" + DECL, 1)

io.open(PATH, "w", encoding="utf-8", newline="").write(s)
print("ok: run41 root dump installed")
