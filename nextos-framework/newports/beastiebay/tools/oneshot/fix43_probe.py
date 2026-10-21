# -*- coding: utf-8 -*-
"""run43: probe the value Setup actually stores into the static slot.

run42 called RecordStore.Setup() through reflection and the slot stayed
NULL, which contradicts reading the code: Setup stores factory(4,1,null)
into T+0x18, and for w0=4 the factory takes jump-table case 3, allocates
and returns that pointer -- only w0=0 or w0>4 reach `mov x22, xzr`.

A contradiction between a reading and a measurement means the reading is
wrong somewhere, and guessing which part costs another build each time.
So measure the value itself: hook the first instruction AFTER the store.

    0xa4ae70  str  x8, [x9, #0x18]   ; the store
    0xa4ae74  ldp  x29, x30, [sp, #0x10]   <- hook here

At 0xa4ae74 the epilogue has not touched x8 yet, so x8 still holds what
the factory returned.  That single number decides between "factory
returned null" and "Setup wrote somewhere else".

The stub saves every callee-saved register, not just the ones this code
happens to use: the C callee clobbers x19-x28 freely, and a probe that
silently corrupts the method it is observing produces a measurement of
the probe rather than of the game.
"""
import io
import os

HERE = os.path.dirname(os.path.abspath(__file__))
PATH = os.path.join(os.path.dirname(os.path.dirname(HERE)),
                    "port", "src", "jni.c")

s = io.open(PATH, encoding="utf-8", newline="").read()

# 1. slot
OLD = "FP2_TRAMP(xthrow);\n#undef FP2_TRAMP"
NEW = "FP2_TRAMP(xthrow);\nFP2_TRAMP(setupexit);\n#undef FP2_TRAMP"
assert OLD in s, "tramp slot anchor not found"
s = s.replace(OLD, NEW, 1)

# 2. probe stub + C sink, inserted just before the throw-hook section
ANCHOR = """/* ====================================================================
 * run39h: hook do AUXILIAR DE EXCEPCAO."""
assert ANCHOR in s, "throw section anchor not found"

PROBE = """/* ====================================================================
 * run43: sonda na SAIDA de RecordStore::Setup.
 *
 * run42 chamou Setup() por reflexao e o slot continuou NULL.  Isso
 * contradiz a leitura do codigo: para w0=4 o factory passa pelo caso 3 da
 * tabela de salto, aloca e devolve o ponteiro -- so' w0=0 ou w0>4 chegam
 * ao `mov x22, xzr`.  Uma contradicao entre leitura e medicao significa
 * que a leitura esta' errada em algum ponto, e adivinhar qual custa um
 * build por tentativa.  Entao mede-se o valor em si.
 *
 * O ponto e' a primeira instrucao DEPOIS do store:
 *
 *   0xa4ae70  str x8, [x9, #0x18]        ; o store
 *   0xa4ae74  ldp x29, x30, [sp, #0x10]  ; <- hook aqui
 *
 * Em 0xa4ae74 o epilogo ainda nao tocou x8, entao x8 ainda e' o que o
 * factory devolveu.  Esse unico numero decide entre "o factory devolveu
 * nulo" e "o Setup escreveu em outro lugar".
 *
 * O stub salva TODOS os registradores callee-saved, nao apenas os que
 * este trecho usa: o callee C destrui x19-x28 livremente, e uma sonda
 * que corrompe silenciosamente o metodo que observa acaba medindo a
 * sonda, nao o jogo. */
static struct fp2_hook fp2_setupexit_hook;

__attribute__((used)) void fp2_probe_event(void *factory, void *t, void *lr)
{
    nx_mod *mod = nx_find_mod("libil2cpp.so");
    unsigned long off = 0;
    if (mod && (char *)lr >= (char *)mod->base)
        off = (unsigned long)((char *)lr - (char *)mod->base);
    nx_log("probe: saida de Setup lr=+0x%lx  factory=%p  T=%p  T+0x18=%p",
           off, factory, t,
           t ? *(void *const *)((const char *)t + 0x18) : NULL);
}

__attribute__((naked, used)) static void fp2_stub_setupexit(void)
{
    __asm__ __volatile__(
        "stp x29, x30, [sp, #-0x60]!\\n"
        "mov  x29, sp\\n"
        "stp  x19, x20, [sp, #0x10]\\n"
        "stp  x21, x22, [sp, #0x20]\\n"
        "stp  x23, x24, [sp, #0x30]\\n"
        "stp  x25, x26, [sp, #0x40]\\n"
        "stp  x27, x28, [sp, #0x50]\\n"
        "mov  x0, x8\\n"          /* o que o factory devolveu */
        "mov  x1, x9\\n"          /* T, o bloco estatico */
        "mov  x2, x30\\n"
        "bl   fp2_probe_event\\n"
        "ldp  x27, x28, [sp, #0x50]\\n"
        "ldp  x25, x26, [sp, #0x40]\\n"
        "ldp  x23, x24, [sp, #0x30]\\n"
        "ldp  x21, x22, [sp, #0x20]\\n"
        "ldp  x19, x20, [sp, #0x10]\\n"
        "ldp  x29, x30, [sp], #0x60\\n"
        FP2_W_TAIL(setupexit));
}

static void fp2_install_setupexit_hook(void)
{
    static int done;
    if (done)
        return;
    nx_mod *m = nx_find_mod("libil2cpp.so");
    if (!m) {
        nx_log("probe: libil2cpp.so ausente");
        return;
    }
    void *fn = (char *)m->base + 0xa4ae74;
    if (!fp2_hook_install(&fp2_setupexit_hook, fn, fp2_stub_setupexit)) {
        nx_log("probe: hook na saida de Setup falhou");
        return;
    }
    fp2_watch_tramp_setupexit = fp2_setupexit_hook.trampoline;
    done = 1;
    nx_log("probe: saida de Setup observada em %p (tramp %p)", fn,
           fp2_setupexit_hook.trampoline);
}

"""
s = s.replace(ANCHOR, PROBE + ANCHOR, 1)

# 3. install it right after the throw hook is installed
OLD_CALL = "        fp2_install_throw_hook();"
NEW_CALL = ("        fp2_install_throw_hook();\n"
            "        fp2_install_setupexit_hook();")
assert OLD_CALL in s, "install call anchor not found"
s = s.replace(OLD_CALL, NEW_CALL, 1)

# 4. forward declaration of the probe sink for the naked asm
DECL = "static void fp2_dump_storage_root(nx_mod *m);"
assert DECL in s, "fwd decl anchor not found"
s = s.replace(DECL, DECL, 1)

io.open(PATH, "w", encoding="utf-8", newline="").write(s)
print("ok: run43 Setup-exit probe installed")
