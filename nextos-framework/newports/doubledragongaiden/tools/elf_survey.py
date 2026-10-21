#!/usr/bin/env python3
"""Read-only survey of every .so under lib/arm64-v8a of the unpacked APK.

For each ELF64 aarch64 shared object report:
  - class/machine/flags
  - SONAME and DT_NEEDED entries
  - exported JNI entry points (Java_* / JNI_OnLoad / JNI_OnUnload)
  - undefined-symbol totals plus a curated breakdown of Android/JNI/GL/mem
    families that a Linux port must satisfy
  - whether it links libc++_shared / liblog / libandroid / other bionic-only
    libraries (i.e. needs bionic-shim support)

Usage: python elf_survey.py [apk_dir]
Default apk_dir is ../apk relative to this file.
"""
import os
import struct
import sys

from elftools.elf.elffile import ELFFile
from elftools.elf.dynamic import DynamicSection

CURATED = {
    "JNI":      lambda s: s.startswith("JNI") or "_Java_" in s or s.startswith("Java_"),
    "android":  lambda s: s.startswith(("AAsset", "AAssetManager", "AConfiguration",
                                        "AAudio", "AChoreographer", "ANativeActivity",
                                        "ANativeWindow", "ALooper", "AInputQueue",
                                        "AWindow", "ARect", "Java_")),
    "jnigraph": lambda s: s.startswith(("jniGraphics", "jni Surface", "JNI_OnLoad")),
    "egl":      lambda s: s.startswith(("egl", "EGL")),
    "gles":     lambda s: s.startswith(("gl", "GL", "glDraw", "glTexImage")),
    "vulkan":   lambda s: s.startswith(("vk", "Vk")),
    "posix":    lambda s: s.startswith(("pthread", "sem_", "mmap", "mprotect", "dlopen",
                                        "dlsym", "dlclose", "dladdr", "dlerror", "pipe",
                                        "epoll", "inotify", "fork", "exec", "socket",
                                        "clock_", "timer_", "sched_", "__android_log")),
    "bionic":   lambda s: s.startswith(("__android", "android_", "bionic", "__cxa_thread",
                                        "__emutls", "__tls_get_addr", "tcache", "mallopt",
                                        "mallinfo", "__malloc", "__system_property")),
}


def survey(path):
    out = {"path": path}
    with open(path, "rb") as f:
        elf = ELFFile(f)
        out["class"] = elf.elfclass
        out["machine"] = elf.header["e_machine"]
        out["flags"] = elf.header["e_flags"]
        out["type"] = elf.header["e_type"]

        needed, soname = [], None
        dynsyms = None
        for sec in elf.iter_sections():
            if isinstance(sec, DynamicSection):
                for tag in sec.iter_tags():
                    if tag.entry.d_tag == "DT_NEEDED":
                        needed.append(tag.needed)
                    elif tag.entry.d_tag == "DT_SONAME":
                        soname = tag.soname
            if sec.header["sh_type"] in ("SHT_DYNSYM", "SHT_SYMTAB"):
                dynsyms = sec
        out["needed"] = needed
        out["soname"] = soname

        defined, undefined = [], []
        if dynsyms is not None:
            for sym in dynsyms.iter_symbols():
                name = sym.name
                if not name:
                    continue
                if sym["st_shndx"] == "SHN_UNDEF":
                    undefined.append(name)
                else:
                    defined.append(name)
        out["n_defined"] = len(defined)
        out["n_undefined"] = len(undefined)

        java_exp = [s for s in defined
                    if s.startswith("Java_") or s in ("JNI_OnLoad", "JNI_OnUnload")]
        out["java_exports"] = java_exp

        fam = {}
        for fam_name, pred in CURATED.items():
            hits = sorted({s for s in undefined if pred(s)})
            if hits:
                fam[fam_name] = hits
        out["families"] = fam

        # sample of remaining undefined (largest-interest first: not libc-ish)
        libcish = pred_boring = None
        def _boring(s):
            return s.startswith("__") or s in (
                "abort", "malloc", "free", "calloc", "realloc", "memcpy", "memset",
                "strlen", "strcmp", "strncmp", "strcpy", "strncpy", "strcat",
                "memcmp", "memmove", "printf", "fprintf", "snprintf", "sprintf",
                "pthread_create", "pthread_join", "pthread_mutex_lock",
                "pthread_mutex_unlock", "pthread_cond_wait", "pthread_cond_signal",
                "close", "read", "write", "open", "fopen", "fclose", "fread",
                "fwrite", "lseek", "mmap", "munmap", "usleep", "sleep", "exit",
                "getenv", "atexit", "sscanf", "strtod", "strtol", "strtoul",
                "qsort", "bsearch", "time", "gettimeofday", "rand", "srand",
                "abs", "labs", "ceil", "floor", "sqrt", "pow", "log", "log2",
                "exp", "sin", "cos", "tan", "asin", "acos", "atan", "atan2",
                "fmod", "fabs", "ldexp", "frexp", "modf", "round", "trunc",
                "isnan", "isinf", "isfinite", "memcpy", "memchr", "strchr",
                "strrchr", "strstr", "strdup", "strndup", "strcasecmp", "strncasecmp",
                "putc", "puts", "fputs", "fgets", "fflush", "fseek", "ftell",
                "remove", "rename", "tmpfile", "setvbuf", "ferror", "feof",
                "clearerr", "ungetc", "vsnprintf", "vprintf", "vasprintf",
                "gmtime", "localtime", "mktime", "strftime", "asctime", "ctime",
                "tzset", "sysconf", "pathconf", "fpathconf", "uname", "getpid",
                "gettid", "kill", "signal", "sigaction", "sigemptyset", "sigaddset",
                "sigprocmask", "raise", "alarm", "getrlimit", "setrlimit",
                "getrusage", "fcntl", "ioctl", "stat", "fstat", "lstat", "access",
                "unlink", "mkdir", "rmdir", "opendir", "readdir", "closedir",
                "getcwd", "chdir", "realpath", "isatty", "ttyname", "fileno",
                "dup", "dup2", "select", "poll", "nanosleep", "ctime_r",
                "gmtime_r", "localtime_r", "rand_r", "random", "srandom",
                "drand48", "erand48", "lrand48", "srand48", "posix_memalign",
                "memalign", "aligned_alloc", "valloc", "strerror", "perror",
                "__errno", "__errno_location", "errno")
        interesting = sorted({s for s in undefined
                              if not _boring(s) and not s.startswith(("std::", "_Z", "__cxa", "__gxx", "operator", "_Unwind"))})
        out["interesting_undefined"] = interesting
        out["n_cpp_symbols"] = sum(1 for s in undefined if s.startswith(("_Z", "std::", "__cxa", "__gxx")))
    return out


def main():
    apk = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..", "apk")
    libdir = os.path.join(apk, "lib", "arm64-v8a")
    sos = sorted(f for f in os.listdir(libdir) if f.endswith(".so"))
    for so in sos:
        r = survey(os.path.join(libdir, so))
        size = os.path.getsize(r["path"])
        print(f"\n{'='*78}\n{so}  ({size/1024:.0f} KiB)  type={r['type']} "
              f"class=ELF{r['class']} machine={r['machine']} flags=0x{r['flags']:x}")
        print(f"  soname : {r['soname']}")
        print(f"  needed : {', '.join(r['needed']) if r['needed'] else '(none)'}")
        print(f"  dynsym : {r['n_defined']} defined / {r['n_undefined']} undefined"
              f"  (C++-mangled undefined: {r['n_cpp_symbols']})")
        if r["java_exports"]:
            print(f"  java exports ({len(r['java_exports'])}):")
            for s in r["java_exports"][:20]:
                print(f"    {s}")
            if len(r["java_exports"]) > 20:
                print(f"    ... +{len(r['java_exports'])-20} more")
        for fam, hits in r["families"].items():
            print(f"  [{fam}] {len(hits)}: {', '.join(hits[:12])}"
                  + (f" ... +{len(hits)-12}" if len(hits) > 12 else ""))
        if r["interesting_undefined"]:
            print(f"  [other interesting] {', '.join(r['interesting_undefined'][:25])}"
                  + (f" ... +{len(r['interesting_undefined'])-25}"
                     if len(r["interesting_undefined"]) > 25 else ""))


if __name__ == "__main__":
    main()
