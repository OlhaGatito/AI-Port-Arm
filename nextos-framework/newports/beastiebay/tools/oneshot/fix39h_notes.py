import io

p = 'port/src/jni.c'
d = io.open(p, encoding='utf-8', newline='').read()

# --- 1) replace the slot-block note with the MEASURED answer -----------------
start = d.find(u' * run39h: o slot do stub de excecao mora AQUI')
end = d.find(u'#undef FP2_TRAMP\n', start) + len(u'#undef FP2_TRAMP\n')
assert start > 0 and end > start, (start, end)

new = u''' * run39h: o slot do stub de excecao mora AQUI, e `__attribute__((used))`
 * NAO e' opcional.
 *
 * Sintoma: com um sexto slot, o binario nao ligava --
 *   undefined reference to `fp2_watch_tramp_xthrow'
 *   relocation R_AARCH64_ADR_PREL_PG_HI21 ... dangerous relocation
 * e o throw stub ficava com o slot em `U` (indefinido) enquanto os cinco
 * slots irmaos saiam `b` (local).
 *
 * A causa so apareceu em `readelf -r`: os cinco que funcionam NAO tem
 * registro de realocacao nenhum; o quebrado tem
 * `R_AARCH64_ADR_PRE` + `R_AARCH64_LDST64_`.  Ou seja, o GCC nao estava
 * tratando os dois da mesma forma.
 *
 * `tools/repro_throw_sym9.c` mede quatro declaracoes no mesmo arquivo:
 *
 *   static void *slot_plain_static;                          -> U (com reloc)
 *   static void *slot_hidden_static __attribute__((hidden)); -> U (com reloc)
 *   void *slot_hidden_global __attribute__((hidden));        -> C (com reloc)
 *   static void *slot_used_static __attribute__((used));     -> b (SEM reloc)
 *
 * So' o `used` produz o slot local sem realocacao.  A razao: a variavel e'
 * referenciada APENAS por um `__asm__`, forma que o GCC nao associate a um
 * uso em C; sem `used` ele a trata como descartavel e emite uma referencia
 * preemptivel, que em `-fPIE` tem de poder ser ligada a outro modulo.  Com
 * `used` a definicao e' emitida e a referencia fica local.
 *
 * Isso vale para os CINCO slots tambem.  Eles ja' saiam `b` antes de
 * qualquer mudanca -- mas por um caminho que nao este' escrito em lugar
 * nenhum do codigo, e que a comparacao com o sexto provou ser fragil.  O
 * `used` foi aplicado a todos os seis para que nenhum dependa disso.
 *
 * Verificacao (um comando): nm sobre o jni.o deve mostrar seis `b
 * fp2_watch_tramp_*` e nenhum `U`.
 *
 * Hipoteses que a investigacao DESCARTOU antes de chegar nesta (uma por
 * arquivo, tools/repro_throw_sym1..8.c): o atributo `naked` (ignorado pelo
 * GCC em -O2, e os cinco slots que funcionavam tem o mesmo aviso);
 * `volatile` no ponteiro; `-fno-pie`; ordem de declaracao; ordem alfabetica;
 * passar o rabo por parametro de macro vs inclusao textual; presenca do
 * `typedef` de sincronia; e `visibility("hidden")`.  Nenhuma era a causa --
 * o que reforca que a pista estava no `readelf -r`, e nao no `nm`.
 */
#define FP2_TRAMP(sym) \\
    static void *fp2_watch_tramp_##sym __attribute__((used))
FP2_TRAMP(changestate);
FP2_TRAMP(update);
FP2_TRAMP(init);
FP2_TRAMP(classload);
FP2_TRAMP(load);
FP2_TRAMP(xthrow);
#undef FP2_TRAMP
'''

d = d[:start] + new + d[end:]

# --- 2) fix the stub note: it claimed the shared-macro theory, which was wrong
start2 = d.find(u'/* The throw stub is emitted by the SAME macro as the five method stubs.')
if start2 < 0:
    start2 = d.find(u'/* O stub de excecao usa EXATAMENTE o mesmo rabo')
assert start2 > 0
end2 = d.find(u'FP2_THROW_STUB();\n', start2) + len(u'FP2_THROW_STUB();\n')
assert end2 > start2

new2 = u'''/* Stub do auxiliar de excecao (0x600888).
 *
 * x30 de entrada e' o endereco de RETORNO do `bl 0x600888` interceptado --
 * ou seja, a instrucao seguinte a chamada, que nomeia exatamente qual das
 * nove verificacoes de nulidade de BootForm::Init disparou.  Ele e' lido
 * ANTES do `bl fp2_throw_event`, porque essa chamada sobrescreveria x30.
 *
 * O stub DEVOLVE o controle ao trampolim: a excecao precisa continuar a ser
 * lancada.  Engolir a excecao faria o dialogo de erro sumir, que e' apagar o
 * sintoma em vez de explica-lo.
 *
 * O slot e' `fp2_watch_tramp_xthrow` e o rabo e' `FP2_W_TAIL(xthrow)`, o
 * mesmo dos cinco stubs de metodo.  O que o run39h custou nao foi acertar
 * este texto -- era identico desde a primeira tentativa -- e sim descubrir
 * que o slot precisa de `__attribute__((used))` para o GCC o emitir como
 * definicao local.  A nota completa, com as medicoes, esta no bloco
 * FP2_TRAMP. */
#define FP2_THROW_STUB()                                                \\
    __attribute__((naked, used)) static void fp2_stub_throw(void)        \\
    {                                                                   \\
        __asm__ __volatile__(                                           \\
            "stp x29, x30, [sp, #-0x30]!\\n"                            \\
            "mov  x29, sp\\n"                                           \\
            "str  x19, [sp, #0x10]\\n"                                 \\
            "mov  x19, x30\\n"          /* x19 = endereco de retorno */ \\
            "bl   fp2_throw_event\\n"   /* registra; nunca engole */    \\
            "ldr  x19, [sp, #0x10]\\n"                                 \\
            "ldp  x29, x30, [sp], #0x30\\n"                            \\
            FP2_W_TAIL(xthrow));                                       \\
    }

FP2_THROW_STUB();
'''

d = d[:start2] + new2 + d[end2:]

io.open(p, 'w', encoding='utf-8', newline='').write(d)
print('ok')
