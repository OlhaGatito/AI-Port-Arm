// imports.gen.c — GERADO por new-port.sh para 'ShadowFight2' (libil2cpp.so)
// 216 simbolos. Resolva os UNKNOWN no fim do arquivo.
#include <stdlib.h>
#include "imports.h"
#include "so_util.h"
#include <stdio.h>
#include <math.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include "android_shim.h"
#include "egl_shim.h"
#include "bionic_shims.h"

void __google_potentially_blocking_region_begin(void);
void __google_potentially_blocking_region_end(void);

// === passthrough/pthread/shim: ligados automaticamente ===
DynLibFunction dynlib_functions[] = {
  {"__android_log_print", (uintptr_t)&__android_log_print},  // liblog
{"__android_log_vprint", (uintptr_t)&__android_log_vprint},
  {"__ctype_get_mb_cur_max", (uintptr_t)&__ctype_get_mb_cur_max},
  {"__cxa_atexit", (uintptr_t)&__cxa_atexit},  // cxx
  {"__cxa_finalize", (uintptr_t)&__cxa_finalize},  // cxx
  {"__errno", (uintptr_t)&__errno},  // pass
 {"__google_potentially_blocking_region_begin", (uintptr_t)&__google_potentially_blocking_region_begin},
{"__google_potentially_blocking_region_end", (uintptr_t)&__google_potentially_blocking_region_end},
  {"__sF", (uintptr_t)&__sF},
  {"__system_property_get", (uintptr_t)&__system_property_get},
{"_ctype_", (uintptr_t)&sf2_ctype_p},
  {"abort", (uintptr_t)&abort},  // pass
  {"accept", (uintptr_t)&accept},
  {"access", (uintptr_t)&access},  // pass
  {"acos", (uintptr_t)&acos},  // pass
  {"acosf", (uintptr_t)&acosf},
  {"asin", (uintptr_t)&asin},  // pass
  {"atan", (uintptr_t)&atan},  // pass
  {"atan2", (uintptr_t)&atan2},  // pass
  {"atan2f", (uintptr_t)&atan2f},  // pass
 {"atanf", (uintptr_t)&atanf},
  {"atoi", (uintptr_t)&atoi},  // pass
  {"atol", (uintptr_t)&atol},  // pass
  {"bind", (uintptr_t)&bind},
  {"bsearch", (uintptr_t)&bsearch},  // pass
  {"btowc", (uintptr_t)&btowc},
  {"calloc", (uintptr_t)&calloc},  // pass
{"chmod", (uintptr_t)&chmod},
{"clock", (uintptr_t)&clock},
 {"clock_getres", (uintptr_t)&clock_getres},
  {"clock_gettime", (uintptr_t)&clock_gettime},  // pass
  {"close", (uintptr_t)&close},  // pass
  {"closedir", (uintptr_t)&closedir},  // pass
  {"connect", (uintptr_t)&connect},
  {"cos", (uintptr_t)&cos},  // pass
  {"cosf", (uintptr_t)&cosf},  // pass
{"difftime", (uintptr_t)&difftime},
{"dl_iterate_phdr", (uintptr_t)&dl_iterate_phdr},
{"dladdr", (uintptr_t)&sf2_dladdr},
{"dlclose", (uintptr_t)&sf2_dlclose},
{"dlopen", (uintptr_t)&sf2_dlopen},
{"dlsym", (uintptr_t)&sf2_dlsym},
  {"dup", (uintptr_t)&dup},  // pass
  {"exit", (uintptr_t)&exit},  // pass
  {"exp", (uintptr_t)&exp},  // pass
  {"exp2", (uintptr_t)&exp2},  // pass
{"exp2f", (uintptr_t)&exp2f},
  {"expf", (uintptr_t)&expf},  // pass
  {"fclose", (uintptr_t)&fclose},  // pass
  {"fcntl", (uintptr_t)&fcntl},  // pass
  {"fileno", (uintptr_t)&fileno},  // pass
  {"fmod", (uintptr_t)&fmod},  // pass
  {"fmodf", (uintptr_t)&fmodf},  // pass
  {"fopen", (uintptr_t)&fopen},  // pass
  {"fputc", (uintptr_t)&fputc},  // pass
  {"fputs", (uintptr_t)&fputs},  // pass
  {"free", (uintptr_t)&free},  // pass
  {"freeaddrinfo", (uintptr_t)&freeaddrinfo},
  {"fstat", (uintptr_t)&fstat},  // pass
{"ftruncate", (uintptr_t)&ftruncate},
  {"fwrite", (uintptr_t)&fwrite},  // pass
  {"getaddrinfo", (uintptr_t)&getaddrinfo},
  {"getcwd", (uintptr_t)&getcwd},  // pass
  {"getenv", (uintptr_t)&getenv},  // pass
 {"gethostname", (uintptr_t)&gethostname},
{"getnameinfo", (uintptr_t)&getnameinfo},
  {"getpid", (uintptr_t)&getpid},  // pass
  {"getsockname", (uintptr_t)&getsockname},
  {"getsockopt", (uintptr_t)&getsockopt},
  {"gettimeofday", (uintptr_t)&gettimeofday},  // pass
  {"gmtime", (uintptr_t)&gmtime},  // pass
  {"inet_ntop", (uintptr_t)&inet_ntop},
  {"inet_pton", (uintptr_t)&inet_pton},
  {"ioctl", (uintptr_t)&ioctl},  // pass
  {"isalpha", (uintptr_t)&isalpha},  // pass
  {"isspace", (uintptr_t)&isspace},  // pass
 {"iswctype", (uintptr_t)&iswctype},
  {"listen", (uintptr_t)&listen},
  {"localtime", (uintptr_t)&localtime},  // pass
  {"log", (uintptr_t)&log},  // pass
  {"log10", (uintptr_t)&log10},  // pass
  {"lseek", (uintptr_t)&lseek},  // pass
  {"lstat", (uintptr_t)&lstat},  // pass
  {"malloc", (uintptr_t)&malloc},  // pass
{"mbrtowc", (uintptr_t)&mbrtowc},
  {"memalign", (uintptr_t)&memalign},  // pass
  {"memchr", (uintptr_t)&memchr},  // pass
  {"memcmp", (uintptr_t)&memcmp},  // pass
  {"memcpy", (uintptr_t)&memcpy},  // pass
  {"memmove", (uintptr_t)&memmove},  // pass
  {"memset", (uintptr_t)&memset},  // pass
  {"mkdir", (uintptr_t)&mkdir},  // pass
  {"mktime", (uintptr_t)&mktime},  // pass
  {"mmap", (uintptr_t)&mmap},  // pass
  {"modf", (uintptr_t)&modf},  // pass
  {"mprotect", (uintptr_t)&mprotect},  // pass
  {"munmap", (uintptr_t)&munmap},  // pass
  {"nanosleep", (uintptr_t)&nanosleep},  // pass
  {"open", (uintptr_t)&open},  // pass
  {"opendir", (uintptr_t)&opendir},  // pass
  {"pipe", (uintptr_t)&pipe},  // pass
{"poll", (uintptr_t)&poll},
  {"pow", (uintptr_t)&pow},  // pass
  {"powf", (uintptr_t)&powf},  // pass
  {"pthread_atfork", (uintptr_t)&pthread_atfork_fake},  // pthread wrapper (core)
  {"pthread_attr_destroy", (uintptr_t)&pthread_attr_destroy_fake},  // pthread wrapper (core)
  {"pthread_attr_getstack", (uintptr_t)&pthread_attr_getstack_fake},  // pthread wrapper (core)
  {"pthread_attr_init", (uintptr_t)&pthread_attr_init_fake},  // pthread wrapper (core)
  {"pthread_cond_broadcast", (uintptr_t)&pthread_cond_broadcast_fake},  // pthread wrapper (core)
  {"pthread_cond_destroy", (uintptr_t)&pthread_cond_destroy_fake},  // pthread wrapper (core)
  {"pthread_cond_init", (uintptr_t)&pthread_cond_init_fake},  // pthread wrapper (core)
  {"pthread_cond_signal", (uintptr_t)&pthread_cond_signal_fake},  // pthread wrapper (core)
  {"pthread_cond_timedwait", (uintptr_t)&pthread_cond_timedwait_fake},  // pthread wrapper (core)
  {"pthread_cond_wait", (uintptr_t)&pthread_cond_wait_fake},  // pthread wrapper (core)
  {"pthread_condattr_destroy", (uintptr_t)&pthread_condattr_destroy_fake},  // pthread wrapper (core)
  {"pthread_condattr_init", (uintptr_t)&pthread_condattr_init_fake},  // pthread wrapper (core)
  {"pthread_condattr_setclock", (uintptr_t)&pthread_condattr_setclock_fake},  // pthread wrapper (core)
  {"pthread_create", (uintptr_t)&pthread_create_fake},  // pthread wrapper (core)
  {"pthread_detach", (uintptr_t)&pthread_detach_fake},  // pthread wrapper (core)
  {"pthread_getattr_np", (uintptr_t)&pthread_getattr_np_fake},  // pthread wrapper (core)
  {"pthread_getspecific", (uintptr_t)&pthread_getspecific_fake},  // pthread wrapper (core)
  {"pthread_key_create", (uintptr_t)&pthread_key_create_fake},  // pthread wrapper (core)
  {"pthread_key_delete", (uintptr_t)&pthread_key_delete_fake},  // pthread wrapper (core)
  {"pthread_kill", (uintptr_t)&pthread_kill_fake},  // pthread wrapper (core)
  {"pthread_mutex_destroy", (uintptr_t)&pthread_mutex_destroy_fake},  // pthread wrapper (core)
  {"pthread_mutex_init", (uintptr_t)&pthread_mutex_init_fake},  // pthread wrapper (core)
  {"pthread_mutex_lock", (uintptr_t)&pthread_mutex_lock_fake},  // pthread wrapper (core)
  {"pthread_mutex_unlock", (uintptr_t)&pthread_mutex_unlock_fake},  // pthread wrapper (core)
  {"pthread_mutexattr_destroy", (uintptr_t)&pthread_mutexattr_destroy_fake},  // pthread wrapper (core)
  {"pthread_mutexattr_init", (uintptr_t)&pthread_mutexattr_init_fake},  // pthread wrapper (core)
  {"pthread_mutexattr_settype", (uintptr_t)&pthread_mutexattr_settype_fake},  // pthread wrapper (core)
  {"pthread_once", (uintptr_t)&pthread_once_fake},  // pthread wrapper (core)
  {"pthread_rwlock_destroy", (uintptr_t)&pthread_rwlock_destroy_fake},  // pthread wrapper (core)
  {"pthread_rwlock_init", (uintptr_t)&pthread_rwlock_init_fake},  // pthread wrapper (core)
  {"pthread_rwlock_rdlock", (uintptr_t)&pthread_rwlock_rdlock_fake},  // pthread wrapper (core)
  {"pthread_rwlock_unlock", (uintptr_t)&pthread_rwlock_unlock_fake},  // pthread wrapper (core)
  {"pthread_rwlock_wrlock", (uintptr_t)&pthread_rwlock_wrlock_fake},  // pthread wrapper (core)
  {"pthread_self", (uintptr_t)&pthread_self_fake},  // pthread wrapper (core)
  {"pthread_setspecific", (uintptr_t)&pthread_setspecific_fake},  // pthread wrapper (core)
  {"pthread_sigmask", (uintptr_t)&pthread_sigmask_fake},  // pthread wrapper (core)
  {"read", (uintptr_t)&read},  // pass
  {"readdir", (uintptr_t)&readdir},  // pass
{"readlink", (uintptr_t)&readlink},
  {"realloc", (uintptr_t)&realloc},  // pass
  {"recvfrom", (uintptr_t)&recvfrom},
  {"rename", (uintptr_t)&rename},  // pass
  {"rmdir", (uintptr_t)&rmdir},  // pass
  {"sched_yield", (uintptr_t)&sched_yield},  // pass
  {"sem_getvalue", (uintptr_t)&sem_getvalue_fake},  // pthread wrapper (core)
  {"sem_init", (uintptr_t)&sem_init_fake},  // pthread wrapper (core)
  {"sem_post", (uintptr_t)&sem_post_fake},  // pthread wrapper (core)
  {"sem_wait", (uintptr_t)&sem_wait_fake},  // pthread wrapper (core)
  {"send", (uintptr_t)&send},
  {"sendto", (uintptr_t)&sendto},
  {"setenv", (uintptr_t)&setenv},  // pass
  {"setjmp", (uintptr_t)&setjmp},
  {"setlocale", (uintptr_t)&setlocale},  // pass
  {"setsockopt", (uintptr_t)&setsockopt},
  {"shutdown", (uintptr_t)&shutdown},
  {"sigaction", (uintptr_t)&my_sigaction},
  {"sigaddset", (uintptr_t)&sigaddset},
  {"sigdelset", (uintptr_t)&sigdelset},
  {"sigemptyset", (uintptr_t)&sigemptyset},
  {"sigfillset", (uintptr_t)&sigfillset},
  {"signal", (uintptr_t)&signal},
  {"sigsuspend", (uintptr_t)&sigsuspend},
  {"sin", (uintptr_t)&sin},  // pass
  {"sinf", (uintptr_t)&sinf},  // pass
  {"socket", (uintptr_t)&socket},
  {"sprintf", (uintptr_t)&sprintf},  // pass
  {"sqrt", (uintptr_t)&sqrt},  // pass
  {"sqrtf", (uintptr_t)&sqrtf},  // pass
  {"stat", (uintptr_t)&stat},  // pass
  {"strchr", (uintptr_t)&strchr},  // pass
  {"strcmp", (uintptr_t)&strcmp},  // pass
  {"strcoll", (uintptr_t)&strcoll},  // pass
  {"strcpy", (uintptr_t)&strcpy},  // pass
  {"strerror", (uintptr_t)&strerror},  // pass
  {"strftime", (uintptr_t)&strftime},  // pass
  {"strlcpy", (uintptr_t)&my_strlcpy},
  {"strlen", (uintptr_t)&strlen},  // pass
  {"strncmp", (uintptr_t)&strncmp},  // pass
  {"strncpy", (uintptr_t)&strncpy},  // pass
  {"strrchr", (uintptr_t)&strrchr},  // pass
  {"strtod", (uintptr_t)&strtod},  // pass
  {"strtof", (uintptr_t)&strtof},  // pass
  {"strtol", (uintptr_t)&strtol},  // pass
{"strtold", (uintptr_t)&strtold},
  {"strtoul", (uintptr_t)&strtoul},  // pass
{"strxfrm", (uintptr_t)&strxfrm},
  {"syscall", (uintptr_t)&syscall},
  {"sysconf", (uintptr_t)&sysconf},  // pass
  {"tan", (uintptr_t)&tan},  // pass
  {"tanf", (uintptr_t)&tanf},  // pass
  {"time", (uintptr_t)&time},  // pass
  {"tolower", (uintptr_t)&tolower},  // pass
{"towlower", (uintptr_t)&towlower},
{"towupper", (uintptr_t)&towupper},
{"uname", (uintptr_t)&uname},
  {"unlink", (uintptr_t)&unlink},  // pass
{"unsetenv", (uintptr_t)&unsetenv},
  {"usleep", (uintptr_t)&usleep},  // pass
  {"vsnprintf", (uintptr_t)&vsnprintf},  // pass
  {"vsprintf", (uintptr_t)&vsprintf},  // pass
{"wcrtomb", (uintptr_t)&wcrtomb},
{"wcscoll", (uintptr_t)&wcscoll},
{"wcsftime", (uintptr_t)&wcsftime},
{"wcslen", (uintptr_t)&wcslen},
{"wcsxfrm", (uintptr_t)&wcsxfrm},
{"wctob", (uintptr_t)&wctob},
{"wctype", (uintptr_t)&wctype},
{"wmemchr", (uintptr_t)&wmemchr},
{"wmemcpy", (uintptr_t)&wmemcpy},
{"wmemmove", (uintptr_t)&wmemmove},
{"wmemset", (uintptr_t)&wmemset},
  {"write", (uintptr_t)&write},  // pass
 {"writev", (uintptr_t)&writev},
  {"eglChooseConfig", (uintptr_t)&egl_shim_ChooseConfig},
  {"eglCreateContext", (uintptr_t)&egl_shim_CreateContext},
  {"eglCreatePbufferSurface", (uintptr_t)&egl_shim_CreatePbufferSurface},
  {"eglCreateWindowSurface", (uintptr_t)&egl_shim_CreateWindowSurface},
  {"eglDestroyContext", (uintptr_t)&egl_shim_DestroyContext},
  {"eglDestroySurface", (uintptr_t)&egl_shim_DestroySurface},
  {"eglGetConfigAttrib", (uintptr_t)&egl_shim_GetConfigAttrib},
  {"eglGetCurrentContext", (uintptr_t)&egl_shim_GetCurrentContext},
  {"eglGetCurrentSurface", (uintptr_t)&egl_shim_GetCurrentSurface},
  {"eglGetDisplay", (uintptr_t)&egl_shim_GetDisplay},
  {"eglGetError", (uintptr_t)&egl_shim_GetError},
  {"eglGetProcAddress", (uintptr_t)&egl_shim_GetProcAddress},
  {"eglInitialize", (uintptr_t)&egl_shim_Initialize},
  {"eglMakeCurrent", (uintptr_t)&egl_shim_MakeCurrent},
  {"eglQueryString", (uintptr_t)&egl_shim_QueryString},
  {"eglQuerySurface", (uintptr_t)&egl_shim_QuerySurface},
  {"eglSwapBuffers", (uintptr_t)&egl_shim_SwapBuffers},
  {"eglSwapInterval", (uintptr_t)&egl_shim_SwapInterval},
  {"eglTerminate", (uintptr_t)&egl_shim_Terminate},
  {"ALooper_prepare", (uintptr_t)&ALooper_prepare},
  {"ALooper_addFd", (uintptr_t)&ALooper_addFd},
  {"ALooper_pollAll", (uintptr_t)&ALooper_pollAll},

  {"ASensorManager_getInstance", (uintptr_t)&ASensorManager_getInstance},
  {"ASensorManager_getDefaultSensor", (uintptr_t)&ASensorManager_getDefaultSensor},
  {"ASensorManager_createEventQueue", (uintptr_t)&ASensorManager_createEventQueue},
  {"ASensorEventQueue_enableSensor", (uintptr_t)&ASensorEventQueue_enableSensor},
  {"ASensorEventQueue_setEventRate", (uintptr_t)&ASensorEventQueue_setEventRate},

  {"ALooper_forThread", (uintptr_t)&ALooper_forThread},
  {"ASensorManager_getSensorList", (uintptr_t)&ASensorManager_getSensorList},
  {"ASensorManager_destroyEventQueue", (uintptr_t)&ASensorManager_destroyEventQueue},
  {"ASensorEventQueue_disableSensor", (uintptr_t)&ASensorEventQueue_disableSensor},
  {"ASensorEventQueue_getEvents", (uintptr_t)&ASensorEventQueue_getEvents},
  {"ASensorEventQueue_hasEvents", (uintptr_t)&ASensorEventQueue_hasEvents},
  {"ASensor_getMinDelay", (uintptr_t)&ASensor_getMinDelay},
  {"ASensor_getType", (uintptr_t)&ASensor_getType},
  {"ASensor_getName", (uintptr_t)&ASensor_getName},
  {"ASensor_getVendor", (uintptr_t)&ASensor_getVendor},
  {"ASensor_getResolution", (uintptr_t)&ASensor_getResolution},

  {"ANativeWindow_fromSurface", (uintptr_t)&ANativeWindow_fromSurface},
  {"ANativeWindow_acquire", (uintptr_t)&ANativeWindow_acquire},
  {"ANativeWindow_release", (uintptr_t)&ANativeWindow_release},
  {"ANativeWindow_getWidth", (uintptr_t)&ANativeWindow_getWidth},
  {"ANativeWindow_getHeight", (uintptr_t)&ANativeWindow_getHeight},
  {"ANativeWindow_setBuffersGeometry", (uintptr_t)&ANativeWindow_setBuffersGeometry},
};
const int dynlib_functions_count = sizeof(dynlib_functions)/sizeof(dynlib_functions[0]);

// ===================== SIMBOLOS A IMPLEMENTAR =====================
//   __ctype_get_mb_cur_max
//   __google_potentially_blocking_region_begin
//   __google_potentially_blocking_region_end
//   __sF
//   __system_property_get
//   _ctype_
//   accept
//   acosf
//   atanf
//   bind
//   btowc
//   chmod
//   clock
//   clock_getres
//   connect
//   difftime
//   dl_iterate_phdr
//   dladdr
//   dlclose
//   dlopen
//   dlsym
//   exp2f
//   freeaddrinfo
//   ftruncate
//   getaddrinfo
//   gethostname
//   getnameinfo
//   getsockname
//   getsockopt
//   inet_ntop
//   inet_pton
//   iswctype
//   listen
//   mbrtowc
//   poll
//   readlink
//   recvfrom
//   send
//   sendto
//   setjmp
//   setsockopt
//   shutdown
//   sigaction
//   sigaddset
//   sigdelset
//   sigemptyset
//   sigfillset
//   signal
//   sigsuspend
//   socket
//   strlcpy
//   strtold
//   strxfrm
//   syscall
//   towlower
//   towupper
//   uname
//   unsetenv
//   wcrtomb
//   wcscoll
//   wcsftime
//   wcslen
//   wcsxfrm
//   wctob
//   wctype
//   wmemchr
//   wmemcpy
//   wmemmove
//   wmemset
//   writev
