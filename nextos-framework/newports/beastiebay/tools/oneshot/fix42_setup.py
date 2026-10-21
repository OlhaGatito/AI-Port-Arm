# -*- coding: utf-8 -*-
"""run42: call RecordStore.Setup() on our behalf, and measure whether it sticks.

Everything up to here says Setup is the only writer of T+0x18:

  .ctor (0xa49e5c)   ldr x8,[s,#0xb8]; ldr x8,[x8,#0x18]; str -> storage_
  Setup (0xa4adf8)   ldr x9,[s,#0xb8]; str <factory result>,[x9,#0x18]

run41b measured T live at throw time: T exists, T+0x50 is already written,
but T+0x18 is NULL.  T+0x50 is assigned by the instruction immediately
BEFORE `bl Setup` (0x772f14), so the init routine reached that line -- and
yet storage_ is still empty when GetNumRecords runs.

Two readings remain, and they need different fixes:
  (a) Setup never ran at all, or ran after GetNumRecords.
  (b) Setup ran and factory returned null.

Reading the factory says (b) is impossible for the argument Setup passes:
w0=4 -> w8=3 -> jump-table case 3 -> allocates with 0x60c650 and returns
that pointer; only w0=0 or w0>4 reach `mov x22, xzr`.  So (a) is the
reading to test.

Calling Setup through reflection tests (a) and fixes it in one pass: if
T+0x18 becomes non-null afterwards, the store was simply never initialised
and the game can proceed.  If it stays null or throws, the answer is
elsewhere and we have lost one build, not a direction.

This is a deliberate exception to "diagnose before changing behaviour":
Setup is idempotent-looking, runs before any save data exists, and il2_call
catches managed exceptions, so the worst case is a logged failure.
"""
import io
import os

HERE = os.path.dirname(os.path.abspath(__file__))
PATH = os.path.join(os.path.dirname(os.path.dirname(HERE)),
                    "port", "src", "jni.c")

s = io.open(PATH, encoding="utf-8", newline="").read()

FUNC = """/* run42: chamar RecordStore.Setup() por nossa conta.
 *
 * A cadeia medida e curta: .ctor COPIA T+0x18 para storage_, e Setup e' o
 * unico escritor de T+0x18.  run41b viu T vivo, T+0x50 ja escrito (essa
 * linha e' logo ANTES do `bl Setup`) e T+0x18 ainda NULL.
 *
 * Ler o factory diz que ele nao pode devolver NULL para o argumento que o
 * Setup passa: w0=4 -> caso 3 da tabela de salto -> aloca e devolve o
 * ponteiro; so' w0=0 ou w0>4 chegam ao `mov x22, xzr`.  Entao a hipotese
 * viva e' "Setup nao rodou".
 *
 * Chama-lo por reflexao TESTA isso e CONSERTA na mesma passada.  E' uma
 * excecao deliberada a regra de diagnosticar antes de mudar comportamento:
 * Setup nao tem efeito colateral visivel antes de existir dado gravado, e
 * il2_call captura excecoes gerenciadas -- o pior caso e' uma linha de log. */
static int fp2_setup_tries;

static void fp2_try_setup_recordstore(const char *onde)
{
    if (fp2_setup_tries >= 3)
        return;
    fp2_setup_tries++;
    nx_log("setup: tentativa #%d a partir de %s", fp2_setup_tries, onde);
    if (!il2_load()) {
        nx_log("setup: il2_load falhou");
        return;
    }
    Il2CppClass *k = il2_class_by_name("RecordStore");
    if (!k) {
        nx_log("setup: classe RecordStore ausente");
        return;
    }
    nx_log("setup: classe %s", il2_class_label(k));
    const MethodInfo *m = il2_method(k, "Setup", 0);
    if (!m) {
        nx_log("setup: metodo Setup/0 ausente");
        return;
    }
    Il2CppObject *r = il2_call(m, NULL, NULL, "RecordStore.Setup");
    nx_log("setup: Setup() retornou %p", (void *)r);
    /* A unica prova que importa: o slot que .ctor copia. */
    fp2_dump_storage_root(nx_find_mod("libil2cpp.so"));
}

"""

ANCHOR = "static int64_t j_Config_init(jctx *c)\n{"
assert ANCHOR in s, "j_Config_init anchor not found"
s = s.replace(ANCHOR, FUNC + ANCHOR, 1)

OLD_TAIL = """    fprintf(stderr, "[fp2/sdk] Config.init(%d, %d, \\"%s\\")\\n",
            i0, z, s && s->str ? s->str : "(null)");
    return 0;
}"""
NEW_TAIL = """    fprintf(stderr, "[fp2/sdk] Config.init(%d, %d, \\"%s\\")\\n",
            i0, z, s && s->str ? s->str : "(null)");
    /* Config.init e' o ultimo ponto medido ANTES do throw (o log mostra
     * getScaleRatio logo depois e a excecao em seguida), e e' tarde o
     * bastante para o metadado ja estar montado -- sem isso a reflexao
     * nao acharia a classe. */
    fp2_try_setup_recordstore("Config.init");
    return 0;
}"""
assert OLD_TAIL in s, "Config.init tail anchor not found"
s = s.replace(OLD_TAIL, NEW_TAIL, 1)

io.open(PATH, "w", encoding="utf-8", newline="").write(s)
print("ok: run42 Setup call installed")
