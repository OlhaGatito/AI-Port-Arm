// Third reproduction: the fix candidate.
//
// repro_throw_sym2 showed the rule is positional, not semantic: whichever
// slot the inline asm mentions FIRST comes out as `U` (undefined), every
// later one as `b` (local bss).  Relying on that ordering is unacceptable --
// it would break the moment someone adds a sixth stub.
//
// Candidate fix: never let the assembly name the variable.  Publish the
// trampoline through a FUNCTION whose address is taken, and let the asm
// reference that function symbol.  Function symbols for `static` functions
// referenced from asm are emitted locally, and there is no ordering
// sensitivity because each stub names a different function.
//
// If slot_viafn comes out as `t`/`T`/`b` (defined) while a bare slot_first
// stays `U`, this is the shape to use in jni.c.
static void *slot_first;

extern void use(void *);

/* One tiny accessor per stub.  The asm below names THESE, not the
 * variables, so the variables are only ever reached through C. */
static void *tramp_throw(void) { return slot_first; }

__attribute__((used)) static void stub_first(void)
{
    __asm__ __volatile__(
        "adrp x16, slot_first\n"
        "ldr  x16, [x16, #:lo12:slot_first]\n"
        "br   x16\n"
        "nop\n");
}

__attribute__((used)) static void stub_viafn(void)
{
    __asm__ __volatile__(
        "adrp x16, tramp_throw\n"
        "ldr  x16, [x16, #:lo12:tramp_throw]\n"
        "br   x16\n"
        "nop\n");
}

void install(void)
{
    slot_first = stub_first;
    use(slot_first);
    use((void *)stub_viafn);
    use((void *)tramp_throw);
}
