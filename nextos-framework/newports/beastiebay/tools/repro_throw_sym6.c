// repro 6: is the "first slot is undefined" rule actually LEXICAL?
//
// jni.o shows:
//     U fp2_throw_tramp_throw          <- alphabetically FIRST  (t < w)
//     b fp2_watch_tramp_changestate    <- alphabetically later  (c)
//     b fp2_watch_tramp_classload
//     b fp2_watch_tramp_init
//     b fp2_watch_tramp_load
//     b fp2_watch_tramp_update
//
// The five working slots are `fp2_watch_*` and the broken one is
// `fp2_throw_*`.  `t` sorts before `w`, so the broken symbol is exactly the
// alphabetically first one.  In repro 5 the broken slot was `slot_a` and the
// working ones were `slot_b`..`slot_f` -- again first alphabetically.
//
// That is consistent with GCC emitting the FIRST asm-named symbol of a group
// as a preemptible reference and the rest as local.  If the rule is lexical,
// renaming the slot to sort AFTER the others fixes it with no other change.
//
// Test: two slots named so the order under test is inverted relative to
// declaration order -- `aaa_slot` declared first, `zzz_slot` second.  If the
// lexical rule holds, `aaa_slot` is `U` even though it is declared first and
// `zzz_slot` is `b`.
static void *aaa_slot;
static void *zzz_slot;
static void *mmm_slot;

extern void use(void *);

#define TAIL(sym)                                 \
    "adrp x16, " #sym "\n"                         \
    "ldr  x16, [x16, #:lo12:" #sym "]\n"           \
    "br   x16\n"                                  \
    "nop\n"

__attribute__((naked, used)) static void stub_aaa(void)
{
    __asm__ __volatile__("nop\n" TAIL(aaa_slot));
}
__attribute__((naked, used)) static void stub_zzz(void)
{
    __asm__ __volatile__("nop\n" TAIL(zzz_slot));
}
__attribute__((naked, used)) static void stub_mmm(void)
{
    __asm__ __volatile__("nop\n" TAIL(mmm_slot));
}

void install(void)
{
    aaa_slot = stub_aaa;
    zzz_slot = stub_zzz;
    mmm_slot = stub_mmm;
    use(aaa_slot); use(zzz_slot); use(mmm_slot);
}
