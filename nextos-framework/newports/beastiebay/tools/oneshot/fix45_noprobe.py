# -*- coding: utf-8 -*-
"""run45: drop the Setup probes, set usePreference_ at throw time instead.

run44 died with SIGILL (signum 4).  The cause is the entry probe: Setup's
first four instructions are

    0xa4adf8  str  x19, [sp, #-0x20]!
    0xa4adfc  stp  x29, x30, [sp, #0x10]
    0xa4ae00  add  x29, sp, #0x10
    0xa4ae04  adrp x19, #0x1767000     <- PC-relative

and the trampoline copies those four somewhere else.  `adrp` is
PC-relative, so at the new address x19 gets the wrong page, and the next
`strb w8, [x19, #0xe8a]` writes somewhere it must not.  A hook that
relocates PC-relative instructions measures the hook, then kills the game.

Rule for this framework: never hook a site whose first four instructions
contain adrp/adr or a branch.  The five method stubs happened to be safe;
this one was not.

What survives from run44 is the important part:
  * probe #3 fired with lr=+0x7eff54, i.e. the GAME calls Setup itself
    from 0x7eff50.  So "Setup never ran" is finally settled: it runs.
  * the static block is not allocated yet at Config.init time (*p is NULL
    there), which is why run42's Setup call could not have worked and why
    run44's flag write silently returned.

So the flag has to be set at a moment when the block exists.  Throw time
is exactly that, and it is also the last possible moment before the game
gives up.  Setting it there does not repair the call that already failed,
but every GetNumRecords after it takes the preference branch instead.
"""
import io
import os

HERE = os.path.dirname(os.path.abspath(__file__))
PATH = os.path.join(os.path.dirname(os.path.dirname(HERE)),
                    "port", "src", "jni.c")

s = io.open(PATH, encoding="utf-8", newline="").read()

# 1. stop installing the two probes
OLD = ("        fp2_install_throw_hook();\n"
       "        fp2_install_setupexit_hook();\n"
       "        fp2_install_setupent_hook();")
NEW = ("        fp2_install_throw_hook();\n"
       "        /* run45: as duas sondas de Setup ficam SEM instalar.  A de\n"
       "         * entrada copiava um `adrp`, que e' PC-relativo: reexecutado\n"
       "         * no trampolim, x19 recebia a pagina errada e o `strb`\n"
       "         * seguinte escrevia onde nao devia -- SIGILL (signum 4) no\n"
       "         * run44.  Nunca hookar um ponto cujas 4 primeiras instrucoes\n"
       "         * tenham adrp/adr ou desvio. */")
assert OLD in s, "probe install anchor not found"
s = s.replace(OLD, NEW, 1)

# 2. stop calling Setup from Config.init -- it cannot work there and it
#    perturbs the ordering.
OLD2 = """    /* Config.init e' o ultimo ponto medido ANTES do throw (o log mostra
     * getScaleRatio logo depois e a excecao em seguida), e e' tarde o
     * bastante para o metadado ja estar montado -- sem isso a reflexao
     * nao acharia a classe. */
    fp2_try_setup_recordstore("Config.init");"""
NEW2 = """    /* run45: NAO chamar Setup aqui.  run44 mediu que neste instante
     * *(0x1644f60) existe mas *p ainda e' NULL -- o bloco estatico do
     * RecordStore ainda nao foi montado, entao a chamada nao pode ter
     * efeito e ainda perturba a ordenacao.  A sonda #3 do run44 tambem
     * mostrou que o JOGO chama Setup por conta propria (lr=+0x7eff54),
     * entao "Setup nunca rodou" esta' encerrado: ele roda. */
    (void)i0;
    (void)z;
    (void)s;"""
assert OLD2 in s, "Config.init call anchor not found"
s = s.replace(OLD2, NEW2, 1)

# 3. set the flag at throw time, right after the dump
OLD3 = """    if (fp2_throw_hits == 1 && self) {
        fp2_dump_bootform_fields(self);
        fp2_dump_storage_root(mod);
    }
}"""
NEW3 = """    if (fp2_throw_hits == 1 && self) {
        fp2_dump_bootform_fields(self);
        fp2_dump_storage_root(mod);
        fp2_set_use_preference();
    }
}"""
assert OLD3 in s, "throw event anchor not found"
s = s.replace(OLD3, NEW3, 1)

io.open(PATH, "w", encoding="utf-8", newline="").write(s)
print("ok: run45 probes removed, flag set at throw time")
