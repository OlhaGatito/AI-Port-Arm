#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include <link.h>

#include "so_util.h"
#include "util.h"
#include "jni_shim.h"
#include "android_shim.h"

#define UNITY_HEAP_MB 256
#define IL2CPP_HEAP_MB 128

static uintptr_t unity_base = 0;
static uintptr_t il2cpp_base = 0;

static void *load_module(const char *name, size_t size)
{
    void *heap = mmap(
        NULL,
        size,
        PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0
    );

    if (heap == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }

    printf("[loader] carregando %s...\n", name);

    if (so_load(name, heap, size) < 0) {
        printf("[loader] ERRO: so_load(%s) falhou\n", name);
        munmap(heap, size);
        return NULL;
    }
    so_record_phdr(name);

    if (so_relocate() < 0) {
        printf("[loader] ERRO: relocacao de %s falhou\n", name);
        return NULL;
    }

    if (so_resolve(dynlib_functions, dynlib_functions_count, 0) < 0) {
        printf("[loader] ERRO: resolve de %s falhou\n", name);
        return NULL;
    }

    so_finalize();
    so_flush_caches();
    so_execute_init_array();

    printf("[loader] %s carregado.\n", name);

    return heap;
}

int dl_iterate_phdr(int (*cb)(struct dl_phdr_info *, size_t, void *), void *data) {
    static int (*real)(int (*)(struct dl_phdr_info *, size_t, void *), void *);
    if (!real) real = (void *)dlsym(RTLD_NEXT, "dl_iterate_phdr");
    int r = real ? real(cb, data) : 0;
    if (r) return r;
    for (int i = 0; i < g_so_nmods; i++) {
        struct dl_phdr_info info;
        memset(&info, 0, sizeof info);
        info.dlpi_addr = (ElfW(Addr))g_so_mods[i].base;
        info.dlpi_name = g_so_mods[i].name;
        info.dlpi_phdr = (const ElfW(Phdr) *)g_so_mods[i].ph;
        info.dlpi_phnum = (ElfW(Half))g_so_mods[i].phnum;
        r = cb(&info, sizeof info, data);
        if (r) return r;
    }
    return r;
}
static void *sf2_dlopen(const char *name, int flags)
{
    fprintf(stderr, "[SF2 dlopen] %s\n", name ? name : "(null)");
    return dlopen(name, flags);
}

static void *sf2_dlsym(void *handle, const char *symbol)
{
    fprintf(stderr, "[SF2 dlsym] %s\n", symbol ? symbol : "(null)");
    return dlsym(handle, symbol);
}

static int sf2_dlclose(void *handle)
{
    return dlclose(handle);
}

static int sf2_dladdr(const void *addr, Dl_info *info)
{
    return dladdr(addr, info);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("========================================\n");
    printf(" Shadow Fight 2 - Unity ARM64 Loader\n");
    printf(" Unity 2018.4.8f1 / IL2CPP\n");
    printf("========================================\n");

    dlopen("libz.so.1", RTLD_NOW | RTLD_GLOBAL);
    dlopen("libGLESv2.so.2", RTLD_NOW | RTLD_GLOBAL);
    dlopen("libEGL.so.1", RTLD_NOW | RTLD_GLOBAL);

    android_shim_init();

    printf("[F0] libmain.so\n");

    if (!load_module("libmain.so", 32 * 1024 * 1024))
        return 1;

    void *vm = NULL;
    void *env = NULL;

    jni_shim_init(&vm, &env);

    printf("[F1] libil2cpp.so\n");

    void *il2cpp_heap = load_module(
        "libil2cpp.so",
        IL2CPP_HEAP_MB * 1024UL * 1024UL
    );

    if (!il2cpp_heap)
        return 1;

    il2cpp_base = (uintptr_t)il2cpp_heap;

    so_module *il2cpp_module = so_save();

    printf("[F2] libunity.so\n");

    void *unity_heap = load_module(
        "libunity.so",
        UNITY_HEAP_MB * 1024UL * 1024UL
    );

    if (!unity_heap)
        return 1;

    unity_base = (uintptr_t)unity_heap;

    so_module *unity_module = so_save();

    so_use(il2cpp_module);
    uintptr_t il2_onload = so_find_addr_safe("JNI_OnLoad");

    if (il2_onload) {
        printf("[JNI] libil2cpp.so JNI_OnLoad...\n");

        int version =
            ((int (*)(void *, void *))il2_onload)(vm, NULL);

        printf("[JNI] IL2CPP retornou 0x%x\n", version);
    } else {
        printf("[JNI] libil2cpp.so nao possui JNI_OnLoad.\\n");
    }

    so_use(unity_module);
    uintptr_t unity_onload = so_find_addr_safe("JNI_OnLoad");

    if (unity_onload) {
        printf("[JNI] libunity.so JNI_OnLoad...\n");

        int version =
            ((int (*)(void *, void *))unity_onload)(vm, NULL);

        printf("[JNI] Unity retornou 0x%x\n", version);
    } else {
        printf("[JNI] libunity.so nao possui JNI_OnLoad.\\n");
    }

    printf("----------------------------------------\n");
    printf(" Unity   base: %p\n", (void *)unity_base);
    printf(" IL2CPP  base: %p\n", (void *)il2cpp_base);
    printf("----------------------------------------\n");

    printf("[OK] Shadow Fight 2 modules loaded.\n");

    return 0;
}
