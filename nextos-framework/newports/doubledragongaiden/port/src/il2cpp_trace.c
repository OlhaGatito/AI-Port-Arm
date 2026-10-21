/* il2cpp_trace.c -- method-level instrumentation of the IL2CPP runtime.
 *
 * Why: the boot chain stops right after "VideoPlayerMgr!Init" and never
 * prints again (10 minute soak: 18300 frames, log frozen).  The managed
 * stack walker (il2cpp_format_stack_trace) cannot render that thread's
 * chain -- it faults on the Swappy vsync wait frames -- so the next best
 * evidence is to watch which C# methods the runtime keeps invoking.
 *
 * Unity binds its scripting backend through dlsym, so wrapping
 * il2cpp_runtime_invoke at the fake dlsym door (see bionic.c) catches every
 * reflected/virtual entry into managed code: coroutine MoveNext, awaiter
 * predicates, event handlers -- exactly the calls that reveal what the
 * stalled coroutine is waiting for.
 *
 * Names come from the runtime's own reflection API (no struct layout
 * assumptions): il2cpp_method_get_name / _get_class / il2cpp_class_get_name
 * / _get_namespace are all exported by libil2cpp.so.
 *
 * Off unless SF_TRACE_INVOKE=1.  Printing is budgeted so a long session
 * cannot flood the log: SF_TRACE_INVOKE_MAX (default 20000).
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nx_elf.h"
#include "sf.h"

typedef void *(*invoke_fn)(void *method, void *obj, void **args, void **exc);
typedef const char *(*method_name_fn)(const void *method);
typedef void *(*method_class_fn)(const void *method);
typedef const char *(*class_name_fn)(void *klass);

static invoke_fn real_invoke;
static method_name_fn method_get_name_p;
static method_class_fn method_get_class_p;
static class_name_fn class_get_name_p;
static class_name_fn class_get_namespace_p;

static int enabled;
static int resolved;
static long budget;

/* Noise filter.  The engine enters managed code a few thousand times per
 * second through UpdateFunction::Invoke, which drains the budget in seconds
 * and hides the few calls that matter.  SF_TRACE_INVOKE_SKIP is a
 * comma-separated list of substrings; matching names are not printed and do
 * not consume budget. */
#define MAX_SKIP 8
static const char *skip[MAX_SKIP];
static int skip_count;

/* Matched against the fully qualified "ns.class::method" string, which is
 * what the skip list is written in. */
static int is_noise(const char *full)
{
    for (int i = 0; i < skip_count; i++) {
        if (strstr(full, skip[i]))
            return 1;
    }
    return 0;
}

/* Resolve lazily: libil2cpp is only mapped once the loader chain has run. */
static void resolve_once(void)
{
    if (resolved)
        return;
    resolved = 1;

    const char *on = getenv("SF_TRACE_INVOKE");
    if (!on || !*on || strcmp(on, "0") == 0)
        return;

    const char *max = getenv("SF_TRACE_INVOKE_MAX");
    budget = max && *max ? atol(max) : 20000;
    if (budget <= 0)
        budget = 20000;

    const char *sk = getenv("SF_TRACE_INVOKE_SKIP");
    if (!sk)
        /* Default: drop the per-frame engine tick noise (measured on the box:
         * UpdateFunction::Invoke alone is ~10k calls in 45s) so the budget
         * lasts long enough to cover the stall window itself. */
        sk = "UpdateFunction::Invoke,"
             "NativeInputSystem,"
             "Canvas::Send,"
             "RenderPipelineManager,"
             "ScriptableRuntimeReflectionSystemWrapper,"
             "NavMesh::Internal_CallOnNavMeshPreUpdate,"
             "OnDemandRendering,"
             "SupportedRenderingFeatures,"
             "GameFramework::Update";
    if (*sk) {
        char buf[512];
        snprintf(buf, sizeof buf, "%s", sk);
        char *p = buf;
        while (*p && skip_count < MAX_SKIP) {
            char *comma = strchr(p, ',');
            if (comma)
                *comma = '\0';
            if (*p)
                skip[skip_count++] = strdup(p);
            if (!comma)
                break;
            p = comma + 1;
        }
    }

    real_invoke = (invoke_fn)nx_lookup("il2cpp_runtime_invoke");
    method_get_name_p =
        (method_name_fn)nx_lookup("il2cpp_method_get_name");
    method_get_class_p =
        (method_class_fn)nx_lookup("il2cpp_method_get_class");
    class_get_name_p = (class_name_fn)nx_lookup("il2cpp_class_get_name");
    class_get_namespace_p =
        (class_name_fn)nx_lookup("il2cpp_class_get_namespace");

    if (!real_invoke) {
        fprintf(stderr, "[sf/invoke] disabled: il2cpp_runtime_invoke not "
                        "resolvable\n");
        return;
    }
    enabled = 1;
    fprintf(stderr, "[sf/invoke] armed: budget=%ld name_api=%d "
                    "class_api=%d\n",
            budget, method_get_name_p != NULL, class_get_name_p != NULL);
}

/* SF_DUMP_METHODS=<path>: walk the loaded assemblies through the runtime's
 * own reflection API and write "namespace.class::method @ code_address" for
 * every type whose name matches a keyword.  This is how a method gets a code
 * address without parsing global-metadata.dat by hand: MethodInfo slot 0 is
 * the AOT entry point, so the dump is enough to disassemble any C# method on
 * the host.
 *
 * Only interesting families are written; a full walk is hundreds of MB. */
static void dump_methods(const char *path)
{
    typedef void *(*domain_get_fn)(void);
    typedef const void **(*asm_get_fn)(void *, size_t *);
    typedef void *(*asm_image_fn)(const void *);
    typedef size_t (*img_count_fn)(const void *);
    typedef void *(*img_class_fn)(const void *, size_t);
    typedef const void *(*cls_methods_fn)(void *, void **);

    domain_get_fn domain_get = (domain_get_fn)nx_lookup("il2cpp_domain_get");
    asm_get_fn assemblies =
        (asm_get_fn)nx_lookup("il2cpp_domain_get_assemblies");
    asm_image_fn image_of = (asm_image_fn)nx_lookup("il2cpp_assembly_get_image");
    img_count_fn class_count =
        (img_count_fn)nx_lookup("il2cpp_image_get_class_count");
    img_class_fn class_at =
        (img_class_fn)nx_lookup("il2cpp_image_get_class");
    cls_methods_fn methods_of =
        (cls_methods_fn)nx_lookup("il2cpp_class_get_methods");

    if (!domain_get || !assemblies || !image_of || !class_count || !class_at ||
        !methods_of || !class_get_name_p) {
        fprintf(stderr, "[sf/dump] reflection API incomplete: dom=%d asm=%d "
                        "img=%d cnt=%d at=%d mth=%d\n",
                domain_get != NULL, assemblies != NULL, image_of != NULL,
                class_count != NULL, class_at != NULL, methods_of != NULL);
        return;
    }

    static const char *kw[] = {"Video",  "Cabbage",   "Game.",   "Login",
                               "Sdk",    "XLua",      "PlayerMgr", "Helper",
                               "Coroutine", "Hotfix",  "YCore",   "Manager"};
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "[sf/dump] cannot open %s\n", path);
        return;
    }
    void *domain = domain_get();
    size_t na = 0;
    const void **asms = assemblies(domain, &na);
    long written = 0;
    for (size_t a = 0; a < na; a++) {
        void *image = image_of(asms[a]);
        if (!image)
            continue;
        size_t nc = class_count(image);
        for (size_t i = 0; i < nc; i++) {
            void *klass = class_at(image, i);
            if (!klass)
                continue;
            const char *cn = class_get_name_p(klass);
            if (!cn)
                continue;
            const char *ns = class_get_namespace_p ? class_get_namespace_p(klass)
                                                   : "";
            /* Match against the fully qualified name: the interesting
             * families live in namespaces such as Game.Framework, whose class
             * names alone (GameResMgr, UIMgr ...) carry no keyword. */
            char qname[512];
            snprintf(qname, sizeof qname, "%s%s%s", ns ? ns : "",
                     (ns && *ns) ? "." : "", cn);
            int hit = 0;
            for (size_t k = 0; k < sizeof kw / sizeof *kw; k++)
                if (strstr(qname, kw[k])) {
                    hit = 1;
                    break;
                }
            if (!hit)
                continue;
            void *iter = NULL;
            const void *m;
            while ((m = methods_of(klass, &iter))) {
                const char *mn = method_get_name_p ? method_get_name_p(m) : "?";
                void *code = *(void **)m;
                fprintf(f, "%s%s%s::%s @ %p\n", ns ? ns : "",
                        (ns && *ns) ? "." : "", cn, mn ? mn : "?", code);
                written++;
            }
        }
    }
    fclose(f);
    fprintf(stderr, "[sf/dump] wrote %ld methods to %s\n", written, path);
}

void *sf_il2cpp_trace_invoke(void)
{
    resolve_once();
    return enabled ? (void *)sf_il2cpp_trace_dispatch : NULL;
}

/* The installed entry point: log, then hand over to the real runtime. */
void *sf_il2cpp_trace_dispatch(void *method, void *obj, void **args,
                               void **exc)
{
    /* First managed entry means the domain is populated; that is the safe
     * moment to enumerate methods. */
    static int dumped;
    if (!dumped) {
        dumped = 1;
        const char *path = getenv("SF_DUMP_METHODS");
        if (path && *path)
            dump_methods(path);
    }
    if (enabled && budget > 0 && method) {
        const char *mname = method_get_name_p ? method_get_name_p(method)
                                              : NULL;
        const char *cname = NULL;
        const char *ns = NULL;
        if (method_get_class_p) {
            void *klass = method_get_class_p(method);
            if (klass) {
                if (class_get_name_p)
                    cname = class_get_name_p(klass);
                if (class_get_namespace_p)
                    ns = class_get_namespace_p(klass);
            }
        }
        if (!mname)
            mname = "?";
        if (!cname)
            cname = "?";
        if (!ns)
            ns = "";
        char full[512];
        snprintf(full, sizeof full, "%s%s%s::%s", ns, (ns && *ns) ? "." : "",
                 cname, mname);
        if (is_noise(full))
            return real_invoke(method, obj, args, exc);
        /* MethodInfo::methodPointer (slot 0 of the IL2CPP MethodInfo) is the
         * AOT code address: printing it lets us disassemble the exact body
         * on the host when a method's name alone cannot explain a stall. */
        void *code = *(void **)method;
        nx_mod *il2 = nx_find_mod("libil2cpp.so");
        if (il2 && code) {
            unsigned long base = (unsigned long)il2->base;
            unsigned long addr = (unsigned long)code;
            if (addr >= base && addr < base + il2->span)
                fprintf(stderr, "[sf/invoke] %s%s%s::%s @ libil2cpp+0x%lx\n",
                        ns, (ns && *ns) ? "." : "", cname, mname,
                        addr - base);
            else
                fprintf(stderr, "[sf/invoke] %s%s%s::%s @ %p (outside)\n", ns,
                        (ns && *ns) ? "." : "", cname, mname, code);
        } else {
            fprintf(stderr, "[sf/invoke] %s%s%s::%s\n", ns,
                    (ns && *ns) ? "." : "", cname, mname);
        }
        if (--budget == 0)
            fprintf(stderr, "[sf/invoke] budget exhausted\n");
    }
    return real_invoke(method, obj, args, exc);
}
