// Second minimal reproduction.  The first one showed that BOTH slots become
// `U` under -fPIE, yet the real jni.c has five slots as local `b`.  So the
// difference is not the register and not the attribute.  The remaining
// candidates, tested here side by side:
//
//   slot_direct : asm references the variable NAME directly (what I wrote)
//   slot_indirect: asm references a variable reached through a `used`
//                  function's own static, i.e. the reference is emitted
//                  from a context the compiler must keep
//   slot_volatile: the variable is `volatile`, which forces it into
//                  .data and makes the symbol non-preemptible
//
// If slot_volatile comes out as `b`/`d` while slot_direct stays `U`, the fix
// is a one-word change to the declaration.
static void *slot_direct;
static void *slot_indirect;
static void *volatile slot_volatile;

extern void use(void *);
extern void sink(void *);

#define TAIL_DIRECT(sym)                                 \
    "adrp x16, slot_" #sym "\n"                           \
    "ldr  x16, [x16, #:lo12:slot_" #sym "]\n"             \
    "br   x16\n"                                          \
    "nop\n"

__attribute__((used)) static void stub_direct(void)
{
    __asm__ __volatile__("nop\n" TAIL_DIRECT(direct));
}

__attribute__((used)) static void stub_indirect(void)
{
    __asm__ __volatile__("nop\n" TAIL_DIRECT(indirect));
}

__attribute__((used)) static void stub_volatile(void)
{
    __asm__ __volatile__("nop\n" TAIL_DIRECT(volatile_));
}

/* A second definition of the volatile one, so the asm string above can use a
 * name that also exists as a C identifier. */
static void *volatile slot_volatile_;

#define TAIL_VOLATILE(sym)                                \
    "adrp x16, slot_" #sym "\n"                           \
    "ldr  x16, [x16, #:lo12:slot_" #sym "]\n"             \
    "br   x16\n"                                          \
    "nop\n"

__attribute__((used)) static void stub_vol2(void)
{
    __asm__ __volatile__("nop\n" TAIL_VOLATILE(volatile_));
}

void install(void)
{
    slot_direct = stub_direct;
    slot_indirect = stub_indirect;
    slot_volatile = stub_volatile;
    slot_volatile_ = stub_vol2;
    use(slot_direct);
    use(slot_indirect);
    use(slot_volatile);
    use(slot_volatile_);
    sink(0);
}
