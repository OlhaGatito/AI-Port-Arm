#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "il2.h"
#include "nx_elf.h"

typedef void Il2CppDomain;
typedef void Il2CppAssembly;
typedef void Il2CppImage;

static struct {
    int ready;
    Il2CppDomain *(*domain_get)(void);
    void *(*thread_attach)(Il2CppDomain *);
    const Il2CppAssembly **(*domain_get_assemblies)(const Il2CppDomain *,
                                                    size_t *);
    const Il2CppImage *(*assembly_get_image)(const Il2CppAssembly *);
    Il2CppClass *(*class_from_name)(const Il2CppImage *, const char *,
                                    const char *);
    FieldInfo *(*class_get_field_from_name)(Il2CppClass *, const char *);
    void (*field_static_get_value)(FieldInfo *, void *);
    void (*field_get_value)(Il2CppObject *, FieldInfo *, void *);
    void (*field_set_value)(Il2CppObject *, FieldInfo *, void *);
    Il2CppObject *(*runtime_invoke)(const MethodInfo *, void *, void **,
                                    Il2CppObject **);
    const MethodInfo *(*class_get_methods)(Il2CppClass *, void **);
    const char *(*method_get_name)(const MethodInfo *);
    uint32_t (*method_get_param_count)(const MethodInfo *);
    uint8_t (*method_is_generic)(const MethodInfo *);
    void *(*object_unbox)(Il2CppObject *);
    Il2CppClass *(*object_get_class)(Il2CppObject *);
    const char *(*class_get_name)(Il2CppClass *);
    void *(*class_get_type)(Il2CppClass *);
    Il2CppObject *(*type_get_object)(void *);
    Il2CppObject *(*string_new)(const char *);
    const void *(*method_get_param)(const MethodInfo *, uint32_t);
    Il2CppClass *(*class_from_il2cpp_type)(const void *);
    uint32_t (*gchandle_new)(Il2CppObject *, int);
    Il2CppClass *(*class_get_parent)(Il2CppClass *);
    size_t (*image_get_class_count)(const Il2CppImage *);
    Il2CppClass *(*image_get_class)(const Il2CppImage *, size_t);
    const char *(*class_get_namespace)(Il2CppClass *);
    /* run39: field enumeration.  Optional for the same reason as the three
     * above -- the control binding must not die because a diagnostic cannot
     * list fields.  il2cpp_class_get_fields(klass, void **iter) walks a linked
     * list, and field_get_name / field_get_type / type_get_type name and
     * classify each entry. */
   FieldInfo *(*class_get_fields)(Il2CppClass *, void **);
   const char *(*field_get_name)(FieldInfo *);
   const void *(*field_get_type)(FieldInfo *);
   int (*type_get_type)(const void *);
   int (*field_get_flags)(FieldInfo *);
   /* run39f: o offset e' o que amarra o nome do metadado a instrucao
    * `ldr xN, [x19, #0x??]` que a leitura offline do corpo gerado mostra.
    * Sem ele, "o campo X e' nulo" e "o codigo le +0x78" sao duas frases que
    * nao se encontram. */
   int (*field_get_offset)(FieldInfo *);
} il2;

int il2_load(void)
{
    if (il2.ready)
        return il2.ready > 0;
    nx_mod *m = nx_find_mod("libil2cpp.so");
    if (!m)
        return 0;
    il2.ready = -1;
#define GET(field, name) \
    do { *(void **)&il2.field = nx_lookup_in(m, name); \
         if (!il2.field) { nx_log("il2: falta %s", name); return 0; } } while (0)
    GET(domain_get, "il2cpp_domain_get");
    GET(thread_attach, "il2cpp_thread_attach");
    GET(domain_get_assemblies, "il2cpp_domain_get_assemblies");
    GET(assembly_get_image, "il2cpp_assembly_get_image");
    GET(class_from_name, "il2cpp_class_from_name");
    GET(class_get_field_from_name, "il2cpp_class_get_field_from_name");
    GET(field_static_get_value, "il2cpp_field_static_get_value");
    GET(field_get_value, "il2cpp_field_get_value");
    GET(field_set_value, "il2cpp_field_set_value");
    GET(runtime_invoke, "il2cpp_runtime_invoke");
    GET(class_get_methods, "il2cpp_class_get_methods");
    GET(method_get_name, "il2cpp_method_get_name");
    GET(method_get_param_count, "il2cpp_method_get_param_count");
    GET(method_is_generic, "il2cpp_method_is_generic");
    GET(object_unbox, "il2cpp_object_unbox");
    GET(object_get_class, "il2cpp_object_get_class");
    GET(class_get_name, "il2cpp_class_get_name");
    GET(class_get_type, "il2cpp_class_get_type");
    GET(type_get_object, "il2cpp_type_get_object");
    GET(string_new, "il2cpp_string_new");
    GET(method_get_param, "il2cpp_method_get_param");
    GET(class_from_il2cpp_type, "il2cpp_class_from_il2cpp_type");
    GET(gchandle_new, "il2cpp_gchandle_new");
    GET(class_get_parent, "il2cpp_class_get_parent");
    /* Optional: used only by il2_class_by_name().  These must NOT go through
     * GET -- that macro aborts the whole load when a symbol is missing, and a
     * convenience lookup is not worth taking the control binding down with it.
     * (This is the same mistake shape as run33's crash: assume an ABI, get a
     * segfault.  Here it would be a silent loss of every il2_* feature.) */
    *(void **)&il2.image_get_class_count =
        nx_lookup_in(m, "il2cpp_image_get_class_count");
    *(void **)&il2.image_get_class =
        nx_lookup_in(m, "il2cpp_image_get_class");
    *(void **)&il2.class_get_namespace =
        nx_lookup_in(m, "il2cpp_class_get_namespace");
    *(void **)&il2.class_get_fields =
        nx_lookup_in(m, "il2cpp_class_get_fields");
    *(void **)&il2.field_get_name =
        nx_lookup_in(m, "il2cpp_field_get_name");
    *(void **)&il2.field_get_type =
        nx_lookup_in(m, "il2cpp_field_get_type");
    *(void **)&il2.type_get_type =
        nx_lookup_in(m, "il2cpp_type_get_type");
    *(void **)&il2.field_get_flags =
        nx_lookup_in(m, "il2cpp_field_get_flags");
    *(void **)&il2.field_get_offset =
        nx_lookup_in(m, "il2cpp_field_get_offset");
#undef GET
    il2.ready = 1;
    return 1;
}

void il2_attach(void)
{
    Il2CppDomain *d = il2.domain_get ? il2.domain_get() : NULL;
    if (d)
        il2.thread_attach(d);
}

Il2CppClass *il2_class(const char *ns, const char *name)
{
    Il2CppDomain *domain = il2.domain_get();
    if (!domain)
        return NULL;
    size_t n = 0;
    const Il2CppAssembly **as = il2.domain_get_assemblies(domain, &n);
    for (size_t i = 0; i < n; i++) {
        const Il2CppImage *img = il2.assembly_get_image(as[i]);
        if (!img)
            continue;
        Il2CppClass *k = il2.class_from_name(img, ns, name);
        if (k)
            return k;
    }
    nx_log("il2: classe %s.%s ausente", ns, name);
    return NULL;
}

Il2CppClass *il2_class_by_name(const char *name)
{
    Il2CppDomain *domain = il2.domain_get();
    if (!domain || !name || !il2.image_get_class_count || !il2.image_get_class)
        return NULL;
    size_t n = 0;
    const Il2CppAssembly **as = il2.domain_get_assemblies(domain, &n);
    for (size_t i = 0; i < n; i++) {
        const Il2CppImage *img = il2.assembly_get_image(as[i]);
        if (!img)
            continue;
        size_t count = il2.image_get_class_count(img);
        for (size_t k = 0; k < count; k++) {
            Il2CppClass *c = il2.image_get_class(img, k);
            if (!c)
                continue;
            const char *cn = il2.class_get_name(c);
            if (cn && strcmp(cn, name) == 0)
                return c;
        }
    }
    return NULL;
}

const char *il2_class_ns(Il2CppClass *k)
{
    if (!k || !il2.class_get_namespace)
        return "";
    const char *ns = il2.class_get_namespace(k);
    return ns ? ns : "";
}

Il2CppClass *il2_class_of(Il2CppObject *obj)
{
    return obj ? il2.object_get_class(obj) : NULL;
}

const char *il2_class_name(Il2CppClass *k)
{
    return k ? il2.class_get_name(k) : "(null)";
}

/* run39h: "namespace.classe" num buffer estatico.
 *
 * Buffer estatico de proposito: o chamador e' um nx_log, que consome a
 * string imediatamente, e um caller's local seria um bug esperando --
 * a API de reflexao devolve ponteiros que o chamador nao deve liberar. */
const char *il2_class_label(Il2CppClass *k)
{
    static char buf[192];
    if (!k) {
        snprintf(buf, sizeof buf, "(null)");
        return buf;
    }
    const char *ns = il2_class_ns(k);
    const char *nm = il2_class_name(k);
    snprintf(buf, sizeof buf, "%s%s%s", ns, (ns && *ns) ? "." : "", nm);
    return buf;
}

/* O IL2CPP remove sobrecargas nao usadas; escolher por nome+aridade MEDIDOS
 * (licao do GetComponentInParent), pulando definicoes genericas, que tem a
 * mesma aridade mas nao podem ser invocadas sem inflar. */
const MethodInfo *il2_method(Il2CppClass *k, const char *name, unsigned argc)
{
    if (!k)
        return NULL;
    void *iter = NULL;
    const MethodInfo *m;
    while ((m = il2.class_get_methods(k, &iter))) {
        const char *n = il2.method_get_name(m);
        if (n && strcmp(n, name) == 0 &&
            il2.method_get_param_count(m) == argc &&
            !il2.method_is_generic(m))
            return m;
    }
    nx_log("il2: metodo %s/%u ausente em %s", name, argc,
           il2.class_get_name(k));
    return NULL;
}

/* Mesma coisa, desempatando sobrecargas pelo nome da classe do 1o parametro
 * ("String", "Int32"...). */
const MethodInfo *il2_method_p(Il2CppClass *k, const char *name, unsigned argc,
                               const char *param0_class)
{
    if (!k)
        return NULL;
    void *iter = NULL;
    const MethodInfo *m;
    while ((m = il2.class_get_methods(k, &iter))) {
        const char *n = il2.method_get_name(m);
        if (!n || strcmp(n, name) != 0 ||
            il2.method_get_param_count(m) != argc || il2.method_is_generic(m))
            continue;
        if (argc == 0)
            return m;
        Il2CppClass *pk = il2.class_from_il2cpp_type(il2.method_get_param(m, 0));
        if (pk && strcmp(il2.class_get_name(pk), param0_class) == 0)
            return m;
    }
    nx_log("il2: metodo %s/%u(%s) ausente em %s", name, argc, param0_class,
           il2.class_get_name(k));
    return NULL;
}

Il2CppObject *il2_call(const MethodInfo *m, void *self, void **args,
                       const char *what)
{
    if (!m)
        return NULL;
    Il2CppObject *ex = NULL;
    Il2CppObject *r = il2.runtime_invoke(m, self, args, &ex);
    if (ex) {
        nx_log("il2: %s lancou %s", what,
               il2.class_get_name(il2.object_get_class(ex)));
        return NULL;
    }
    return r;
}

FieldInfo *il2_field(Il2CppClass *k, const char *name)
{
    FieldInfo *f = k ? il2.class_get_field_from_name(k, name) : NULL;
    if (!f)
        nx_log("il2: campo %s ausente em %s", name, il2_class_name(k));
    return f;
}

void il2_field_get(Il2CppObject *obj, FieldInfo *f, void *out)
{
    il2.field_get_value(obj, f, out);
}

void il2_field_set(Il2CppObject *obj, FieldInfo *f, void *value)
{
    il2.field_set_value(obj, f, value);
}

void il2_static_get(FieldInfo *f, void *out)
{
    il2.field_static_get_value(f, out);
}

void *il2_unbox(Il2CppObject *boxed)
{
    return boxed ? il2.object_unbox(boxed) : NULL;
}

Il2CppObject *il2_type(Il2CppClass *k)
{
    return k ? il2.type_get_object(il2.class_get_type(k)) : NULL;
}

/* Cabecalho de array do il2cpp (32 bits): klass, monitor, bounds, max_length,
 * e so' entao os elementos. */
uint32_t il2_arr_len(Il2CppObject *arr)
{
    return arr ? *(uint32_t *)((char *)arr + 3 * sizeof(void *)) : 0;
}

void *il2_arr_at(Il2CppObject *arr, uint32_t i)
{
    return *(void **)((char *)arr + 4 * sizeof(void *) + i * sizeof(void *));
}

/* String do il2cpp: klass, monitor, length(int32), chars UTF-16. */
const char *il2_str_utf8(Il2CppObject *s, char *out, size_t size)
{
    if (!s) {
        snprintf(out, size, "(null)");
        return out;
    }
    int32_t len = *(int32_t *)((char *)s + 2 * sizeof(void *));
    const uint16_t *ch = (const uint16_t *)((char *)s + 2 * sizeof(void *) + 4);
    size_t o = 0;
    for (int32_t i = 0; i < len && o + 1 < size; i++)
        out[o++] = ch[i] < 0x80 ? (char)ch[i] : '?';
    out[o] = 0;
    return out;
}

Il2CppObject *il2_str_new(const char *utf8)
{
    return il2.string_new(utf8);
}

/* Segura o objeto contra o GC (o ponteiro nativo sozinho nao conta como raiz). */
void il2_pin(Il2CppObject *obj)
{
    if (obj)
        il2.gchandle_new(obj, 1);
}

/* Properties declared on a base class (GraphOwner.graph, MonoBehaviour.enabled)
 * are not in the derived class's own method list, and asking only the derived
 * class reports them as "missing" -- the same shape of mistake as picking a
 * stripped overload.  Walk the inheritance chain instead. */
const MethodInfo *il2_method_deep(Il2CppClass *k, const char *name,
                                  unsigned argc)
{
    for (Il2CppClass *c = k; c; c = il2.class_get_parent(c)) {
        void *iter = NULL;
        const MethodInfo *m;
        while ((m = il2.class_get_methods(c, &iter))) {
            const char *n = il2.method_get_name(m);
            if (n && strcmp(n, name) == 0 &&
                il2.method_get_param_count(m) == argc &&
                !il2.method_is_generic(m))
                return m;
        }
    }
    nx_log("il2: metodo %s/%u ausente na cadeia de %s", name, argc,
           il2_class_name(k));
    return NULL;
}

Il2CppClass *il2_class_parent(Il2CppClass *k)
{
    return k ? il2.class_get_parent(k) : NULL;
}

/* run33/run36: o "endereco nativo" de um MethodInfo.
 *
 * NINGUM offset fixo funciona aqui.  A layout de Il2CppMethod variou entre
 * versoes do runtime, e a medicao em run36 mostrou que o campo que o codigo
 * original apontava (wrapper + 2 ponteiros) aponta para FORA de
 * libil2cpp.so -- logo ele nao e o ponteiro de codigo nesta build.  A
 * medicao de run36 (base 0x7f7b407000, .ctor fn 0x7f7a715436, ou seja o
 * "endereco" abaixo da base) tambem refuta a suposicao de que o ponteiro de
 * funcao vem logo apos os dois ponteiros do wrapper.  Nao confie no offset:
 * para descobrir a base correta use fp2_list_modules(), que pergunta ao loader.
 *
 * Em vez de trocar um chute por outro, varremos os primeiros campos do
 * MethodInfo e devolvemos o primeiro que caia na imagem carregada do
 * libil2cpp.  A regiao de codigo gerado comeca em 0x66f8cc e o arquivo tem
 * 0x1730000 bytes, o que da uma janela estreita: um ponteiro de codigo
 * gerado cai nela com alta probabilidade, enquanto um ponteiro para o heap
 * gerenciado (0x55..) ou para a pilha nao. */
static const void *il2_method_fn(const MethodInfo *m, uint8_t *il2_base,
                                 size_t il2_size)
{
    if (!m)
        return NULL;
    if (!il2_base || !il2_size)
        return NULL;
    /* Managed code lives in the "il2cpp" section at file offset 0x66f8cc (see
     * tools/elf_sections.py), and .text sits just below it.  Anchor the window
     * on the first executable section rather than a round number so a data
     * pointer into the module's own rodata cannot be mistaken for code. */
    const uintptr_t lo = (uintptr_t)il2_base + 0x540000;
    const uintptr_t hi = (uintptr_t)il2_base + il2_size;
    /* 12 candidate slots covers every Il2CppMethod layout seen in the wild
     * (method, klass, invoker, methodPointer, ...); the first hit wins and
     * the loop is bounded so a wrong guess can never run off the struct. */
    for (int i = 0; i < 12; i++) {
        const void *p = *(void *const *)((const char *)m + i * sizeof(void *));
        uintptr_t v = (uintptr_t)p;
        if (v >= lo && v < hi)
            return p;
    }
    return NULL;
}

void il2_methods_foreach(Il2CppClass *k, il2_method_cb cb, void *ctx)
{
    if (!k || !cb)
        return;
    /* Hand il2_method_fn the real image bounds so it can recognise a code
     * pointer by where it lands, rather than by a remembered struct offset. */
    nx_mod *mod = nx_find_mod("libil2cpp.so");
    void *iter = NULL;
    const MethodInfo *m;
    while ((m = il2.class_get_methods(k, &iter))) {
        const char *n = il2.method_get_name(m);
        cb(ctx, n ? n : "?", m,
           il2_method_fn(m, mod ? mod->base : NULL, mod ? mod->span : 0));
    }
}

/* run39: mesmo scan, porem para um unico metodo -- e o que o hook de
 * ChangeState precisa.  Reaproveitar il2_method_fn e' obrigatorio: foi
 * justamente por reimplementar a busca do MethodInfo que o run33 terminou em
 * SIGSEGV dentro do libil2cpp. */
static const void *il2_method_find(Il2CppClass *k, const char *name,
                                   unsigned argc)
{
    if (!k || !name)
        return NULL;
    nx_mod *mod = nx_find_mod("libil2cpp.so");
    void *iter = NULL;
    const MethodInfo *m;
    while ((m = il2.class_get_methods(k, &iter))) {
        const char *n = il2.method_get_name(m);
        if (!n || strcmp(n, name) != 0)
            continue;
        /* -1u: ChangeState/1 tem uma unica sobrecarga, mas o filtro por
         * aridade evita escolher um generic sem corpo (method_is_generic)
         * caso o jogo o declare. */
        if (il2.method_get_param_count(m) != argc || il2.method_is_generic(m))
            continue;
        return il2_method_fn(m, mod ? mod->base : NULL,
                             mod ? mod->span : (size_t)0);
    }
    return NULL;
}

const void *il2_method_addr(Il2CppClass *k, const char *name, unsigned argc)
{
    const void *fn = il2_method_find(k, name, argc);
    if (!fn)
        nx_log("il2: sem corpo nativo para %s::%s/%u", il2_class_name(k),
               name, argc);
    return fn;
}

/* run39: percorre os campos.  O iterador daqui e o do proprio runtime: assim
 * como o de metodos, ele NAO pode ser reimplementado com um indice proprio.
 *
 * run39f: `ctx` agora e' o proprio objeto da classe, e nao um contexto
 * generico.  E' o que permite responder a pergunta que motivou o run39
 * inteiro: qual dos NOVE `cbnz / bl <throw>` de BootForm::Init disparou.
 * O valor lido direto do offset e' mais confiavel que
 * il2cpp_field_get_value, que exige acertar o tipo gerenciado de cada campo
 * -- e um erro de tipo ali devolve lixo em silencio, sem sinal nenhum. */
void il2_fields_foreach(Il2CppClass *k, il2_field_cb cb, void *ctx)
{
    if (!k || !cb)
        return;
    if (!il2.class_get_fields || !il2.field_get_name) {
        nx_log("il2: class_get_fields/field_get_name ausentes "
               "(esta build nao expoe enumeracao de campos)");
        return;
    }
    void *iter = NULL;
    FieldInfo *f;
    while ((f = il2.class_get_fields(k, &iter))) {
        const char *n = il2.field_get_name(f);
        int kind = -1;
        if (il2.field_get_type && il2.type_get_type) {
            const void *t = il2.field_get_type(f);
            if (t)
                kind = il2.type_get_type(t);
        }
        /* is_static vem do bit 0x10 de field_get_flags (FIELD_ATTRIBUTE_STATIC
         * em ECMA-335, o mesmo bit que o runtime gerenciado usa).  Nao
         * adivinhar pelo layout: um campo estatico precisa de
         * field_static_get_value, e passar o objeto por ele le garbage. */
        int is_static = 0;
        if (il2.field_get_flags)
            is_static = (il2.field_get_flags(f) & 0x10) ? 1 : 0;
        int off = -1;
        if (il2.field_get_offset)
            off = il2.field_get_offset(f);
        /* NULL para campo estatico: o valor de um estatico nao mora no
         * objeto, e fingir que mora seria uma leitura invalida. */
        const void *val = NULL;
        if (!is_static && ctx && off >= 0 && off < 4096)
            val = *(const void *const *)((const char *)ctx + off);
        cb(ctx, n ? n : "?", f, is_static, kind, off, val);
    }

    /* run39g: subir a cadeia de bases.
     *
     * run39f mostrou que +0x70/+0x78/+0x80 -- os tres slots que
     * BootForm::Init constroi e logo depois testa com `cbnz` -- NAO
     * aparecem na lista da propria classe: sao herdados.  Sem esta
     * passada, o despejo listava 13 campos e omitia exatamente os tres
     * que a analise estava procurando.
     *
     * Fica DEPOIS do laco, e nao dentro: dentro dele a climb seria
     * reexecutada uma vez por campo da classe derivada, e o log
     * repetiria a base uma vez por linha da lista derivada.
     *
     * A API de reflexao expoe il2cpp_class_get_parent, entao a subida
     * e' feita aqui em vez de ser adivinhada.  Itera a cadeia inteira
     * (Form -> ... -> Object) porque nao ha garantia de que o campo
     * procurado esteja no primeiro nivel herdado. */
    for (Il2CppClass *pk = il2.class_get_parent ? il2.class_get_parent(k)
                                                : NULL;
         pk;
         pk = il2.class_get_parent ? il2.class_get_parent(pk) : NULL) {
        const char *pn = il2.class_get_name ? il2.class_get_name(pk) : "?";
        void *it2 = NULL;
        FieldInfo *f2;
        while ((f2 = il2.class_get_fields(pk, &it2))) {
            int o2 = il2.field_get_offset ? il2.field_get_offset(f2) : -1;
            if (o2 < 0 || o2 > 0x100)
                continue;
            if (il2.field_get_flags && (il2.field_get_flags(f2) & 0x10))
                continue;
            cb(ctx, il2.field_get_name(f2) ? il2.field_get_name(f2) : "?",
               f2, 0, -1, o2,
               ctx ? *(const void *const *)((const char *)ctx + o2) : NULL);
        }
    }
}
