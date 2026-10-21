// Which variable attribute decides local-vs-undefined under -fPIE?
//
// repro 1: two plain `static void *` slots named from asm -> first is `U`.
// repro 2: four slots -> only the first is `U`.
// repro 3: the first slot became `b` once a `static` FUNCTION was also named.
//
// The competing explanations, each isolated here:
//
//   (A) ORDER: the first asm-named variable is treated as external, later
//       ones local.  Predicts: slot_a `U`, slot_b `b`.
//   (B) The variable is fine but GCC only emits the local definition when
//       the same name is ALSO named from a different context.  Predicts the
//       same thing -- indistinguishable from (A) so far.
//   (C) The `used` attribute on the enclosing function.  Predicts: a stub
//       WITHOUT `used` referencing the only slot gives `b`.
//
// (C) is the one that would actually be actionable, so it gets a direct
// test: three slots, three stubs, and the FIRST stub has no `used`.
static void *slot_p;
static void *slot_q;
static void *slot_r;

extern void use(void *);

#define TAIL(sym)                                 \
    "adrp x16, slot_" #sym "\n"                   \
    "ldr  x16, [x16, #:lo12:slot_" #sym "]\n"     \
    "br   x16\n"                                  \
    "nop\n"

__attribute__((used)) static void stub_p(void)
{
    __asm__ __volatile__("nop\n" TAIL(p));
}

__attribute__((used)) static void stub_q(void)
{
    __asm__ __volatile__("nop\n" TAIL(q));
}

/* NO `used` here. */
static void stub_r(void)
{
    __asm__ __volatile__("nop\n" TAIL(r));
}

void install(void)
{
    slot_p = stub_p;
    slot_q = stub_q;
    slot_r = stub_r;
    use(slot_p);
    use(slot_q);
    use(slot_r);
}
