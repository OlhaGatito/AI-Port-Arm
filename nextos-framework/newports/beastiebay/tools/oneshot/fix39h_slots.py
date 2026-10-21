import io

p = 'port/src/jni.c'
d = io.open(p, encoding='utf-8', newline='').read()

start = d.find(u' * run39h: o slot do stub de excecao mora AQUI')
end = d.find(u'#undef FP2_TRAMP\n', start) + len(u'#undef FP2_TRAMP\n')
assert start > 0 and end > start, (start, end)

new = u""" * run39h: o slot do stub de excecao mora AQUI, e nao ao lado do proprio stub.
 *
 * A razao e' MEDIDA, nao teorizada.  Sob `-fPIE`, um slot `static` de ponteiro
 * nomeado por um `__asm__` inline pode sair com ligacao `U` (indefinido,
 * preemptivel) em vez de `b` (local) -- e um `U` aqui faz a ligacao final
 * falhar com "undefined reference".  Sete hipoteses foram testadas e
 * descartadas uma a uma (tools/repro_throw_sym1..8.c, um arquivo por
 * hipotese): `naked` ignorado pelo GCC em -O2 (os cinco que funcionam tambem
 * o tem), `volatile`, `-fno-pie`, ordem no arquivo, ordem alfabetica, forma
 * de passar o rabo (parametro de macro vs inclusao textual) e presenca do
 * `typedef` de sincronia.  Nenhuma explica -- e o resultado ainda muda
 * entre arquivos com codigo equivalente, o que impede fixar a regra.
 *
 * A solucao aplicada e' NAO depender da regra: o stub de excecao ganha um
 * slot proprio, `fp2_watch_tramp_xthrow`, declarado aqui ao lado dos outros
 * cinco, e o `asm` alcanca esse slot pelo VALOR de um acessor em C
 * (`fp2_xthrow_tramp_addr`) em vez de nomear o ponteiro.  Repro 3 mediu que
 * um simbolo de FUNCAO `static` referenciado pelo assembler sai local (`t`),
 * enquanto o ponteiro nomeado diretamente saia `U`.
 *
 * Sucesso aqui e' verificavel em um comando: `nm` sobre o jni.o mostrando
 * seis simbolos `b fp2_watch_tramp_*` e nenhum `U`. */
#define FP2_TRAMP(sym) static void *fp2_watch_tramp_##sym
FP2_TRAMP(changestate);
FP2_TRAMP(update);
FP2_TRAMP(init);
FP2_TRAMP(classload);
FP2_TRAMP(load);
#undef FP2_TRAMP

/* Slot do stub de excecao, no mesmo grupo dos cinco. */
static void *fp2_watch_tramp_xthrow;
"""

d = d[:start] + new + d[end:]
io.open(p, 'w', encoding='utf-8', newline='').write(d)
print('ok')
