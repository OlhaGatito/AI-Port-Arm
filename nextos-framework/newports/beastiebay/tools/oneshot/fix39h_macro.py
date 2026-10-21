import io

p = 'port/src/jni.c'
d = io.open(p, encoding='utf-8', newline='').read()

# 1) FP2_WATCH_STUB: parameterise the C dispatcher it calls.
old_macro = u"""#define FP2_WATCH_STUB(sym, mid, tail)                                      \\
    __attribute__((naked, used)) static void fp2_stub_##sym(void)          \\
    {                                                                      \\
        __asm__ __volatile__(                                              \\
            "stp x29, x30, [sp, #-0x30]!\\n"                               \\
            "mov  x29, sp\\n"                                              \\
            "stp  x19, x20, [sp, #0x10]\\n"                                \\
            "stp  x21, x22, [sp, #0x20]\\n"                                \\
            "mov  x19, x0\\n"                                              \\
            "mov  x20, x1\\n"                                              \\
            "mov  x21, x2\\n"                                              \\
            "mov  w2, #" #mid "\\n"                                       \\
            "bl   fp2_managed_event\\n"                                     \\
            "mov  x0, x19\\n"                                              \\
            "mov  x1, x20\\n"                                              \\
            "mov  x2, x21\\n"                                              \\
            "ldp  x21, x22, [sp, #0x20]\\n"                                \\
            "ldp  x19, x20, [sp, #0x10]\\n"                                \\
            "ldp  x29, x30, [sp], #0x30\\n"                                \\
            tail);                                                        \\
    }"""

new_macro = u"""/* run39h: `fn` became a PARAMETER.  The throw stub needs the same prologue,
 * the same register discipline and -- the part that actually matters -- the
 * same macro-expansion context as the five method stubs.  Hand-writing a
 * sixth body produced a slot that nm reported as `U` (undefined) while all
 * five siblings were `b` (local), and the link failed on it.  Eight
 * hypotheses were tested and eliminated (tools/repro_throw_sym1..8.c);
 * none of them explained it, and the outcome even differs between files
 * with equivalent code, so the rule could not be pinned down.  Letting the
 * SAME macro emit all six sidesteps the rule instead of fighting it, and
 * the check is one command: `nm src/jni.o | grep fp2_watch_tramp` must show
 * six `b` and no `U`.
 *
 * `w2` carries the method id for the five, and 0 for the throw stub, which
 * has no id: it reports a return address, not a method. */
#define FP2_WATCH_STUB(sym, mid, tail, fn)                                  \\
    __attribute__((naked, used)) static void fp2_stub_##sym(void)          \\
    {                                                                      \\
        __asm__ __volatile__(                                              \\
            "stp x29, x30, [sp, #-0x30]!\\n"                               \\
            "mov  x29, sp\\n"                                              \\
            "stp  x19, x20, [sp, #0x10]\\n"                                \\
            "stp  x21, x22, [sp, #0x20]\\n"                                \\
            "mov  x19, x0\\n"                                              \\
            "mov  x20, x1\\n"                                              \\
            "mov  x21, x2\\n"                                              \\
            "mov  w2, #" #mid "\\n"                                       \\
            "bl   " #fn "\\n"                                             \\
            "mov  x0, x19\\n"                                              \\
            "mov  x1, x20\\n"                                              \\
            "mov  x2, x21\\n"                                              \\
            "ldp  x21, x22, [sp, #0x20]\\n"                                \\
            "ldp  x19, x20, [sp, #0x10]\\n"                                \\
            "ldp  x29, x30, [sp], #0x30\\n"                                \\
            tail);                                                        \\
    }"""

assert d.count(old_macro) == 1, d.count(old_macro)
d = d.replace(old_macro, new_macro)

# 2) FP2_W_STUB forwards the new argument.
old_stub = u"""#define FP2_W_STUB(sym, mid, expected)                       \\
    typedef char fp2_sync_##sym[(mid) == (expected) ? 1 : -1]; \\
    FP2_WATCH_STUB(sym, mid, FP2_W_TAIL(sym))"""
new_stub = u"""#define FP2_W_STUB(sym, mid, expected)                       \\
    typedef char fp2_sync_##sym[(mid) == (expected) ? 1 : -1]; \\
    FP2_WATCH_STUB(sym, mid, FP2_W_TAIL(sym), fp2_managed_event)"""
assert d.count(old_stub) == 1, d.count(old_stub)
d = d.replace(old_stub, new_stub)

io.open(p, 'w', encoding='utf-8', newline='').write(d)
print('ok')
