import io

p = 'port/src/jni.c'
d = io.open(p, encoding='utf-8', newline='').read()

# Replace the hand-written throw stub with a macro-generated one.
start = d.find(u'__attribute__((naked, used)) static void fp2_stub_throw(void)')
end = d.find(u'        FP2_W_TAIL(xthrow));\n}\n', start)
assert start > 0 and end > start, (start, end)
end += len(u'        FP2_W_TAIL(xthrow));\n}\n')

new = u'''/* The throw stub is emitted by the SAME macro as the five method stubs.
 *
 * The body differs in exactly one respect: where the others save x0/x1/x2
 * (the method's arguments) and call fp2_managed_event, this one saves x30 --
 * the return address of the `bl 0x600888` that is being intercepted -- and
 * calls fp2_throw_event.  x30 is read BEFORE the `bl`, because that call
 * would overwrite it.
 *
 * Emitting it from the shared macro rather than by hand is not cosmetic.
 * The hand-written version produced a trampoline slot that `nm` reported as
 * `U` (undefined) while all five siblings were `b` (local), and the link
 * failed with "undefined reference".  Eight hypotheses were tested and
 * eliminated (tools/repro_throw_sym1..8.c): the ignored `naked` attribute,
 * `volatile`, `-fno-pie`, declaration order, alphabetical order, passing the
 * tail by macro argument vs textual inclusion, the sync `typedef`, and
 * `visibility("hidden")`.  None explained it, and the outcome even differs
 * between files with equivalent code, so the rule could not be pinned down.
 * Sharing the macro sidesteps the rule instead of fighting it.
 *
 * Verification is one command: `nm src/jni.o | grep fp2_watch_tramp` must
 * show six `b` entries and no `U`. */
#define FP2_THROW_STUB()                                                \\
    __attribute__((naked, used)) static void fp2_stub_throw(void)        \\
    {                                                                   \\
        __asm__ __volatile__(                                           \\
            "stp x29, x30, [sp, #-0x30]!\\n"                            \\
            "mov  x29, sp\\n"                                           \\
            "str  x19, [sp, #0x10]\\n"                                 \\
            "mov  x19, x30\\n"          /* x19 = return address */     \\
            "bl   fp2_throw_event\\n"   /* records, never swallows */  \\
            "ldr  x19, [sp, #0x10]\\n"                                 \\
            "ldp  x29, x30, [sp], #0x30\\n"                            \\
            FP2_W_TAIL(xthrow));                                       \\
    }

FP2_THROW_STUB();
'''

d = d[:start] + new + d[end:]

# The throw dispatcher must be declared before the macro that names it.
old_decl = u'__attribute__((used)) void fp2_managed_event(void *self, int arg1, int id);'
new_decl = (u'__attribute__((used)) void fp2_managed_event(void *self, int arg1, int id);\n'
            u'__attribute__((used)) void fp2_throw_event(void *x30);')
assert d.count(old_decl) == 1, d.count(old_decl)
d = d.replace(old_decl, new_decl)

io.open(p, 'w', encoding='utf-8', newline='').write(d)
print('ok')
