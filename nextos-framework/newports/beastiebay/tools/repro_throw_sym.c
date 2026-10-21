// Minimal reproduction: does a `static` file-scope pointer referenced ONLY
// from an inline-asm string inside a function survive -O2 as a LOCAL symbol?
//
// run39h failed at link time with:
//   undefined reference to `fp2_throw_tramp_throw'
//   relocation R_AARCH64_ADR_PREL_PG_HI21 ... can not be used when making a
//   shared object; recompile with -fPIC
//
// The declaration and the asm string looked identical, so the difference has
// to be in HOW the compiler treats the reference.  This file isolates the two
// variables that differ from the working FP2_WATCH_STUB case:
//
//   slot_a : referenced from asm AND from ordinary C code
//   slot_b : referenced from asm ONLY
//
// If slot_b shows up UNDEFINED while slot_a is LOCAL, the rule is confirmed
// and the fix is to touch the pointer from C as well.
static void *slot_a;
static void *slot_b;

extern void use(void *);
extern void sink(void *);

#define TAIL(sym)                                    \
    "adrp x16, slot_" #sym "\n"                      \
    "ldr  x16, [x16, #:lo12:slot_" #sym "]\n"        \
    "br   x16\n"                                     \
    "nop\n"

__attribute__((used)) static void stub_a(void)
{
    __asm__ __volatile__("nop\n" TAIL(a));
}

__attribute__((used)) static void stub_b(void)
{
    __asm__ __volatile__("nop\n" TAIL(b));
}

void install(void)
{
    slot_a = (void *)stub_a;   /* referenced from C too */
    use(slot_a);
    sink((void *)stub_b);      /* stub_b referenced, slot_b NOT assigned */
}
