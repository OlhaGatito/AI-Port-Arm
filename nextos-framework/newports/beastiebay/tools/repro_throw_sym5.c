// repro 5: reproduce the EXACT macro shape used in jni.c, then add a sixth
// stub.  If the sixth comes out `U` while the first five are `b`, the rule is
// positional.  If the sixth is also `b`, then something else about jni.c's
// throw stub is responsible, and this file has isolated the difference.
//
// The typedef line is kept because it is part of the real macro and may be
// what forces the symbol to be emitted locally.
static void *slot_a;
static void *slot_b;
static void *slot_c;
static void *slot_d;
static void *slot_e;
static void *slot_f;

extern void use(void *);
extern void inner(unsigned long);

#define TAIL(sym)                                 \
    "adrp x16, slot_" #sym "\n"                   \
    "ldr  x16, [x16, #:lo12:slot_" #sym "]\n"     \
    "br   x16\n"                                  \
    "nop\n"                                       \
    "nop\n"

#define STUB(sym, mid, tail)                                       \
    typedef char sync_##sym[(mid) == (mid) ? 1 : -1];              \
    __attribute__((naked, used)) static void stub_##sym(void)     \
    {                                                              \
        __asm__ __volatile__(                                      \
            "stp x29, x30, [sp, #-0x30]!\n"                       \
            "mov x29, sp\n"                                       \
            "mov w2, #" #mid "\n"                                 \
            "bl inner\n"                                          \
            "ldp x29, x30, [sp], #0x30\n"                        \
            tail);                                                 \
    }

#define STUB_TAIL(sym, mid) STUB(sym, mid, TAIL(sym))

STUB_TAIL(a, 0)
STUB_TAIL(b, 1)
STUB_TAIL(c, 2)
STUB_TAIL(d, 3)
STUB_TAIL(e, 4)
STUB_TAIL(f, 5)

void install(void)
{
    slot_a = stub_a; slot_b = stub_b; slot_c = stub_c;
    slot_d = stub_d; slot_e = stub_e; slot_f = stub_f;
    use(slot_a); use(slot_b); use(slot_c);
    use(slot_d); use(slot_e); use(slot_f);
}
