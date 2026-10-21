# -*- coding: utf-8 -*-
"""run43b: add an ENTRY probe on Setup, and make the sink self-sufficient.

run43 hooked the instruction after the store (0xa4ae74) and the probe never
fired, even though run42 called Setup() twice by reflection.  So either the
call did not reach Setup's code at all, or Setup leaves before 0xa4ae70 --
most plausibly at 0xa4ae44 (il2cpp_runtime_class_init), which reports
failure by throwing, and il2_call would have logged that.  It did not.

Two probes on the same method separate those readings cheaply:
  entry 0xa4adf8  -- did Setup's code run at all?
  exit  0xa4ae74  -- did it reach the store?

The sink now derives T from the static chain instead of trusting x9, so an
entry probe prints something meaningful too, and both probes share one
stub and one counter.
"""
import io
import os

HERE = os.path.dirname(os.path.abspath(__file__))
PATH = os.path.join(os.path.dirname(os.path.dirname(HERE)),
                    "port", "src", "jni.c")

s = io.open(PATH, encoding="utf-8", newline="").read()

# 1. extra slot
OLD = "FP2_TRAMP(setupexit);\n#undef FP2_TRAMP"
NEW = "FP2_TRAMP(setupexit);\nFP2_TRAMP(setupent);\n#undef FP2_TRAMP"
assert OLD in s, "slot anchor not found"
s = s.replace(OLD, NEW, 1)

# 2. make the sink self-sufficient: derive T itself, count the hits
OLD_SINK = """__attribute__((used)) void fp2_probe_event(void *factory, void *t, void *lr)
{
    nx_mod *mod = nx_find_mod("libil2cpp.so");
    unsigned long off = 0;
    if (mod && (char *)lr >= (char *)mod->base)
        off = (unsigned long)((char *)lr - (char *)mod->base);
    nx_log("probe: saida de Setup lr=+0x%lx  factory=%p  T=%p  T+0x18=%p",
           off, factory, t,
           t ? *(void *const *)((const char *)t + 0x18) : NULL);
}"""

NEW_SINK = """__attribute__((used)) void fp2_probe_event(void *factory, void *t, void *lr)
{
    static int hits;
    hits++;
    nx_mod *mod = nx_find_mod("libil2cpp.so");
    unsigned long off = 0;
    if (mod && (char *)lr >= (char *)mod->base)
        off = (unsigned long)((char *)lr - (char *)mod->base);

    /* O slot e' derivado aqui, e nao recebido em x9, porque a sonda de
     * ENTRADA tambem chama isto e la' x9 e' lixo.  A cadeia e' a mesma que
     * o .ctor usa: *( *( *(base+0x1644f60) ) + 0xb8 ). */
    void *t2 = NULL;
    if (mod && mod->base) {
        void *p = *(void *const *)((const char *)mod->base + 0x1644000u + 0xf60u);
        if (p) {
            void *sv = *(void *const *)p;
            if (sv)
                t2 = *(void *const *)((const char *)sv + 0xb8);
        }
    }
    nx_log("probe: #%d lr=+0x%lx  factory=%p  T(x9)=%p  T(calc)=%p  "
           "T+0x18=%p", hits, off, factory, t, t2,
           t2 ? *(void *const *)((const char *)t2 + 0x18) : NULL);
    if (hits > 20)
        nx_log("probe: limite de 20 atingido, silenciando");
}"""
assert OLD_SINK in s, "sink anchor not found"
s = s.replace(OLD_SINK, NEW_SINK, 1)

# 3. entry hook, sharing the same stub
OLD_INST = """static void fp2_install_setupexit_hook(void)
{"""
NEW_INST = """static struct fp2_hook fp2_setupent_hook;

static void fp2_install_setupent_hook(void)
{
    static int done;
    if (done)
        return;
    nx_mod *m = nx_find_mod("libil2cpp.so");
    if (!m) {
        nx_log("probe: libil2cpp.so ausente");
        return;
    }
    void *fn = (char *)m->base + 0xa4adf8;
    if (!fp2_hook_install(&fp2_setupent_hook, fn, fp2_stub_setupexit)) {
        nx_log("probe: hook na entrada de Setup falhou");
        return;
    }
    fp2_watch_tramp_setupent = fp2_setupent_hook.trampoline;
    done = 1;
    nx_log("probe: entrada de Setup observada em %p (tramp %p)", fn,
           fp2_setupent_hook.trampoline);
}

static void fp2_install_setupexit_hook(void)
{"""
assert OLD_INST in s, "install anchor not found"
s = s.replace(OLD_INST, NEW_INST, 1)

OLD_CALL = "        fp2_install_setupexit_hook();"
NEW_CALL = ("        fp2_install_setupexit_hook();\n"
            "        fp2_install_setupent_hook();")
assert OLD_CALL in s, "call anchor not found"
s = s.replace(OLD_CALL, NEW_CALL, 1)

io.open(PATH, "w", encoding="utf-8", newline="").write(s)
print("ok: run43b entry probe installed")
