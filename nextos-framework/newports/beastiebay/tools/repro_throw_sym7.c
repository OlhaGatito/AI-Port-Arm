// repro 7: the last structural difference between the working five and the
// broken throw stub.
//
// Working (jni.c 2096-2113):  FP2_WATCH_STUB(sym, mid, tail) is a MACRO whose
//                              `tail` argument is FP2_W_TAIL(sym), itself a
//                              macro.  The slot is declared by a SEPARATE
//                              macro, FP2_TRAMP(sym), expanded at 2065-2071.
//
// Broken  (jni.c 2315-2326): the stub is written OUT BY HAND and its tail is
//                              a macro, FP2_THROW_TAIL(throw), but the slot is
//                              declared by FP2_THROW_TRAMP(throw) -- which has
//                              the same shape as FP2_TRAMP.
//
// Renaming did NOT fix it, so the difference is not lexical.  What is left:
// the tail reaches the slot through a macro that is passed as an ARGUMENT in
// the working case, and through direct textual inclusion in the broken one.
// When GCC assembles an `__asm__` string it must decide, per symbol, whether
// the reference can be preempted.  A symbol that appears in an asm string
// assembled in a macro-expansion context may be treated differently from one
// assembled in a directly written function.
//
// Test: two stubs in ONE file, identical except that one gets its tail
// through a macro ARGUMENT (the working shape) and the other inlines the
// macro (the broken shape).  The slots get names chosen so that lexical
// order CANNOT explain the outcome: `zzz_arg` for the macro-argument stub
// and `aaa_inline` for the inlined one.  If the argument shape is what
// matters, `zzz_arg` is `b` despite sorting LAST; if lexical order is what
// matters, `aaa_inline` is `U` and `zzz_arg` is `b` too -- so add a third
// case to separate them.
static void *zzz_arg;
static void *aaa_inline;
static void *mmm_passthru;

extern void use(void *);
extern void inner(unsigned long);

#define TAIL_OF(sym)                                 \
    "adrp x16, " #sym "\n"                           \
    "ldr  x16, [x16, #:lo12:" #sym "]\n"             \
    "br   x16\n"                                    \
    "nop\n"                                         \
    "nop\n"

#define STUB_ARG(sym, mid, tail)                                     \
    __attribute__((naked, used)) static void stub_##sym(void)        \
    {                                                                \
        __asm__ __volatile__(                                        \
            "stp x29, x30, [sp, #-0x30]!\n"                         \
            "mov x29, sp\n"                                         \
            "mov w2, #" #mid "\n"                                   \
            "bl inner\n"                                            \
            "ldp x29, x30, [sp], #0x30\n"                           \
            tail);                                                   \
    }

#define STUB_INLINE(sym, mid)                                        \
    __attribute__((naked, used)) static void stub_##sym(void)        \
    {                                                                \
        __asm__ __volatile__(                                        \
            "stp x29, x30, [sp, #-0x30]!\n"                         \
            "mov x29, sp\n"                                         \
            "mov w2, #" #mid "\n"                                   \
            "bl inner\n"                                            \
            "ldp x29, x30, [sp], #0x30\n"                           \
            TAIL_OF(sym));                                           \
    }

/* Case 1: tail arrives as a macro ARGUMENT (the working shape). */
STUB_ARG(zzz_arg, 0, TAIL_OF(zzz_arg))

/* Case 2: tail macro is textually inlined (the broken shape). */
STUB_INLINE(aaa_inline, 1)

/* Case 3: same as case 2 but the name sorts LAST of all. */
STUB_INLINE(mmm_passthru, 2)

void install(void)
{
    zzz_arg = stub_zzz_arg;
    aaa_inline = stub_aaa_inline;
    mmm_passthru = stub_mmm_passthru;
    use(zzz_arg); use(aaa_inline); use(mmm_passthru);
}
