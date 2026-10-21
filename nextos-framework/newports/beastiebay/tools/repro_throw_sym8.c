// repro 8: the one structural element of FP2_WATCH_STUB that has not been
// tested yet -- the `typedef char sync_##sym[...]` line that precedes EVERY
// working stub inside the macro:
//
//   #define FP2_W_STUB(sym, mid, expected)                       \
//       typedef char fp2_sync_##sym[(mid) == (expected) ? 1 : -1]; \
//       FP2_WATCH_STUB(sym, mid, FP2_W_TAIL(sym))
//
// The typedef is a compile-time assertion (it fails the build if `mid` and
// `expected` drift apart).  It has no runtime effect, so it cannot plausibly
// change symbol binding -- but "implausibly" is exactly what has to be
// measured rather than argued, because five other explanations have already
// been eliminated in this investigation.
//
// Test: three stubs, identical except one has the typedef and one does not,
// with names chosen so lexical order cannot explain the outcome.
//
//   zzz_typedef  : has the typedef, sorts LAST
//   aaa_plain    : no typedef,   sorts FIRST
//   mmm_typedef  : has the typedef, sorts middle
//
// If lexical order rules, only aaa_plain is U.  If the typedef rules, both
// aaa_plain and mmm_typedef differ from zzz_typedef in the same way, which
// would mean the typedef is what makes the symbol local.
static void *zzz_typedef;
static void *aaa_plain;
static void *mmm_typedef;

extern void use(void *);
extern void inner(unsigned long);

#define TAIL(sym)                                 \
    "adrp x16, " #sym "\n"                         \
    "ldr  x16, [x16, #:lo12:" #sym "]\n"           \
    "br   x16\n"                                  \
    "nop\n"                                       \
    "nop\n"

#define STUB_TYPEDEF(sym, mid)                                        \
    typedef char sync_##sym[(mid) == (mid) ? 1 : -1];                \
    __attribute__((naked, used)) static void stub_##sym(void)        \
    {                                                                \
        __asm__ __volatile__(                                        \
            "stp x29, x30, [sp, #-0x30]!\n"                         \
            "mov x29, sp\n"                                         \
            "mov w2, #" #mid "\n"                                   \
            "bl inner\n"                                            \
            "ldp x29, x30, [sp], #0x30\n"                           \
            TAIL(sym));                                               \
    }

#define STUB_PLAIN(sym, mid)                                         \
    __attribute__((naked, used)) static void stub_##sym(void)        \
    {                                                                \
        __asm__ __volatile__(                                        \
            "stp x29, x30, [sp, #-0x30]!\n"                         \
            "mov x29, sp\n"                                         \
            "mov w2, #" #mid "\n"                                   \
            "bl inner\n"                                            \
            "ldp x29, x30, [sp], #0x30\n"                           \
            TAIL(sym));                                               \
    }

STUB_TYPEDEF(zzz_typedef, 0)
STUB_PLAIN(aaa_plain, 1)
STUB_TYPEDEF(mmm_typedef, 2)

void install(void)
{
    zzz_typedef = stub_zzz_typedef;
    aaa_plain = stub_aaa_plain;
    mmm_typedef = stub_mmm_typedef;
    use(zzz_typedef); use(aaa_plain); use(mmm_typedef);
}
