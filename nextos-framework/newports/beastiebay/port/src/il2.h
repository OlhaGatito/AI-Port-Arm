/* Acesso enxuto ao IL2CPP do jogo pela API exportada do libil2cpp.
 *
 * Tudo aqui e' resolvido por NOME de classe/metodo em tempo de execucao —
 * nenhum RVA cravado, entao nao herda offset de outro jogo (licao do Hitman
 * GO).  Usado pela sonda de diagnostico (probe.c) e pela ligacao do controle
 * (pad.c).
 */
#ifndef FP2_IL2_H
#define FP2_IL2_H

#include <stddef.h>
#include <stdint.h>

typedef void Il2CppClass;
typedef void Il2CppObject;
typedef void MethodInfo;
typedef void FieldInfo;

/* Resolve a tabela de funcoes; devolve 1 quando o libil2cpp esta carregado e
 * todos os simbolos existem.  Idempotente e barato depois da 1a vez. */
int il2_load(void);

/* Prende a thread corrente ao dominio (obrigatorio antes de invocar C#). */
void il2_attach(void);

/* Classe por (namespace, nome) varrendo todos os assemblies. NULL se ausente
 * (e loga).  Classes ANINHADAS nao sao achadas assim — use il2_class_of. */
Il2CppClass *il2_class(const char *ns, const char *name);
/* run34: procura pelo NOME SIMPLES em todos os assemblies, sem depender do
 * namespace.  il2_class() exige o par (ns, name) exato, e adivinhar o
 * namespace a partir do metadado e fragil -- o layout de TypeDefinition do
 * v24 nao bate com a suposicao de 32 bytes.  Como a API de reflexao expoe
 * image_get_class_count/image_get_class, enumerar e mais barato do que
 * interpretar metadado.  Prefira esta funcao quando o nome ja basta. */
Il2CppClass *il2_class_by_name(const char *name);
Il2CppClass *il2_class_of(Il2CppObject *obj);
const char *il2_class_name(Il2CppClass *k);

/* run39h: a classe REAL de um objeto, e o rotulo "namespace.classe".
 *
 * A razao de existir: a leitura offline nao diz qual e' o tipo, porque o
 * `libil2cpp.so` esta stripped e o deslocamento medido (0xa4a674) nao
 * pertence a nenhum metodo gerado -- a tabela de ponteiros de metodo
 * (43373 entradas) nao o contem.  Sobrar para a reflexao e' a unica saida:
 * e' a mesma API que o run39g provou funcional. */
Il2CppClass *il2_class_of(Il2CppObject *obj);
const char *il2_class_label(Il2CppClass *k);
const char *il2_class_ns(Il2CppClass *k);

/* Metodo por nome + aridade, pulando definicoes genericas (nao invocaveis).
 * NULL se ausente (e loga). */
const MethodInfo *il2_method(Il2CppClass *k, const char *name, unsigned argc);
/* Desempata sobrecargas de mesma aridade pela classe do 1o parametro
 * ("String", "Int32", "Type"...). */
const MethodInfo *il2_method_p(Il2CppClass *k, const char *name, unsigned argc,
                               const char *param0_class);
/* Igual, mas subindo a cadeia de heranca (propriedade de classe base). */
const MethodInfo *il2_method_deep(Il2CppClass *k, const char *name,
                                  unsigned argc);
Il2CppClass *il2_class_parent(Il2CppClass *k);

/* run33: percorre TODOS os metodos de uma classe devolvendo nome + endereco
 * nativo.  O objetivo e descobrir qual metodo gerado contem uma dada
 * comparacao -- o metadado v24 nao guarda RVA de metodo (verificado: nenhuma
 * das 31 tabelas e densa em valores dentro da secao gerenciada), entao a
 * unica fonte de endereco e a API de reflexao em tempo de execucao.
 * O chamador DEVE usar o iterador fornecido, nunca reimplementar a
 * assinatura: il2cpp_class_from_name recebe (image, namespace, name) e
 * passar o domain no lugar do image segfaulta dentro do libil2cpp. */
typedef void (*il2_method_cb)(void *ctx, const char *name,
                              const MethodInfo *m, const void *fn);
void il2_methods_foreach(Il2CppClass *k, il2_method_cb cb, void *ctx);

/* run39: endereco nativo de UM metodo, ou NULL.  Era preciso porque o hook de
 * ChangeState instalado precisa do alvo, e usar o MethodInfo exigiria
 * reimplementar o scan de la dentro de il2.c -- que e justamente o que deu
 * errado no run33.  Aqui a unica fonte continua sendo a mesma varredura que
 * ja foi validada. */
const void *il2_method_addr(Il2CppClass *k, const char *name, unsigned argc);

/* run39: enumera TODOS os campos (instancia e estaticos) de uma classe.  O
 * codigo de diagnostico precisa ler resLoad_ / RESLOAD_SEB_* para saber qual
 * das quatro checagens falhou, e esses nomes vem do metadado, nao de um layout
 * fixo -- logo o unico jeito confiavel e pedir a lista a API de reflexao.
 *
 * run39f: acrescentei `offset` e `value` ao callback.  Sem eles a lista de
 * nomes nao respondia a pergunta que importava: o codigo gerado de
 * BootForm::Init tem NOVE pontos `cbnz xN / bl <throw>` e eu precisava
 * saber QUAL objeto veio NULL.  Com o offset, o nome do metadado casa com a
 * instrucao `ldr x20, [x19, #0x78]` que a leitura offline ja mostrou; com o
 * valor, a resposta vem sem passar por hypotese. */
typedef void (*il2_field_cb)(void *ctx, const char *name, FieldInfo *f,
                             int is_static, int type_kind,
                             int offset, const void *value);
void il2_fields_foreach(Il2CppClass *k, il2_field_cb cb, void *ctx);

/* Invoca; em excecao gerenciada loga o tipo e devolve NULL. */
Il2CppObject *il2_call(const MethodInfo *m, void *self, void **args,
                       const char *what);

FieldInfo *il2_field(Il2CppClass *k, const char *name);
void il2_field_get(Il2CppObject *obj, FieldInfo *f, void *out);
/* Para campo de tipo-referencia passe o PROPRIO objeto (nao &obj): o il2cpp
 * grava `value` direto.  Para tipo-valor passe o ponteiro para o valor. */
void il2_field_set(Il2CppObject *obj, FieldInfo *f, void *value);
void il2_static_get(FieldInfo *f, void *out);

void *il2_unbox(Il2CppObject *boxed);
/* System.Type de uma classe (para FindObjectsOfType/GetComponent). */
Il2CppObject *il2_type(Il2CppClass *k);

/* Arrays e strings gerenciadas (layout de 32 bits). */
uint32_t il2_arr_len(Il2CppObject *arr);
void *il2_arr_at(Il2CppObject *arr, uint32_t i);
const char *il2_str_utf8(Il2CppObject *s, char *out, size_t size);
Il2CppObject *il2_str_new(const char *utf8);
/* Raiz de GC permanente para um objeto guardado so' do lado nativo. */
void il2_pin(Il2CppObject *obj);

#endif
