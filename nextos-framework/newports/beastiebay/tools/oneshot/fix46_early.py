# -*- coding: utf-8 -*-
"""run46: set usePreference_ BEFORE the first GetNumRecords, not after it.

run45 proof that the byte is the right one:

    usepref: usePreference_ 0 -> 1   (and throw dropped from 2 to 1)

and the game stopped dying at frame 2 -- it ran the full 45 s, loaded
assets and wrote SharedPreferences.  But the error dialog still appeared,
because the flag was set inside the throw handler: the FIRST
GetNumRecords had already raised, and the self-check had already decided.

So the fix has to land before that call.  The static block does not exist
yet at Config.init (run44 measured *p == NULL there), so a single early
site is not enough -- it has to be tried repeatedly, and each attempt must
be free when the block is still absent.

Sites, in the order the log shows them:
    Config.init      -> block absent (measured)
    getAppWidth/Height
    getScaleRatio    -> last measured call before the throw
    throw handler    -> kept as the backstop

Idempotent, and a NULL block must not count as "done", or the first
(no-op) attempt would silently disable every later one.
"""
import io
import os

HERE = os.path.dirname(os.path.abspath(__file__))
PATH = os.path.join(os.path.dirname(os.path.dirname(HERE)),
                    "port", "src", "jni.c")

s = io.open(PATH, encoding="utf-8", newline="").read()

# 1. make the setter idempotent and retry-safe
OLD = """static void fp2_set_use_preference(void)
{
    nx_mod *m = nx_find_mod("libil2cpp.so");
    if (!m || !m->base) {
        nx_log("usepref: libil2cpp.so ausente");
        return;
    }
    void *p = *(void *const *)((const char *)m->base + 0x1644000u + 0xf60u);
    if (!p)
        return;
    void *sv = *(void *const *)p;
    if (!sv)
        return;
    void *t = *(void *const *)((const char *)sv + 0xb8);
    if (!t) {
        nx_log("usepref: bloco estatico T e' NULL");
        return;
    }
    unsigned char *flag = (unsigned char *)t + 0x20;
    nx_log("usepref: usePreference_ %u -> 1  (rootStorage_ T+0x18=%p)",
           (unsigned)*flag, *(void *const *)((const char *)t + 0x18));
    *flag = 1;
}"""

NEW = """static void fp2_set_use_preference(void)
{
    static int done;
    if (done)
        return;
    nx_mod *m = nx_find_mod("libil2cpp.so");
    if (!m || !m->base)
        return;
    void *p = *(void *const *)((const char *)m->base + 0x1644000u + 0xf60u);
    if (!p)
        return;
    void *sv = *(void *const *)p;
    if (!sv)
        return;
    void *t = *(void *const *)((const char *)sv + 0xb8);
    if (!t) {
        /* NAO marcar done aqui.  run44 mediu que em Config.init o bloco
         * ainda nao existe; marcar done na primeira tentativa inutil
         * desligaria todas as seguintes, que e' exatamente a armadilha
         * de uma bandeira "ja' tentei". */
        return;
    }
    unsigned char *flag = (unsigned char *)t + 0x20;
    nx_log("usepref: usePreference_ %u -> 1  (rootStorage_ T+0x18=%p)",
           (unsigned)*flag, *(void *const *)((const char *)t + 0x18));
    *flag = 1;
    done = 1;
}"""
assert OLD in s, "setter anchor not found"
s = s.replace(OLD, NEW, 1)

# 2. try it at getAppWidth / getAppHeight
OLD2 = """static int64_t j_Utility_app_size(jctx *c)
{
    const jmethod *m = c->m;
    int w = 720, h = 720;"""
NEW2 = """static int64_t j_Utility_app_size(jctx *c)
{
    const jmethod *m = c->m;
    int w = 720, h = 720;
    /* run46: primeira chance de virar a bandeira.  Se o bloco estatico
     * ainda nao existir, a funcao sai sem marcar nada e tentamos de novo
     * adiante. */
    fp2_set_use_preference();"""
assert OLD2 in s, "app_size anchor not found"
s = s.replace(OLD2, NEW2, 1)

# 3. and again just before the throw point
OLD3 = """    int bits;
    memcpy(&bits, &ratio, sizeof bits);
    fprintf(stderr,
            "[fp2/sdk] Utility.getScaleRatio(%d, %d, %d, %d) -> %.3f\\n",
            a, b, sw, sh, ratio);
    return bits;
}"""
NEW3 = """    int bits;
    memcpy(&bits, &ratio, sizeof bits);
    fprintf(stderr,
            "[fp2/sdk] Utility.getScaleRatio(%d, %d, %d, %d) -> %.3f\\n",
            a, b, sw, sh, ratio);
    /* run46: ultima chance antes do GetNumRecords que estourava. */
    fp2_set_use_preference();
    return bits;
}"""
assert OLD3 in s, "scale ratio anchor not found"
s = s.replace(OLD3, NEW3, 1)

io.open(PATH, "w", encoding="utf-8", newline="").write(s)
print("ok: run46 early usePreference_ installed")
