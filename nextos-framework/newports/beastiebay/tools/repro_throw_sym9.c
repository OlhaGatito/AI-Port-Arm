/* run39h: which declaration makes an asm-named pointer LOCAL under -fPIE?
 *
 * The jni.c evidence: five slots referenced from the same kind of inline asm
 * come out `b` (local bss) with NO relocation record at all, while a sixth
 * -- added later, from the same macros -- comes out `U` (undefined) WITH an
 * R_AARCH64_ADR_PRE relocation.  readelf -r shows the difference plainly:
 * the working five have no relocation, the broken one does.
 *
 * Four candidate declarations, one file, so the answer is a single nm:
 *
 *   plain_static   : what the five working slots do (static, plain)
 *   hidden_static  : static + visibility("hidden")
 *   hidden_global  : non-static + visibility("hidden")
 *   used_static    : static + __attribute__((used))  (keeps it alive)
 *
 * Whichever yields a defined local symbol is the declaration to use.
 */

#define MAKE(sym, decl)                                        \
    decl;                                                      \
    __attribute__((used)) void s_##sym(void)                   \
    {                                                          \
        __asm__ __volatile__(                                  \
            "adrp x16, slot_" #sym "\n"                        \
            "ldr  x16, [x16, #:lo12:slot_" #sym "]\n"          \
            "br   x16\n");                                     \
    }                                                          \
    void set_##sym(void *p) { slot_##sym = p; }

MAKE(plain_static, static void *slot_plain_static)
MAKE(hidden_static,
     static void *slot_hidden_static __attribute__((visibility("hidden"))))
MAKE(hidden_global,
     void *slot_hidden_global __attribute__((visibility("hidden"))))
MAKE(used_static, static void *slot_used_static __attribute__((used)))
