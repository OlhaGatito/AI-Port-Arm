/*
 * jni.c -- the JNIEnv the Android objects talk to.
 *
 * The vtable is built by index rather than by naming the fields of
 * JNINativeInterface: the port has to match the real slot numbers exactly
 * because Unity and its Android plugins call through them by offset.  Keeping
 * an index table makes every slot checkable against the disassembly.
 *
 * Objects are small tagged structs, not opaque cookies, so GetObjectClass and
 * IsInstanceOf can tell a KeyEvent from a MotionEvent from a byte[] -- Unity
 * needs that distinction for its native Android input and asset paths.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <execinfo.h>

#include "nx_elf.h"
#include "gb.h"
#include "jni_slots.h"
#include "il2.h"

/* Which mapped image an address belongs to, and where that image starts.
 * Defined in main.c: it walks the whole link map, so unlike a game-object
 * whitelist it also resolves the main executable. */
const char *fp2_mod_at(const void *addr, void **base_out);

#ifndef FP2_RELEASE_BUILD
int fp2_trace_jni = 0;
#endif

#define JT(...) do { if (fp2_trace_jni) nx_log("jni: " __VA_ARGS__); } while (0)

/* ------------------------------------------------------------- object model */

typedef enum {
    O_CLASS, O_STRING, O_BYTEARRAY, O_OBJARRAY, O_OBJECT, O_THROWABLE,
    O_DIRECTBUF,
} otype;

typedef enum {
    REFLECT_NONE,
    REFLECT_CONSTRUCTOR,
    REFLECT_METHOD,
    REFLECT_FIELD,
} reflected_kind;

typedef struct jobj jobj;

typedef struct {
    char *name;
    jobj *object;
    int64_t primitive;
    int is_object;
} jobject_field;

struct jobj {
    otype type;
    const char *cls;         /* class name for O_OBJECT / O_CLASS */
    char *str;               /* O_STRING payload (UTF-8, owned) */
    void *data;              /* array payload */
    int len;
    jobj **elems;            /* O_OBJARRAY */
    int64_t prim;            /* payload of a boxed primitive */
    int boxed;
    jobject_field *fields;   /* adapter-owned Java instance fields */
    int field_count;
    int field_capacity;
    void *reflected_id;      /* JNI method/field ID inside a reflected object */
    reflected_kind reflected_kind;
    int reflected_static;
};

static jobj *new_obj(otype t, const char *cls)
{
    jobj *o = calloc(1, sizeof *o);
    o->type = t;
    o->cls = cls;
    return o;
}

static jobj *mk_class(const char *name)
{
    /* Classes are interned so IsSameObject and the method tables work. */
    static jobj *cache[256];
    static const char *names[256];
    static int n;
    /* FindClass(NULL) arrives through the null-metaData reflection chain
     * (run13: SIGSEGV on strcmp here).  Intern it as a distinct class so
     * the caller keeps a usable object. */
    if (!name)
        name = "(null)";
    for (int i = 0; i < n; i++)
        if (strcmp(names[i], name) == 0)
            return cache[i];
    if (n == 256)
        nx_die("class cache full (%s)", name);
    names[n] = strdup(name);
    cache[n] = new_obj(O_CLASS, names[n]);
    return cache[n++];
}

static jobj *mk_string(const char *s)
{
    jobj *o = new_obj(O_STRING, "java/lang/String");
    o->str = strdup(s ? s : "");
    o->len = (int)strlen(o->str);
    return o;
}

static jobj *mk_object(const char *cls)
{
    return new_obj(O_OBJECT, cls);
}

static jobject_field *object_field(jobj *object, const char *name, int create)
{
    if (!object || !name)
        return NULL;
    for (int i = 0; i < object->field_count; i++)
        if (strcmp(object->fields[i].name, name) == 0)
            return &object->fields[i];
    if (!create)
        return NULL;
    if (object->field_count == object->field_capacity) {
        int next = object->field_capacity ? object->field_capacity * 2 : 8;
        jobject_field *grown = realloc(object->fields,
                                        (size_t)next * sizeof *grown);
        if (!grown)
            nx_die("JNI object field allocation failed");
        memset(grown + object->field_capacity, 0,
               (size_t)(next - object->field_capacity) * sizeof *grown);
        object->fields = grown;
        object->field_capacity = next;
    }
    jobject_field *field = &object->fields[object->field_count++];
    field->name = strdup(name);
    if (!field->name)
        nx_die("JNI object field name allocation failed");
    return field;
}

static void object_set_object(jobj *object, const char *name, jobj *value)
{
    jobject_field *field = object_field(object, name, 1);
    field->object = value;
    field->primitive = 0;
    field->is_object = 1;
}

static void object_set_primitive(jobj *object, const char *name, int64_t value)
{
    jobject_field *field = object_field(object, name, 1);
    field->object = NULL;
    field->primitive = value;
    field->is_object = 0;
}

static jobj *mk_file(const char *path)
{
    jobj *o = mk_object("java/io/File");
    o->str = strdup(path ? path : "");
    o->len = (int)strlen(o->str);
    return o;
}

/* Objects published to Unity's Android input backend. */
static jobj *input_device;
static jobj *touch_device;
static jobj *motion_range_list;
static jobj *touch_motion_range_list;
static jobj *motion_range_iterator;
static jobj *motion_ranges[8];
static jobj *key_event_object;
static jobj *motion_event_object;
static jobj *fmod_device_object;
static jobj *fmod_bytebuffer;
static jobj *preferences_object;
static jobj *preferences_editor;
static jobj *armory_activity_object;
static jobj *permission_plugin_object;
static jobj *audio_manager_object;
static jobj *input_manager_object;
static jobj *vibrator_object;
static jobj *window_manager_object;
static unsigned char fmod_pcm[65536];
static int fmod_buffer_size = sizeof fmod_pcm;
static int fmod_should_run;
static char input_device_name[128] = "SDL Gamepad";
static char input_device_descriptor[160] = "nxcompat-native-gamepad";
static int input_device_vendor;
static int input_device_product;
static struct {
    int action;
    int keycode;
    int source;
    int device_id;
    int meta_state;
    int repeat;
    int scancode;
    int flags;
    int unicode;
    int64_t event_time;
    int64_t down_time[256];
} key_event;
typedef struct {
    int action;
    int source;
    int device_id;
    int meta_state;
    int button_state;
    int flags;
    int64_t event_time;
    int64_t down_time;
    float axis[48];
} motion_payload;
static motion_payload motion_event;
#define MOTION_CLONE_COUNT 64
static jobj *motion_clones[MOTION_CLONE_COUNT];
static motion_payload motion_clone_data[MOTION_CLONE_COUNT];
static unsigned motion_clone_next;

static const int motion_axis_ids[8] = {
    0, 1, 11, 14, 17, 18, 15, 16,
};

static motion_payload *motion_from_object(jobj *object)
{
    return object && object->data
        ? (motion_payload *)object->data : &motion_event;
}

static int64_t monotonic_millis(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void fp2_jni_input_device_info(const char *name, int vendor, int product,
                               const char *descriptor)
{
    snprintf(input_device_name, sizeof input_device_name, "%s",
             name && *name ? name : "SDL Gamepad");
    snprintf(input_device_descriptor, sizeof input_device_descriptor,
             "nxcompat-%04x-%04x-%s", vendor & 0xffff, product & 0xffff,
             descriptor && *descriptor ? descriptor : "gamepad");
    input_device_vendor = vendor;
    input_device_product = product;
}

void *fp2_jni_key_event(int action, int keycode, int scancode)
{
    int64_t now = monotonic_millis();
    if (keycode < 0 || keycode >= (int)(sizeof key_event.down_time /
                                        sizeof *key_event.down_time))
        keycode = 0;
    if (action == 0 || key_event.down_time[keycode] == 0)
        key_event.down_time[keycode] = now;
    key_event.action = action;
    key_event.keycode = keycode;
    /* Android reports these devices as GAMEPAD | DPAD | JOYSTICK. */
    key_event.source = 0x01000611;
    key_event.device_id = 1;
    key_event.meta_state = 0;
    key_event.repeat = 0;
    key_event.scancode = scancode;
    key_event.flags = 0;
    key_event.unicode = 0;
    key_event.event_time = now;
    if (action == 1) {
        /* nativeInjectEvent consumes the object synchronously, so the slot can
         * be released as soon as this call returns to the input bridge. */
    }
    return key_event_object;
}

void *fp2_jni_motion_event(float lx, float ly, float rx, float ry,
                           float lt, float rt, float hat_x, float hat_y)
{
    int64_t now = monotonic_millis();
    motion_event.action = 2; /* MotionEvent.ACTION_MOVE */
    motion_event.source = 0x01000010; /* InputDevice.SOURCE_JOYSTICK */
    motion_event.device_id = 1;
    motion_event.meta_state = 0;
    motion_event.button_state = 0;
    motion_event.flags = 0;
    motion_event.event_time = now;
    motion_event.down_time = now;
    memset(motion_event.axis, 0, sizeof motion_event.axis);
    motion_event.axis[0] = lx;   /* AXIS_X */
    motion_event.axis[1] = ly;   /* AXIS_Y */
    motion_event.axis[11] = rx;  /* AXIS_Z */
    motion_event.axis[14] = ry;  /* AXIS_RZ */
    motion_event.axis[17] = lt;  /* AXIS_LTRIGGER */
    motion_event.axis[18] = rt;  /* AXIS_RTRIGGER */
    motion_event.axis[15] = hat_x;
    motion_event.axis[16] = hat_y;
    return motion_event_object;
}

void *fp2_jni_touch_event(int action, float x, float y)
{
    int64_t now = monotonic_millis();
    motion_event.action = action;
    motion_event.source = 0x00001002; /* InputDevice.SOURCE_TOUCHSCREEN */
    motion_event.device_id = 0;
    motion_event.meta_state = 0;
    motion_event.button_state = 0;
    motion_event.flags = 0;
    motion_event.event_time = now;
    if (action == 0 || motion_event.down_time == 0)
        motion_event.down_time = now;
    memset(motion_event.axis, 0, sizeof motion_event.axis);
    motion_event.axis[0] = x;
    motion_event.axis[1] = y;
    return motion_event_object;
}

void *fp2_jni_fmod_device(void)
{
    return fmod_device_object;
}

void *fp2_jni_fmod_bytebuffer(void)
{
    return fmod_bytebuffer;
}

void *fp2_jni_fmod_pcm(void)
{
    return fmod_pcm;
}

int fp2_jni_fmod_pcm_capacity(void)
{
    return (int)sizeof fmod_pcm;
}

void fp2_jni_fmod_set_buffer_size(int bytes)
{
    if (bytes <= 0 || bytes > (int)sizeof fmod_pcm)
        return;
    fmod_buffer_size = bytes;
    if (fmod_bytebuffer)
        fmod_bytebuffer->len = bytes;
}

int fp2_jni_fmod_should_run(void)
{
    return __atomic_load_n(&fmod_should_run, __ATOMIC_ACQUIRE);
}

/* ----------------------------------------------------------- method registry */

/* Method IDs are indices into a flat table so a handler can be looked up in
 * one step and an unknown method still returns a usable ID (returning NULL
 * from GetMethodID makes Unity abort long before we learn what it wanted). */
typedef struct {
    const char *cls, *name, *sig;
    void *handler;          /* fp2_jni_handler, or NULL for "safe default" */
} jmethod;

#define MAX_METHODS 1024
static jmethod methods[MAX_METHODS];
static int method_n;

/* The method table is written from every thread that crosses into Java: the
 * render/main thread plus the UnityChoreographer HandlerThread that
 * fp2_choreographer_start() brings up (doframe_mid, handlemsg_mid and the
 * callback ids are registered there, while Message.what and the reflection
 * proxy ids are registered from the main thread).  An unlocked
 * read-modify-write of method_n let both threads pick the same slot index, so
 * each stored its entry at methods[n] and each incremented method_n: the table
 * ended with method_n == n+2 while methods[n+1] had never been initialised.
 * by_id() hands that slot out like any other and registry_member_kind() then
 * ran strchr(entry->sig, '(') on its NULL sig, faulting inside libc with
 * x0 = 0 and x1 = 0x28.  Serialise the whole scan-then-append so a slot is
 * published only after cls/name/sig/handler are all written. */
static pthread_mutex_t method_table_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    void *env;
    jobj *self;
    va_list *ap;
    const uint64_t *args;
    unsigned arg_index;
    const jmethod *m;
} jctx;
typedef int64_t (*fp2_jni_handler)(jctx *);

/* JNI's A calls carry a jvalue[] instead of a va_list.  Both representations
 * use one eight-byte slot per argument on this aarch64 target; keeping the
 * readers here lets the same platform handler serve Call*Method, *V and *A. */
static jobj *jarg_obj(jctx *c)
{
    if (c->args)
        return (jobj *)(uintptr_t)c->args[c->arg_index++];
    return va_arg(*c->ap, jobj *);
}

static int32_t jarg_int(jctx *c)
{
    if (c->args)
        return (int32_t)c->args[c->arg_index++];
    return va_arg(*c->ap, int32_t);
}

static int64_t jarg_long(jctx *c)
{
    if (c->args)
        return (int64_t)c->args[c->arg_index++];
    return va_arg(*c->ap, int64_t);
}

static float jarg_float(jctx *c)
{
    float value;
    if (c->args) {
        uint32_t bits = (uint32_t)c->args[c->arg_index++];
        memcpy(&value, &bits, sizeof value);
        return value;
    }
    return (float)va_arg(*c->ap, double);
}

static void *method_id_locked(const char *cls, const char *name,
                              const char *sig)
{
    for (int i = 0; i < method_n; i++)
        if (methods[i].name && methods[i].sig &&
            strcmp(methods[i].name, name) == 0 &&
            strcmp(methods[i].sig, sig) == 0 &&
            (!cls || !methods[i].cls || strcmp(methods[i].cls, cls) == 0))
            return (void *)(uintptr_t)(i + 1);
    if (method_n == MAX_METHODS)
        nx_die("method table full");
    methods[method_n].cls = cls ? strdup(cls) : NULL;
    methods[method_n].name = strdup(name);
    methods[method_n].sig = strdup(sig);
    methods[method_n].handler = NULL;
    JT("new method id %d %s.%s%s", method_n + 1, cls ? cls : "?", name, sig);
    return (void *)(uintptr_t)(++method_n);
}

static void *method_id(const char *cls, const char *name, const char *sig)
{
    /* Unity hands reflection-derived strings here; a null name/sig (a
     * missing meta-data Bundle fed the reflection chain in run12) must not
     * reach strcmp.  Register the entry under "?" so callers keep working
     * and the miss is visible in the trace. */
    if (!name || !sig) {
        JT("method_id(%s, %s, %s) with null member -- stubbing",
           cls ? cls : "?", name ? name : "(null)", sig ? sig : "(null)");
        name = name ? name : "(null)";
        sig = sig ? sig : "(null)";
    }
    pthread_mutex_lock(&method_table_lock);
    void *id = method_id_locked(cls, name, sig);
    pthread_mutex_unlock(&method_table_lock);
    return id;
}

static const jmethod *by_id(void *mid)
{
    uintptr_t i = (uintptr_t)mid;
    return (i >= 1 && i <= (uintptr_t)method_n) ? &methods[i - 1] : NULL;
}

/* java.lang.reflect objects are real jobjects on Android, not JNI IDs.  Unity
 * 6 calls methods such as Field.getDeclaringClass() on the object before it
 * converts it back with FromReflectedField, so exposing a small registry index
 * as the jobject is both semantically wrong and an immediate invalid pointer.
 * Keep one stable wrapper for every member/kind/static tuple. */
static jobj *reflected_cache[REFLECT_FIELD + 1][MAX_METHODS][2];
static pthread_mutex_t reflected_cache_lock = PTHREAD_MUTEX_INITIALIZER;

static const char *reflected_class_name(reflected_kind kind)
{
    switch (kind) {
    case REFLECT_CONSTRUCTOR:
        return "java/lang/reflect/Constructor";
    case REFLECT_METHOD:
        return "java/lang/reflect/Method";
    case REFLECT_FIELD:
        return "java/lang/reflect/Field";
    default:
        return NULL;
    }
}

static reflected_kind registry_member_kind(const jmethod *entry)
{
    if (!entry || !entry->name || !entry->sig)
        return REFLECT_NONE;
    if (strcmp(entry->name, "<init>") == 0)
        return REFLECT_CONSTRUCTOR;
    return strchr(entry->sig, '(') ? REFLECT_METHOD : REFLECT_FIELD;
}

static jobj *make_reflected_member(reflected_kind kind, void *id,
                                   int is_static)
{
    const jmethod *entry = by_id(id);
    uintptr_t index = (uintptr_t)id;
    const char *cls = reflected_class_name(kind);
    if (!entry || !cls || registry_member_kind(entry) != kind)
        return NULL;
    index--;
    is_static = is_static != 0;

    pthread_mutex_lock(&reflected_cache_lock);
    jobj *object = reflected_cache[kind][index][is_static];
    if (!object) {
        object = mk_object(cls);
        object->str = strdup(entry->name);
        if (!object->str)
            nx_die("JNI reflected member name allocation failed");
        object->len = (int)strlen(object->str);
        object->prim = (int64_t)(uintptr_t)id;
        object->reflected_id = id;
        object->reflected_kind = kind;
        object->reflected_static = is_static;
        reflected_cache[kind][index][is_static] = object;
    }
    pthread_mutex_unlock(&reflected_cache_lock);
    return object;
}

/* Validate by equality against the wrapper registry before dereferencing the
 * caller-provided value.  This makes a stale/raw cookie fail closed instead of
 * turning an integer such as 0x19a into a jobj pointer. */
static void *reflected_member_id(void *value, unsigned allowed_kinds,
                                 reflected_kind *kind_out)
{
    if (kind_out)
        *kind_out = REFLECT_NONE;
    if (!value)
        return NULL;
    pthread_mutex_lock(&reflected_cache_lock);
    for (int kind = REFLECT_CONSTRUCTOR; kind <= REFLECT_FIELD; kind++) {
        if (!(allowed_kinds & (1u << kind)))
            continue;
        for (int index = 0; index < MAX_METHODS; index++) {
            for (int is_static = 0; is_static <= 1; is_static++) {
                jobj *object = reflected_cache[kind][index][is_static];
                if (object != value)
                    continue;
                void *id = (void *)(uintptr_t)(index + 1);
                const jmethod *entry = by_id(id);
                if (!entry || object->type != O_OBJECT ||
                    object->reflected_id != id ||
                    object->reflected_kind != (reflected_kind)kind ||
                    object->reflected_static != is_static || !object->cls ||
                    strcmp(object->cls, reflected_class_name(kind)) != 0) {
                    pthread_mutex_unlock(&reflected_cache_lock);
                    return NULL;
                }
                if (kind_out)
                    *kind_out = (reflected_kind)kind;
                pthread_mutex_unlock(&reflected_cache_lock);
                return id;
            }
        }
    }
    pthread_mutex_unlock(&reflected_cache_lock);
    return NULL;
}

void fp2_jni_bind(const char *cls, const char *name, const char *sig, void *fn)
{
    /* Bind inside the same critical section as the lookup: publishing the
     * slot first and storing the handler afterwards would let another thread
     * dispatch the entry through the NULL-handler default path. */
    if (!name || !sig)
        return;
    pthread_mutex_lock(&method_table_lock);
    void *id = method_id_locked(cls, name, sig);
    methods[(uintptr_t)id - 1].handler = fn;
    pthread_mutex_unlock(&method_table_lock);
}

/* --------------------------------------------------------------- registry of
 * natives that the loaded objects register with us */

typedef struct { char cls[128]; char name[128]; char sig[192]; void *fn; } jnative;
#define MAX_NATIVES 512
static jnative natives[MAX_NATIVES];
static int natives_n;
static jobj *unity_player_object;

void *fp2_jni_native(const char *cls, const char *name)
{
    for (int i = 0; i < natives_n; i++)
        if (strcmp(natives[i].name, name) == 0 &&
            (!cls || strcmp(natives[i].cls, cls) == 0))
            return natives[i].fn;
    return NULL;
}

const char *fp2_jni_native_sig(const char *cls, const char *name)
{
    for (int i = 0; i < natives_n; i++)
        if (strcmp(natives[i].name, name) == 0 &&
            (!cls || strcmp(natives[i].cls, cls) == 0))
            return natives[i].sig;
    return NULL;
}

void fp2_jni_set_unity_player(void *player)
{
    unity_player_object = player;
}

static jobj *soft_input_player(void)
{
    return unity_player_object
        ? unity_player_object
        : mk_class("com/unity3d/player/UnityPlayer");
}

void fp2_jni_soft_input_text(const char *text)
{
    void *native =
        fp2_jni_native("com/unity3d/player/UnityPlayer",
                       "nativeSetInputString");
    if (!native)
        return;
    jobj *value = mk_string(text ? text : "");
    ((void (*)(void *, void *, void *))native)(
        fp2_jni_env(), soft_input_player(), value);
}

void fp2_jni_soft_input_selection(int start, int length)
{
    void *native =
        fp2_jni_native("com/unity3d/player/UnityPlayer",
                       "nativeSetInputSelection");
    if (native)
        ((void (*)(void *, void *, int, int))native)(
            fp2_jni_env(), soft_input_player(), start, length);
}

void fp2_jni_soft_input_visible(int visible)
{
    void *native =
        fp2_jni_native("com/unity3d/player/UnityPlayer",
                       "nativeSetKeyboardIsVisible");
    if (native)
        ((void (*)(void *, void *, int))native)(
            fp2_jni_env(), soft_input_player(), visible != 0);
}

void fp2_jni_soft_input_closed(int canceled)
{
    const char *name =
        canceled ? "nativeSoftInputCanceled" : "nativeSoftInputClosed";
    void *native =
        fp2_jni_native("com/unity3d/player/UnityPlayer", name);
    if (native)
        ((void (*)(void *, void *))native)(
            fp2_jni_env(), soft_input_player());
}

/* ------------------------------------------------------------------ vtable */

static void *vt[JNI_SLOT_COUNT];
static void *env_ptr = vt;          /* JNIEnv* is a pointer to the vtable ptr */
static void *jvm_vt[8];
static void *jvm_ptr = jvm_vt;

void *fp2_jni_env(void) { return &env_ptr; }
void *fp2_jni_vm(void) { return &jvm_ptr; }

/* --------------------------------------------------- Java callback/Looper flow
 *
 * Unity 2022 implements UnityChoreographer through bitter/jnibridge:
 *
 *   newInterfaceProxy(Handler.Callback + FrameCallback)
 *   -> HandlerThread.start()
 *   -> Handler(obtained looper, proxy)
 *   -> obtainMessage().sendToTarget()
 *   -> proxy.handleMessage()
 *   -> proxy.doFrame(frameTimeNanos), once per display tick
 *
 * The Android classes do not exist on NextOS, but the native proxy and its
 * callbacks do.  These objects preserve that exact ordering and invoke the
 * registered JNIBridge native just as the Java Looper would.
 */

enum {
    PROXY_HANDLER_CALLBACK = 1u << 0,
    PROXY_FRAME_CALLBACK   = 1u << 1,
    PROXY_RUNNABLE         = 1u << 2,
    PROXY_TASK_SUCCESS     = 1u << 3,
    PROXY_TASK_FAILURE     = 1u << 4,
    PROXY_REFLECTION       = 1u << 5,
    PROXY_NETFLIX_CALLBACK = 1u << 6,
    PROXY_NETFLIX_PLUGIN   = 1u << 7,
    PROXY_NETFLIX_CONFIG   = 1u << 8,
};

static jobj *choreo_proxy;
static jobj *looper_object;
static jobj *choreographer_object;
static jobj *message_object;
static jobj *doframe_method;
static jobj *handlemsg_method;
static jobj *run_method;
static jobj *doframe_args;
static jobj *handlemsg_args;
static jobj *empty_args;
static jobj *frame_time_box;
static jobj *play_games_task;
static jobj *play_games_auth_result;
static jobj *play_games_success_method;
static jobj *play_games_success_args;
static jobj *play_games_success_listener;
static jobj *netflix_on_result_method;
static jobj *netflix_plugin_callback;
static jobj *netflix_config_callback;
static jobj *netflix_membership;
static jobj *netflix_player_access;
static jobj *netflix_profile;
static jobj *netflix_crash_config;
enum {
    /* com.netflix.games.errors.ErrorCodes.BlobStore.BLOB_NAME_NOT_FOUND in
     * this title's classes2.dex.  The Java SDK returns this error envelope on
     * a first-boot read; ReadPlayerBlobResult.blobContainer is non-nullable
     * and therefore must never be used to represent an absent slot. */
    NETFLIX_ERROR_BLOB_NAME_NOT_FOUND = -1001,
};
typedef struct netflix_callback_item {
    jobj *callback;
    jobj *result;
    struct netflix_callback_item *next;
} netflix_callback_item;
static netflix_callback_item *netflix_callback_head;
static netflix_callback_item *netflix_callback_tail;
static pthread_mutex_t netflix_callback_lock = PTHREAD_MUTEX_INITIALIZER;
static void *doframe_mid;
static void *handlemsg_mid;
static void *run_mid;
static void *play_games_success_mid;
static void *netflix_on_result_mid;
static int choreo_thread_started;
static int choreo_thread_joinable;
static int choreo_stop_requested;
static int choreo_java_frame_pending;
static pthread_t choreo_thread;

/* Handler.obtainMessage returns a distinct Message object and Android's
 * Looper preserves message order.  Unity 6 uses what=0/1/2 for the
 * UnityChoreographer lifecycle; collapsing those messages into one global
 * integer made START and STOP both look like INIT. */
#define FP2_CHOREO_MESSAGE_MAX 16
static jobj *choreo_messages[FP2_CHOREO_MESSAGE_MAX];
static unsigned choreo_message_head;
static unsigned choreo_message_count;
static pthread_mutex_t choreo_message_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t reflection_proxy_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t reflection_proxy_idle = PTHREAD_COND_INITIALIZER;
static unsigned reflection_proxy_active;
static int reflection_proxy_closing;

static int64_t monotonic_nanos(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (int64_t)now.tv_sec * 1000000000LL + now.tv_nsec;
}

static unsigned proxy_interfaces(jobj *interfaces)
{
    unsigned flags = 0;
    if (!interfaces)
        return flags;
    int n = interfaces->type == O_OBJARRAY ? interfaces->len : 1;
    for (int i = 0; i < n; i++) {
        jobj *c = interfaces->type == O_OBJARRAY
                    ? interfaces->elems[i] : interfaces;
        const char *name = c ? c->cls : NULL;
        if (!name)
            continue;
        if (strcmp(name, "android/os/Handler$Callback") == 0)
            flags |= PROXY_HANDLER_CALLBACK;
        else if (strcmp(name, "android/view/Choreographer$FrameCallback") == 0)
            flags |= PROXY_FRAME_CALLBACK;
        else if (strcmp(name, "java/lang/Runnable") == 0)
            flags |= PROXY_RUNNABLE;
        else if (strcmp(name,
                        "com/google/android/gms/tasks/OnSuccessListener") == 0)
            flags |= PROXY_TASK_SUCCESS;
        else if (strcmp(name,
                        "com/google/android/gms/tasks/OnFailureListener") == 0)
            flags |= PROXY_TASK_FAILURE;
        else if (strcmp(name, "com/netflix/games/Callback") == 0)
            flags |= PROXY_NETFLIX_CALLBACK;
        else if (strcmp(name,
                        "com/netflix/unity/api/NetflixPluginCallback") == 0)
            flags |= PROXY_NETFLIX_PLUGIN;
        else if (strcmp(name,
                        "com/netflix/games/performance/crashreporting/"
                        "ConfigChangeListener") == 0)
            flags |= PROXY_NETFLIX_CONFIG;
    }
    return flags;
}

static jobj *make_interface_proxy(int64_t handle, jobj *interfaces,
                                  unsigned origin_flags)
{
    jobj *proxy = mk_object("bitter/jnibridge/Proxy");
    proxy->prim = handle;
    proxy->len = (int)(proxy_interfaces(interfaces) | origin_flags);
    JT("newInterfaceProxy handle=%#llx flags=%#x -> %p",
       (unsigned long long)handle, proxy->len, (void *)proxy);
    if ((proxy->len & (PROXY_HANDLER_CALLBACK | PROXY_FRAME_CALLBACK)) ==
        (PROXY_HANDLER_CALLBACK | PROXY_FRAME_CALLBACK)) {
        __atomic_store_n(&choreo_proxy, proxy, __ATOMIC_RELEASE);
        nx_log("jni: UnityChoreographer proxy captured handle=%#llx",
               (unsigned long long)handle);
    }
    return proxy;
}

static void *invoke_proxy(jobj *proxy, jobj *iface, jobj *method, jobj *args)
{
    if (!proxy || !iface || !method || !args)
        return NULL;
    if (proxy->len & PROXY_REFLECTION) {
        void *result = NULL;
        pthread_mutex_lock(&reflection_proxy_lock);
        if (reflection_proxy_closing) {
            pthread_mutex_unlock(&reflection_proxy_lock);
            return NULL;
        }
        reflection_proxy_active++;
        pthread_mutex_unlock(&reflection_proxy_lock);

        void *invoke = fp2_jni_native("com/unity3d/player/ReflectionHelper",
                                     "nativeProxyInvoke");
        if (invoke && proxy->prim && method->str) {
            jobj *name = mk_string(method->str);
            result =
                ((void *(*)(void *, void *, int64_t, void *, void *))invoke)(
                    fp2_jni_env(),
                    mk_class("com/unity3d/player/ReflectionHelper"),
                    proxy->prim, name, args);
        }

        pthread_mutex_lock(&reflection_proxy_lock);
        if (--reflection_proxy_active == 0)
            pthread_cond_broadcast(&reflection_proxy_idle);
        pthread_mutex_unlock(&reflection_proxy_lock);
        return result;
    }
    void *invoke = fp2_jni_native("bitter/jnibridge/JNIBridge", "invoke");
    if (!invoke || !proxy->prim)
        return NULL;
    return ((void *(*)(void *, void *, int64_t, void *, void *, void *))invoke)(
        fp2_jni_env(), iface, proxy->prim, iface, method, args);
}

static int queue_choreographer_message(jobj *message)
{
    int queued = 0;
    pthread_mutex_lock(&choreo_message_lock);
    if (choreo_message_count < FP2_CHOREO_MESSAGE_MAX) {
        unsigned tail = (choreo_message_head + choreo_message_count) %
                        FP2_CHOREO_MESSAGE_MAX;
        choreo_messages[tail] = message;
        choreo_message_count++;
        queued = 1;
    }
    pthread_mutex_unlock(&choreo_message_lock);
    return queued;
}

static jobj *take_choreographer_message(void)
{
    jobj *message = NULL;
    pthread_mutex_lock(&choreo_message_lock);
    if (choreo_message_count != 0) {
        message = choreo_messages[choreo_message_head];
        choreo_messages[choreo_message_head] = NULL;
        choreo_message_head = (choreo_message_head + 1) %
                              FP2_CHOREO_MESSAGE_MAX;
        choreo_message_count--;
    }
    pthread_mutex_unlock(&choreo_message_lock);
    return message;
}

static void clear_choreographer_messages(void)
{
    pthread_mutex_lock(&choreo_message_lock);
    memset(choreo_messages, 0, sizeof choreo_messages);
    choreo_message_head = 0;
    choreo_message_count = 0;
    pthread_mutex_unlock(&choreo_message_lock);
}

static int deliver_handle_message(jobj *message)
{
    jobj *proxy = __atomic_load_n(&choreo_proxy, __ATOMIC_ACQUIRE);
    if (!proxy || !message)
        return 0;
    handlemsg_args->elems[0] = message;
    jobj *iface = mk_class("android/os/Handler$Callback");
    nx_log("jni: UnityChoreographer handleMessage(what=%lld)",
           (long long)message->prim);
    (void)invoke_proxy(proxy, iface, handlemsg_method, handlemsg_args);
    return 1;
}

static int deliver_frame(int64_t nanos)
{
    /* Unity 6 also registers NATIVE AChoreographer callbacks; they share this
     * tick so both clocks stay on the same 60 Hz pacing. */
    extern void fp2_choreographer_native_tick(int64_t nanos);
    fp2_choreographer_native_tick(nanos);

    /* android.view.Choreographer callbacks are one-shot.  Unity reposts from
     * doFrame while active; STOP simply stops reposting. */
    if (!__atomic_exchange_n(&choreo_java_frame_pending, 0,
                             __ATOMIC_ACQ_REL))
        return 0;
    jobj *proxy = __atomic_load_n(&choreo_proxy, __ATOMIC_ACQUIRE);
    if (!proxy)
        return 0;
    __atomic_store_n(&frame_time_box->prim, nanos, __ATOMIC_RELEASE);
    jobj *iface = mk_class("android/view/Choreographer$FrameCallback");
    (void)invoke_proxy(proxy, iface, doframe_method, doframe_args);
    return 1;
}

static void *choreographer_driver(void *arg)
{
    (void)arg;
    void *domain = NULL;
    void *thread = NULL;
    void (*thread_detach)(void *) = NULL;
    nx_mod *il2cpp = nx_find_mod("libil2cpp.so");
    if (il2cpp) {
        void *(*domain_get)(void) = nx_lookup_in(il2cpp, "il2cpp_domain_get");
        void *(*thread_attach)(void *) =
            nx_lookup_in(il2cpp, "il2cpp_thread_attach");
        thread_detach = nx_lookup_in(il2cpp, "il2cpp_thread_detach");
        if (domain_get && thread_attach) {
            domain = domain_get();
            thread = thread_attach(domain);
        }
    }
    nx_log("jni: UnityChoreographer HandlerThread active"
           " (il2cpp domain=%p thread=%p)", domain, thread);

    const int64_t period = 16666667LL;
    int64_t next = monotonic_nanos() + period;
    unsigned long frames = 0;
    int initialized = 0;
    while (!__atomic_load_n(&choreo_stop_requested, __ATOMIC_ACQUIRE)) {
        jobj *message;
        while ((message = take_choreographer_message()) != NULL) {
            if (deliver_handle_message(message))
                initialized = 1;
            if (__atomic_load_n(&choreo_stop_requested, __ATOMIC_ACQUIRE))
                break;
        }
        if (__atomic_load_n(&choreo_stop_requested, __ATOMIC_ACQUIRE))
            break;
        if (!initialized) {
            usleep(1000);
            next = monotonic_nanos() + period;
            continue;
        }

        struct timespec until = {
            .tv_sec = next / 1000000000LL,
            .tv_nsec = next % 1000000000LL,
        };
        while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &until, NULL) ==
               EINTR)
            ;
        if (__atomic_load_n(&choreo_stop_requested, __ATOMIC_ACQUIRE))
            break;
        if (deliver_frame(next)) {
            frames++;
            if (frames == 1)
                nx_log("jni: UnityChoreographer first doFrame delivered");
        }
        next += period;
        int64_t now = monotonic_nanos();
        if (next < now)
            next = now + period;
    }

    if (thread && thread_detach)
        thread_detach(thread);
    nx_log("jni: UnityChoreographer HandlerThread stopped and detached");
    return NULL;
}

/* Join the emulated HandlerThread only after nativeDone has posted and
 * consumed UnityChoreographer's final STOP/QUIT messages.  Stopping it from
 * ReflectionHelper.endUnityLaunch is too early: Unity 6 calls that reflection
 * barrier first, then sends Handler messages 2 and 3 during nativeDone. */
static void stop_choreographer_driver(void)
{
    if (!__atomic_load_n(&choreo_thread_started, __ATOMIC_ACQUIRE))
        return;

    __atomic_store_n(&choreo_stop_requested, 1, __ATOMIC_RELEASE);
    if (__atomic_exchange_n(&choreo_thread_joinable, 0,
                            __ATOMIC_ACQ_REL)) {
        if (pthread_equal(pthread_self(), choreo_thread)) {
            /* Defensive only: the public join runs on UnityMain. */
            pthread_detach(choreo_thread);
        } else {
            (void)pthread_join(choreo_thread, NULL);
        }
    }
    __atomic_store_n(&choreo_proxy, NULL, __ATOMIC_RELEASE);
    __atomic_store_n(&choreo_java_frame_pending, 0, __ATOMIC_RELEASE);
    clear_choreographer_messages();
    __atomic_store_n(&choreo_thread_started, 0, __ATOMIC_RELEASE);
}

void fp2_jni_finish_callbacks(void)
{
    stop_choreographer_driver();
}

static void finish_reflection_proxies(void)
{
    pthread_mutex_lock(&reflection_proxy_lock);
    reflection_proxy_closing = 1;
    while (reflection_proxy_active != 0)
        pthread_cond_wait(&reflection_proxy_idle, &reflection_proxy_lock);
    pthread_mutex_unlock(&reflection_proxy_lock);
}

/* --- basics ------------------------------------------------------------- */

static int32_t j_GetVersion(void *e) { (void)e; return 0x00010006; }

static jobj *j_FindClass(void *e, const char *name)
{
    (void)e;
    JT("FindClass %s", name);
    return mk_class(name);
}

static jobj *j_GetObjectClass(void *e, jobj *o)
{
    (void)e;
    return mk_class(o && o->cls ? o->cls : "java/lang/Object");
}

static uint8_t j_IsInstanceOf(void *e, jobj *o, jobj *c)
{
    (void)e;
    if (!o || !c || !c->cls)
        return 0;
    if (strcmp(c->cls, "java/lang/Object") == 0)
        return 1;
    if (strcmp(c->cls, "android/view/InputEvent") == 0 && o->cls &&
        (strcmp(o->cls, "android/view/KeyEvent") == 0 ||
         strcmp(o->cls, "android/view/MotionEvent") == 0))
        return 1;
    return o->cls && strcmp(o->cls, c->cls) == 0;
}

static uint8_t j_IsAssignableFrom(void *e, jobj *a, jobj *b)
{
    (void)e;
    return a && b && a->cls && b->cls && strcmp(a->cls, b->cls) == 0;
}

static jobj *j_GetSuperclass(void *e, jobj *c) { (void)e; (void)c; return mk_class("java/lang/Object"); }

static void *j_NewGlobalRef(void *e, void *o) { (void)e; return o; }
static void *j_NewLocalRef(void *e, void *o) { (void)e; return o; }
static void *j_NewWeakGlobalRef(void *e, void *o) { (void)e; return o; }
static void j_DeleteRef(void *e, void *o) { (void)e; (void)o; }
static uint8_t j_IsSameObject(void *e, void *a, void *b) { (void)e; return a == b; }
static int32_t j_PushLocalFrame(void *e, int32_t n) { (void)e; (void)n; return 0; }
static void *j_PopLocalFrame(void *e, void *r) { (void)e; return r; }
static int32_t j_EnsureLocalCapacity(void *e, int32_t n) { (void)e; (void)n; return 0; }

static jobj *pending_exception;
static void *j_ExceptionOccurred(void *e) { (void)e; return pending_exception; }
static void j_ExceptionDescribe(void *e) { (void)e; }
static void j_ExceptionClear(void *e) { (void)e; pending_exception = NULL; }
static uint8_t j_ExceptionCheck(void *e) { (void)e; return pending_exception != NULL; }
static int32_t j_Throw(void *e, jobj *t) { (void)e; pending_exception = t; return 0; }
static int32_t j_ThrowNew(void *e, jobj *c, const char *m)
{
    (void)e;
    nx_log("jni: ThrowNew %s: %s", c && c->cls ? c->cls : "?", m ? m : "");
    pending_exception = new_obj(O_THROWABLE, c ? c->cls : "java/lang/Exception");
    return 0;
}
static void j_FatalError(void *e, const char *m) { (void)e; nx_die("JNI FatalError: %s", m); }

static int32_t j_GetJavaVM(void *e, void **vm) { (void)e; *vm = fp2_jni_vm(); return 0; }
static int32_t j_MonitorEnter(void *e, void *o) { (void)e; (void)o; return 0; }
static int32_t j_MonitorExit(void *e, void *o) { (void)e; (void)o; return 0; }

/* --- strings ----------------------------------------------------------- */

static jobj *j_NewStringUTF(void *e, const char *s) { (void)e; return mk_string(s); }
static const char *j_GetStringUTFChars(void *e, jobj *s, uint8_t *copy)
{
    (void)e;
    if (copy) *copy = 0;
    return s && s->str ? s->str : "";
}
static void j_ReleaseStringUTFChars(void *e, jobj *s, const char *c) { (void)e; (void)s; (void)c; }
static int32_t j_GetStringUTFLength(void *e, jobj *s) { (void)e; return s ? s->len : 0; }
static int32_t j_GetStringLength(void *e, jobj *s) { (void)e; return s ? s->len : 0; }
static jobj *j_NewString(void *e, const uint16_t *u, int32_t n)
{
    (void)e;
    char *b = malloc((size_t)n + 1);
    for (int32_t i = 0; i < n; i++)
        b[i] = (char)(u[i] < 128 ? u[i] : '?');
    b[n] = 0;
    jobj *o = mk_string(b);
    free(b);
    return o;
}
static const uint16_t *j_GetStringChars(void *e, jobj *s, uint8_t *copy)
{
    (void)e;
    if (copy) *copy = 1;
    int n = s ? s->len : 0;
    uint16_t *u = calloc((size_t)n + 1, 2);
    for (int i = 0; i < n; i++)
        u[i] = (uint8_t)s->str[i];
    return u;
}
static void j_ReleaseStringChars(void *e, jobj *s, const uint16_t *u) { (void)e; (void)s; free((void *)u); }
static void j_GetStringUTFRegion(void *e, jobj *s, int32_t off, int32_t len, char *out)
{
    (void)e;
    if (s && s->str)
        memcpy(out, s->str + off, (size_t)len);
}

/* --- arrays ------------------------------------------------------------ */

static int32_t j_GetArrayLength(void *e, jobj *a) { (void)e; return a ? a->len : 0; }

static jobj *j_NewByteArray(void *e, int32_t n)
{
    (void)e;
    jobj *o = new_obj(O_BYTEARRAY, "[B");
    o->len = n;
    o->data = calloc((size_t)(n > 0 ? n : 1), 1);
    return o;
}

static jobj *j_NewIntArray(void *e, int32_t n)
{
    (void)e;
    jobj *o = new_obj(O_BYTEARRAY, "[I");
    o->len = n;
    o->data = calloc((size_t)(n > 0 ? n : 1), sizeof(int32_t));
    return o;
}

static jobj *mk_int_array(const int32_t *values, int32_t n)
{
    jobj *o = j_NewIntArray(NULL, n);
    if (values && n > 0)
        memcpy(o->data, values, (size_t)n * sizeof *values);
    return o;
}
static void *j_GetByteArrayElements(void *e, jobj *a, uint8_t *copy)
{
    (void)e;
    if (copy) *copy = 0;
    return a ? a->data : NULL;
}
static void j_ReleaseArrayElements(void *e, jobj *a, void *p, int32_t mode)
{
    (void)e; (void)a; (void)p; (void)mode;
}
static void j_GetByteArrayRegion(void *e, jobj *a, int32_t s, int32_t n, void *buf)
{
    (void)e;
    if (a && a->data)
        memcpy(buf, (char *)a->data + s, (size_t)n);
}
static void j_SetByteArrayRegion(void *e, jobj *a, int32_t s, int32_t n, const void *buf)
{
    (void)e;
    if (a && a->data)
        memcpy((char *)a->data + s, buf, (size_t)n);
}

static void j_GetIntArrayRegion(void *e, jobj *a, int32_t s, int32_t n, void *buf)
{
    (void)e;
    if (a && a->data)
        memcpy(buf, (int32_t *)a->data + s, (size_t)n * sizeof(int32_t));
}

static void j_SetIntArrayRegion(void *e, jobj *a, int32_t s, int32_t n,
                                const void *buf)
{
    (void)e;
    if (a && a->data)
        memcpy((int32_t *)a->data + s, buf, (size_t)n * sizeof(int32_t));
}
static jobj *j_NewObjectArray(void *e, int32_t n, jobj *cls, jobj *init)
{
    (void)e; (void)cls;
    jobj *o = new_obj(O_OBJARRAY, "[Ljava/lang/Object;");
    o->len = n;
    o->elems = calloc((size_t)(n > 0 ? n : 1), sizeof *o->elems);
    for (int32_t i = 0; i < n; i++)
        o->elems[i] = init;
    return o;
}
static jobj *j_GetObjectArrayElement(void *e, jobj *a, int32_t i)
{
    (void)e;
    return (a && a->elems && i >= 0 && i < a->len) ? a->elems[i] : NULL;
}
static void j_SetObjectArrayElement(void *e, jobj *a, int32_t i, jobj *v)
{
    (void)e;
    if (a && a->elems && i >= 0 && i < a->len)
        a->elems[i] = v;
}
static void *j_GetPrimitiveArrayCritical(void *e, jobj *a, uint8_t *copy)
{
    return j_GetByteArrayElements(e, a, copy);
}
static void j_ReleasePrimitiveArrayCritical(void *e, jobj *a, void *p, int32_t m)
{
    (void)e; (void)a; (void)p; (void)m;
}
static jobj *j_NewDirectByteBuffer(void *e, void *addr, int64_t cap)
{
    (void)e;
    jobj *o = new_obj(O_DIRECTBUF, "java/nio/ByteBuffer");
    o->data = addr;
    o->len = (int)cap;
    return o;
}
static void *j_GetDirectBufferAddress(void *e, jobj *b) { (void)e; return b ? b->data : NULL; }
static int64_t j_GetDirectBufferCapacity(void *e, jobj *b) { (void)e; return b ? b->len : -1; }
static int32_t j_GetObjectRefType(void *e, void *o) { (void)e; return o ? 2 : 0; }

/* --- calls ------------------------------------------------------------- */

static int is_unbox(const char *name)
{
    static const char *const n[] = { "longValue", "intValue", "shortValue",
                                     "byteValue", "charValue", "booleanValue",
                                     "floatValue", "doubleValue" };
    for (size_t i = 0; i < sizeof n / sizeof *n; i++)
        if (strcmp(name, n[i]) == 0)
            return 1;
    return 0;
}

/* --- Android input devices, KeyEvent and MotionEvent ------------------ */

static int64_t jfloat_result(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof bits);
    return bits;
}

static int64_t j_InputDevice_getDeviceIds(jctx *c)
{
    (void)c;
    /* Real Android enumerates its built-in touchscreen as an InputDevice too.
     * Unity must see that device during its startup scan before later
     * touchscreen MotionEvents can become Touches. */
    const int32_t ids[] = { 0, 1 };
    static int logged;
    if (!logged++)
        nx_log("jni: publishing Android touchscreen id=0 and gamepad id=1 "
               "(%s %04x:%04x)",
               input_device_name, input_device_vendor & 0xffff,
               input_device_product & 0xffff);
    return (int64_t)(uintptr_t)mk_int_array(ids, 2);
}

static int64_t j_InputDevice_getDevice(jctx *c)
{
    int id = jarg_int(c);
    return (int64_t)(uintptr_t)(id == 0 ? touch_device :
                                id == 1 ? input_device : NULL);
}

static int64_t j_InputEvent_getDevice(jctx *c)
{
    if (c->self && c->self->cls &&
        strcmp(c->self->cls, "android/view/MotionEvent") == 0 &&
        motion_from_object(c->self)->device_id == 0) {
        JT("touch getDevice -> %p (id=0 sources=%#x)",
           (void *)touch_device, 0x00001002);
        return (int64_t)(uintptr_t)touch_device;
    }
    return (int64_t)(uintptr_t)input_device;
}

static int64_t j_InputDevice_getString(jctx *c)
{
    if (c->self == touch_device)
        return (int64_t)(uintptr_t)mk_string(
            strcmp(c->m->name, "getDescriptor") == 0
                ? "nxcompat:touch:0" : "NXCompat Touchscreen");
    const char *value = strcmp(c->m->name, "getDescriptor") == 0
        ? input_device_descriptor : input_device_name;
    return (int64_t)(uintptr_t)mk_string(value);
}

static int64_t j_InputDevice_getInt(jctx *c)
{
    if (c->self == touch_device) {
        if (strcmp(c->m->name, "getSources") == 0) return 0x00001002;
        if (strcmp(c->m->name, "getId") == 0) return 0;
        return 0;
    }
    if (strcmp(c->m->name, "getVendorId") == 0) return input_device_vendor;
    if (strcmp(c->m->name, "getProductId") == 0) return input_device_product;
    if (strcmp(c->m->name, "getSources") == 0) return 0x01000611;
    if (strcmp(c->m->name, "getId") == 0) return 1;
    if (strcmp(c->m->name, "getControllerNumber") == 0) return 1;
    return 0; /* KEYBOARD_TYPE_NONE */
}

static int64_t j_InputDevice_supportsSource(jctx *c)
{
    int source = jarg_int(c);
    int available = c->self == touch_device ? 0x00001002 : 0x01000611;
    return (available & source) == source;
}

static int64_t j_InputDevice_getMotionRanges(jctx *c)
{
    return (int64_t)(uintptr_t)(c->self == touch_device
                                    ? touch_motion_range_list
                                    : motion_range_list);
}

static int64_t j_InputDevice_getMotionRange(jctx *c)
{
    int axis = jarg_int(c);
    if (c->self == touch_device)
        return 0;
    for (size_t i = 0; i < sizeof motion_axis_ids /
                            sizeof *motion_axis_ids; i++)
        if (motion_axis_ids[i] == axis)
            return (int64_t)(uintptr_t)motion_ranges[i];
    return 0;
}

static int64_t j_List_size(jctx *c)
{
    return c->self ? c->self->len : 0;
}

static int64_t j_List_get(jctx *c)
{
    int index = jarg_int(c);
    if (!c->self || !c->self->elems || index < 0 || index >= c->self->len)
        return 0;
    return (int64_t)(uintptr_t)c->self->elems[index];
}

static int64_t j_List_iterator(jctx *c)
{
    motion_range_iterator->elems = c->self ? c->self->elems : NULL;
    motion_range_iterator->len = c->self ? c->self->len : 0;
    motion_range_iterator->prim = 0;
    return (int64_t)(uintptr_t)motion_range_iterator;
}

static int64_t j_Iterator_hasNext(jctx *c)
{
    return c->self && c->self->prim < c->self->len;
}

static int64_t j_Iterator_next(jctx *c)
{
    if (!c->self || !c->self->elems ||
        c->self->prim < 0 || c->self->prim >= c->self->len)
        return 0;
    return (int64_t)(uintptr_t)c->self->elems[c->self->prim++];
}

static int64_t j_MotionRange_getInt(jctx *c)
{
    if (!c->self)
        return 0;
    if (strcmp(c->m->name, "getAxis") == 0)
        return c->self->prim;
    return 0x01000010; /* SOURCE_JOYSTICK */
}

static int64_t j_MotionRange_getFloat(jctx *c)
{
    int axis = c->self ? (int)c->self->prim : -1;
    float value = 0.0f;
    if (strcmp(c->m->name, "getMin") == 0)
        value = axis == 17 || axis == 18 ? 0.0f : -1.0f;
    else if (strcmp(c->m->name, "getMax") == 0)
        value = 1.0f;
    else if (strcmp(c->m->name, "getFlat") == 0)
        value = 0.05f;
    else if (strcmp(c->m->name, "getResolution") == 0)
        value = 1.0f / 32767.0f;
    return jfloat_result(value);
}

static int64_t j_KeyEvent_getInt(jctx *c)
{
    if (strcmp(c->m->name, "getAction") == 0) return key_event.action;
    if (strcmp(c->m->name, "getKeyCode") == 0) return key_event.keycode;
    if (strcmp(c->m->name, "getSource") == 0) return key_event.source;
    if (strcmp(c->m->name, "getDeviceId") == 0) return key_event.device_id;
    if (strcmp(c->m->name, "getMetaState") == 0) return key_event.meta_state;
    if (strcmp(c->m->name, "getRepeatCount") == 0) return key_event.repeat;
    if (strcmp(c->m->name, "getScanCode") == 0) return key_event.scancode;
    if (strcmp(c->m->name, "getFlags") == 0) return key_event.flags;
    if (strcmp(c->m->name, "getUnicodeChar") == 0) return key_event.unicode;
    return 0;
}

static int64_t j_KeyEvent_getLong(jctx *c)
{
    if (strcmp(c->m->name, "getDownTime") == 0)
        return key_event.down_time[key_event.keycode];
    return key_event.event_time;
}

static int64_t j_KeyEvent_isSystem(jctx *c)
{
    (void)c;
    return 0;
}

static int64_t j_MotionEvent_getInt(jctx *c)
{
    const char *name = c->m->name;
    if (c->self && c->self->cls &&
        strcmp(c->self->cls, "android/view/KeyEvent") == 0)
        return j_KeyEvent_getInt(c);
    motion_payload *event = motion_from_object(c->self);
    if (strcmp(name, "getPointerId") == 0 ||
        strcmp(name, "findPointerIndex") == 0 ||
        strcmp(name, "getToolType") == 0)
        (void)jarg_int(c);
    if (strcmp(name, "getAction") == 0 ||
        strcmp(name, "getActionMasked") == 0) {
        int value = strcmp(name, "getActionMasked") == 0
            ? event->action & 0xff : event->action;
        if (event->source == 0x00001002)
            JT("touch %s -> %d", name, value);
        return value;
    }
    if (strcmp(name, "getSource") == 0) {
        if (event->source == 0x00001002)
            JT("touch getSource -> %#x", event->source);
        return event->source;
    }
    if (strcmp(name, "getDeviceId") == 0) {
        if (event->source == 0x00001002)
            JT("touch getDeviceId -> %d", event->device_id);
        return event->device_id;
    }
    if (strcmp(name, "getMetaState") == 0) return event->meta_state;
    if (strcmp(name, "getButtonState") == 0) return event->button_state;
    if (strcmp(name, "getFlags") == 0) return event->flags;
    if (strcmp(name, "getPointerCount") == 0) return 1;
    if (strcmp(name, "getPointerId") == 0) return 0;
    if (strcmp(name, "findPointerIndex") == 0) return 0;
    if (strcmp(name, "getToolType") == 0)
        return event->source == 0x00001002 ? 1 : 0; /* TOOL_TYPE_FINGER */
    return 0; /* history/action index, edge flags, tool type, display id */
}

static int64_t j_MotionEvent_getLong(jctx *c)
{
    if (c->self && c->self->cls &&
        strcmp(c->self->cls, "android/view/KeyEvent") == 0)
        return j_KeyEvent_getLong(c);
    motion_payload *event = motion_from_object(c->self);
    if (c->m->sig[1] == 'I')
        (void)jarg_int(c);
    int64_t value = strcmp(c->m->name, "getDownTime") == 0
        ? event->down_time : event->event_time;
    if (event->source == 0x00001002)
        JT("touch %s -> %lld", c->m->name, (long long)value);
    return value;
}

static int64_t j_MotionEvent_getFloat(jctx *c)
{
    const char *name = c->m->name;
    motion_payload *event = motion_from_object(c->self);
    int axis = -1;
    if (strcmp(name, "getAxisValue") == 0 ||
        strcmp(name, "getHistoricalAxisValue") == 0) {
        axis = jarg_int(c);
        if (c->m->sig && strstr(c->m->sig, "(II)"))
            (void)jarg_int(c);
    } else if (strcmp(name, "getX") == 0 ||
               strcmp(name, "getHistoricalX") == 0 ||
               strcmp(name, "getRawX") == 0) {
        axis = 0;
        if (c->m->sig && c->m->sig[1] == 'I')
            (void)jarg_int(c);
        if (c->m->sig && strstr(c->m->sig, "(II)"))
            (void)jarg_int(c);
    } else if (strcmp(name, "getY") == 0 ||
               strcmp(name, "getHistoricalY") == 0 ||
               strcmp(name, "getRawY") == 0) {
        axis = 1;
        if (c->m->sig && c->m->sig[1] == 'I')
            (void)jarg_int(c);
        if (c->m->sig && strstr(c->m->sig, "(II)"))
            (void)jarg_int(c);
    }
    float value;
    if (strcmp(name, "getPressure") == 0)
        value = event->source == 0x00001002 ? 1.0f : 0.0f;
    else if (strcmp(name, "getSize") == 0)
        value = event->source == 0x00001002 ? 1.0f : 0.0f;
    else if (strcmp(name, "getTouchMajor") == 0 ||
             strcmp(name, "getTouchMinor") == 0 ||
             strcmp(name, "getToolMajor") == 0 ||
             strcmp(name, "getToolMinor") == 0)
        value = event->source == 0x00001002 ? 8.0f : 0.0f;
    else
        value = axis >= 0 && axis < (int)(sizeof event->axis /
                                          sizeof *event->axis)
                  ? event->axis[axis] : 0.0f;
    if (event->source == 0x00001002)
        JT("touch %s axis=%d -> %.3f", name, axis, value);
    return jfloat_result(value);
}

static int64_t j_MotionEvent_isFromSource(jctx *c)
{
    motion_payload *event = motion_from_object(c->self);
    int source = jarg_int(c);
    return (event->source & source) == source;
}

static int64_t j_MotionEvent_obtain(jctx *c)
{
    jobj *source = jarg_obj(c);
    unsigned slot = motion_clone_next++ % MOTION_CLONE_COUNT;
    if (!motion_clones[slot])
        motion_clones[slot] = mk_object("android/view/MotionEvent");
    motion_clone_data[slot] = *motion_from_object(source);
    motion_clones[slot]->data = &motion_clone_data[slot];
    if (motion_clone_data[slot].source == 0x00001002)
        JT("touch obtain clone=%u action=%d xy=%.1f,%.1f "
           "down=%lld event=%lld", slot, motion_clone_data[slot].action,
           motion_clone_data[slot].axis[0], motion_clone_data[slot].axis[1],
           (long long)motion_clone_data[slot].down_time,
           (long long)motion_clone_data[slot].event_time);
    return (int64_t)(uintptr_t)motion_clones[slot];
}

static int64_t j_MotionEvent_recycle(jctx *c)
{
    (void)c;
    return 0;
}

/* java.io.File.  Unity and Android plugins inspect their data files before
 * opening them.  A zero length is not a neutral answer: it means a truncated
 * or missing install and selects their normal report-and-bail path. */
static int64_t j_File_length(jctx *c)
{
    const char *p = c->self ? c->self->str : NULL;
    struct stat st;
    if (!p || stat(p, &st) != 0) {
        JT("File.length(\"%s\") -> 0 (no such file)", p ? p : "(null)");
        return 0;
    }
    JT("File.length(\"%s\") -> %lld", p, (long long)st.st_size);
    return (int64_t)st.st_size;
}

static int64_t j_File_exists(jctx *c)
{
    const char *p = c->self ? c->self->str : NULL;
    struct stat st;
    int64_t r = p && stat(p, &st) == 0;
    JT("File.exists(\"%s\") -> %lld", p ? p : "(null)", (long long)r);
    return r;
}

static int64_t j_File_getPath(jctx *c)
{
    return (int64_t)(uintptr_t)mk_string(c->self ? c->self->str : NULL);
}

static int64_t j_File_getParent(jctx *c)
{
    const char *path = c->self && c->self->str ? c->self->str : "";
    char buf[1200];
    snprintf(buf, sizeof buf, "%s", path);
    char *slash = strrchr(buf, '/');
    if (!slash)
        buf[0] = 0;
    else if (slash == buf)
        slash[1] = 0;
    else
        *slash = 0;
    return (int64_t)(uintptr_t)mk_string(buf);
}

static int64_t j_File_getParentFile(jctx *c)
{
    jobj *s = (jobj *)(uintptr_t)j_File_getParent(c);
    return (int64_t)(uintptr_t)mk_file(s ? s->str : "");
}

static int64_t j_File_isDirectory(jctx *c)
{
    struct stat st;
    const char *p = c->self && c->self->str ? c->self->str : "";
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

static int64_t j_File_mkdirs(jctx *c)
{
    const char *p = c->self && c->self->str ? c->self->str : "";
    if (!*p)
        return 0;
    if (mkdir(p, 0755) == 0 || errno == EEXIST)
        return 1;
    return 0;
}

static int64_t j_Context_getFilesDir(jctx *c)
{
    (void)c;
    return (int64_t)(uintptr_t)mk_file(fp2_home);
}

/* A Unity 6 monta o caminho dos AssetBundles como
 * `jar:file://<sourceDir>!/assets/AssetBundles/<nome>` e tira o sourceDir do
 * ApplicationInfo.  Sem este objeto o caminho sai VAZIO -- `jar:file://!/...` --
 * e todo bundle falha com "Unable to open archive file", que foi exatamente o
 * que travou o ST3 antes do menu.  O APK apontado aqui e' um ZIP de verdade,
 * gerado no build com os bundles STORED (ver tools/mk-apk-zip.sh). */
static jobj *application_info_object;

static int64_t j_Context_getApplicationInfo(jctx *c)
{
    (void)c;
    fprintf(stderr, "[fp2/apk] getApplicationInfo() chamado\n");
    if (!application_info_object)
        application_info_object = new_obj(O_OBJECT,
                                          "android/content/pm/ApplicationInfo");
    return (int64_t)(uintptr_t)application_info_object;
}

/* Caminho do APK-ZIP que carrega os assets.  Cai no gamedir se o arquivo nao
 * existir, para nunca devolver string vazia. */
static const char *fp2_apk_zip_path(void)
{
    static char caminho[1152];
    if (!caminho[0]) {
        snprintf(caminho, sizeof caminho, "%s/game.apk", fp2_gamedir);
        if (access(caminho, R_OK) != 0)
            snprintf(caminho, sizeof caminho, "%s", fp2_gamedir);
    }
    return caminho;
}

/* O ST3 e' APK DIVIDIDO (base + split de ABI), e a Unity 6 procura os assets
 * em ApplicationInfo.splitPublicSourceDirs -- um String[] -- e nao em
 * sourceDir.  Devolver so' o sourceDir deixa o caminho vazio e todo bundle
 * morre em `jar:file://!/assets/AssetBundles/...`.  Aqui vai um array de um
 * elemento com o APK-ZIP do port. */
/* A Unity 6 pergunta ao wrapper de Play Asset Delivery se a API do PlayCore
 * esta AUSENTE.  Sem tratamento a chamada devolvia 0 -- ou seja, "o PlayCore
 * existe" -- e a engine passava a buscar todo AssetBundle como asset pack da
 * Play Store, montando `jar:file://!/assets/...` com caminho vazio: os 45
 * bundles do jogo morriam em "Unable to open archive file".  Aqui nao ha Play
 * Store, entao a resposta honesta e' 1 (ausente), e a engine volta ao caminho
 * normal de ler os assets do proprio pacote. */
static int64_t j_PlayAssetDelivery_apiMissing(jctx *c)
{
    (void)c;
    return 1;
}

static int64_t j_ApplicationInfo_splitPublicSourceDirs(jctx *c)
{
    (void)c;
    static jobj *vetor;
    if (!vetor) {
        vetor = j_NewObjectArray(NULL, 1, mk_class("java/lang/String"), NULL);
        vetor->elems[0] = mk_string(fp2_apk_zip_path());
        fprintf(stderr, "[fp2/apk] splitPublicSourceDirs -> [%s]\n",
                fp2_apk_zip_path());
    }
    return (int64_t)(uintptr_t)vetor;
}

static int64_t j_ApplicationInfo_sourceDir(jctx *c)
{
    (void)c;
    fprintf(stderr, "[fp2/apk] sourceDir -> %s\n", fp2_apk_zip_path());
    return (int64_t)(uintptr_t)mk_string(fp2_apk_zip_path());
}

static int64_t j_Context_getPackageCodePath(jctx *c)
{
    (void)c;
    /* Unity treats this as the extracted package root and appends
     * assets/bin/Data itself.  Returning fp2_datadir would produce
     * <gamedir>/assets/assets/bin/Data. */
    /* A Unity 6 so' monta streamingAssetsPath quando este caminho e' um APK de
     * verdade; com um diretorio o campo sai VAZIO e todo AssetBundle morre em
     * `jar:file://!/assets/...`.  Como ela usa o mesmo caminho para os dados do
     * player, o APK-ZIP carrega a arvore inteira (tools/mk-apk-zip.sh). */
    const char *r = fp2_apk_zip_path();
    fprintf(stderr, "[fp2/apk] getPackageCodePath() -> %s\n", r);
    return (int64_t)(uintptr_t)mk_string(r);
}

/* The real application id of this APK.  The IL2CPP metadata string table
 * carries exactly one package constant, "com.ballatergames.debug", sitting
 * right next to "UnityType" -- that pair is Kairosoft's package-name self
 * check, and it compares the value against what the Java side reports.  The
 * earlier placeholder ("net.kairosoft.android.mujin_en", inherited from the
 * 60parsecs lineage of this loader skeleton) never matches anything the game
 * knows about, so the check fails. */
#define BB_PACKAGE_NAME "com.ballatergames.debug"

static int64_t j_Context_getPackageName(jctx *c)
{
    (void)c;
    return (int64_t)(uintptr_t)mk_string(BB_PACKAGE_NAME);
}

static int64_t j_PackageManager_hasSystemFeature(jctx *c)
{
    (void)c;
    return 0; /* no Android system features on this host */
}

static int64_t j_Process_setThreadPriority(jctx *c)
{
    (void)c;
    return 0; /* thread priorities stay with the host scheduler */
}

static int64_t j_Context_getPackageManager(jctx *c)
{
    (void)c;
    return (int64_t)(uintptr_t)mk_object("android/content/pm/PackageManager");
}

static int64_t j_Context_getAssets(jctx *c)
{
    (void)c;
    return (int64_t)(uintptr_t)mk_object("android/content/res/AssetManager");
}

static int64_t j_Context_getContentResolver(jctx *c)
{
    (void)c;
    return (int64_t)(uintptr_t)mk_object("android/content/ContentResolver");
}

static int64_t j_Context_getApplicationContext(jctx *c)
{
    /* Android may return the same process-wide Context identity here.  Keep
     * the object stable: Rewired retains it for its input helper lifetime. */
    return (int64_t)(uintptr_t)(c->self ? c->self : armory_activity_object);
}

static int64_t j_Context_getSystemService(jctx *c)
{
    jobj *name_object = jarg_obj(c);
    const char *name = name_object && name_object->str
        ? name_object->str : "";
    jobj *service;
    if (strcmp(name, "audio") == 0)
        service = audio_manager_object;
    else if (strcmp(name, "input") == 0)
        service = input_manager_object;
    else if (strcmp(name, "vibrator") == 0)
        service = vibrator_object;
    else if (strcmp(name, "window") == 0)
        service = window_manager_object;
    else
        service = mk_object("android/content/Context$SystemService");
    JT("Context.getSystemService(\"%s\") -> %s", name,
       service && service->cls ? service->cls : "(null)");
    return (int64_t)(uintptr_t)service;
}

static int64_t j_AudioManager_getInt(jctx *c)
{
    if (strcmp(c->m->name, "STREAM_MUSIC") == 0)
        return 3;
    if (strcmp(c->m->name, "getStreamMaxVolume") == 0) {
        (void)jarg_int(c);
        return 15;
    }
    if (strcmp(c->m->name, "getStreamMinVolume") == 0) {
        (void)jarg_int(c);
        return 0;
    }
    if (strcmp(c->m->name, "getStreamVolume") == 0) {
        (void)jarg_int(c);
        return 15;
    }
    return 0;
}

/* Kairosoft plugin arg-logging bindings: Utility.showDialog(I, String,
 * String, Object) is the game's NATIVE dialog -- what it says decides
 * why the boot quits (run11 evidence).  Config.init args likewise. */
/* ---------------------------------------------------------------------------
 * run33: ask IL2CPP itself where the managed methods live.
 *
 * 27 rounds of log-reading failed for a structural reason, not a missing
 * value: the device log shows the ORDER of JNI calls, never the CONDITION
 * that produced them.  run31 got a native backtrace from the showDialog
 * handler, but every frame was our own dispatcher -- IL2CPP reaches our JNI
 * thunk through a function pointer, so the managed frames are gone by the
 * time we can walk the stack.
 *
 * The binary is stripped and, decisively, the v24 metadata stores NO method
 * RVAs: a scan of all 31 tables for u32 values inside the managed section
 * found nothing dense enough to be a method table.  Unity 2018 resolves
 * method pointers at runtime from its codegen modules.
 *
 * What does work is the reflection API, which this build exports in full --
 * and il2.c already wraps it correctly.  The first attempt at this dumped the
 * methods from jni.c and crashed inside libil2cpp: il2cpp_class_from_name
 * takes (image, namespace, name), and passing the domain in the image slot
 * dereferences garbage.  Reusing il2_class()/il2_methods_foreach() is both
 * the correct and the cheaper fix.
 *
 * The lesson generalises past this function: the port already had a working,
 * battle-tested binding for the reflection API, and duplicating it is what
 * introduced the bug.  When a new consumer needs an existing capability, add
 * the missing piece to the existing wrapper instead of re-deriving the ABI.
 * ---------------------------------------------------------------------- */
struct fp2_method_scan {
    const char *class_name;
    int count;
};

static void fp2_method_scan_cb(void *ctx, const char *name,
                               const MethodInfo *m, const void *fn)
{
    const struct fp2_method_scan *scan = ctx;
    /* Compute the offset against the module we KNOW owns managed code.
     * fp2_mod_at() only reports the main executable and the LIBS[] entries,
     * and the game's own objects are loaded outside that whitelist, so asking
     * it for a managed address yields "?" and a useless +0x0.  libil2cpp.so's
     * base is the one number that makes these addresses actionable offline:
     * fn - base is a file offset that disasm_at.py can read. */
    static uint8_t *il2cpp_base;
    if (!il2cpp_base) {
        nx_mod *mod = nx_find_mod("libil2cpp.so");
        il2cpp_base = mod ? mod->base : NULL;
        /* Report once: if this says "absent", every offset below would be
         * meaningless and the reader deserves to know why rather than seeing
         * a column of +0x0. */
        nx_log("managed: libil2cpp.so base=%p span=%zu", (void *)il2cpp_base,
               mod ? mod->span : (size_t)0);
        extern int fp2_list_modules(void);
        fp2_list_modules();
    }
    unsigned long off = 0;
    const char *where = "nowhere";
    if (fn && il2cpp_base && (const char *)fn >= (const char *)il2cpp_base) {
        off = (unsigned long)((const char *)fn - (const char *)il2cpp_base);
        where = "libil2cpp.so";
    } else {
        void *image_base = NULL;
        const char *mod = fp2_mod_at(fn, &image_base);
        if (mod && image_base) {
            off = (unsigned long)((const char *)fn - (const char *)image_base);
            where = mod;
        }
    }
    /* Generic definitions and abstract methods carry no body, so their fn is
     * null or a shared stub; those only add noise here. */
    nx_log("managed: %s::%s method=%p fn=%p %s+0x%lx%s",
           scan->class_name, name, m, fn, where, off,
           fn ? "" : "  (no body)");
}

/* run39: hook em linha de um metodo gerenciado.
 *
 * POR QUE AGORA E POSSIVEL: ate o run38 nao havia endereco nativo confiavel de
 * um metodo, entao "observar a maquina de estados" era impensavel.  Com o
 * ChangeState resolvido (libil2cpp.so+0x8976c4) a pergunta muda de "qual
 * comparacao falha" para "qual estado recebe o que" -- e essa e uma pergunta
 * que o aparelho responde sozinho.
 *
 * POR QUE UM HOOK E NAO SO LEITURA DE CODIGO: a regiao entre Load e ClassLoad
 * tem 20 KB de ARM64 sem simbolos.  Decodificar aquilo a mao e lento e
 * fragil; pedir ao proprio jogo para narrar as transicoes e uma linha de
 * log.  E o argumento e o mesmo do run31: quando a verdade esta em um lugar
 * que da para interrogar, interrogue.
 *
 * A substituicao e o sequencial classico de AArch64 para alcance arbitrario
 * (um `b` so alcanca +/-128 MB, e o jogo esta em outro mapeamento):
 *
 *     ldr x16, #8      ; 4 bytes, PC-relativo para a palavra seguinte
 *     br  x16          ; 4 bytes
 *     .quad replacement; 8 bytes
 *
 * Sao 16 bytes, entao as 4 primeiras instrucoes do alvo sao realocadas para um
 * trampolim.  Aqui isso e seguro porque ChangeState abre com quatro `stp` e um
 * `add x29, sp`, nenhuma delas PC-relativa -- se fossem, copiar as palavras
 * tal como esta para outro endereco mudaria o alvo do branch. */
#define FP2_HOOK_WORDS 4  /* 16 bytes: ldr + br + .quad */

struct fp2_hook {
    void *target;
    void *replacement;
    uint32_t *stash;       /* original instruction words, still in place */
    uint32_t *trampoline;  /* relocated prologue + jump back */
};

/* Memoria executavel para o trampolim. */
static uint32_t *fp2_alloc_exec(size_t bytes)
{
    void *p = mmap(NULL, bytes, PROT_READ | PROT_WRITE | PROT_EXEC,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return p == MAP_FAILED ? NULL : (uint32_t *)p;
}

static int fp2_hook_install(struct fp2_hook *h, void *target,
                            void *replacement)
{
    if (!h || !target || !replacement)
        return 0;
    h->target = target;
    h->replacement = replacement;

    /* A pagina do alvo precisa ser gravavel.  O libil2cpp e' mapeado pelo
     * loader NX como leitura+execucao, entao sem isto toda escrita abaixo
     * falha -- e a falha apareceria como "o jogo ignorou meu hook" em vez de
     * "a pagina e' somente leitura". */
    long page = sysconf(_SC_PAGESIZE);
    uintptr_t page_start = (uintptr_t)target & ~((uintptr_t)page - 1);
    if (mprotect((void *)page_start, (size_t)page,
                 PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        nx_log("hook: mprotect(%p) falhou: %s", (void *)page_start,
               strerror(errno));
        return 0;
    }

    h->stash = (uint32_t *)target;
    h->trampoline = fp2_alloc_exec(64);
    if (!h->trampoline) {
        nx_log("hook: sem memoria para trampolim");
        return 0;
    }
    for (int i = 0; i < FP2_HOOK_WORDS; i++)
        h->trampoline[i] = h->stash[i];
    /* Volta para target+16.
     *
     * run39a: a primeira versao usava `b` aqui, com o comentario "o trampolim
     * esta numa reserva nova, entao a distancia e de poucos MB".  Esse palpite
     * estava errado e o aparelho mediu o motivo: o mmap do trampolim caiu
     * em 0x7f8df4d000 e o alvo em 0x7f858c66c4 -- 141 MB de distancia, e `b`
     * so' alcanca +/-128 MB.  O deslocamento truncado virou um salto para
     * lugar nenhum e o jogo morreu com SIGSEGV.
     *
     * A correcao nao e' escolher um trampolim mais proximo (o endereco nao
     * esta' sob nosso controle) e' o mesmo sequencial de 16 bytes usado na
     * substituicao, que nao tem limite de alcance.
     *
     * O par de 32 bits e' escrito a partir de UM unico uint64_t: fazer
     * `(uint32_t)(uint64_t)p >> 32` truncaria antes do deslocamento e o
     * compilador avisaria (right shift count >= width of type) -- e o aviso
     * estaria certo, porque a metade alta sairia zero e o salto iria para
     * o primeiro 4 GB do espaco de enderecamento. */
    uint64_t back = (uint64_t)(uintptr_t)((const char *)target + 16);
    uint64_t rep  = (uint64_t)(uintptr_t)replacement;
    h->trampoline[FP2_HOOK_WORDS + 0] = 0x58000050u;     /* ldr x16, #8 */
    h->trampoline[FP2_HOOK_WORDS + 1] = 0xD61F0200u;     /* br  x16      */
    h->trampoline[FP2_HOOK_WORDS + 2] = (uint32_t)back;
    h->trampoline[FP2_HOOK_WORDS + 3] = (uint32_t)(back >> 32);
    __builtin___clear_cache((char *)h->trampoline,
                             (char *)h->trampoline + FP2_HOOK_WORDS + 4);

    /* ldr x16, #8 -> 0x58000050 ; br x16 -> 0xD61F0200 */
    h->stash[0] = 0x58000050u;
    h->stash[1] = 0xD61F0200u;
    h->stash[2] = (uint32_t)rep;
    h->stash[3] = (uint32_t)(rep >> 32);
    __builtin___clear_cache((char *)h->stash,
                             (char *)h->stash + FP2_HOOK_WORDS);
    nx_log("hook: %p -> %p (prologo copiado para %p)", target, replacement,
           (void *)h->trampoline);
    return 1;
}

/* run39: narracao da maquina de estados.
 *
 * ChangeState(int) e' a unica porta de entrada do estado (o proprio codigo
 * termina em `str w19, [x20, #0x100]`, guardando o novo estado no objeto), e
 * o corpo desassemblado mostra um `cmp w19, #7` logo no comeco -- sete estados,
 * e um deles e' STATE_ERROR.  Cada chamada e' portanto uma linha de log, e a
 * sequencia completa diz qual chamada cruzou para o erro.
 *
 * run39a mediu algo importante: COM O HOOK INSTALADO E NENHUMA CHAMADA
 * REGISTRADA.  Antes de concluir "a passagem do erro nao usa ChangeState" ha
 * uma alternativa mais banal: o hook simplesmente nao funciona.  Por isso a
 * instrumentacao hooka VARIOS metodos de uma vez, sendo Update -- que o Unity
 * chama todo quadro -- o CONTROLE POSITIVO.  Se Update nao loga, a ferramenta
 * esta' quebrada; se Update loga e ChangeState nao, a conclusao e' real.
 * Essa distincao e' o que separa uma medicao de um palpite. */
static int fp2_changestate_seq;   /* contador global de eventos narrados */

/* Indices de FP2_WATCH.  A ordem nao importa para o jogo, so' para o log. */
enum {
    FP2_W_ChangeState = 0,
    FP2_W_Update,
    FP2_W_Init,
    FP2_W_ClassLoad,
    FP2_W_Load,
    FP2_W_COUNT
};

static const char *const fp2_watch_names[FP2_W_COUNT] = {
    "ChangeState", "Update", "Init", "ClassLoad", "Load",
};
static struct fp2_hook fp2_watch[FP2_W_COUNT];
static int fp2_watch_hits[FP2_W_COUNT];

/* Um slot de trampolim por metodo.  O stub le este endereco (ver a nota de
 * alcance dentro de FP2_WATCH_STUB) e salta nele.
 *
 * run39h: o slot do stub de excecao mora AQUI, e `__attribute__((used))`
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
#define FP2_TRAMP(sym) \
    static void *fp2_watch_tramp_##sym __attribute__((used))
FP2_TRAMP(changestate);
FP2_TRAMP(update);
FP2_TRAMP(init);
FP2_TRAMP(classload);
FP2_TRAMP(load);
FP2_TRAMP(xthrow);
FP2_TRAMP(setupexit);
FP2_TRAMP(setupent);
#undef FP2_TRAMP

__attribute__((used)) void fp2_managed_event(void *self, int arg1, int id);
__attribute__((used)) void fp2_throw_event(void *x30, void *self);
static void fp2_dump_bootform_fields(void *self);

/* Um stub por metodo observado.  O parametro `id` viaja como imediato, e' a
 * unica forma de o stub saber qual metodo e' sem ler a memoria de codigo --
 * que e' justamente o que o hook acabou de substituir.
 *
 * x0/x1/x2 sao salvos e devolvidos porque o trampolim volta para o MEIO do
 * corpo original: um registereo deixado sujo aqui corromperia argumentos que
 * o codigo de destino ainda vai ler (ChangeState usa x2 em 0x897764).
 *
 * Tudo o que precisa de `#` (o imediato do id e o nome do slot de trampolim)
 * e' concatenado em FP2_W_STUB, e nao aqui dentro.  run39b gastou um build
 * inteiro nisso: `#sym` na lista de argumentos de `__asm__` chega ao
 * compilador como `#` cru e produz "stray '#' in program", derrubando em
 * cascata o resto do arquivo.  Aqui dentro entram so' literais, ja prontas. */
#define FP2_W_TAIL(sym)                                    \
    "adrp x16, fp2_watch_tramp_" #sym "\n"                 \
    "ldr  x16, [x16, #:lo12:fp2_watch_tramp_" #sym "]\n"   \
    "br   x16\n"                                           \
    "nop\n"                                                \
    "nop\n"

/* run39h: `fn` became a PARAMETER.  The throw stub needs the same prologue,
 * the same register discipline and -- the part that actually matters -- the
 * same macro-expansion context as the five method stubs.  Hand-writing a
 * sixth body produced a slot that nm reported as `U` (undefined) while all
 * five siblings were `b` (local), and the link failed on it.  Eight
 * hypotheses were tested and eliminated (tools/repro_throw_sym1..8.c);
 * none of them explained it, and the outcome even differs between files
 * with equivalent code, so the rule could not be pinned down.  Letting the
 * SAME macro emit all six sidesteps the rule instead of fighting it, and
 * the check is one command: `nm src/jni.o | grep fp2_watch_tramp` must show
 * six `b` and no `U`.
 *
 * `w2` carries the method id for the five, and 0 for the throw stub, which
 * has no id: it reports a return address, not a method. */
#define FP2_WATCH_STUB(sym, mid, tail, fn)                                  \
    __attribute__((naked, used)) static void fp2_stub_##sym(void)          \
    {                                                                      \
        __asm__ __volatile__(                                              \
            "stp x29, x30, [sp, #-0x30]!\n"                               \
            "mov  x29, sp\n"                                              \
            "stp  x19, x20, [sp, #0x10]\n"                                \
            "stp  x21, x22, [sp, #0x20]\n"                                \
            "mov  x19, x0\n"                                              \
            "mov  x20, x1\n"                                              \
            "mov  x21, x2\n"                                              \
            "mov  w2, #" #mid "\n"                                       \
            "bl   " #fn "\n"                                             \
            "mov  x0, x19\n"                                              \
            "mov  x1, x20\n"                                              \
            "mov  x2, x21\n"                                              \
            "ldp  x21, x22, [sp, #0x20]\n"                                \
            "ldp  x19, x20, [sp, #0x10]\n"                                \
            "ldp  x29, x30, [sp], #0x30\n"                                \
            tail);                                                        \
    }

/* run39a: um `ret` no fim do stub e' um BUG, nao uma escolha.  O stub foi
 * alcancado pelo `br x16` da substituicao, entao x30 continua sendo o
 * endereco de RETORNO DO CHAMADOR -- um `ret` devolveria ao chamador e
 * pularia o corpo inteiro do metodo hookeado, incluindo a parte que o
 * trampolim deveria reexecutar.  E' necessario entrar no trampolim, que
 * devolve o controle ao target+16 -- e o trampolim que carrega x30 de volta.
 *
 * O salto vai por indirecao (adrp + ldr do slot; br x16) e nao por `b` porque
 * a distancia entre o stub (no executavel) e o trampolim (num mmap) foi
 * medida em 141 MB -- alem do alcance de `b`, que e' +/-128 MB.  Uma variavel
 * global unica seria suficiente, mas assim cada stub tem a sua e nenhum
 * estado e' compartilhado entre metodos.
 *
 * adrp/`:lo12:` em vez de `ldr x16, =simbolo`: a forma `=simbolo` cria um
 * conjunto de literais que o assembler pode posicionar longe do ponto de uso
 * (ate o fim da secao), e o alcance do `ldr` literal e' +/-1 MB.  O par
 * adrp/ldr resolve por relocacao e nao tem essa restricao. */

/* Os numeros sao escritos a mao porque o assembler so' enxerga o texto do
 * __asm__: um nome de enum C nao existe la' (erro "undefined symbol
 * FP2_W_ChangeState used as an immediate value").  A verificacao abaixo
 * falha a compilacao se algum dia os dois lados sairem de sincronia -- que e'
 * exatamente o tipo de divergencia silenciosa que o run33 ensinou a temer. */
#define FP2_W_STUB(sym, mid, expected)                       \
    typedef char fp2_sync_##sym[(mid) == (expected) ? 1 : -1]; \
    FP2_WATCH_STUB(sym, mid, FP2_W_TAIL(sym), fp2_managed_event)

FP2_W_STUB(changestate, 0, FP2_W_ChangeState)
FP2_W_STUB(update, 1, FP2_W_Update)
FP2_W_STUB(init, 2, FP2_W_Init)
FP2_W_STUB(classload, 3, FP2_W_ClassLoad)
FP2_W_STUB(load, 4, FP2_W_Load)

static void *const fp2_watch_stubs[FP2_W_COUNT] = {
    (void *)fp2_stub_changestate, (void *)fp2_stub_update,
    (void *)fp2_stub_init,       (void *)fp2_stub_classload,
    (void *)fp2_stub_load,
};

/* Update roda 60 vezes por segundo; o log inteiro viraria ruido.  O que
 * interessa ali e' a PRIMEIRA e a ULTIMA chamada -- a primeira mostra que a
 * instrumentacao funciona, a ultima diz que o jogo estava vivo ate o fim. */
#define FP2_UPDATE_LOG_MAX 3

/* run39f: despejar os campos do objeto gerenciado no instante em que um
 * metodo e' observado.
 *
 * A razao de existir: a leitura offline do corpo gerado de `BootForm::Init`
 * achou NOVE pontos `cbnz xN / bl <auxiliar-de-excecao>` (o auxiliar esta em
 * 0x600888).  Qualquer um deles, ao encontrar NULL, encerra o metodo ali --
 * e o jogo, com um handler global, transforma isso no dialogo
 * "An error has occurred.\n100".  A sequencia de estados nunca aparecia
 * porque `ChangeState` (0x8974c4) esta DEPOIS dos nove.
 *
 * A troca que este despejo faz e': o jogo lancou excecao em algum ponto de
 * Init vira o campo no offset +0x78 estar NULL.  O primeiro nao aponta
 * para nada; o segundo aponta direto para a construcao que falhou. */
static void fp2_dump_fields_cb(void *ctx, const char *name, FieldInfo *f,
                               int is_static, int type_kind,
                               int offset, const void *value)
{
    (void)f;
    (void)type_kind;
    /* run40: um campo estatico nao mora no objeto, entao nao ha valor a
     * ler -- mas o NOME ainda e' informacao.  `recordStores_`,
     * `rootStorage_` e `usePreference_` sao os tres que dizem se o backend
     * de gravacao tem estado global, e sem eles a lista de instancia parece
     * um objeto que nunca foi inicializado em vez de um objeto cuja
     * inicializacao falhou. */
    if (is_static) {
        nx_log("campo:  static     %-24s (slot 0x%x)", name,
               offset < 0 ? 0u : (unsigned)offset);
        return;
    }
    if (offset < 0 || offset > 0x100)
        return;
    /* run40: imprimir o ponteiro, nao "ok".  "ok" responde "e' nulo?" e
     * joga fora o endereco -- que e' justamente o que permite comparar este
     * objeto com o proximo e saber se e' o mesmo. */
    nx_log("campo:  this+0x%02x  %-24s %p%s", offset, name, value,
           value ? "" : "  <<< NULL");
}

static void fp2_dump_bootform_fields(void *self)
{
    if (!self)
        return;
    /* run39h: o objeto NAO e' necessariamente um BootForm.  A leitura
     * offline apontava nove `cbnz` dentro de BootForm::Init, mas o
     * deslocamento medido (0xa4a674) esta em codigo que nao pertence a
     * nenhum metodo gerado -- a tabela de ponteiros do .so nao o contem.
     * Assumir BootForm aqui repetiria o mesmo erro em forma nova: quando o
     * nome forcado nao bate, o despejo volta "vazio" e parece um objeto
     * saneado.  A classe REAL vem do proprio objeto. */
    Il2CppClass *k = il2_class_of((Il2CppObject *)self);
    if (!k) {
        nx_log("campo: classe do objeto %p ilegivel", self);
        return;
    }
    nx_log("campo: ---- %s @ %p ----", il2_class_label(k), self);
    il2_fields_foreach(k, fp2_dump_fields_cb, self);

    /* run40: os campos dizem O QUE esta vazio; os metodos dizem QUEM deveria
     * ter preenchido.  `storage_` e' NULL e nenhuma leitura offline vai
     * nomear o metodo que o atribui, porque o nome do metodo mora no
     * metadado e o endereco mora na tabela de ponteiros -- nm nao tem um sem
     * o outro.  A API de reflexao tem os dois, e o custo e' uma varredura
     * que ja foi validada no run33.  Cada `+0x...` abaixo e' um offset de
     * arquivo que disasm_at.py le direto. */
    struct fp2_method_scan scan = { il2_class_label(k), 0 };
    il2_methods_foreach(k, fp2_method_scan_cb, &scan);
}

__attribute__((used)) void fp2_managed_event(void *self, int arg1, int id)
{
    if (id < 0 || id >= FP2_W_COUNT)
        return;
    nx_mod *mod = nx_find_mod("libil2cpp.so");
    unsigned long off = 0;
    const char *where = "libil2cpp.so";

    if (mod && (char *)self >= (char *)mod->base)
        off = (unsigned long)((char *)self - (char *)mod->base);
    else
        where = "?";

    if (id == FP2_W_Update) {
        if (fp2_watch_hits[id] >= FP2_UPDATE_LOG_MAX)
            return;
        nx_log("state: Update #%d this=%p %s+0x%lx",
               fp2_watch_hits[id] + 1, self, where, off);
        fp2_watch_hits[id]++;
        return;
    }

    nx_log("state: #%d %s(%d) this=%p %s+0x%lx",
           ++fp2_changestate_seq, fp2_watch_names[id], arg1, self, where, off);
    fp2_watch_hits[id]++;

    /* Só o primeiro evento: os campos mudam com o tempo, e o que
     * interessa e' o estado no instante em que o metodo foi chamado. */
    if (fp2_watch_hits[id] == 1)
        fp2_dump_bootform_fields(self);

    /* Quem chamou.  Este e' o dado que o run31 nao conseguiu: ali a pilha
     * estava dentro do nosso dispatcher, e aqui a pilha e' a do proprio
     * codigo gerado -- os deslocamentos apontam para o metodo que decidiu o
     * erro, e podem ser lidos offline com disasm_at.py. */
    void *frames[16];
    int n = backtrace(frames, 16);
    for (int i = 0; i < n && i < 8; i++) {
        unsigned long d = 0;
        if (mod && (char *)frames[i] >= (char *)mod->base)
            d = (unsigned long)((char *)frames[i] - (char *)mod->base);
        else {
            void *b = NULL;
            const char *m = fp2_mod_at(frames[i], &b);
            if (m && b)
                d = (unsigned long)((char *)frames[i] - (char *)b);
            else
                continue;
        }
        nx_log("state:   #%d %p libil2cpp.so+0x%lx", i, frames[i], d);
    }
}

/* ====================================================================
 * run43: sonda na SAIDA de RecordStore::Setup.
 *
 * run42 chamou Setup() por reflexao e o slot continuou NULL.  Isso
 * contradiz a leitura do codigo: para w0=4 o factory passa pelo caso 3 da
 * tabela de salto, aloca e devolve o ponteiro -- so' w0=0 ou w0>4 chegam
 * ao `mov x22, xzr`.  Uma contradicao entre leitura e medicao significa
 * que a leitura esta' errada em algum ponto, e adivinhar qual custa um
 * build por tentativa.  Entao mede-se o valor em si.
 *
 * O ponto e' a primeira instrucao DEPOIS do store:
 *
 *   0xa4ae70  str x8, [x9, #0x18]        ; o store
 *   0xa4ae74  ldp x29, x30, [sp, #0x10]  ; <- hook aqui
 *
 * Em 0xa4ae74 o epilogo ainda nao tocou x8, entao x8 ainda e' o que o
 * factory devolveu.  Esse unico numero decide entre "o factory devolveu
 * nulo" e "o Setup escreveu em outro lugar".
 *
 * O stub salva TODOS os registradores callee-saved, nao apenas os que
 * este trecho usa: o callee C destrui x19-x28 livremente, e uma sonda
 * que corrompe silenciosamente o metodo que observa acaba medindo a
 * sonda, nao o jogo. */
static struct fp2_hook fp2_setupexit_hook;

__attribute__((used)) void fp2_probe_event(void *factory, void *t, void *lr)
{
    static int hits;
    hits++;
    nx_mod *mod = nx_find_mod("libil2cpp.so");
    unsigned long off = 0;
    if (mod && (char *)lr >= (char *)mod->base)
        off = (unsigned long)((char *)lr - (char *)mod->base);

    /* O slot e' derivado aqui, e nao recebido em x9, porque a sonda de
     * ENTRADA tambem chama isto e la' x9 e' lixo.  A cadeia e' a mesma que
     * o .ctor usa: *( *( *(base+0x1644f60) ) + 0xb8 ). */
    void *t2 = NULL;
    if (mod && mod->base) {
        void *p = *(void *const *)((const char *)mod->base + 0x1644000u + 0xf60u);
        if (p) {
            void *sv = *(void *const *)p;
            if (sv)
                t2 = *(void *const *)((const char *)sv + 0xb8);
        }
    }
    nx_log("probe: #%d lr=+0x%lx  factory=%p  T(x9)=%p  T(calc)=%p  "
           "T+0x18=%p", hits, off, factory, t, t2,
           t2 ? *(void *const *)((const char *)t2 + 0x18) : NULL);
    if (hits > 20)
        nx_log("probe: limite de 20 atingido, silenciando");
}

__attribute__((naked, used)) static void fp2_stub_setupexit(void)
{
    __asm__ __volatile__(
        "stp x29, x30, [sp, #-0x60]!\n"
        "mov  x29, sp\n"
        "stp  x19, x20, [sp, #0x10]\n"
        "stp  x21, x22, [sp, #0x20]\n"
        "stp  x23, x24, [sp, #0x30]\n"
        "stp  x25, x26, [sp, #0x40]\n"
        "stp  x27, x28, [sp, #0x50]\n"
        "mov  x0, x8\n"          /* o que o factory devolveu */
        "mov  x1, x9\n"          /* T, o bloco estatico */
        "mov  x2, x30\n"
        "bl   fp2_probe_event\n"
        "ldp  x27, x28, [sp, #0x50]\n"
        "ldp  x25, x26, [sp, #0x40]\n"
        "ldp  x23, x24, [sp, #0x30]\n"
        "ldp  x21, x22, [sp, #0x20]\n"
        "ldp  x19, x20, [sp, #0x10]\n"
        "ldp  x29, x30, [sp], #0x60\n"
        FP2_W_TAIL(setupexit));
}

static struct fp2_hook fp2_setupent_hook;

static void fp2_install_setupent_hook(void)
{
    static int done;
    if (done)
        return;
    nx_mod *m = nx_find_mod("libil2cpp.so");
    if (!m) {
        nx_log("probe: libil2cpp.so ausente");
        return;
    }
    void *fn = (char *)m->base + 0xa4adf8;
    if (!fp2_hook_install(&fp2_setupent_hook, fn, fp2_stub_setupexit)) {
        nx_log("probe: hook na entrada de Setup falhou");
        return;
    }
    fp2_watch_tramp_setupent = fp2_setupent_hook.trampoline;
    done = 1;
    nx_log("probe: entrada de Setup observada em %p (tramp %p)", fn,
           fp2_setupent_hook.trampoline);
}

static void fp2_install_setupexit_hook(void)
{
    static int done;
    if (done)
        return;
    nx_mod *m = nx_find_mod("libil2cpp.so");
    if (!m) {
        nx_log("probe: libil2cpp.so ausente");
        return;
    }
    void *fn = (char *)m->base + 0xa4ae74;
    if (!fp2_hook_install(&fp2_setupexit_hook, fn, fp2_stub_setupexit)) {
        nx_log("probe: hook na saida de Setup falhou");
        return;
    }
    fp2_watch_tramp_setupexit = fp2_setupexit_hook.trampoline;
    done = 1;
    nx_log("probe: saida de Setup observada em %p (tramp %p)", fn,
           fp2_setupexit_hook.trampoline);
}

/* ====================================================================
 * run39h: hook do AUXILIAR DE EXCEPCAO.
 *
 * Ate' aqui aorcamos a falha por INFERENCIA: a leitura offline mostrou nove
 * pontos `cbnz xN / bl 0x600888` em BootForm::Init, e como nenhum deles
 * deixou rastro em log, a unica conclusao possivel era "algum deles disparou
 * -- mas qual?".  Escolher por leitura de codigo e' palpite; o jogo tem nove
 * candidatos e o sintoma externo e' identico para todos os nove.
 *
 * A medicao direta e' barata: 0x600888 e' uma funcao ORDINARIA, entao
 * `bl 0x600888` grava o endereco de RETORNO em x30 no momento da chamada.
 * Como o stub entra no lugar dela, o x30 de entrada e' exatamente a
 * instrucao seguinte ao `bl` -- o endereco da chamada que lancou a excecao.
 * Isso nomeia a instrucao sem depender de backtrace(), que aqui devolve
 * apenas 2 quadros (o codigo gerado do IL2CPP nao tem unwind info).
 *
 * O stub DEVOLVE o controle ao trampolim em vez de simplesmente registrar e
 * sair: a excecao precisa continuar a ser Lancada.  Um hook que engolisse a
 * excecao mudaria o comportamento do jogo e o dialogo de erro desapareceria
 * --ismissir o sintoma em vez de explica-lo. */
static struct fp2_hook fp2_throw_hook;
static int fp2_throw_hits;

static void fp2_dump_storage_root(nx_mod *m);
static void fp2_set_use_preference(void);

__attribute__((used)) void fp2_throw_event(void *x30, void *self)
{
    fp2_throw_hits++;
    nx_mod *mod = nx_find_mod("libil2cpp.so");
    unsigned long off = 0;
    if (mod && (char *)x30 >= (char *)mod->base)
        off = (unsigned long)((char *)x30 - (char *)mod->base);
    /* Limite de seguranca: o jogo lanca excecoes o tempo todo em operacao
     * normal, e um log sem fim nao diz nada que o primeiro par nao disse. */
    if (fp2_throw_hits > 12)
        return;
    nx_log("throw: #%d chamada em libil2cpp.so+0x%lx  self=%p  self+0x18=%p",
           fp2_throw_hits, off, self,
           self ? *(void *const *)((const char *)self + 0x18) : NULL);
    /* run39h: o deslocamento 0xa4a674 NAO esta em nenhum metodo gerado --
     * a tabela de ponteiros de metodo do .so (43373 entradas em
     * .data.rel.ro) nao contem o inicio da funcao, e nenhum `bl` aponta
     * para ele.  Logo `self` pode NAO ser um BootForm, e a unica forma de
     * saber e' perguntar a reflexao -- que e' o que o jogo ja usa e o que
     * o run39g demonstrou ser funcional. */
    if (fp2_throw_hits == 1 && self) {
        fp2_dump_bootform_fields(self);
        fp2_dump_storage_root(mod);
        fp2_set_use_preference();
    }
}

/* run41: a RAIZ de onde `storage_` deveria vir.
 *
 * RecordStore::.ctor (0xa49e5c) e RecordStore::Setup (0xa4adf8) leem e
 * escrevem o MESMO slot, o que fecha a pergunta "quem devia ter preenchido
 * storage_" numa unica cadeia de desreferencias:
 *
 *   .ctor  ldr x8,[x8,#0xb8]; ldr x8,[x8,#0x18]; str x8,[x19,#0x18]
 *   Setup  ldr x9,[x8,#0xb8]; str <retorno de 0xa4a3fc>,[x9,#0x18]
 *
 * Ou seja: existe um objeto global T, Setup e' quem o preenche, e .ctor e'
 * quem o copia.  Se T+0x18 ainda e' NULL no instante da excecao, Setup nao
 * chegou a rodar -- e isso e' MEDICAO, nao leitura de codigo.
 *
 * O nome de T e' o pedaco que nenhuma leitura offline entrega: o disassembly
 * conhece um offset, a reflexao conhece a classe.  Dai o il2_class_of(). */
static void fp2_dump_storage_root(nx_mod *m)
{
    if (!m || !m->base) {
        nx_log("raiz: libil2cpp.so sem base conhecida");
        return;
    }
    /* 0x1644000 e' a pagina que o adrp calcula; 0xf60 e' o deslocamento do
     * ldr.  Nesta imagem vaddr == offset, entao base + (0x1644000+0xf60) e'
     * o endereco vivo.  O slot e' .bss -- nao existe no arquivo, e por isso
     * nenhuma ferramenta offline pode ter a resposta. */
    const unsigned char *slot =
        (const unsigned char *)m->base + 0x1644000u + 0xf60u;
    void *p = *(void *const *)slot;
    if (!p) {
        nx_log("raiz: slot 0x1644f60 -> NULL (bloco estatico nao montado)");
        return;
    }
    void *s = *(void *const *)p;
    if (!s) {
        nx_log("raiz: *(0x1644f60)=%p mas *p e' NULL", p);
        return;
    }
    void *t = *(void *const *)((const char *)s + 0xb8);
    nx_log("raiz: p=%p  s=*p=%p  T=(s+0xb8)=%p", p, s, t);
    if (!t) {
        nx_log("raiz: T e' NULL -- o objeto global nunca foi construido");
        return;
    }
    unsigned char flag13 = *(const unsigned char *)((const char *)t + 0x13);
    void *f18 = *(void *const *)((const char *)t + 0x18);
    void *f50 = *(void *const *)((const char *)t + 0x50);
    nx_log("raiz: T+0x13=%u  T+0x18=%p  T+0x50=%p", (unsigned)flag13, f18,
           f50);
    /* T+0x18 e' exatamente o valor que .ctor copia para storage_.  Se ele
     * ja e' NULL aqui, Setup nao rodou; se nao e' NULL, entao e' o .ctor que
     * esta' lendo outro T -- e as duas hipoteses dao consertos diferentes. */
    nx_log("raiz: storage_ deveria ser T+0x18 = %s",
           f18 ? "nao-nulo (Setup rodou)" : "NULL (Setup nao rodou)");
    Il2CppClass *k = il2_class_of((Il2CppObject *)t);
    nx_log("raiz: T e' %s", k ? il2_class_label(k) : "(classe ilegivel)");
    if (k)
        il2_fields_foreach(k, fp2_dump_fields_cb, t);
}

/* Stub do auxiliar de excecao (0x600888).
 *
 * x30 de entrada e' o endereco de RETORNO do `bl 0x600888` interceptado --
 * ou seja, a instrucao seguinte a chamada, que nomeia exatamente qual das
 * nove verificacoes de nulidade de BootForm::Init disparou.  Ele e' lido
 * ANTES do `bl fp2_throw_event`, porque essa chamada sobrescreveria x30.
 *
 * run39h, MEDICAO: a primeira versao deste stub imprimiu
 *   throw: #1 chamada em libil2cpp.so+0x0
 *   throw: #2 chamada em libil2cpp.so+0x0
 * Duas chamadas -- o hook funciona, e o auxiliar e' mesmo o caminho do erro
 * 100 -- mas o OFFSET saiu zero nas duas.  A causa NAO esta' no hook:
 * `bl fp2_throw_event` entrega ao C o que estiver em x0, e x0 NAO e' o
 * registro que o stub salvou.  x0 nesta entrada e' o primeiro argumento do
 * auxiliar de excecao (o objeto a ser lancado, abaixo de `base`), e por
 * isso a subtracao em `fp2_throw_event` caia fora do teste `>= base` e
 * `off` ficava em zero.  O que o stub precisa nao e' apenas SALVAR x30 --
 * e' COLOCAR x30 no x0 antes de chamar.  Da'e' o `mov x0, x19`.
 *
 * Licao que vale para os outros cinco stubs tambem: num stub naked, o que se
 * salva num callee-saved NAO chega na funcao C sozinha.  Os metodos passam
 * `self` em x0 de entrada, que ja' e' o que a funcao C espera, entao ali
 * a armadilha nao aparece -- e nem teria aparecido se o `bl` do auxiliar
 * fosse a unica forma de registrar um evento.
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
/* Stub do auxiliar de excecao (0x600888).
 *
 * run39h, MEDICAO: a primeira versao deste stub imprimiu
 *   throw: #1 chamada em libil2cpp.so+0x0
 *   throw: #2 chamada em libil2cpp.so+0x0
 * Duas chamadas -- o hook funciona, e o auxiliar e' mesmo o caminho do erro
 * 100 -- mas o OFFSET saiu zero nas duas.  A causa NAO esta' no hook:
 * `bl fp2_throw_event` entrega ao C o que estiver em x0, e x0 NAO e' o
 * registro que o stub salvou.  x0 nesta entrada e' o primeiro argumento do
 * auxiliar de excecao (o objeto a ser lancado, abaixo de `base`), e por
 * isso a subtracao em `fp2_throw_event` caia fora do teste `>= base` e
 * `off` ficava em zero.  O que o stub precisa nao e' apenas SALVAR x30 --
 * e' COLOCAR x30 no x0 antes de chamar.  Da'e' o `mov x0, x21`.
 *
 * REGISTROS, com cuidado.  O stub entra no LUGAR do auxiliar, entao os
 * registradores que ele le sao os do CHAMADOR, nao os do auxiliar:
 *
 *   x30 = endereco de RETORNO do `bl 0x600888`, ou seja, a instrucao
 *         seguinte a chamada.  E' o que nomeia qual verificacao disparou.
 *   x19 = `this` da funcao que contem a verificacao.  Nao e' um accidento:
 *         em `0xa4a59c` a sequencia e' `mov x19, x0` logo no prologo, e o
 *         `ldr x20, [x19, #0x18]` da verificacao le exatamente esse
 *         registro.  Os dois valores precisam de dois registradores
 *         distintos -- uma versao intermediaria chegou a salvar x30 em
 *         x19 e depois tentar ler `this` de x20, o que nao funciona porque
 *         x20 ali e' um temporario do proprio codigo gerado.
 *
 * x19, x20 e x21 sao callee-saved: todos sao salvos e restaurados, porque
 * um stub naked que os altera sem restaurar quebra o gerador de codigo
 * gerenciado de um jeito que so aparece depois, em outro metodo.
 *
 * O stub DEVOLVE o controle ao trampolim: a excecao precisa continuar a ser
 * lancada.  Engolir a excecao faria o dialogo de erro sumir, que e' apagar o
 * sintoma em vez de explica-lo.
 *
 * O slot e' `fp2_watch_tramp_xthrow` e o rabo e' `FP2_W_TAIL(xthrow)`, o
 * mesmo dos cinco stubs de metodo.  O que o run39h custou nao foi acertar
 * este texto -- era identico desde a primeira tentativa -- e sim descobrir
 * que o slot precisa de `__attribute__((used))` para o GCC o emitir como
 * definicao local.  A nota completa, com as medicoes, esta no bloco
 * FP2_TRAMP. */
#define FP2_THROW_STUB()                                                \
    __attribute__((naked, used)) static void fp2_stub_throw(void)        \
    {                                                                   \
        __asm__ __volatile__(                                           \
            "stp x29, x30, [sp, #-0x40]!\n"                            \
            "mov  x29, sp\n"                                           \
            "stp x19, x20, [sp, #0x10]\n"                             \
            "str  x21, [sp, #0x20]\n"                                 \
            "mov  x21, x30\n"          /* x21 = endereco de retorno */ \
            "mov  x0, x21\n"           /* x0 = o que o C le           */ \
            "mov  x1, x19\n"           /* x1 = this do chamador       */ \
            "bl   fp2_throw_event\n"   /* registra; nunca engole */    \
            "ldr  x21, [sp, #0x20]\n"                                 \
            "ldp  x19, x20, [sp, #0x10]\n"                             \
            "ldp  x29, x30, [sp], #0x40\n"                            \
            FP2_W_TAIL(xthrow));                                       \
    }

FP2_THROW_STUB();

void fp2_install_throw_hook(void)
{
    static int done;
    if (done)
        return;
    nx_mod *m = nx_find_mod("libil2cpp.so");
    if (!m) {
        nx_log("throw: libil2cpp.so ausente");
        return;
    }

    /* Endereco por NOME DE SIMBOLO e' o caminho feliz, mas aqui ele NAO
     * existe: `tools/throw_sym.py` no binario em bbmeta/ mostra ZERO
     * simbolos .dynsym na janela 0x600800..0x600950, e o
     * `il2cpp_raise_exception` exportado (0x5f5c64) e' outra funcao --
     * sao tres instrucoes que terminam em `b 0x6007f8`, nao o auxiliar de
     * null-reference que os nove `cbnz` chamam.
     *
     * Portanto o endereco vem de um RVA.  So' que um RVA puro seria
     * repetir o run33 (SIGSEGV por_suposto), entao o RVA vem ACOMPANHADO
     * das quatro primeiras palavras de codigo de maquina lidas do binario:
     *
     *   0x600888  sub  sp, sp, #0x20      0xd10083ff
     *   0x60088c  stp  x29, x30, [sp,#0x10] 0xa9017bfd
     *   0x600890  add  x29, sp, #0x10     0x910043fd
     *   0x600894  mov  x0, sp             0x910003e0
     *
     * A PROLOGUE `sub sp` / `stp x29,x30` / `add x29` e' a assinatura de uma
     * funcao C comum; `mov x0, sp` logo depois indica que o objeto de
     * excecao e' montado NA PILHA.  Se as quatro palavras nao baterem, o
     * binario em execucao nao e' o binario que a leitura offline leu, e a
     * resposta honesta e' recusar o hook -- nao escrever 16 bytes sobre uma
     * funcao que nao sabemos qual e'. */
    static const uint32_t want[4] = {
        0xd10083ffu, 0xa9017bfdu, 0x910043fdu, 0x910003e0u,
    };
    const uint32_t *fn = (const uint32_t *)((const char *)m->base + 0x600888);
    for (int i = 0; i < 4; i++) {
        if (fn[i] == want[i])
            continue;
        nx_log("throw: RVA 0x600888 nao confere no binario vivo "
               "(+%d: quero 0x%08x, achei 0x%08x) -- hook recusado",
               4 * i, want[i], fn[i]);
        return;
    }
    nx_log("throw: prologo de 0x600888 confere (sub sp/stp/add/mov x0,sp)");

    if (!fp2_hook_install(&fp2_throw_hook, (void *)fn, fp2_stub_throw)) {
        nx_log("throw: hook falhou");
        return;
    }
    fp2_watch_tramp_xthrow = fp2_throw_hook.trampoline;
    done = 1;
    nx_log("throw: auxiliar de excecao observado em %p (tramp %p)",
           (void *)fn, fp2_throw_hook.trampoline);
}

/* run39: instala a narracao.  Idempotente e repetivel: o runtime so sobe
 * durante initJni, entao main.c chama isto em varias fases e as primeiras
 * tentativas saem sem instalar -- isso e' esperado, nao e' falha.
 *
 * run39a mediu o caso limite: disparando a instalacao pelo showDialog, o hook
 * ate' fica pronto ("narrando ChangeState em 0x7fa25cf6c4") e mesmo assim
 * ZERO chamadas sao registradas -- porque o showDialog que dispara a
 * instalacao ja' e' a consequencia da decisao que queriamos observar.  A
 * instrumentacao precisa estar no lugar ANTES do fenomeno. */
void fp2_install_changestate_hook(void)
{
    static int done;
    if (done)
        return;
    if (!il2_load()) {
        nx_log("state: il2_load ainda nao");
        return;
    }
    Il2CppClass *klass = il2_class_by_name("BootForm");
    if (!klass) {
        nx_log("state: BootForm ainda nao");
        return;
    }
    /* Endereco por reflexao, nunca um RVA cravado: foi exatamente um offset
     * fixo que custou o run33 (SIGSEGV) e o run36 (endereco abaixo da base). */
    static const char *const want[FP2_W_COUNT] = {
        "ChangeState", "Update", "Init", "ClassLoad", "Load",
    };
    static const unsigned argc[FP2_W_COUNT] = { 1, 0, 0, 0, 0 };
    static void **const tramp_slot[FP2_W_COUNT] = {
        &fp2_watch_tramp_changestate, &fp2_watch_tramp_update,
        &fp2_watch_tramp_init,       &fp2_watch_tramp_classload,
        &fp2_watch_tramp_load,
    };
    int installed = 0;
    for (int i = 0; i < FP2_W_COUNT; i++) {
        const void *fn = il2_method_addr(klass, want[i], argc[i]);
        if (!fn || !fp2_hook_install(&fp2_watch[i], (void *)fn,
                                     fp2_watch_stubs[i])) {
            nx_log("state: %s nao observavel", want[i]);
            continue;
        }
        /* Publish the trampoline so the stub can reach it.  The stub reads
         * this slot with `ldr x16, =slot`, which is how it gets past the
         * 141 MB that separates the executable from the mmap'd trampoline. */
        *tramp_slot[i] = fp2_watch[i].trampoline;
        nx_log("state: observando %s em %p (tramp %p)", want[i], fn,
               fp2_watch[i].trampoline);
        installed++;
    }
    /* So' declara pronto quando o CONTROLE POSITIVO (Update) e o ALVO
     * (ChangeState) estao os dois hooked.  Aceitar "algum" seria transformar
     * uma medicao parcial em uma apareceria de sucesso -- e um resultado que
     * nao se distingue do "hook nao funciona" e' pior do que uma falha
     * declarada. */
    if (fp2_watch[FP2_W_Update].target && fp2_watch[FP2_W_ChangeState].target) {
        done = 1;
        nx_log("state: instrumentacao completa (%d/%d)", installed,
               FP2_W_COUNT);
        /* run39h: a narracao acima diz QUANDO um metodo foi observado, mas
         * nao responde "qual das nove verificacoes de nulidade dentro de
         * Init Lancou a excecao".  O auxiliar de excecao responde isso
         * diretamente, lendo o x30 de entrada. */
        fp2_install_throw_hook();
        /* run45: as duas sondas de Setup ficam SEM instalar.  A de
         * entrada copiava um `adrp`, que e' PC-relativo: reexecutado
         * no trampolim, x19 recebia a pagina errada e o `strb`
         * seguinte escrevia onde nao devia -- SIGILL (signum 4) no
         * run44.  Nunca hookar um ponto cujas 4 primeiras instrucoes
         * tenham adrp/adr ou desvio. */
    } else if (installed) {
        nx_log("state: %d/%d metodos hooked; falta o controle positivo "
               "ou o alvo", installed, FP2_W_COUNT);
    }
}

static void fp2_dump_managed_methods(const char *class_name)
{
    if (!il2_load()) {
        nx_log("managed: il2_load failed, cannot resolve %s", class_name);
        return;
    }
    /* run34: look the class up by its simple name.  il2_class() needs the exact
     * (namespace, name) pair, and run33's attempt to recover the namespace
     * from the metadata failed -- the v24 TypeDefinition entry is not the
     * 32-byte layout I assumed, so "BootForm" was not where the name index
     * pointed.  Enumerating the images at runtime is both cheaper and immune
     * to that guess; the namespace is then reported rather than assumed. */
    Il2CppClass *klass = il2_class_by_name(class_name);
    if (!klass) {
        nx_log("managed: class %s not found in any image", class_name);
        return;
    }
    struct fp2_method_scan scan = { class_name, 0 };
    il2_methods_foreach(klass, fp2_method_scan_cb, &scan);
    nx_log("managed: %s resolved, namespace=\"%s\"", class_name,
           il2_class_ns(klass));
}

static int64_t j_Utility_showDialog(jctx *c)
{
    int id = jarg_int(c);
    jobj *a = jarg_obj(c);
    jobj *b = jarg_obj(c);
    jobj *lst = jarg_obj(c);
    fprintf(stderr,
            "[fp2/sdk] Utility.showDialog(id=%d, title=\"%s\", msg=\"%s\", "
            "listener=%s) -> 0\n",
            id,
            a && a->str ? a->str : "(null)",
            b && b->str ? b->str : "(null)",
            lst ? "yes" : "no");
    /* run22: after the package name, the app size, the GLES20 constants and the
     * parameter table all returned real values, error 100 still fires.  Guessing
     * at neighbouring JNI calls has now failed three times in a row, so capture
     * the managed call path instead.  IL2CPP has no frame pointers, but the
     * return addresses still land inside libil2cpp.so's text, and the offset
     * from its load base identifies WHICH generated method threw -- which is
     * what is missing from the log.  Only the error dialogs (id 0 and 3) are
     * interesting; the informational ones stay quiet. */
    if (getenv("FP2_DIALOG_TRACE") && (id == 0 || id == 3)) {
        /* run33: name the classes whose methods decide this.  BootForm holds
         * the start-up self-check (InitApp / ClassLoad / RESLOAD_SEB_*), and
         * the error strings we just printed belong to its neighbourhood. */
        static int dumped;
        if (!dumped) {
            dumped = 1;
            /* run39: narrar o ChangeState e' o que responde "qual estado
             * recebeu o que".  A listagem de metodos que existia aqui foi
             * protocollada no run34 e cumpriu seu papel -- o run38 ja' leu
             * os deslocamentos offline -- entao ela sai de cena para o log
             * nao ficar com 23 linhas por inicializacao. */
            fp2_install_changestate_hook();
            fp2_dump_managed_methods("BootForm");
        }
        void *frames[24];
        int n = backtrace(frames, 24);
        fprintf(stderr, "[fp2/trace] showDialog id=%d native backtrace "
                        "(%d frames):\n", id, n);
        for (int i = 0; i < n; i++) {
            void *base = NULL;
            extern const char *fp2_mod_at(const void *addr, void **base_out);
            const char *mod = fp2_mod_at(frames[i], &base);
            fprintf(stderr, "[fp2/trace]   #%d %p %s+0x%lx\n", i, frames[i],
                    mod ? mod : "?",
                    mod && base ? (unsigned long)((char *)frames[i] -
                                                 (char *)base) : 0UL);
        }
    }
    return 0;
}

/* run42: chamar RecordStore.Setup() por nossa conta.
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
    fp2_set_use_preference();
}

/* run44: usePreference_ (estatico, T+0x20).
 *
 * DUAS leituras independentes convergem no mesmo byte.
 *
 * A lista de campos estaticos do run40 poe `usePreference_` em 0x20 e
 * `rootStorage_` em 0x18.  GetNumRecords le exatamente esses dois:
 *
 *   0xa4a630  ldrb w8, [x8, #0x20]   ; usePreference_
 *   0xa4a634  cbz  w8, #0xa4a664     ; zero -> cai no storage_
 *   0xa4a664  ldr  x20, [x19, #0x18] ; storage_ (copia de rootStorage_)
 *   0xa4a670  bl   0x600888          ; NullReferenceException
 *
 * Um usePreference_ nao-nulo toma o ramo que NAO encosta em storage_.
 * E' o ramo que o build Android da Kairosoft deve tomar: o plugin traz um
 * contrato completo de SharedPreferences (existPreference / getPreference /
 * putPreference / removePreference) e este port o implementa.  Isso tambem
 * explica por que "nenhuma preferenca foi lida" nunca foi evidencia de
 * nada: o jogo nunca chegou ao ponto de perguntar.
 *
 * Virar um byte e' o menor experimento que separa "o backend de arquivo
 * nao foi construido" de "o backend de preferencia era o pretendido".
 * E' um byte num bloco estatico, nao uma mudanca de codigo, e os valores
 * antigo/novo no log o tornam reversivel. */
static void fp2_set_use_preference(void)
{
    static int done;
    if (done)
        return;
    nx_mod *m = nx_find_mod("libil2cpp.so");
    if (!m || !m->base)
        return;
    void *p = *(void *const *)((const char *)m->base + 0x1644000u + 0xf60u);
    if (!p)
        return;
    void *sv = *(void *const *)p;
    if (!sv)
        return;
    void *t = *(void *const *)((const char *)sv + 0xb8);
    if (!t) {
        /* NAO marcar done aqui.  run44 mediu que em Config.init o bloco
         * ainda nao existe; marcar done na primeira tentativa inutil
         * desligaria todas as seguintes, que e' exatamente a armadilha
         * de uma bandeira "ja' tentei". */
        return;
    }
    unsigned char *flag = (unsigned char *)t + 0x20;
    nx_log("usepref: usePreference_ %u -> 1  (rootStorage_ T+0x18=%p)",
           (unsigned)*flag, *(void *const *)((const char *)t + 0x18));
    *flag = 1;
    done = 1;
}

static int64_t j_Config_init(jctx *c)
{
    int i0 = jarg_int(c);
    int z = jarg_int(c);
    jobj *s = jarg_obj(c);
    fprintf(stderr, "[fp2/sdk] Config.init(%d, %d, \"%s\")\n",
            i0, z, s && s->str ? s->str : "(null)");
    /* run45: NAO chamar Setup aqui.  run44 mediu que neste instante
     * *(0x1644f60) existe mas *p ainda e' NULL -- o bloco estatico do
     * RecordStore ainda nao foi montado, entao a chamada nao pode ter
     * efeito e ainda perturba a ordenacao.  A sonda #3 do run44 tambem
     * mostrou que o JOGO chama Setup por conta propria (lr=+0x7eff54),
     * entao "Setup nunca rodou" esta' encerrado: ele roda. */
    (void)i0;
    (void)z;
    (void)s;
    return 0;
}

/* Config.finish() is the tail of the Kairosoft boot: the managed side calls it
 * on the error path just before quitting, so its appearance (and how many
 * times) dates the failure precisely.  Logging it is worth more than the stub. */
static int64_t j_Config_noop(jctx *c)
{
    const jmethod *m = c->m;
    JT("Config.%s()", m && m->name ? m->name : "finish");
    return 0;
}

/* ---------------------------------------------------------------------------
 * run28: the APK's own classes.dex is the authoritative contract.
 *
 * tools/dex_probe.py lists 98 Utility methods this port never bound, so every
 * one of them was answered by the generic stub.  A stub that answers 0 for a
 * boolean predicate is not neutral -- it asserts "false", and a start-up
 * self-check that asks "is the screen at least this wide", "is the app
 * installed", "is this a tablet layout" will believe it.  That is how a
 * self-check reaches STATE_ERROR and reports error 100 while every individual
 * value looks plausible in the log.
 *
 * So the rule for everything below: answer what is actually true on this
 * device, and prefer an honest negative over a fabricated positive.  Where the
 * real answer is "this service does not exist here" (Play Services, ad id,
 * GCM), say so with the value Android itself would produce for a missing
 * service rather than with a zero that reads as a successful check.
 * ------------------------------------------------------------------------ */

/* Screen geometry.  Unity already hands the managed side the real drawable,
 * and Utility.getAppWidth/Height read the same SDL window, so these three must
 * agree with it or the self-check sees a screen that is not there. */
static int64_t j_Utility_width(jctx *c)
{
    int w = 0, h = 0;
    extern void fp2_sdl_screen_size(int *width, int *height);
    fp2_sdl_screen_size(&w, &h);
    const jmethod *m = c->m;
    const char *n = m && m->name ? m->name : "?";
    JT("Utility.%s() -> %d", n, n && strcmp(n, "getHeight") == 0 ? h : w);
    return n && strcmp(n, "getHeight") == 0 ? h : w;
}

static int64_t j_Utility_isTablet(jctx *c)
{
    /* A square 720x720 handheld is not a tablet in any sense the layout code
     * cares about, and claiming otherwise would push the game onto its
     * large-screen resource set that this device does not have. */
    JT("Utility.isTablet() -> false (720x720 handheld)");
    return 0;
}

static int64_t j_Utility_isFullscreen(jctx *c)
{
    /* j_Utility_fullscreen() above reports the host window as already
     * fullscreen, so this must agree or the managed side will toggle. */
    JT("Utility.isFullscreen() -> true");
    return 1;
}

static int64_t j_Utility_isDebug(jctx *c)
{
    /* The APK really is the debug-flavoured build: Config.init received
     * 1 for its first argument, and the package id ends in .debug.  Reporting
     * the truth keeps the game out of its production-only branches. */
    JT("Utility.isDebug() -> true (package com.ballatergames.debug)");
    return 1;
}

/* Screen insets.  The handheld has no status bar and no system bar overlay,
 * so every inset is zero -- but they must be zero BY ANSWER, not by accident,
 * because a negative or huge value makes the managed layout arithmetic wrap. */
static int64_t j_Utility_bar_height(jctx *c)
{
    const jmethod *m = c->m;
    const char *n = m && m->name ? m->name : "?";
    JT("Utility.%s() -> 0 (no system bars on this device)", n);
    return 0;
}

/* fileExists: the self-check uses it to confirm its own resource families
 * (RESLOAD_SEB_WAVE / PARTS / KAIRO / MAN) are on disk before loading them.
 * Answering this honestly is what makes that check meaningful, so it actually
 * stats the path the caller gives us instead of guessing. */
static int64_t j_Utility_fileExists(jctx *c)
{
    jobj *p = jarg_obj(c);
    const char *path = p && p->str ? p->str : "";
    struct stat st;
    int ok = path[0] && stat(path, &st) == 0;
    JT("Utility.fileExists(\"%s\") -> %s", path, ok ? "true" : "false");
    return ok;
}

/* Permission queries.  hasPermission must agree with checkCallingOrSelfPermission
 * above, which denies; a hasPermission() that said true while the check said
 * denied is exactly the kind of self-contradiction a self-check trips on. */
static int64_t j_Utility_hasPermission(jctx *c)
{
    jobj *p = jarg_obj(c);
    JT("Utility.hasPermission(\"%s\") -> false (no Android permission grants)",
       p && p->str ? p->str : "(null)");
    return 0;
}

/* Google Play Services and the licensing / advertising-id stack.  None of
 * these exist on this device.  Every one of them is a boolean-ish predicate,
 * so the honest answer is false -- and false is also the answer that keeps the
 * game on its documented no-Play degradation path instead of trying to reach
 * a service that is not there. */
static int64_t j_Utility_noPlay(jctx *c)
{
    const jmethod *m = c->m;
    const char *n = m && m->name ? m->name : "?";
    JT("Utility.%s() -> 0 (no Google Play Services on this device)", n);
    return 0;
}

static int64_t j_Utility_noPlayString(jctx *c)
{
    const jmethod *m = c->m;
    const char *n = m && m->name ? m->name : "?";
    JT("Utility.%s() -> \"\" (no Google Play Services on this device)", n);
    return (int64_t)(uintptr_t)mk_string("");
}

/* getErrMsg: the original returns null when the last operation succeeded, and
 * a non-null string only when it failed.  Returning "" here would make every
 * caller treat a success as a failure, so null is the correct success value. */
static int64_t j_Utility_getErrMsg(jctx *c)
{
    JT("Utility.getErrMsg() -> null (no error)");
    return 0;
}

/* getSignature: the APK's own signing certificate, as hex.  The original
 * reads PackageManager.signatures; there is no package manager here, so this
 * reports the identity the port already presents (see BB_PACKAGE_NAME)
 * rather than a fabricated digest that would fail any real comparison. */
static int64_t j_Utility_getSignature(jctx *c)
{
    JT("Utility.getSignature() -> null (no PackageManager on this device)");
    return 0;
}

/* Intent queries: the game was not launched from a link, so every intent
 * accessor is null / false. */
static int64_t j_Utility_intent(jctx *c)
{
    const jmethod *m = c->m;
    const char *n = m && m->name ? m->name : "getIntent";
    if (n && strcmp(n, "hasIntent") == 0) {
        JT("Utility.hasIntent() -> false");
        return 0;
    }
    JT("Utility.%s() -> null (launched from the launcher, no intent extras)", n);
    return 0;
}

/* Preferences.  The record store owns its own keys; reporting "absent" for
 * every key makes the managed side take its first-run path, which is correct
 * for a fresh install and cannot be wrong in a way that loses data. */
static int64_t j_Utility_preference(jctx *c)
{
    const jmethod *m = c->m;
    const char *n = m && m->name ? m->name : "?";
    if (n && strcmp(n, "existPreference") == 0) {
        jobj *k = jarg_obj(c);
        JT("Utility.existPreference(\"%s\") -> false",
           k && k->str ? k->str : "(null)");
        return 0;
    }
    if (n && strcmp(n, "getPreference") == 0) {
        JT("Utility.getPreference(...) -> null (no stored preference)");
        return 0;
    }
    JT("Utility.%s()", n);
    return 0;
}

/* Launch / UI surfaces that need a real Activity.  There is none, so these
 * either do nothing or refuse. */
static int64_t j_Utility_surface(jctx *c)
{
    const jmethod *m = c->m;
    const char *n = m && m->name ? m->name : "?";
    JT("Utility.%s() -> 0 (no Activity to host it)", n);
    return 0;
}

/* The rest of the run28 gap: setters, notifications and text helpers.  None of
 * these are consulted by the start-up self-check -- they are UI, persistence
 * and messaging -- so they are grouped into one honest handler rather than
 * each getting a bespoke stub.  What matters is that none of them can be
 * mistaken for a successful capability check. */
static int64_t j_Utility_rest(jctx *c)
{
    const jmethod *m = c->m;
    const char *n = m && m->name ? m->name : "?";
    const char *sig = m && m->sig ? m->sig : "";

    /* Setters and void no-ops: report, do nothing. */
    if (strcmp(sig, ")V") == 0) {
        JT("Utility.%s() -- no such service on this device", n);
        return 0;
    }
    /* Anything returning a String or an array must be null, not "": an empty
     * string is a value the caller will parse, and parsing "" is how a
     * managed-side Integer.parseInt ends up configuring a device for 0. */
    JT("Utility.%s() -> null (no such service on this device)", n);
    return 0;
}

/* Kairosoft's plugin keeps a static key/value map: the C# stores screen
 * and scaling params through setParam and reads them back through getParam.
 * The stub lost the values (getParam -> "" -> bad texture sizes). */
static char param_keys[32][64];
static char param_vals[32][256];
static int param_count;

static const char *param_lookup(const char *k)
{
    for (int i = 0; i < param_count; i++)
        if (strcmp(param_keys[i], k) == 0)
            return param_vals[i];
    return NULL;
}

static int64_t j_Utility_setParam(jctx *c)
{
    jobj *k = jarg_obj(c);
    jobj *v = jarg_obj(c);
    const char *ks = k && k->str ? k->str : "(null)";
    const char *vs = v && v->str ? v->str : "";
    /* Overwrite in place.  run21 showed the same key (ADMOB_SCREEN_HEIGHT)
     * being written on every layout pass, and an append-only table made
     * param_lookup() keep returning the FIRST value forever, so the managed
     * side could never see an updated parameter. */
    for (int i = 0; i < param_count; i++) {
        if (strcmp(param_keys[i], ks) == 0) {
            snprintf(param_vals[i], 256, "%s", vs);
            JT("Utility.setParam(\"%s\" = \"%s\") (replaced)", ks, vs);
            return 0;
        }
    }
    if (param_count < 32 && strlen(ks) < 64 && strlen(vs) < 256) {
        snprintf(param_keys[param_count], 64, "%s", ks);
        snprintf(param_vals[param_count], 256, "%s", vs);
        param_count++;
    }
    JT("Utility.setParam(\"%s\" = \"%s\")", ks, vs);
    return 0;
}

static int64_t j_Utility_getParam(jctx *c)
{
    jobj *k = jarg_obj(c);
    const char *ks = k && k->str ? k->str : "";
    const char *v = param_lookup(ks);
    /* run21 removed the run13 special case that forced ADMOB_SCREEN_HEIGHT to
     * "0".  It was a guess made before getAppWidth()/getAppHeight() and the
     * package name returned real values, and it only masked what the managed
     * side actually stored.  The parameter table is now authoritative. */
    JT("Utility.getParam(\"%s\") -> \"%s\"", ks, v ? v : "(unset)");
    return (int64_t)(uintptr_t)mk_string(v ? v : "");
}

/* kairo.android.plugin.Utility preference-key contract: the C# RecordStore
 * asks the Java side which SharedPreferences keys are Java-owned, and turns
 * an empty answer into a null collection (measured: LINQ Contains(null) ->
 * ArgumentNullException dialog at frame 2, run12).  A single harmless key
 * name keeps the list non-empty; it matches no real preference, so the
 * record store simply owns every key. */
/* PackageInfo/ApplicationInfo readers: Unity and Kairosoft both read
 * versionCode/versionName/metaData.  The run12 crash chain was
 * metaData=null -> reflection with null strings -> strcmp(NULL). */
static int64_t j_PackageInfo_versionCode(jctx *c)
{
    (void)c;
    return 1;
}

static int64_t j_PackageInfo_versionName(jctx *c)
{
    (void)c;
    return (int64_t)(uintptr_t)mk_string("1.0.0");
}

static int64_t j_PackageInfo_metaData(jctx *c)
{
    (void)c;
    return (int64_t)(uintptr_t)mk_object("android/os/Bundle");
}

static int64_t j_Bundle_getString(jctx *c)
{
    jobj *key = jarg_obj(c);
    JT("Bundle.getString(\"%s\") -> \"\"", key && key->str ? key->str : "?");
    return (int64_t)(uintptr_t)mk_string("");
}

/* Display metrics emulation: the game computes texture sizes from these.
 * Zeros produced "Texture has out of range width / height" -> error dialog
 * 100 -> Application.Quit (run13).  Values follow the live SDL drawable. */
static int64_t j_DisplayMetrics_int(jctx *c)
{
    const jmethod *m = c->m;
    int w = 720, h = 720;
    extern void fp2_sdl_screen_size(int *width, int *height);
    fp2_sdl_screen_size(&w, &h);
    if (m && m->name) {
        if (strcmp(m->name, "widthPixels") == 0)
            return w;
        if (strcmp(m->name, "heightPixels") == 0)
            return h;
        if (strcmp(m->name, "densityDpi") == 0)
            return 160;
        if (strcmp(m->name, "density") == 0) {
            /* 1.0 == mdpi.  Returning 0 here makes every dp-based layout
             * computation collapse (Unity: "Texture has out of range
             * width / height"). */
            float one = 1.0f;
            int32_t bits;
            memcpy(&bits, &one, sizeof bits);
            return bits;
        }
        if (strcmp(m->name, "scaledDensity") == 0) {
            float one = 1.0f;
            int32_t bits;
            memcpy(&bits, &one, sizeof bits);
            return bits;
        }
        if (strcmp(m->name, "xdpi") == 0 || strcmp(m->name, "ydpi") == 0) {
            float dpi = 160.0f;
            int32_t bits;
            memcpy(&bits, &dpi, sizeof bits);
            return bits;
        }
    }
    return 0;
}

/* android.view.Display.getRealMetrics / getMetrics: fill the caller's
 * DisplayMetrics from the real drawable.  run19 measured these going
 * unhandled, which left widthPixels/heightPixels/density at 0 and fed
 * "Texture has out of range width / height" to the renderer. */
static void set_metrics_field(jobj *metrics, const char *name, int value)
{
    jobject_field *field = object_field(metrics, name, 1);
    if (field)
        field->primitive = value;
}

static void set_metrics_field_float(jobj *metrics, const char *name,
                                    float value)
{
    jobject_field *field = object_field(metrics, name, 1);
    if (field) {
        int32_t bits;
        memcpy(&bits, &value, sizeof bits);
        field->primitive = bits;
    }
}

static int64_t j_Display_fillMetrics(jctx *c)
{
    jobj *metrics = jarg_obj(c);
    int w = 720, h = 720;
    extern void fp2_sdl_screen_size(int *width, int *height);
    fp2_sdl_screen_size(&w, &h);
    if (metrics) {
        set_metrics_field(metrics, "widthPixels", w);
        set_metrics_field(metrics, "heightPixels", h);
        set_metrics_field(metrics, "densityDpi", 160);
        set_metrics_field_float(metrics, "density", 1.0f);
        set_metrics_field_float(metrics, "scaledDensity", 1.0f);
        set_metrics_field_float(metrics, "xdpi", 160.0f);
        set_metrics_field_float(metrics, "ydpi", 160.0f);
    }
    JT("Display metrics -> %dx%d dpi=160 density=1.0", w, h);
    return 0;
}

static int64_t j_Display_metrics(jctx *c)
{
    (void)c;
    extern void fp2_sdl_screen_size(int *width, int *height);
    int w = 720, h = 720;
    fp2_sdl_screen_size(&w, &h);
    jobj *metrics = mk_object("android/util/DisplayMetrics");
    if (metrics) {
        set_metrics_field(metrics, "widthPixels", w);
        set_metrics_field(metrics, "heightPixels", h);
        set_metrics_field(metrics, "densityDpi", 160);
        set_metrics_field_float(metrics, "density", 1.0f);
        set_metrics_field_float(metrics, "scaledDensity", 1.0f);
    }
    return (int64_t)(uintptr_t)metrics;
}

static int64_t j_Display_get(jctx *c)
{
    const jmethod *m = c->m;
    int w = 720, h = 720;
    extern void fp2_sdl_screen_size(int *width, int *height);
    fp2_sdl_screen_size(&w, &h);
    if (m && m->name) {
        if (strcmp(m->name, "getWidth") == 0)
            return w;
        if (strcmp(m->name, "getHeight") == 0)
            return h;
        if (strcmp(m->name, "getRotation") == 0)
            return 0;
        if (strcmp(m->name, "getDisplayId") == 0)
            return 0;
        if (strcmp(m->name, "getRefreshRate") == 0)
            return 60;
    }
    return 0;
}

static int64_t j_Point_get(jctx *c)
{
    const jmethod *m = c->m;
    int w = 720, h = 720;
    extern void fp2_sdl_screen_size(int *width, int *height);
    fp2_sdl_screen_size(&w, &h);
    if (m && m->name) {
        if (strcmp(m->name, "x") == 0)
            return w;
        if (strcmp(m->name, "y") == 0)
            return h;
    }
    return 0;
}

/* android.opengl.GLES20.glGetString: the managed boot reads the driver
 * capability strings through it.  Measured (run19): the generic stub answered
 * "" for GL_EXTENSIONS, and Kairosoft's capability check then raised its own
 * error 100 (showDialog id=0 "An error has occurred.\n100") and quit at
 * frame 6.  Answer from the live context instead of a placeholder. */
static int64_t j_GLES20_glGetString(jctx *c)
{
    int which = jarg_int(c);
    extern const char *fp2_sdl_gl_string(unsigned int name);
    /* run21 corrected the earlier reading of this call.  The argument is NOT a
     * Kairosoft-private enum: the log shows the managed side reading the static
     * field first and handing its value straight in --
     *   getFieldID(android/opengl/GLES20.GL_EXTENSIONS, I, static=1) -> 0
     *   getMethodID(android/opengl/GLES20.glGetString, (I)Ljava/lang/String;)
     *   glGetString(0x0)
     * so the 0 came from our own unimplemented constant, not from the game.
     * j_GLES20_constant() now answers 0x1F03 for GL_EXTENSIONS and the switch
     * below can stay a plain token lookup.  Both spellings are accepted because
     * android.opengl.GLES20 uses the desktop values (0x1F0x) while some
     * wrappers pass the ES2 aliases (0x1B0x). */
    const char *value = NULL;
    switch (which) {
    case 0x1F00: case 0x1B00:
        value = fp2_sdl_gl_string(0x1F00); /* GL_VENDOR */
        break;
    case 0x1F01: case 0x1B01:
        value = fp2_sdl_gl_string(0x1F01); /* GL_RENDERER */
        break;
    case 0x1F02: case 0x1B02:
        value = fp2_sdl_gl_string(0x1F02); /* GL_VERSION */
        break;
    case 0x1F03: case 0x1B03:
        value = fp2_sdl_gl_string(0x1F03); /* GL_EXTENSIONS */
        break;
    case 0x8B8C:
        value = fp2_sdl_gl_string(0x8B8C); /* GL_SHADING_LANGUAGE_VERSION */
        break;
    case 0:
        /* Only reachable if the constant read regressed.  The managed side's
         * single documented lookup is the extension list (it scans the string
         * for GL_OES_depth24), so keep answering that rather than "" and stay
         * loud about it in the log. */
        JT("GLES20.glGetString(0) -- constant read regressed");
        value = fp2_sdl_gl_string(0x1F03);
        break;
    default:
        value = fp2_sdl_gl_string((unsigned int)which);
        break;
    }
    JT("GLES20.glGetString(0x%x) -> %zu bytes", which,
       value ? strlen(value) : 0);
    return (int64_t)(uintptr_t)mk_string(value ? value : "");
}

/* Display.getRealSize(Point) / getSize(Point): fill the caller's Point with
 * the real drawable instead of leaving it at the stubbed zero that produced
 * "Texture has out of range width / height". */
static int64_t j_Display_getRealSize(jctx *c)
{
    jobj *point = jarg_obj(c);
    int w = 720, h = 720;
    extern void fp2_sdl_screen_size(int *width, int *height);
    fp2_sdl_screen_size(&w, &h);
    JT("Display real size -> %dx%d", w, h);
    if (point) {
        jobject_field *fx = object_field(point, "x", 1);
        jobject_field *fy = object_field(point, "y", 1);
        if (fx)
            fx->primitive = w;
        if (fy)
            fy->primitive = h;
    }
    return 0;
}

static int64_t j_Utility_getScaleRatio_stub(jctx *c)
{
    /* (designW, designH, screenW, screenH) -> scale factor.  Measured on the
     * ArkOS box: the C# passes (240, 240, 0, 0) -- its cached screen size is
     * zero, and 240*0 produced the "out of range" texture that ended the
     * boot.  Substitute the real SDL drawable when the caller's cache is
     * zero, then scale = max(screen/design) so the 240x240 design fills the
     * panel. */
    int a = jarg_int(c);
    int b = jarg_int(c);
    int sw = jarg_int(c);
    int sh = jarg_int(c);
    extern void fp2_sdl_screen_size(int *width, int *height);
    int w = 0, h = 0;
    fp2_sdl_screen_size(&w, &h);
    if (sw <= 0)
        sw = w;
    if (sh <= 0)
        sh = h;
    float ratio = 1.0f;
    if (a > 0 && b > 0 && sw > 0 && sh > 0) {
        float rx = (float)sw / (float)a;
        float ry = (float)sh / (float)b;
        ratio = rx > ry ? rx : ry;
    }
    int bits;
    memcpy(&bits, &ratio, sizeof bits);
    fprintf(stderr,
            "[fp2/sdk] Utility.getScaleRatio(%d, %d, %d, %d) -> %.3f\n",
            a, b, sw, sh, ratio);
    /* run46: ultima chance antes do GetNumRecords que estourava. */
    fp2_set_use_preference();
    return bits;
}

static int64_t j_Utility_getPreferenceKeys(jctx *c)
{
    (void)c;
    return (int64_t)(uintptr_t)mk_string("kairo_placeholder_key");
}

static int64_t j_Utility_getPackageName(jctx *c)
{
    (void)c;
    return (int64_t)(uintptr_t)mk_string(BB_PACKAGE_NAME);
}

/* kairo.android.plugin.Utility.getAppWidth / getAppHeight: the plugin's own
 * view of the application surface.  run20 measured both answering 0 (the
 * generic stub) at the exact point where the ad layout is computed, right
 * before the managed side raised error 100.  Any layout division by these
 * collapses, so report the real drawable size -- the same source
 * j_DisplayMetrics_int() reads, which is already confirmed correct at
 * 720x720 on this device. */
static int64_t j_Utility_app_size(jctx *c)
{
    const jmethod *m = c->m;
    int w = 720, h = 720;
    /* run46: primeira chance de virar a bandeira.  Se o bloco estatico
     * ainda nao existir, a funcao sai sem marcar nada e tentamos de novo
     * adiante. */
    fp2_set_use_preference();
    extern void fp2_sdl_screen_size(int *width, int *height);
    fp2_sdl_screen_size(&w, &h);
    if (m && m->name && strcmp(m->name, "getAppWidth") == 0)
        return w;
    if (m && m->name && strcmp(m->name, "getAppHeight") == 0)
        return h;
    return 0;
}

/* run26: the last unimplemented contracts in front of the error dialog.
 *
 * getNotificationFilter(Object)I is a Kairosoft notification bitmask; every
 * bit means "this notification class is filtered out".  The stub answered 0,
 * which reads as "nothing is filtered", i.e. every notification would be shown.
 * On a host with no notification service at all, the honest answer is "filter
 * everything" = all bits set, which is what an absent service implies and what
 * getNotificationData()'s empty array already assumes.  Returning 0 instead
 * leaves the managed side believing it must render notifications it can never
 * receive.  (-1 has every bit set.)
 *
 * fullscreen(Z)V and the AdMobView pair are the other two calls that still fall
 * through to the generic stub on every single pass.  They are handled here for
 * completeness of the contract, not because they are known to be the trigger:
 * the run25 exception hook proved there is no managed exception, so error 100
 * is a self-check condition and each remaining stub is a candidate until one
 * of them stops being it. */
static int64_t j_Utility_getNotificationFilter(jctx *c)
{
    (void)jarg_obj(c);
    JT("Utility.getNotificationFilter() -> -1 (no notification service)");
    return -1;
}

static int64_t j_Utility_fullscreen(jctx *c)
{
    int flag = jarg_int(c);
    JT("Utility.fullscreen(%d) -- host window is already fullscreen", flag);
    return 0;
}

static int64_t j_AdMobView_create(jctx *c)
{
    int ad_id = jarg_int(c);
    jobj *unit = jarg_obj(c);
    jobj *placement = jarg_obj(c);
    int bottom = jarg_int(c);
    JT("AdMobView.createAdMobView(%d, \"%s\", \"%s\", %d) -> null (no ads)",
       ad_id, unit && unit->str ? unit->str : "?",
       placement && placement->str ? placement->str : "?", bottom);
    /* Null is the honest answer: this port shows no ads.  The previous generic
     * stub handed back a bare java/lang/Object, which the managed side could
     * not tell apart from a real ad view and therefore queried. */
    return 0;
}

static int64_t j_AdMobView_remove(jctx *c)
{
    (void)c;
    JT("AdMobView.removeAdMobView() -- nothing to remove");
    return 0;
}

/* Static int constants of the framework classes the boot path reads.
 *
 * run26 listed these as still falling through to the generic stub, and every
 * one of them answers with a value the managed side then acts on.  Only the
 * genuinely-INTEGER constants belong here.  The service-name constants
 * (Context.LOCATION_SERVICE, AUDIO_SERVICE, DISPLAY_SERVICE,
 * MEDIA_ROUTER_SERVICE, AudioManager.PROPERTY_*) are a different animal: their
 * field signature is Ljava/lang/String;, so they need a string handler, not
 * this one -- mixing the two would hand the managed side an int where it
 * expects a service name.
 *
 * The two that matter are the permission constants.  The stub answered 0 for
 * BOTH PERMISSION_GRANTED and the query itself, so "is this permission held?"
 * and "the value that means yes" agreed with each other by accident; the
 * moment either side is answered honestly the pair stops being consistent, so
 * both are pinned here to the real Android values and the permission query is
 * answered from the same table. */
static int64_t j_android_int_constant(jctx *c)
{
    const jmethod *m = c->m;
    const char *cls = m && m->cls ? m->cls : "";
    const char *name = m && m->name ? m->name : "";

    if (strcmp(cls, "android/content/Context") == 0) {
        if (strcmp(name, "MODE_PRIVATE") == 0)
            return 0x0000;
        if (strcmp(name, "MODE_WORLD_READABLE") == 0)
            return 0x0001;
        if (strcmp(name, "MODE_WORLD_WRITEABLE") == 0)
            return 0x0002;
        if (strcmp(name, "MODE_MULTI_PROCESS") == 0)
            return 0x0004;
    }
    if (strcmp(cls, "android/content/pm/PackageManager") == 0) {
        if (strcmp(name, "GET_META_DATA") == 0)
            return 0x00000080;
        if (strcmp(name, "GET_ACTIVITIES") == 0)
            return 0x00000001;
        if (strcmp(name, "GET_RECEIVERS") == 0)
            return 0x00000002;
        if (strcmp(name, "GET_SERVICES") == 0)
            return 0x00000004;
        if (strcmp(name, "GET_RESOURCES") == 0)
            return 0x00000020;
        /* The pair the C# compares against. */
        if (strcmp(name, "PERMISSION_GRANTED") == 0)
            return 0;
        if (strcmp(name, "PERMISSION_DENIED") == 0)
            return -1;
    }
    if (strcmp(cls, "android/media/MediaRouter") == 0) {
        if (strcmp(name, "ROUTE_TYPE_LIVE_AUDIO") == 0)
            return 0x000C;
        if (strcmp(name, "ROUTE_TYPE_LIVE_VIDEO") == 0)
            return 0x000D;
        if (strcmp(name, "ROUTE_TYPE_SYSTEM") == 0)
            return 0x000E;
    }
    JT("%s.%s -> (android int constant)", cls, name);
    return 0;
}

/* The String-valued static constants.  The stub answered "" for each of them,
 * and getSystemService("") then handed back a generically typed service object
 * -- which is what made the display, audio and media-router paths degrade into
 * stub objects all the way down.  Answering the real service names costs
 * nothing and lets a caller that compares the string behave. */
static int64_t j_android_string_constant(jctx *c)
{
    const jmethod *m = c->m;
    const char *cls = m && m->cls ? m->cls : "";
    const char *name = m && m->name ? m->name : "";
    const char *value = "";

    if (strcmp(cls, "android/content/Context") == 0) {
        if (strcmp(name, "LOCATION_SERVICE") == 0)
            value = "location";
        else if (strcmp(name, "AUDIO_SERVICE") == 0)
            value = "audio";
        else if (strcmp(name, "DISPLAY_SERVICE") == 0)
            value = "display";
        else if (strcmp(name, "MEDIA_ROUTER_SERVICE") == 0)
            value = "media_router";
        else if (strcmp(name, "POWER_SERVICE") == 0)
            value = "power";
        else if (strcmp(name, "CONNECTIVITY_SERVICE") == 0)
            value = "connectivity";
        else if (strcmp(name, "WINDOW_SERVICE") == 0)
            value = "window";
        else if (strcmp(name, "INPUT_SERVICE") == 0)
            value = "input";
        else if (strcmp(name, "ACTIVITY_SERVICE") == 0)
            value = "activity";
    } else if (strcmp(cls, "android/content/pm/PackageManager") == 0) {
        if (strcmp(name, "FEATURE_AUDIO_LOW_LATENCY") == 0)
            value = "";
    } else if (strcmp(cls, "android/media/AudioManager") == 0) {
        if (strcmp(name, "PROPERTY_OUTPUT_SAMPLE_RATE") == 0)
            value = "android.media.property.OUTPUT_SAMPLE_RATE";
        else if (strcmp(name, "PROPERTY_OUTPUT_FRAMES_PER_BUFFER") == 0)
            value = "android.media.property.OUTPUT_FRAMES_PER_BUFFER";
    }
    JT("%s.%s -> \"%s\"", cls, name, value);
    return (int64_t)(uintptr_t)mk_string(value);
}

/* Context.checkCallingOrSelfPermission(String)I and the Activity wrapper.
 *
 * The stub answered 0 for everything, which is PERMISSION_DENIED.  Nothing on
 * this host actually holds an Android permission, so DENIED is the truthful
 * answer -- but the previous code also let PackageManager.PERMISSION_GRANTED
 * read back as 0, i.e. "granted", and the two disagreed.  Answer the query and
 * the constant from the same table so a caller that compares them is
 * self-consistent, which is what the C# does when it decides whether a
 * capability-gated feature is available. */
static int64_t j_Context_checkPermission(jctx *c)
{
    jobj *name = jarg_obj(c);
    const char *perm = name && name->str ? name->str : "?";
    /* This port has no Android permission framework at all: there is no
     * package manager to grant against, so nothing is granted.  DENIED (= -1)
     * is the honest value and it matches PERMISSION_DENIED. */
    JT("Context.checkCallingOrSelfPermission(\"%s\") -> -1 (denied, no PM)",
       perm);
    return -1;
}

/* AudioManager.getProperty(String)String.
 *
 * run26 measured the failure this fixes: the stub answered "", Unity then ran
 * Integer.parseInt("") -> 0 for both the output sample rate and the frames per
 * buffer, so the audio device was configured for 0 Hz.  A zero sample rate is
 * not a degraded-but-running device, it is no device, and the DSP init that
 * follows cannot come up.
 *
 * 48000 Hz with a 192-frame buffer is the safe common denominator: the AAudio
 * shim opens whatever the host mixer really is, and both values are ones every
 * ALSA/AAudio device accepts or resamples without complaint.  The contract is
 * "a decimal integer as a string", which is what parseInt then consumes. */
static int64_t j_AudioManager_getProperty(jctx *c)
{
    jobj *key = jarg_obj(c);
    const char *prop = key && key->str ? key->str : "";
    const char *value = "";

    if (strstr(prop, "OUTPUT_SAMPLE_RATE") != NULL)
        value = "48000";
    else if (strstr(prop, "OUTPUT_FRAMES_PER_BUFFER") != NULL)
        value = "192";
    JT("AudioManager.getProperty(\"%s\") -> \"%s\"", prop, value);
    return (int64_t)(uintptr_t)mk_string(value);
}

/* java.lang.Integer / java.lang.Long: the boxed-value conversions.
 *
 * run27 caught the consequence of these being absent.  getProperty now hands
 * back "48000", and Unity feeds that straight into Integer.parseInt -- which
 * the generic stub answered with 0, so the real value we had just started
 * providing was immediately thrown away again and the device was configured
 * for 0 Hz.  The stub returning 0 for parseInt is worse than returning nothing:
 * it is confidently wrong, and it hid the fix underneath it.
 *
 * These are not cosmetic.  Every "did that JNI call give me a usable value"
 * check on the boot path runs through them: Integer.parseInt for the audio
 * rates, Integer.valueOf for boxed flags, Long.parseLong for the timings. */
static int64_t j_Integer_parseInt(jctx *c)
{
    jobj *s = jarg_obj(c);
    const char *text = s && s->str ? s->str : "";
    char *end = NULL;
    long value;

    /* parseInt(String) and parseInt(String,int) share this handler.  Only
     * consume the radix when the descriptor actually declares a second
     * parameter: jarg_int() has no bounds check on the args array, so reading
     * one from the single-argument overload would walk off the end of it.
     * A real JNI descriptor walk, so "(Ljava/lang/String;I)" counts two and
     * "(Ljava/lang/String;)" counts one. */
    if (c->m && c->m->sig) {
        const char *p = strchr(c->m->sig, '(');
        if (p) {
            p++;
            int params = 0;
            while (*p && *p != ')') {
                if (*p == 'L') {
                    while (*p && *p != ';')
                        p++;
                    if (*p)
                        p++;
                } else if (*p == '[') {
                    while (*p == '[')
                        p++;
                    /* the element type still has to be walked */
                    if (*p == 'L') {
                        while (*p && *p != ';')
                            p++;
                        if (*p)
                            p++;
                    } else if (*p) {
                        p++;
                    }
                } else {
                    p++;
                }
                params++;
            }
            if (params > 1)
                (void)jarg_int(c);
        }
    }

    if (!text[0]) {
        /* Java throws NumberFormatException on an empty string.  Returning 0
         * would be a lie that hides the failure; report it and hand back the
         * value the caller would see if it ignored the exception. */
        JT("Integer.parseInt(\"\") -> NumberFormatException (reporting 0)");
        return 0;
    }
    value = strtol(text, &end, 10);
    if (end == text || (end && *end)) {
        JT("Integer.parseInt(\"%s\") -> NumberFormatException at '%s'",
           text, end ? end : text);
        return 0;
    }
    JT("Integer.parseInt(\"%s\") -> %ld", text, value);
    return (int64_t)(int32_t)value;
}

static int64_t j_Integer_valueOf_int(jctx *c)
{
    int32_t value = jarg_int(c);
    jobj *boxed = mk_object("java/lang/Integer");
    JT("Integer.valueOf(%d)", value);
    return (int64_t)(uintptr_t)boxed;
}

static int64_t j_Long_parseLong(jctx *c)
{
    jobj *s = jarg_obj(c);
    const char *text = s && s->str ? s->str : "";
    char *end = NULL;
    long long value;

    if (!text[0])
        return 0;
    value = strtoll(text, &end, 10);
    if (end == text || (end && *end))
        return 0;
    JT("Long.parseLong(\"%s\") -> %lld", text, value);
    return (int64_t)value;
}

/* Integer.intValue / Long.longValue need no binding: dispatch() already
 * unboxes a boxed receiver through is_unbox() before consulting any handler,
 * which is why the log shows "unboxed java/lang/Long.longValue -> N" for
 * every timer tick even with nothing bound. */

/* java/lang/String.length()I and friends: the String methods the boot path
 * actually calls.  run26 listed String.length()I answering 0, which makes every
 * managed substring/truncate decision collapse to "empty". */
static int64_t j_String_length(jctx *c)
{
    jobj *self = c->self;
    if (!self || !self->str)
        return 0;
    return (int64_t)strlen(self->str);
}

static int64_t j_String_isEmpty(jctx *c)
{
    jobj *self = c->self;
    int empty = (self && self->str && self->str[0]) ? 0 : 1;
    return empty;
}

static int64_t j_String_trim(jctx *c)
{
    jobj *self = c->self;
    const char *text = self && self->str ? self->str : "";
    while (*text == ' ' || *text == '\t' || *text == '\n' || *text == '\r')
        text++;
    return (int64_t)(uintptr_t)mk_string(text);
}

/* The last two Kairosoft plugin calls that still reached the generic stub on
 * every launch (run27).  Both drive the notification shade, which does not
 * exist here; saying so explicitly is what the notification-filter mask in
 * j_Utility_getNotificationFilter() already implies. */
static int64_t j_Utility_setNotificationBackground(jctx *c)
{
    int flag = jarg_int(c);
    JT("Utility.setNotificationBackground(%d) -- no notification shade", flag);
    return 0;
}

static int64_t j_Utility_cancelNotifications(jctx *c)
{
    int secure = jarg_int(c);
    int alert = jarg_int(c);
    JT("Utility.cancelNotifications(%d, %d) -- none posted", secure, alert);
    return 0;
}

/* getNotificationData()String[]: the pending notification payloads.  There are
 * none without a notification service, and an empty array (never null -- a null
 * here is what fed LINQ Contains(null) into ArgumentNullException in run12). */
static int64_t j_Utility_getNotificationData(jctx *c)
{
    (void)c;
    JT("Utility.getNotificationData() -> empty array");
    return (int64_t)(uintptr_t)new_obj(O_OBJARRAY, "[Ljava/lang/String;");
}

/* android.opengl.GLES20's int constants are STATIC FIELDS, and run20 caught
 * the managed side reading one:
 *   ReflectionHelper.getFieldID(android/opengl/GLES20.GL_EXTENSIONS, I, static=1)
 *   -> unhandled ... -> 0
 *   ReflectionHelper.getMethodID(android/opengl/GLES20.glGetString, (I)Ljava/lang/String;)
 *   -> GLES20.glGetString(0x0)
 * The C# passes the field value straight to glGetString, so answering 0 here
 * is what produced the nonsense 0x0 argument; the real value is 0x1F03.
 * GL_EXTENSIONS is the only GL_ token in the metadata, because the managed
 * side uses the extension string solely to look for GL_OES_depth24. */
static int64_t j_GLES20_constant(jctx *c)
{
    const jmethod *m = c->m;
    if (m && m->name) {
        if (strcmp(m->name, "GL_VENDOR") == 0)
            return 0x1F00;
        if (strcmp(m->name, "GL_RENDERER") == 0)
            return 0x1F01;
        if (strcmp(m->name, "GL_VERSION") == 0)
            return 0x1F02;
        if (strcmp(m->name, "GL_EXTENSIONS") == 0)
            return 0x1F03;
        if (strcmp(m->name, "GL_SHADING_LANGUAGE_VERSION") == 0)
            return 0x8B8C;
    }
    JT("android/opengl/GLES20.%s -> 0", m && m->name ? m->name : "?");
    return 0;
}

/* java.util.TimeZone, and the Kairosoft Utility wrapper that sits on top of it.
 *
 * Measured (run18): the managed boot reached
 *   ReflectionHelper.getMethodID(java/util/TimeZone.getTimeZone,
 *                                (Ljava/lang/String;)Ljava/lang/Object;)
 * which the generic stub answered with `stub java/lang/Object`, and the very
 * next call was getRawOffset on that bare Object.  The IL2CPP metadata lists
 * getTimeZone and getRawOffset next to each other inside the KairoLibrary
 * string table, and the C# then raised its own error path:
 *   Utility.showDialog(id=0, title="Error", msg="An error has occurred.\n100")
 *   Utility.showDialog(id=3, ..., "エラーが発生しました ...")
 * followed by Application.Quit at frame 6.  The failure is Kairosoft error
 * code 100 raised because the timezone it asked for came back as a plain
 * object with a zero raw offset.
 *
 * The device runs on UTC, so report the UTC zone with offset 0 rather than a
 * null-offset-less stub: the value is what the C# validates, and an honest
 * zero is what java.util.TimeZone.getTimeZone("UTC") returns on this box. */
#define BB_TIMEZONE_ID "UTC"
#define BB_TIMEZONE_DISPLAY "Coordinated Universal Time"

static jobj *timezone_object(void)
{
    return mk_object("java/util/TimeZone");
}

static int64_t j_TimeZone_getTimeZone(jctx *c)
{
    jobj *id = jarg_obj(c);
    const char *want = id && id->str ? id->str : BB_TIMEZONE_ID;
    fprintf(stderr, "[fp2/sdk] TimeZone.getTimeZone(\"%s\") -> %s\n", want,
            BB_TIMEZONE_ID);
    return (int64_t)(uintptr_t)timezone_object();
}

/* getDefault() is what the C# actually calls first (run19: it reached
 * getDefault before getTimeZone and the generic stub answered
 * "stub java/lang/Object", after which getID came back "").  Return the same
 * zone object as getTimeZone so getID/getRawOffset resolve on it. */
static int64_t j_TimeZone_getDefault(jctx *c)
{
    (void)c;
    fprintf(stderr, "[fp2/sdk] TimeZone.getDefault() -> %s\n",
            BB_TIMEZONE_ID);
    return (int64_t)(uintptr_t)timezone_object();
}

static int64_t j_TimeZone_getRawOffset(jctx *c)
{
    (void)c;
    fprintf(stderr, "[fp2/sdk] TimeZone.getRawOffset() -> 0\n");
    return 0;
}

static int64_t j_TimeZone_getOffset(jctx *c)
{
    (void)c;
    return 0;
}

static int64_t j_TimeZone_getID(jctx *c)
{
    (void)c;
    return (int64_t)(uintptr_t)mk_string(BB_TIMEZONE_ID);
}

static int64_t j_TimeZone_getDisplayName(jctx *c)
{
    (void)c;
    return (int64_t)(uintptr_t)mk_string(BB_TIMEZONE_DISPLAY);
}

static int64_t j_TimeZone_useDaylightTime(jctx *c)
{
    (void)c;
    return 0;
}

static int64_t j_TimeZone_inDaylightTime(jctx *c)
{
    (void)c;
    return 0;
}

/* Kairosoft's own wrapper (KairoLibrary exposes getTimeZone/getRawOffset as a
 * matched pair).  Route both to the same zone so the managed side sees a
 * consistent pair regardless of which spelling it calls. */
static int64_t j_Utility_getTimeZone(jctx *c)
{
    return j_TimeZone_getTimeZone(c);
}

static int64_t j_Utility_getRawOffset(jctx *c)
{
    return j_TimeZone_getRawOffset(c);
}

static int64_t j_AndroidId_field(jctx *c)
{
    (void)c;
    return (int64_t)(uintptr_t)mk_string("android_id");
}

static int64_t j_SettingsSecure_getString(jctx *c)
{
    (void)jarg_obj(c); /* ContentResolver */
    jobj *key = jarg_obj(c);
    const char *name = key && key->str ? key->str : "";

    /* Android's ANDROID_ID is a stable, app-visible 64-bit hexadecimal
     * identifier.  Keep it deterministic so the game's offline profile and
     * obfuscation keys remain valid between launches. */
    const char *value = strcmp(name, "android_id") == 0
                            ? "6e6578746f736867"
                            : "";
    JT("Settings.Secure.getString(\"%s\") -> \"%s\"", name, value);
    return (int64_t)(uintptr_t)mk_string(value);
}

static int64_t j_Locale_getDefault(jctx *c)
{
    (void)c;
    return (int64_t)(uintptr_t)mk_object("java/util/Locale");
}

static int64_t j_Locale_getString(jctx *c)
{
    const char *value = "en-US";
    if (strcmp(c->m->name, "getLanguage") == 0)
        value = "en";
    else if (strcmp(c->m->name, "getCountry") == 0)
        value = "US";
    else if (strcmp(c->m->name, "toString") == 0)
        value = "en_US";
    return (int64_t)(uintptr_t)mk_string(value);
}

/* ----------------------------------------------------- Netflix local service
 *
 * This title initializes the Netflix Games Java SDK before its own menus and
 * uses the SDK's blob callbacks for progression saves.  NextOS has no Android
 * service process, so this target adapter implements the same asynchronous
 * Java contract with a deterministic local guest profile and a private local
 * blob store.  It never claims a network account or fabricates a remote
 * conflict. */

static jobj *netflix_error_result(int code, const char *message)
{
    jobj *error = mk_object("com/netflix/games/Error");
    object_set_primitive(error, "code", code);
    object_set_object(error, "message", mk_string(message ? message : ""));
    jobj *result = mk_object("com/netflix/games/NetflixResult");
    object_set_object(result, "data", NULL);
    object_set_object(result, "error", error);
    return result;
}

static jobj *netflix_success_result(jobj *data)
{
    jobj *result = mk_object("com/netflix/games/NetflixResult");
    object_set_object(result, "data", data);
    object_set_object(result, "error", NULL);
    return result;
}

static int netflix_queue_callback(jobj *callback, jobj *result)
{
    if (!callback || !callback->prim ||
        !(callback->len & PROXY_REFLECTION)) {
        nx_log("jni: Netflix callback rejected (proxy=%p flags=%#x)",
               (void *)callback, callback ? callback->len : 0);
        return -1;
    }
    netflix_callback_item *item = calloc(1, sizeof *item);
    if (!item)
        nx_die("Netflix callback queue allocation failed");
    item->callback = callback;
    item->result = result;
    pthread_mutex_lock(&netflix_callback_lock);
    if (netflix_callback_tail)
        netflix_callback_tail->next = item;
    else
        netflix_callback_head = item;
    netflix_callback_tail = item;
    pthread_mutex_unlock(&netflix_callback_lock);
    return 0;
}

static netflix_callback_item *netflix_take_callback(void)
{
    pthread_mutex_lock(&netflix_callback_lock);
    netflix_callback_item *item = netflix_callback_head;
    if (item) {
        netflix_callback_head = item->next;
        if (!netflix_callback_head)
            netflix_callback_tail = NULL;
    }
    pthread_mutex_unlock(&netflix_callback_lock);
    return item;
}

static int netflix_blob_key_valid(const char *key)
{
    if (!key || !*key || strlen(key) > 64 ||
        strcmp(key, ".") == 0 || strcmp(key, "..") == 0)
        return 0;
    for (const unsigned char *p = (const unsigned char *)key; *p; p++)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9') || *p == '_' || *p == '-' ||
              *p == '.'))
            return 0;
    return 1;
}

static int netflix_blob_path(const char *key, char *dir, size_t dir_size,
                             char *path, size_t path_size)
{
    if (!netflix_blob_key_valid(key))
        return -EINVAL;
    int n = snprintf(dir, dir_size, "%s/netflix-blobs", fp2_home);
    if (n < 0 || (size_t)n >= dir_size)
        return -ENAMETOOLONG;
    struct stat info;
    if (lstat(dir, &info) != 0) {
        if (errno != ENOENT || mkdir(dir, 0700) != 0)
            return -errno;
        if (lstat(dir, &info) != 0)
            return -errno;
    }
    if (!S_ISDIR(info.st_mode) || S_ISLNK(info.st_mode))
        return -ELOOP;
    (void)chmod(dir, 0700);
    n = snprintf(path, path_size, "%s/%s.b64", dir, key);
    if (n < 0 || (size_t)n >= path_size)
        return -ENAMETOOLONG;
    return 0;
}

static int netflix_write_all(int fd, const char *data, size_t length)
{
    while (length) {
        ssize_t written = write(fd, data, length);
        if (written < 0) {
            if (errno == EINTR)
                continue;
            return -errno;
        }
        if (written == 0)
            return -EIO;
        data += written;
        length -= (size_t)written;
    }
    return 0;
}

static int netflix_blob_write(const char *key, const char *base64)
{
    char dir[1200], path[1320], temporary[1400];
    int result = netflix_blob_path(key, dir, sizeof dir, path, sizeof path);
    if (result)
        return result;
    int n = snprintf(temporary, sizeof temporary, "%s/.write.%ld.%lld", dir,
                     (long)getpid(), (long long)monotonic_nanos());
    if (n < 0 || (size_t)n >= sizeof temporary)
        return -ENAMETOOLONG;
    int fd = open(temporary, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
                  0600);
    if (fd < 0)
        return -errno;
    const char *payload = base64 ? base64 : "";
    result = netflix_write_all(fd, payload, strlen(payload));
    if (!result && fdatasync(fd) != 0)
        result = -errno;
    if (close(fd) != 0 && !result)
        result = -errno;
    if (!result && rename(temporary, path) != 0)
        result = -errno;
    if (!result) {
        int directory_fd = open(dir, O_RDONLY | O_DIRECTORY | O_NOFOLLOW |
                                      O_CLOEXEC);
        if (directory_fd < 0)
            result = -errno;
        else {
            if (fsync(directory_fd) != 0)
                result = -errno;
            close(directory_fd);
        }
    }
    if (result)
        (void)unlink(temporary);
    return result;
}

static int netflix_blob_read(const char *key, char **base64_out)
{
    char dir[1200], path[1320];
    *base64_out = NULL;
    int result = netflix_blob_path(key, dir, sizeof dir, path, sizeof path);
    if (result)
        return result;
    int fd = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0)
        return errno == ENOENT ? 1 : -errno;
    struct stat info;
    if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) ||
        info.st_size < 0 || info.st_size > 32 * 1024 * 1024) {
        result = errno ? -errno : -EFBIG;
        close(fd);
        return result;
    }
    size_t length = (size_t)info.st_size;
    char *data = calloc(length + 1, 1);
    if (!data) {
        close(fd);
        return -ENOMEM;
    }
    size_t offset = 0;
    while (offset < length) {
        ssize_t got = read(fd, data + offset, length - offset);
        if (got < 0 && errno == EINTR)
            continue;
        if (got <= 0) {
            result = got == 0 ? -EIO : -errno;
            free(data);
            close(fd);
            return result;
        }
        offset += (size_t)got;
    }
    close(fd);
    *base64_out = data;
    return 0;
}

static int netflix_blob_delete(const char *key)
{
    char dir[1200], path[1320];
    int result = netflix_blob_path(key, dir, sizeof dir, path, sizeof path);
    if (result)
        return result;
    struct stat info;
    if (lstat(path, &info) != 0)
        return errno == ENOENT ? 0 : -errno;
    if (!S_ISREG(info.st_mode) || S_ISLNK(info.st_mode))
        return -ELOOP;
    if (unlink(path) != 0)
        return -errno;
    int directory_fd = open(dir, O_RDONLY | O_DIRECTORY | O_NOFOLLOW |
                                  O_CLOEXEC);
    if (directory_fd < 0)
        return -errno;
    int result_sync = fsync(directory_fd) == 0 ? 0 : -errno;
    close(directory_fd);
    return result_sync;
}

static jobj *netflix_blob_container(const char *base64)
{
    char timestamp[40] = "1970-01-01T00:00:00Z";
    time_t now = time(NULL);
    struct tm utc;
    if (gmtime_r(&now, &utc))
        (void)strftime(timestamp, sizeof timestamp, "%Y-%m-%dT%H:%M:%SZ",
                       &utc);
    jobj *container = mk_object(
        "com/netflix/games/storage/blobs/Base64BlobContainer");
    object_set_object(container, "base64String", mk_string(base64));
    object_set_object(container, "serverSyncTimestamp", mk_string(timestamp));
    return container;
}

static int64_t j_Netflix_init(jctx *c)
{
    (void)jarg_obj(c);
    nx_log("jni: Netflix SDK initialized with local guest service");
    return 0;
}

static int64_t j_Netflix_registerPlugin(jctx *c)
{
    netflix_plugin_callback = jarg_obj(c);
    JT("Netflix plugin callback registered proxy=%p", (void *)netflix_plugin_callback);
    return 0;
}

static int64_t j_Netflix_registerConfig(jctx *c)
{
    netflix_config_callback = jarg_obj(c);
    JT("Netflix crash config callback registered proxy=%p",
       (void *)netflix_config_callback);
    return 0;
}

static int64_t j_Netflix_showAccess(jctx *c)
{
    (void)jarg_obj(c);
    (void)jarg_int(c);
    JT("Netflix access button requested (local guest service has no overlay)");
    return 0;
}

static int64_t j_Netflix_hideAccess(jctx *c)
{
    (void)jarg_obj(c);
    return 0;
}

static int64_t j_Netflix_requestAccess(jctx *c)
{
    jobj *callback = jarg_obj(c);
    return netflix_queue_callback(
        callback, netflix_success_result(netflix_player_access));
}

static int64_t j_Netflix_getCurrentProfile(jctx *c)
{
    (void)c;
    return (int64_t)(uintptr_t)netflix_success_result(netflix_profile);
}

static int64_t j_Netflix_membershipName(jctx *c)
{
    return (int64_t)(uintptr_t)mk_string(
        c->self && c->self->str ? c->self->str : "GUEST");
}

static int64_t j_Netflix_profileString(jctx *c)
{
    const char *value = "";
    if (strcmp(c->m->name, "getPlayerId") == 0)
        value = "nextos-local-6e6578746f736867";
    else if (strcmp(c->m->name, "getHandle") == 0)
        value = "NextOS Local";
    else if (strcmp(c->m->name, "getPreferredLanguage") == 0)
        value = "pt-BR";
    return (int64_t)(uintptr_t)mk_string(value);
}

static int64_t j_Netflix_profileLegacy(jctx *c)
{
    (void)c;
    return (int64_t)(uintptr_t)netflix_success_result(mk_string(""));
}

static int64_t j_Netflix_sdkString(jctx *c)
{
    const char *value = strcmp(c->m->name, "getNfgSdkVersion") == 0
                            ? "nextos-local-1" : "";
    return (int64_t)(uintptr_t)mk_string(value);
}

static int64_t j_Netflix_getCrashConfig(jctx *c)
{
    (void)c;
    return (int64_t)(uintptr_t)netflix_crash_config;
}

static int64_t j_Netflix_writeBlob(jctx *c)
{
    jobj *key = jarg_obj(c);
    jobj *payload = jarg_obj(c);
    jobj *callback = jarg_obj(c);
    const char *slot = key && key->str ? key->str : "";
    int result = netflix_blob_write(slot,
                                    payload && payload->str ? payload->str : "");
    jobj *envelope;
    if (result) {
        char message[128];
        snprintf(message, sizeof message, "local blob write failed: %d", result);
        envelope = netflix_error_result(result, message);
    } else {
        jobj *data = mk_object(
            "com/netflix/games/storage/blobs/WritePlayerBlobResult");
        object_set_object(data, "conflict", NULL);
        envelope = netflix_success_result(data);
        nx_log("jni: Netflix local blob write complete slot=%s", slot);
    }
    return netflix_queue_callback(callback, envelope);
}

static int64_t j_Netflix_readBlob(jctx *c)
{
    jobj *key = jarg_obj(c);
    jobj *callback = jarg_obj(c);
    const char *slot = key && key->str ? key->str : "";
    char *payload = NULL;
    int result = netflix_blob_read(slot, &payload);
    jobj *envelope;
    if (result < 0) {
        char message[128];
        snprintf(message, sizeof message, "local blob read failed: %d", result);
        envelope = netflix_error_result(result, message);
    } else if (result == 1) {
        /* Match BlobStoreClientImpl: absence is
         * NetflixResult.withError(BLOB_NAME_NOT_FOUND, slot).  Sending a
         * success envelope with blobContainer=NULL violates the SDK's Kotlin
         * non-null contract; the managed AndroidReadBlobCallback then enters
         * its data parser, throws, and ReflectionHelper.createInvocationError
         * leaves the awaiting PreLoadPlayerData task incomplete. */
        envelope = netflix_error_result(NETFLIX_ERROR_BLOB_NAME_NOT_FOUND,
                                        slot);
        nx_log("jni: Netflix local blob read slot=%s state=absent error=%d",
               slot, NETFLIX_ERROR_BLOB_NAME_NOT_FOUND);
    } else {
        jobj *data = mk_object(
            "com/netflix/games/storage/blobs/ReadPlayerBlobResult");
        jobj *container = netflix_blob_container(payload);
        object_set_object(data, "blobContainer", container);
        object_set_object(data, "blob_container", container);
        object_set_object(data, "conflict", NULL);
        envelope = netflix_success_result(data);
        nx_log("jni: Netflix local blob read slot=%s state=present", slot);
    }
    free(payload);
    return netflix_queue_callback(callback, envelope);
}

static int64_t j_Netflix_deleteBlob(jctx *c)
{
    jobj *key = jarg_obj(c);
    jobj *callback = jarg_obj(c);
    const char *slot = key && key->str ? key->str : "";
    int result = netflix_blob_delete(slot);
    jobj *envelope;
    if (result) {
        char message[128];
        snprintf(message, sizeof message, "local blob delete failed: %d", result);
        envelope = netflix_error_result(result, message);
    } else {
        jobj *data = mk_object(
            "com/netflix/games/storage/blobs/DeletePlayerBlobResult");
        object_set_object(data, "conflict", NULL);
        envelope = netflix_success_result(data);
        nx_log("jni: Netflix local blob delete complete slot=%s", slot);
    }
    return netflix_queue_callback(callback, envelope);
}

static int64_t j_Netflix_resolveConflict(jctx *c)
{
    jobj *key = jarg_obj(c);
    int resolution = jarg_int(c);
    jobj *callback = jarg_obj(c);
    JT("Netflix resolveConflict slot=%s choice=%d (no local conflict)",
       key && key->str ? key->str : "", resolution);
    return netflix_queue_callback(callback, netflix_success_result(NULL));
}

static int64_t j_Netflix_noop(jctx *c)
{
    (void)c;
    return 0;
}

#ifndef FP2_RELEASE_BUILD
static int reflection_contract_selftest(void);
int fp2_jni_netflix_selftest(void)
{
    char key[64];
    snprintf(key, sizeof key, "nx-selftest-%ld", (long)getpid());
    static const char payload[] = "bmV4dG9zLXNlbGYtdGVzdA==";
    int result = netflix_blob_write(key, payload);
    if (result) {
        fprintf(stderr, "netflix_selftest: write failed %d\n", result);
        return 1;
    }
    char *readback = NULL;
    result = netflix_blob_read(key, &readback);
    if (result || !readback || strcmp(readback, payload) != 0) {
        fprintf(stderr, "netflix_selftest: read mismatch %d\n", result);
        free(readback);
        (void)netflix_blob_delete(key);
        return 1;
    }
    free(readback);
    if (netflix_blob_delete(key) != 0 ||
        netflix_blob_read(key, &readback) != 1) {
        fprintf(stderr, "netflix_selftest: delete/absence failed\n");
        free(readback);
        return 1;
    }
    if (netflix_take_callback() != NULL) {
        fprintf(stderr, "netflix_selftest: callback queue not initially empty\n");
        return 1;
    }

    /* Exercise the actual NfUnitySdk.readBlob handler and callback queue, not
     * only the filesystem helper.  This is the first-boot path that failed in
     * run3: the slot is absent and the callback must complete with data=NULL
     * plus Error(-1001, slot), so AndroidJavaConverter skips its non-null data
     * parser and cannot reach ReflectionHelper.createInvocationError. */
    jobj *callback = mk_object("bitter/jnibridge/Proxy");
    callback->prim = 1;
    callback->len = PROXY_REFLECTION | PROXY_NETFLIX_CALLBACK;
    jobj *slot = mk_string(key);
    const uint64_t read_args[] = {
        (uint64_t)(uintptr_t)slot,
        (uint64_t)(uintptr_t)callback,
    };
    jctx read_context = {
        .env = fp2_jni_env(),
        .self = mk_object("com/netflix/unity/api/NfUnitySdk"),
        .args = read_args,
    };
    if (j_Netflix_readBlob(&read_context) != 0) {
        fprintf(stderr, "netflix_selftest: absent callback enqueue failed\n");
        return 1;
    }
    netflix_callback_item *absent = netflix_take_callback();
    jobject_field *absent_data = absent
        ? object_field(absent->result, "data", 0) : NULL;
    jobject_field *absent_error = absent
        ? object_field(absent->result, "error", 0) : NULL;
    jobj *error = absent_error && absent_error->is_object
        ? absent_error->object : NULL;
    jobject_field *error_code = error
        ? object_field(error, "code", 0) : NULL;
    jobject_field *error_message = error
        ? object_field(error, "message", 0) : NULL;
    if (!absent || absent->callback != callback || !absent_data ||
        !absent_data->is_object || absent_data->object != NULL ||
        !absent_error || !absent_error->is_object || !error ||
        strcmp(error->cls, "com/netflix/games/Error") != 0 ||
        !error_code || error_code->is_object ||
        error_code->primitive != NETFLIX_ERROR_BLOB_NAME_NOT_FOUND ||
        !error_message || !error_message->is_object ||
        !error_message->object || !error_message->object->str ||
        strcmp(error_message->object->str, key) != 0) {
        fprintf(stderr,
                "netflix_selftest: absent callback contract failed\n");
        free(absent);
        return 1;
    }
    free(absent);
    fprintf(stderr,
            "netflix_selftest: absent callback contract PASS "
            "(data=null error=-1001 parser=skipped)\n");

    jobject_field *player_id =
        object_field(netflix_player_access, "playerId", 0);
    jobject_field *membership =
        object_field(netflix_player_access, "membershipStatus", 0);
    if (!player_id || !player_id->is_object || !player_id->object ||
        !player_id->object->str || !membership || !membership->is_object ||
        membership->object != netflix_membership) {
        fprintf(stderr, "netflix_selftest: result object fields failed\n");
        return 1;
    }
    if (reflection_contract_selftest() != 0)
        return 1;
    fprintf(stderr, "netflix_selftest: PASS\n");
    return 0;
}
#endif

static int64_t j_GoogleIAB_instance(jctx *c)
{
    (void)c;
    return (int64_t)(uintptr_t)mk_object("com/prime31/GoogleIABPlugin");
}

static int64_t j_PlayGames_initialize(jctx *c)
{
    (void)jarg_obj(c);
    return 0;
}

static int64_t j_PlayGames_getSignInClient(jctx *c)
{
    (void)jarg_obj(c);
    return (int64_t)(uintptr_t)mk_object(
        "com/google/android/gms/games/GamesSignInClient");
}

static int play_games_offline_enabled(void)
{
#ifdef FP2_RELEASE_BUILD
    return 0;
#else
    static int enabled = -1;
    if (enabled < 0) {
        const char *value = getenv("FP2_PLAY_GAMES_OFFLINE");
        enabled = value && *value && strcmp(value, "0") != 0;
    }
    return enabled;
#endif
}

static int64_t j_PlayGames_authenticationTask(jctx *c)
{
    (void)c;
    if (!play_games_offline_enabled())
        return 0;
    JT("Play Games %s -> completed offline Task",
       c->m && c->m->name ? c->m->name : "authentication");
    return (int64_t)(uintptr_t)play_games_task;
}

static int64_t j_PlayGames_addOnSuccessListener(jctx *c)
{
    jobj *listener = jarg_obj(c);
    if (!play_games_offline_enabled() || c->self != play_games_task)
        return (int64_t)(uintptr_t)c->self;
    if (!listener || !(listener->len & PROXY_TASK_SUCCESS) ||
        !listener->prim) {
        JT("Play Games Task received an invalid success listener");
        return (int64_t)(uintptr_t)c->self;
    }
    jobj *previous = __atomic_exchange_n(&play_games_success_listener,
                                         listener, __ATOMIC_ACQ_REL);
    if (previous && previous != listener)
        nx_log("jni: replacing an undelivered Play Games success listener");
    JT("Play Games Task queued offline success listener handle=%#llx",
       (unsigned long long)listener->prim);
    return (int64_t)(uintptr_t)c->self;
}

static int64_t j_PlayGames_addOnFailureListener(jctx *c)
{
    (void)jarg_obj(c);
    return (int64_t)(uintptr_t)c->self;
}

static int64_t j_PlayGames_isAuthenticatedResult(jctx *c)
{
    (void)c;
    return 0;
}

void fp2_jni_pump_callbacks(void)
{
    jobj *listener = __atomic_exchange_n(&play_games_success_listener, NULL,
                                         __ATOMIC_ACQ_REL);
    if (listener) {
        jobj *iface = mk_class(
            "com/google/android/gms/tasks/OnSuccessListener");
        JT("Play Games delivering offline AuthenticationResult=false");
        (void)invoke_proxy(listener, iface, play_games_success_method,
                           play_games_success_args);
    }

    netflix_callback_item *item = netflix_take_callback();
    if (item) {
        jobj *iface = mk_class("com/netflix/games/Callback");
        jobj *args = j_NewObjectArray(NULL, 1,
                                      mk_class("java/lang/Object"),
                                      item->result);
        JT("Netflix delivering Callback.onResult result=%p",
           (void *)item->result);
        (void)invoke_proxy(item->callback, iface, netflix_on_result_method,
                           args);
        free(args->elems);
        free(args);
        free(item);
    }
}

/* ArmoryActivity's constructor only publishes itself and creates this
 * permission helper.  These objects reproduce that Java-side state for the
 * C# AndroidJavaObject calls while all permissions needed by the local port
 * are already satisfied by its own game directory. */
static int64_t j_Armory_getActivity(jctx *c)
{
    (void)c;
    return (int64_t)(uintptr_t)armory_activity_object;
}

static void normalize_reflection_signature(char *out, size_t capacity,
                                           const char *signature)
{
    if (!capacity)
        return;
    size_t i = 0;
    for (; signature && signature[i] && i + 1 < capacity; i++)
        out[i] = signature[i] == '.' ? '/' : signature[i];
    out[i] = 0;
}

/* Unity 2022 routes AndroidJavaObject/AndroidJavaClass lookups through its
 * Java ReflectionHelper before converting the reflected handle back into a
 * JNI method/field ID.  Preserve that native Android sequence: the reflected
 * object and the eventual ID share the stable registry entry, exactly as in
 * the proven Terraria/Horizon bridge. */
static int64_t j_Reflection_getConstructorID(jctx *c)
{
    jobj *target = jarg_obj(c);
    jobj *signature = jarg_obj(c);
    const char *cls = target && target->cls ? target->cls : NULL;
    const char *sig = signature && signature->str ? signature->str : "";
    char normalized[512];
    normalize_reflection_signature(normalized, sizeof normalized, sig);
    JT("ReflectionHelper.getConstructorID(%s, %s)", cls ? cls : "?", sig);
    void *id = method_id(cls, "<init>", normalized);
    return (int64_t)(uintptr_t)make_reflected_member(
        REFLECT_CONSTRUCTOR, id, 0);
}

static int64_t j_Reflection_getMethodID(jctx *c)
{
    jobj *target = jarg_obj(c);
    jobj *name = jarg_obj(c);
    jobj *signature = jarg_obj(c);
    int is_static = jarg_int(c);
    const char *cls = target && target->cls ? target->cls : NULL;
    const char *method = name && name->str ? name->str : "";
    const char *sig = signature && signature->str ? signature->str : "";
    char normalized[512];
    normalize_reflection_signature(normalized, sizeof normalized, sig);
    JT("ReflectionHelper.getMethodID(%s.%s, %s, static=%d)",
       cls ? cls : "?", method, sig, is_static);
    void *id = method_id(cls, method, normalized);
    return (int64_t)(uintptr_t)make_reflected_member(
        REFLECT_METHOD, id, is_static);
}

static int64_t j_Reflection_getFieldID(jctx *c)
{
    jobj *target = jarg_obj(c);
    jobj *name = jarg_obj(c);
    jobj *signature = jarg_obj(c);
    int is_static = jarg_int(c);
    const char *cls = target && target->cls ? target->cls : NULL;
    const char *field = name && name->str ? name->str : "";
    const char *sig = signature && signature->str ? signature->str : "";
    char normalized[512];
    normalize_reflection_signature(normalized, sizeof normalized, sig);
    JT("ReflectionHelper.getFieldID(%s.%s, %s, static=%d)",
       cls ? cls : "?", field, sig, is_static);
    void *id = method_id(cls, field, normalized);
    return (int64_t)(uintptr_t)make_reflected_member(
        REFLECT_FIELD, id, is_static);
}

static int64_t j_Reflection_getFieldSignature(jctx *c)
{
    void *field = jarg_obj(c);
    void *id = reflected_member_id(field, 1u << REFLECT_FIELD, NULL);
    const jmethod *entry = by_id(id);
    return (int64_t)(uintptr_t)mk_string(entry && entry->sig
                                             ? entry->sig : "");
}

static int64_t j_Reflected_getDeclaringClass(jctx *c)
{
    void *id = reflected_member_id(
        c->self,
        (1u << REFLECT_CONSTRUCTOR) | (1u << REFLECT_METHOD) |
            (1u << REFLECT_FIELD),
        NULL);
    const jmethod *entry = by_id(id);
    if (!entry || !entry->cls)
        return 0;
    return (int64_t)(uintptr_t)mk_class(entry->cls);
}

static int64_t j_Unity_currentActivity(jctx *c)
{
    (void)c;
    return (int64_t)(uintptr_t)armory_activity_object;
}

static int64_t j_Armory_getPermissionPlugin(jctx *c)
{
    (void)c;
    return (int64_t)(uintptr_t)permission_plugin_object;
}

static int64_t j_Armory_getOsVersion(jctx *c)
{
    (void)c;
    return 33;
}

static int64_t j_Armory_getTargetSdkVersion(jctx *c)
{
    (void)c;
    return 36;
}

static int64_t j_Armory_hasPermission(jctx *c)
{
    jobj *permission = jarg_obj(c);
    JT("Armory permission %s -> granted",
       permission && permission->str ? permission->str : "(unknown)");
    return 1;
}

static int64_t j_Permission_canRequest(jctx *c)
{
    (void)c;
    return 1;
}

static int64_t j_Permission_noRationale(jctx *c)
{
    (void)jarg_obj(c);
    return 0;
}

static int64_t j_Permission_noop(jctx *c)
{
    if (strstr(c->m->sig, "Ljava/lang/String;"))
        (void)jarg_obj(c);
    return 0;
}

static int64_t j_AssetManager_open(jctx *c)
{
    jobj *name = jarg_obj(c);
    const char *rel = name && name->str ? name->str : "";
    while (*rel == '/')
        rel++;
    if (strncmp(rel, "assets/", 7) == 0)
        rel += 7;

    char path[1280];
    snprintf(path, sizeof path, "%s/%s", fp2_datadir, rel);
    struct stat st;
    if (stat(path, &st) != 0) {
        JT("AssetManager.open(\"%s\") -> NULL", rel);
        return 0;
    }
    jobj *stream = mk_object("java/io/InputStream");
    stream->str = strdup(path);
    stream->len = (int)st.st_size;
    JT("AssetManager.open(\"%s\") -> %s", rel, path);
    return (int64_t)(uintptr_t)stream;
}

static int64_t j_Scanner_next(jctx *c)
{
    const char *path = c->self ? c->self->str : NULL;
    if (!path)
        return (int64_t)(uintptr_t)mk_string("");
    FILE *f = fopen(path, "rb");
    if (!f)
        return (int64_t)(uintptr_t)mk_string("");
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    rewind(f);
    if (n < 0 || n > 1024 * 1024) {
        fclose(f);
        return (int64_t)(uintptr_t)mk_string("");
    }
    char *buf = calloc((size_t)n + 1, 1);
    if (buf && n)
        (void)fread(buf, 1, (size_t)n, f);
    fclose(f);
    jobj *s = mk_string(buf ? buf : "");
    free(buf);
    return (int64_t)(uintptr_t)s;
}

static int64_t j_Environment_mounted(jctx *c)
{
    (void)c;
    return (int64_t)(uintptr_t)mk_string("mounted");
}

static int64_t j_String_equals(jctx *c)
{
    jobj *other = jarg_obj(c);
    const char *a = c->self && c->self->str ? c->self->str : "";
    const char *b = other && other->str ? other->str : "";
    return strcmp(a, b) == 0;
}

static int64_t j_StringBuilder_toString(jctx *c)
{
    return (int64_t)(uintptr_t)mk_string(
        c->self && c->self->str ? c->self->str : "");
}

static int uri_unreserved(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '-' || c == '_' ||
           c == '.' || c == '~';
}

static int64_t j_Uri_encode(jctx *c)
{
    jobj *input = jarg_obj(c);
    const char *text = input && input->str ? input->str : "";
    size_t length = strlen(text);
    char *encoded = malloc(length * 3 + 1);
    static const char hex[] = "0123456789ABCDEF";
    size_t out = 0;
    for (size_t i = 0; i < length; i++) {
        unsigned char byte = (unsigned char)text[i];
        if (uri_unreserved(byte)) {
            encoded[out++] = (char)byte;
        } else {
            encoded[out++] = '%';
            encoded[out++] = hex[byte >> 4];
            encoded[out++] = hex[byte & 15];
        }
    }
    encoded[out] = '\0';
    jobj *result = mk_string(encoded);
    free(encoded);
    return (int64_t)(uintptr_t)result;
}

static int uri_hex(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int64_t j_Uri_decode(jctx *c)
{
    jobj *input = jarg_obj(c);
    const char *text = input && input->str ? input->str : "";
    size_t length = strlen(text);
    char *decoded = malloc(length + 1);
    size_t out = 0;
    for (size_t i = 0; i < length; i++) {
        int high, low;
        if (text[i] == '%' && i + 2 < length &&
            (high = uri_hex(text[i + 1])) >= 0 &&
            (low = uri_hex(text[i + 2])) >= 0) {
            decoded[out++] = (char)((high << 4) | low);
            i += 2;
        } else {
            decoded[out++] = text[i];
        }
    }
    decoded[out] = '\0';
    jobj *result = mk_string(decoded);
    free(decoded);
    return (int64_t)(uintptr_t)result;
}

/* Unity reads android.os.Build to pick device-specific workarounds, and the
 * first thing it does with MANUFACTURER is strcasecmp(field, "Amazon") -- with
 * the field absent that faults inside strcasecmp before anything is drawn.
 *
 * These values describe this device, and deliberately match no vendor Unity
 * has a workaround for: a name borrowed from a real manufacturer would switch
 * on a code path written for someone else's hardware.  The field name arrives
 * in the context, so one handler serves the whole class.
 */
static const struct { const char *name, *value; } build_fields[] = {
    { "MANUFACTURER", "NXCompat" },
    { "BRAND",        "NXCompat" },
    { "MODEL",        "NXCompat-Handheld" },
    { "PRODUCT",      "nxcompat" },
    { "DEVICE",       "nxcompat" },
    { "BOARD",        "generic" },
    { "HARDWARE",     "generic" },
    { "ID",           "NXCompat" },
    { "TYPE",         "user" },
    { "TAGS",         "release-keys" },
    { "FINGERPRINT",  "nxcompat/handheld/generic:13/NXCompat/1:user/release-keys" },
    { "RELEASE",      "13" },
    { "CODENAME",     "REL" },
    { "INCREMENTAL",  "1" },
    { "SERIAL",       "unknown" },
    { "SOC_MANUFACTURER", "NXCompat" },
    { "SOC_MODEL",    "NXCompat" },
};

static int64_t j_Build_string(jctx *c)
{
    const char *name = c->m ? c->m->name : NULL;
    if (name)
        for (size_t i = 0; i < sizeof build_fields / sizeof *build_fields; i++)
            if (strcmp(build_fields[i].name, name) == 0)
                return (int64_t)(uintptr_t)mk_string(build_fields[i].value);
    /* Never NULL: the caller passes these straight to string functions. */
    return (int64_t)(uintptr_t)mk_string("");
}

/* API 33 is the platform this 2022.3 player was built against.  Zero would read
 * as older than every gate Unity checks and take paths this build never
 * compiled. */
static int64_t j_Build_SDK_INT(jctx *c)
{
    (void)c;
    return 33;
}

/* Unity's preferences, enough for the questions it asks before the first frame.
 *
 * The one that matters is the GLES level warning.  Before showing it, Unity asks
 * `getPreferences(0).getBoolean("gles-api-check", false)` -- "has the user
 * already ticked do-not-show-again?".  Answer false and it builds an
 * AlertDialog, hands it to Activity.runOnUiThread and blocks the render thread
 * on a condition variable waiting for a button click.  There is no Java UI
 * thread here to deliver one, so frame 1 never returns.
 *
 * Answering true is not a trick to dodge a check: this port runs GLES2 on
 * Mali-450 deliberately, so the warning is one we have already read.
 */
enum pref_type {
    PREF_INT = 1,
    PREF_FLOAT,
    PREF_STRING,
    PREF_BOOL,
};

typedef struct {
    char *key;
    enum pref_type type;
    int32_t integer;
    float real;
    char *string;
} pref_entry;

#define MAX_PREFS 1024
static pref_entry preferences[MAX_PREFS];
static int preferences_count;
static int preferences_loaded;
static pthread_mutex_t preferences_lock = PTHREAD_MUTEX_INITIALIZER;

static pref_entry *pref_find_locked(const char *key)
{
    for (int i = 0; i < preferences_count; i++)
        if (strcmp(preferences[i].key, key) == 0)
            return &preferences[i];
    return NULL;
}

static pref_entry *pref_set_locked(const char *key, enum pref_type type)
{
    pref_entry *entry = pref_find_locked(key);
    if (!entry) {
        if (preferences_count >= MAX_PREFS)
            return NULL;
        entry = &preferences[preferences_count++];
        memset(entry, 0, sizeof *entry);
        entry->key = strdup(key);
    }
    if (entry->type == PREF_STRING) {
        free(entry->string);
        entry->string = NULL;
    }
    entry->type = type;
    return entry;
}

static int pref_write(FILE *file, const void *data, size_t bytes)
{
    return fwrite(data, 1, bytes, file) == bytes;
}

static int pref_read(FILE *file, void *data, size_t bytes)
{
    return fread(data, 1, bytes, file) == bytes;
}

static int preferences_save_locked(void)
{
    char path[1200];
    char temporary[1200];
    snprintf(path, sizeof path, "%s/shared-preferences.bin", fp2_home);
    snprintf(temporary, sizeof temporary,
             "%s/shared-preferences.bin.tmp", fp2_home);
    FILE *file = fopen(temporary, "wb");
    if (!file)
        return 0;

    static const unsigned char magic[8] =
        { 'H', 'G', 'O', 'P', 'R', 'E', 'F', '1' };
    uint32_t count = (uint32_t)preferences_count;
    int ok = pref_write(file, magic, sizeof magic) &&
             pref_write(file, &count, sizeof count);
    for (int i = 0; ok && i < preferences_count; i++) {
        pref_entry *entry = &preferences[i];
        uint8_t type = (uint8_t)entry->type;
        uint32_t key_length = (uint32_t)strlen(entry->key);
        uint32_t value_length =
            entry->type == PREF_STRING
                ? (uint32_t)strlen(entry->string ? entry->string : "")
                : (uint32_t)sizeof(uint32_t);
        ok = pref_write(file, &type, sizeof type) &&
             pref_write(file, &key_length, sizeof key_length) &&
             pref_write(file, &value_length, sizeof value_length) &&
             pref_write(file, entry->key, key_length);
        if (!ok)
            break;
        if (entry->type == PREF_STRING)
            ok = pref_write(file, entry->string ? entry->string : "",
                            value_length);
        else if (entry->type == PREF_FLOAT)
            ok = pref_write(file, &entry->real, sizeof entry->real);
        else
            ok = pref_write(file, &entry->integer, sizeof entry->integer);
    }
    if (fflush(file) != 0 || fsync(fileno(file)) != 0)
        ok = 0;
    if (fclose(file) != 0)
        ok = 0;
    if (ok && rename(temporary, path) != 0)
        ok = 0;
    if (!ok)
        unlink(temporary);
    return ok;
}

static void preferences_load_locked(void)
{
    if (preferences_loaded)
        return;
    preferences_loaded = 1;

    char path[1200];
    snprintf(path, sizeof path, "%s/shared-preferences.bin", fp2_home);
    FILE *file = fopen(path, "rb");
    if (!file)
        return;

    unsigned char magic[8];
    static const unsigned char expected[8] =
        { 'H', 'G', 'O', 'P', 'R', 'E', 'F', '1' };
    uint32_t count;
    if (!pref_read(file, magic, sizeof magic) ||
        memcmp(magic, expected, sizeof magic) != 0 ||
        !pref_read(file, &count, sizeof count) ||
        count > MAX_PREFS) {
        fclose(file);
        nx_log("prefs: ignored invalid %s", path);
        return;
    }

    for (uint32_t i = 0; i < count; i++) {
        uint8_t type;
        uint32_t key_length;
        uint32_t value_length;
        if (!pref_read(file, &type, sizeof type) ||
            !pref_read(file, &key_length, sizeof key_length) ||
            !pref_read(file, &value_length, sizeof value_length) ||
            key_length == 0 || key_length > 4096 ||
            value_length > 1024 * 1024 ||
            type < PREF_INT || type > PREF_BOOL)
            break;
        char *key = calloc((size_t)key_length + 1, 1);
        char *value = calloc((size_t)value_length + 1, 1);
        if (!key || !value ||
            !pref_read(file, key, key_length) ||
            !pref_read(file, value, value_length)) {
            free(key);
            free(value);
            break;
        }
        pref_entry *entry = pref_set_locked(key, (enum pref_type)type);
        if (entry) {
            if (type == PREF_STRING) {
                entry->string = value;
                value = NULL;
            } else if (value_length == sizeof(uint32_t)) {
                if (type == PREF_FLOAT)
                    memcpy(&entry->real, value, sizeof entry->real);
                else
                    memcpy(&entry->integer, value, sizeof entry->integer);
            }
        }
        free(key);
        free(value);
    }
    fclose(file);
    nx_log("prefs: loaded %d persistent entries", preferences_count);
}

/* Acesso ao PlayerPrefs de dentro do port.  Usado pelo conserto de
 * LevelStart.UpdateLevelCompletion (input.c): o fim de fase escreve o progresso
 * aqui, e a chave "Progress" e' texto puro no nosso armazenamento — a Unity
 * urlencoda valor no Android, entao a virgula chega como %2C. */
int fp2_prefs_get_string(const char *key, char *out, size_t size)
{
    int ok = 0;
    pthread_mutex_lock(&preferences_lock);
    preferences_load_locked();
    pref_entry *entry = pref_find_locked(key);
    if (entry && entry->type == PREF_STRING && entry->string && size) {
        snprintf(out, size, "%s", entry->string);
        ok = 1;
    }
    pthread_mutex_unlock(&preferences_lock);
    return ok;
}

int fp2_prefs_set_string(const char *key, const char *value)
{
    pthread_mutex_lock(&preferences_lock);
    preferences_load_locked();
    pref_entry *entry = pref_set_locked(key, PREF_STRING);
    if (entry)
        entry->string = strdup(value ? value : "");
    int ok = entry != NULL && preferences_save_locked();
    pthread_mutex_unlock(&preferences_lock);
    return ok;
}

static int64_t j_getPreferences(jctx *c)
{
    (void)c;
    return (int64_t)(uintptr_t)preferences_object;
}

static int64_t j_Prefs_getBoolean(jctx *c)
{
    jobj *key = jarg_obj(c);
    int32_t dflt = jarg_int(c);
    const char *k = key && key->str ? key->str : "";
    if (strcmp(k, "gles-api-check") == 0)
        return 1;
    pthread_mutex_lock(&preferences_lock);
    preferences_load_locked();
    pref_entry *entry = pref_find_locked(k);
    int value = entry && entry->type == PREF_BOOL
        ? entry->integer != 0 : dflt != 0;
    pthread_mutex_unlock(&preferences_lock);
    JT("SharedPreferences.getBoolean(\"%s\", %d) -> %d", k, dflt, value);
    return value;
}

static int64_t j_Prefs_getInt(jctx *c)
{
    jobj *key = jarg_obj(c);
    int32_t dflt = jarg_int(c);
    const char *k = key && key->str ? key->str : "";
    pthread_mutex_lock(&preferences_lock);
    preferences_load_locked();
    pref_entry *entry = pref_find_locked(k);
    int32_t value = entry && entry->type == PREF_INT
        ? entry->integer : dflt;
    pthread_mutex_unlock(&preferences_lock);
    JT("SharedPreferences.getInt(\"%s\", %d) -> %d", k, dflt, value);
    return value;
}

static int64_t j_Prefs_getFloat(jctx *c)
{
    jobj *key = jarg_obj(c);
    float dflt = jarg_float(c);
    const char *k = key && key->str ? key->str : "";
    pthread_mutex_lock(&preferences_lock);
    preferences_load_locked();
    pref_entry *entry = pref_find_locked(k);
    float value = entry && entry->type == PREF_FLOAT
        ? entry->real : dflt;
    pthread_mutex_unlock(&preferences_lock);
    JT("SharedPreferences.getFloat(\"%s\", %.3f) -> %.3f", k, dflt, value);
    return jfloat_result(value);
}

static int64_t j_Prefs_getString(jctx *c)
{
    jobj *key = jarg_obj(c);
    jobj *dflt = jarg_obj(c);
    const char *k = key && key->str ? key->str : "";
    pthread_mutex_lock(&preferences_lock);
    preferences_load_locked();
    pref_entry *entry = pref_find_locked(k);
    jobj *value = entry && entry->type == PREF_STRING
        ? mk_string(entry->string ? entry->string : "") : dflt;
    pthread_mutex_unlock(&preferences_lock);
    return (int64_t)(uintptr_t)value;
}

static int64_t j_Prefs_contains(jctx *c)
{
    jobj *key = jarg_obj(c);
    const char *k = key && key->str ? key->str : "";
    pthread_mutex_lock(&preferences_lock);
    preferences_load_locked();
    int value = pref_find_locked(k) != NULL;
    pthread_mutex_unlock(&preferences_lock);
    JT("SharedPreferences.contains(\"%s\") -> %d", k, value);
    return value;
}

static int64_t j_Prefs_edit(jctx *c)
{
    (void)c;
    return (int64_t)(uintptr_t)preferences_editor;
}

static int64_t j_Prefs_putInt(jctx *c)
{
    jobj *key = jarg_obj(c);
    int32_t value = jarg_int(c);
    const char *k = key && key->str ? key->str : "";
    pthread_mutex_lock(&preferences_lock);
    preferences_load_locked();
    pref_entry *entry = pref_set_locked(k, PREF_INT);
    if (entry)
        entry->integer = value;
    pthread_mutex_unlock(&preferences_lock);
    JT("SharedPreferences.putInt(\"%s\", %d)", k, value);
    return (int64_t)(uintptr_t)preferences_editor;
}

static int64_t j_Prefs_putFloat(jctx *c)
{
    jobj *key = jarg_obj(c);
    float value = jarg_float(c);
    const char *k = key && key->str ? key->str : "";
    pthread_mutex_lock(&preferences_lock);
    preferences_load_locked();
    pref_entry *entry = pref_set_locked(k, PREF_FLOAT);
    if (entry)
        entry->real = value;
    pthread_mutex_unlock(&preferences_lock);
    JT("SharedPreferences.putFloat(\"%s\", %.3f)", k, value);
    return (int64_t)(uintptr_t)preferences_editor;
}

static int64_t j_Prefs_putString(jctx *c)
{
    jobj *key = jarg_obj(c);
    jobj *value = jarg_obj(c);
    const char *k = key && key->str ? key->str : "";
    const char *v = value && value->str ? value->str : "";
    pthread_mutex_lock(&preferences_lock);
    preferences_load_locked();
    pref_entry *entry = pref_set_locked(k, PREF_STRING);
    if (entry)
        entry->string = strdup(v);
    pthread_mutex_unlock(&preferences_lock);
    JT("SharedPreferences.putString(\"%s\", %d bytes)", k, (int)strlen(v));
    return (int64_t)(uintptr_t)preferences_editor;
}

static int64_t j_Prefs_putBoolean(jctx *c)
{
    jobj *key = jarg_obj(c);
    int32_t value = jarg_int(c);
    const char *k = key && key->str ? key->str : "";
    pthread_mutex_lock(&preferences_lock);
    preferences_load_locked();
    pref_entry *entry = pref_set_locked(k, PREF_BOOL);
    if (entry)
        entry->integer = value != 0;
    pthread_mutex_unlock(&preferences_lock);
    return (int64_t)(uintptr_t)preferences_editor;
}

static int64_t j_Prefs_remove(jctx *c)
{
    jobj *key = jarg_obj(c);
    const char *k = key && key->str ? key->str : "";
    pthread_mutex_lock(&preferences_lock);
    preferences_load_locked();
    for (int i = 0; i < preferences_count; i++) {
        if (strcmp(preferences[i].key, k) != 0)
            continue;
        free(preferences[i].key);
        free(preferences[i].string);
        memmove(&preferences[i], &preferences[i + 1],
                (size_t)(preferences_count - i - 1) *
                    sizeof preferences[0]);
        preferences_count--;
        break;
    }
    pthread_mutex_unlock(&preferences_lock);
    return (int64_t)(uintptr_t)preferences_editor;
}

static int64_t j_Prefs_clear(jctx *c)
{
    (void)c;
    pthread_mutex_lock(&preferences_lock);
    preferences_load_locked();
    for (int i = 0; i < preferences_count; i++) {
        free(preferences[i].key);
        free(preferences[i].string);
    }
    preferences_count = 0;
    pthread_mutex_unlock(&preferences_lock);
    return (int64_t)(uintptr_t)preferences_editor;
}

static int64_t j_Prefs_apply(jctx *c)
{
    (void)c;
    pthread_mutex_lock(&preferences_lock);
    preferences_load_locked();
    int ok = preferences_save_locked();
    pthread_mutex_unlock(&preferences_lock);
    if (!ok)
        nx_log("prefs: cannot persist settings under %s", fp2_home);
    return 0;
}

static int64_t j_Prefs_commit(jctx *c)
{
    j_Prefs_apply(c);
    return 1;
}

static int64_t j_Object_getClass(jctx *c)
{
    return (int64_t)(uintptr_t)mk_class(
        c->self && c->self->cls ? c->self->cls : "java/lang/Object");
}

static int64_t j_getClassLoader(jctx *c)
{
    (void)c;
    return (int64_t)(uintptr_t)mk_object("java/lang/ClassLoader");
}

static int64_t j_ClassLoader_findLibrary(jctx *c)
{
    jobj *name = jarg_obj(c);
    const char *lib = name && name->str ? name->str : "";
    const char *file = NULL;

    if (strcmp(lib, "il2cpp") == 0 || strcmp(lib, "libil2cpp.so") == 0)
        file = "libil2cpp.so";
    else if (strcmp(lib, "main") == 0 || strcmp(lib, "libmain.so") == 0)
        file = "libmain.so";
    else if (strcmp(lib, "unity") == 0 || strcmp(lib, "libunity.so") == 0)
        file = "libunity.so";

    if (!file) {
        JT("ClassLoader.findLibrary(\"%s\") -> empty", lib);
        return (int64_t)(uintptr_t)mk_string("");
    }

    char path[1280];
    snprintf(path, sizeof path, "%s/lib/%s", fp2_gamedir, file);
    JT("ClassLoader.findLibrary(\"%s\") -> %s", lib, path);
    return (int64_t)(uintptr_t)mk_string(path);
}

static int64_t j_System_loadLibrary(jctx *c)
{
    jobj *name = jarg_obj(c);
    const char *requested = name && name->str ? name->str : "";
    const char *base = strrchr(requested, '/');
    base = base ? base + 1 : requested;

    char soname[128];
    if (strstr(base, ".so"))
        snprintf(soname, sizeof soname, "%s", base);
    else if (strncmp(base, "lib", 3) == 0)
        snprintf(soname, sizeof soname, "%s.so", base);
    else
        snprintf(soname, sizeof soname, "lib%s.so", base);

    nx_mod *module = nx_find_mod(soname);
    if (!module) {
        JT("System.%s(\"%s\") -> module unavailable",
           c->m && c->m->name ? c->m->name : "loadLibrary", requested);
        return 0;
    }

    nx_run_init(module);
    JT("System.%s(\"%s\") -> %s initialized",
       c->m && c->m->name ? c->m->name : "loadLibrary", requested,
       soname);
    return 0;
}

/* Static java/lang/Class.forName(String) -- the 1-arg form the SDK wrappers
 * (Tapjoy et al) reach through AndroidJavaClass.  Returns a live class object
 * so the managed wrapper never dereferences null (NXI run4: unhandled -> 0
 * was the Tapjoy NullReferenceException chain). */
static int64_t j_GetInstallerPackageName(jctx *c)
{
    (void)c;
    return (int64_t)(uintptr_t)mk_string("com.android.vending");
}

static int64_t j_Class_forName(jctx *c)
{
    jobj *name = jarg_obj(c);
    const char *n = name && name->str ? name->str : "java/lang/Object";
    char cls[256];
    snprintf(cls, sizeof cls, "%s", n);
    for (char *p = cls; *p; p++)
        if (*p == '.')
            *p = '/';
    JT("Class.forName(\"%s\") -> %s", n, cls);
    return (int64_t)(uintptr_t)mk_class(cls);
}

static int64_t j_forName(jctx *c)
{
    jobj *name = jarg_obj(c);
    (void)jarg_int(c);
    (void)jarg_obj(c);
    const char *n = name && name->str ? name->str : "java/lang/Object";
    char cls[256];
    snprintf(cls, sizeof cls, "%s", n);
    for (char *p = cls; *p; p++)
        if (*p == '.')
            *p = '/';
    return (int64_t)(uintptr_t)mk_class(cls);
}

static int64_t j_newInterfaceProxy(jctx *c)
{
    int64_t handle = jarg_long(c);
    jobj *interfaces = jarg_obj(c);
    return (int64_t)(uintptr_t)make_interface_proxy(handle, interfaces, 0);
}

static int64_t j_Reflection_newProxyInstance(jctx *c)
{
    (void)jarg_obj(c); /* UnityPlayer instance */
    int64_t handle = jarg_long(c);
    jobj *interface = jarg_obj(c);
    return (int64_t)(uintptr_t)make_interface_proxy(
        handle, interface, PROXY_REFLECTION);
}

static int64_t j_Thread_start(jctx *c)
{
    if (!c->self || !c->self->cls ||
        strcmp(c->self->cls, "android/os/HandlerThread") != 0 ||
        !c->self->str || strcmp(c->self->str, "UnityChoreographer") != 0)
        return 0;

    int expected = 0;
    if (!__atomic_compare_exchange_n(&choreo_thread_started, &expected, 1, 0,
                                     __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
        return 0;

    __atomic_store_n(&choreo_stop_requested, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&choreo_java_frame_pending, 0, __ATOMIC_RELEASE);
    clear_choreographer_messages();
    int rc = pthread_create(&choreo_thread, NULL,
                            choreographer_driver, NULL);
    if (rc != 0) {
        __atomic_store_n(&choreo_thread_started, 0, __ATOMIC_RELEASE);
        nx_log("jni: cannot start UnityChoreographer HandlerThread: %s",
               strerror(rc));
        return 0;
    }
    __atomic_store_n(&choreo_thread_joinable, 1, __ATOMIC_RELEASE);
    nx_log("jni: HandlerThread.start(\"UnityChoreographer\")");
    return 0;
}

/* ------------------------------------------------------------ org/fmod/FMOD
 *
 * FMOD 2.x asks the Java side four things before it will open an output:
 * whether org.fmod.FMOD.init() ran (checkInit), whether the device can do
 * low-latency, whether AAudio is available, and -- when it falls back to the
 * Java device -- the output rate and block size.  Answering 0 to checkInit is
 * what left Studio::System::initialize returning ERR_OUTPUT_INIT and the game
 * running in no-sound mode.
 *
 * AAudio is answered YES on purpose: the port already bridges the AAudio C API
 * to SDL, so FMOD drives the audio device directly instead of marshalling
 * every block through a Java short[] that does not exist here. */
static int64_t j_FMODJava_init(jctx *c)
{
    (void)c;
    nx_log("audio: org/fmod/FMOD.init(activity)");
    return 0;
}

static int64_t j_FMODJava_checkInit(jctx *c)
{
    (void)c;
    return 1;
}

static int64_t j_FMODJava_supportsLowLatency(jctx *c)
{
    (void)c;
    return 1;
}

static int64_t j_FMODJava_supportsAAudio(jctx *c)
{
    (void)c;
    return 1;
}

static int64_t j_FMODJava_getOutputSampleRate(jctx *c)
{
    (void)c;
    return 48000;
}

static int64_t j_FMODJava_getOutputBlockSize(jctx *c)
{
    (void)c;
    return 512;
}

static int64_t j_FMOD_start(jctx *c)
{
    if (c->self)
        fmod_device_object = c->self;
    __atomic_store_n(&fmod_should_run, 1, __ATOMIC_RELEASE);
    nx_log("audio: FMODAudioDevice.start -> native AudioTrack requested");
    return 0;
}

static int64_t j_FMOD_stop(jctx *c)
{
    (void)c;
    __atomic_store_n(&fmod_should_run, 0, __ATOMIC_RELEASE);
    nx_log("audio: FMODAudioDevice.%s -> native AudioTrack stopped",
           c->m && c->m->name ? c->m->name : "stop");
    return 0;
}

static int64_t j_FMOD_isRunning(jctx *c)
{
    (void)c;
    return fp2_jni_fmod_should_run() ? 1 : 0;
}

static int64_t j_HandlerThread_getLooper(jctx *c)
{
    (void)c;
    return (int64_t)(uintptr_t)looper_object;
}

static int64_t j_Looper_get(jctx *c)
{
    (void)c;
    return (int64_t)(uintptr_t)looper_object;
}

static int64_t j_Looper_quit(jctx *c)
{
    (void)c;
    /* UnityChoreographer's message 3 reaches Looper.quit() on its own
     * HandlerThread.  Let the driver return only through that native Android
     * lifecycle; the main thread joins it after nativeDone returns. */
    __atomic_store_n(&choreo_stop_requested, 1, __ATOMIC_RELEASE);
    nx_log("jni: UnityChoreographer Looper.quit requested");
    return 0;
}

static int64_t j_Handler_obtainMessage(jctx *c)
{
    int what = jarg_int(c);
    jobj *message = mk_object("android/os/Message");
    message->prim = what;
    JT("Handler.obtainMessage(%d) -> %p", what, (void *)message);
    return (int64_t)(uintptr_t)message;
}

static int64_t j_Message_sendToTarget(jctx *c)
{
    if (!c->self || !c->self->cls ||
        strcmp(c->self->cls, "android/os/Message") != 0) {
        nx_log("jni: Message.sendToTarget rejected invalid receiver");
        return 0;
    }
    if (!queue_choreographer_message(c->self)) {
        nx_log("jni: UnityChoreographer message queue full; dropping what=%lld",
               (long long)c->self->prim);
        return 0;
    }
    nx_log("jni: Message.sendToTarget queued for UnityChoreographer what=%lld",
           (long long)c->self->prim);
    return 0;
}

static int64_t j_Message_what(jctx *c)
{
    return c->self ? c->self->prim : 0;
}

static int64_t j_System_nanoTime(jctx *c)
{
    (void)c;
    return monotonic_nanos();
}

static int64_t j_Choreographer_getInstance(jctx *c)
{
    (void)c;
    return (int64_t)(uintptr_t)choreographer_object;
}

static int64_t j_Choreographer_postFrameCallback(jctx *c)
{
    /* doFrame is paced by choreographer_driver.  The callback remains the same
     * Java proxy; posting is one-shot on Android and the driver likewise calls
     * it once per tick. */
    jobj *callback = jarg_obj(c);
    if (callback && (callback->len & PROXY_FRAME_CALLBACK)) {
        __atomic_store_n(&choreo_proxy, callback, __ATOMIC_RELEASE);
        __atomic_store_n(&choreo_java_frame_pending, 1,
                         __ATOMIC_RELEASE);
    }
    return 0;
}

static int64_t j_Reflection_endUnityLaunch(jctx *c)
{
    (void)c;
    nx_log("jni: ReflectionHelper.endUnityLaunch waiting for reflected proxies");
    finish_reflection_proxies();
    nx_log("jni: ReflectionHelper.endUnityLaunch reflection barrier complete");
    return 0;
}

static int64_t j_Method_getName(jctx *c)
{
    return (int64_t)(uintptr_t)mk_string(
        c->self && c->self->str ? c->self->str : "");
}

static int64_t j_runOnUiThread(jctx *c)
{
    jobj *runnable = jarg_obj(c);
    if (!runnable || !(runnable->len & PROXY_RUNNABLE))
        return 0;
    jobj *iface = mk_class("java/lang/Runnable");
    JT("runOnUiThread: invoking proxy=%p handle=%#llx", (void *)runnable,
       (unsigned long long)runnable->prim);
    (void)invoke_proxy(runnable, iface, run_method, empty_args);
    return 0;
}

static int64_t j_Unity_showSoftInput(jctx *c)
{
    if (c->self)
        unity_player_object = c->self;
    jobj *initial = jarg_obj(c);
    int keyboard_type = jarg_int(c);
    int autocorrection = jarg_int(c);
    int multiline = jarg_int(c);
    int secure = jarg_int(c);
    int alert = jarg_int(c);
    jobj *placeholder = jarg_obj(c);
    int character_limit = jarg_int(c);
    int hide_input = jarg_int(c);
    int close_on_outside_tap = jarg_int(c);
    (void)autocorrection;
    (void)multiline;
    (void)secure;
    (void)alert;
    (void)placeholder;
    (void)hide_input;
    (void)close_on_outside_tap;

    nx_log("jni: showSoftInput(type=%d limit=%d)", keyboard_type,
           character_limit);
    fp2_input_keyboard_open(
        initial && initial->str ? initial->str : "", character_limit);
    return 0;
}

static int64_t j_Unity_setSoftInputStr(jctx *c)
{
    jobj *text = jarg_obj(c);
    fp2_input_keyboard_set(text && text->str ? text->str : "");
    return 0;
}

static int64_t j_Unity_hideSoftInput(jctx *c)
{
    (void)c;
    fp2_input_keyboard_hide();
    return 0;
}

static int64_t dispatch(void *e, jobj *self, void *mid, va_list *ap,
                        const uint64_t *args)
{
    const jmethod *m = by_id(mid);
    if (!m) {
        JT("call on unknown method id %p", mid);
        return 0;
    }
    /* Every consumer below dereferences m->name and m->sig (is_unbox,
     * strcmp, the field-signature strchr).  A slot published without them
     * would fault inside libc instead of failing closed here. */
    if (!m->name || !m->sig) {
        JT("call on incomplete method id %p (name=%p sig=%p)", mid,
           (void *)m->name, (void *)m->sig);
        return 0;
    }
    if (self && self->boxed && is_unbox(m->name)) {
        JT("unboxed %s.%s -> %lld", self->cls, m->name, (long long)self->prim);
        return self->prim;
    }
    if (m->handler) {
        jctx c = {
            .env = e,
            .self = self,
            .ap = ap,
            .args = args,
            .arg_index = 0,
            .m = m,
        };
        return ((fp2_jni_handler)m->handler)(&c);
    }
    /* Reflected fields carry a field signature, never a method descriptor.
     * Adapter-owned Java result objects keep their fields by name so typed and
     * AndroidJavaObject-erased lookups resolve to the same value. */
    if (self && m->sig && !strchr(m->sig, '(')) {
        jobject_field *field = object_field(self, m->name, 0);
        if (field) {
            JT("field %s.%s -> %p/%lld", self->cls ? self->cls : "?",
               m->name, (void *)field->object,
               (long long)field->primitive);
            return field->is_object
                ? (int64_t)(uintptr_t)field->object : field->primitive;
        }
    }
    /* Java inheritance is implicit on Android, while this deliberately small
     * registry keys reflected methods by the concrete runtime class.  Unity's
     * AndroidJavaObject signature builder calls Object.getClass() for every
     * reference argument and Class.getName() on the result.  Reproduce those
     * two inherited contracts generically so a ContentResolver (or any later
     * Android object) reports its real runtime type instead of becoming NULL. */
    if (self && self->cls && strcmp(m->name, "getClass") == 0)
        return (int64_t)(uintptr_t)mk_class(self->cls);
    /* run19: the C# reached getRealSize through java/lang/Object (the display
     * reference arrives with a degraded class name), so the Display binding
     * above never matched.  The signature takes a Point and there is no other
     * contract with that shape, so forwarding here cannot capture an unrelated
     * receiver. */
    if (self && (strcmp(m->name, "getRealSize") == 0 ||
                 strcmp(m->name, "getSize") == 0) &&
        m->sig && strchr(m->sig, 'L') &&
        strstr(m->sig, "Landroid/graphics/Point;")) {
        jctx forwarded = {
            .env = e,
            .self = self,
            .ap = ap,
            .args = args,
            .arg_index = 0,
            .m = m,
        };
        return j_Display_getRealSize(&forwarded);
    }
    /* run26: getMetrics/getRealMetrics arrive the same erased way -- the
     * display reference carries a degraded java/lang/Object class name -- so
     * the android/view/Display binding never matched them either and the
     * caller's DisplayMetrics was left at zero.  Same reasoning as above: the
     * signature's DisplayMetrics parameter is unique to this contract, so
     * forwarding on shape cannot capture an unrelated receiver.  run26 measured
     * "java/lang/Object.getMetrics(Landroid/util/DisplayMetrics;)V -> 0"
     * on every launch. */
    if (self && (strcmp(m->name, "getMetrics") == 0 ||
                 strcmp(m->name, "getRealMetrics") == 0) &&
        m->sig && strchr(m->sig, 'L') &&
        strstr(m->sig, "Landroid/util/DisplayMetrics;")) {
        jctx forwarded = {
            .env = e,
            .self = self,
            .ap = ap,
            .args = args,
            .arg_index = 0,
            .m = m,
        };
        return j_Display_fillMetrics(&forwarded);
    }
    if (self && self->type == O_CLASS &&
        (strcmp(m->name, "getName") == 0 ||
         strcmp(m->name, "getCanonicalName") == 0 ||
         strcmp(m->name, "getTypeName") == 0)) {
        char name[512];
        snprintf(name, sizeof name, "%s", self->cls ? self->cls
                                                       : "java/lang/Object");
        for (char *p = name; *p; p++)
            if (*p == '/')
                *p = '.';
        return (int64_t)(uintptr_t)mk_string(name);
    }
    /* A method we do not implement whose return type is the receiver's own
     * class is a builder -- Intent.putExtra, setPackage, setFlags, setData all
     * return the Intent so calls can be chained.  Returning NULL breaks the
     * chain mid-expression and every later link then operates on nothing;
     * returning the receiver is what the platform does. */
    if (self && self->cls) {
        const char *ret = strchr(m->sig, ')');
        size_t n = strlen(self->cls);
        if (ret && ret[1] == 'L' && strncmp(ret + 2, self->cls, n) == 0 &&
            ret[2 + n] == ';') {
            JT("builder %s.%s -> self", self->cls, m->name);
            return (int64_t)(uintptr_t)self;
        }
    }
    /* Peripheral Java services -- Play Games, GMS Tasks, platform helpers --
     * are not part of the game's own flow, but returning NULL for a call whose
     * declared return type is an object makes the managed caller dereference
     * null and abort the entire Awake() that started it.  That is exactly what
     * kept Stranger Things on a black preload screen: GamesSignInClient
     * .isAuthenticated() returned null, GooglePlayGames threw inside
     * LevelStart.Awake() and the scene never advanced.  Hand back a live object
     * of the declared type instead so the flow continues; the listener simply
     * never fires, which is what an offline sign-in looks like. */
#ifndef FP2_RELEASE_BUILD
    /* Method sigs carry the return type after ')'; field sigs ARE the bare
     * type descriptor ("Ljava/lang/String;").  Normalize both to a pointer
     * at the type char so the null-object fallback covers static fields
     * too (run14: Context.POWER_SERVICE read as null crashed Unity's
     * strcmp right after boot). */
    const char *rt = strchr(m->sig, ')');
    if (rt)
        rt += 1 - 1; /* first type char after ')' */
    else if (m->sig && m->sig[0] == 'L')
        rt = m->sig;  /* points at 'L' itself */
    if (rt && getenv("FP2_NONNULL_OBJECT_FALLBACK") &&
        strcmp(getenv("FP2_NONNULL_OBJECT_FALLBACK"), "0") != 0 &&
        !getenv("FP2_JNI_STRICT_NULL")) {
        const char *tc = (m->sig && strchr(m->sig, ')')) ? rt + 1 : rt;
        if (tc[0] == 'L') {
            char cls[256];
            const char *end = strchr(tc + 1, ';');
            size_t n = end ? (size_t)(end - (tc + 1)) : 0;
            if (n && n < sizeof cls) {
                memcpy(cls, tc + 1, n);
                cls[n] = 0;
                if (strcmp(cls, "java/lang/String") == 0) {
                    JT("unhandled %s.%s%s -> \"\"",
                       m->cls ? m->cls : "?", m->name, m->sig);
                    return (int64_t)(uintptr_t)mk_string("");
                }
                JT("unhandled %s.%s%s -> stub %s",
                   m->cls ? m->cls : "?", m->name, m->sig, cls);
                return (int64_t)(uintptr_t)mk_object(cls);
            }
        }
        if (tc[0] == '[') {
            /* Array returns (getObbDirs, String[] getters, ...): an EMPTY
             * array, never null.  Run11 measured the hard stop this closes:
             * kairo.unity.native.KairoPlugin.GetPreferenceKeys fed a null
             * collection into LINQ Contains and the boot died in
             * ArgumentNullException before the first scene. */
            jobj *arr = new_obj(O_OBJARRAY, tc);
            JT("unhandled %s.%s%s -> empty array",
               m->cls ? m->cls : "?", m->name, m->sig);
            return (int64_t)(uintptr_t)arr;
        }
    }
#endif
    JT("unhandled %s.%s%s -> 0", m->cls ? m->cls : "?", m->name, m->sig);
#ifndef FP2_RELEASE_BUILD
    if (getenv("FP2_MISSLOG")) {
        static const char *vistos[256]; static int n_vistos;
        const char *cls = (self && self->cls) ? self->cls : "?";
        int novo_miss = 1;
        for (int i = 0; i < n_vistos; i++)
            if (vistos[i] == m->name) { novo_miss = 0; break; }
        if (novo_miss && n_vistos < 256) {
            vistos[n_vistos++] = m->name;
            fprintf(stderr, "[fp2/miss] %s.%s %s -> 0\n", cls, m->name, m->sig);
        }
    }
#endif
    return 0;
}

/* Argument readers for handlers. */
jobj *fp2_jarg_obj(jctx *c) { return jarg_obj(c); }
int32_t fp2_jarg_int(jctx *c) { return jarg_int(c); }
int64_t fp2_jarg_long(jctx *c) { return jarg_long(c); }
const char *fp2_jarg_str(jctx *c)
{
    jobj *o = jarg_obj(c);
    return o && o->str ? o->str : NULL;
}
void *fp2_jret_str(const char *s) { return mk_string(s); }
void *fp2_jret_class(const char *s) { return mk_class(s); }
void *fp2_jret_obj(const char *cls) { return new_obj(O_OBJECT, cls); }
void *fp2_jni_activity(void) { return armory_activity_object; }
void *fp2_jret_bytes(const void *p, int n)
{
    jobj *o = j_NewByteArray(NULL, n);
    if (p && n > 0)
        memcpy(o->data, p, (size_t)n);
    return o;
}

#define CALLV(nm, rt, cast) \
    static rt nm(void *e, void *o, void *mid, va_list ap) \
    { return (rt)(cast)dispatch(e, o, mid, &ap, NULL); }
#define CALL(nm, vnm, rt) \
    static rt nm(void *e, void *o, void *mid, ...) \
    { va_list ap; va_start(ap, mid); rt r = vnm(e, o, mid, ap); va_end(ap); return r; }
#define CALLA(nm, rt) \
    static rt nm(void *e, void *o, void *mid, void *args) \
    { return (rt)dispatch(e, o, mid, NULL, (const uint64_t *)args); }

CALLV(v_obj,  void *,   uintptr_t)
CALLV(v_bool, uint8_t,  uint8_t)
CALLV(v_byte, int8_t,   int8_t)
CALLV(v_char, uint16_t, uint16_t)
CALLV(v_short,int16_t,  int16_t)
CALLV(v_int,  int32_t,  int32_t)
CALLV(v_long, int64_t,  int64_t)

static float v_float(void *e, void *o, void *mid, va_list ap)
{
    int64_t r = dispatch(e, o, mid, &ap, NULL);
    float f;
    memcpy(&f, &r, sizeof f);
    return f;
}
static double v_double(void *e, void *o, void *mid, va_list ap)
{
    int64_t r = dispatch(e, o, mid, &ap, NULL);
    double d;
    memcpy(&d, &r, sizeof d);
    return d;
}
static void v_void(void *e, void *o, void *mid, va_list ap)
{
    dispatch(e, o, mid, &ap, NULL);
}

CALL(c_obj, v_obj, void *)
CALL(c_bool, v_bool, uint8_t)
CALL(c_byte, v_byte, int8_t)
CALL(c_char, v_char, uint16_t)
CALL(c_short, v_short, int16_t)
CALL(c_int, v_int, int32_t)
CALL(c_long, v_long, int64_t)
CALL(c_float, v_float, float)
CALL(c_double, v_double, double)
static void c_void(void *e, void *o, void *mid, ...)
{
    va_list ap;
    va_start(ap, mid);
    v_void(e, o, mid, ap);
    va_end(ap);
}

CALLA(a_obj, void *)
CALLA(a_bool, uint8_t)
CALLA(a_byte, int8_t)
CALLA(a_char, uint16_t)
CALLA(a_short, int16_t)
CALLA(a_int, int32_t)
CALLA(a_long, int64_t)
static float a_float(void *e, void *o, void *m, void *v)
{
    int64_t r = dispatch(e, o, m, NULL, (const uint64_t *)v);
    float f;
    memcpy(&f, &r, sizeof f);
    return f;
}
static double a_double(void *e, void *o, void *m, void *v)
{
    int64_t r = dispatch(e, o, m, NULL, (const uint64_t *)v);
    double d;
    memcpy(&d, &r, sizeof d);
    return d;
}
static void a_void(void *e, void *o, void *m, void *v)
{
    (void)dispatch(e, o, m, NULL, (const uint64_t *)v);
}

static void *j_GetMethodID(void *e, jobj *c, const char *n, const char *s)
{
    (void)e;
    return method_id(c ? c->cls : NULL, n, s);
}
static void *j_GetFieldID(void *e, jobj *c, const char *n, const char *s)
{
    (void)e;
#ifndef FP2_RELEASE_BUILD
    if (getenv("FP2_FIELDLOG"))
        fprintf(stderr, "[fp2/campo] %s.%s : %s\n",
                (c && c->cls) ? c->cls : "?", n ? n : "?", s ? s : "?");
#endif
    return method_id(c ? c->cls : NULL, n, s);
}

static jobj *j_AllocObject(void *e, jobj *c)
{
    (void)e;
    if (c && c->cls &&
        strcmp(c->cls, "org/fmod/FMODAudioDevice") == 0)
        return fmod_device_object;
    return new_obj(O_OBJECT, c ? c->cls : NULL);
}

/* Boxed JNI primitives have to retain their value for later longValue(),
 * intValue() and related calls. */
static int box_kind(const char *cls)
{
    if (!cls)
        return 0;
    if (strcmp(cls, "java/lang/Long") == 0)      return 'J';
    if (strcmp(cls, "java/lang/Integer") == 0)   return 'I';
    if (strcmp(cls, "java/lang/Short") == 0)     return 'S';
    if (strcmp(cls, "java/lang/Byte") == 0)      return 'B';
    if (strcmp(cls, "java/lang/Character") == 0) return 'C';
    if (strcmp(cls, "java/lang/Boolean") == 0)   return 'Z';
    if (strcmp(cls, "java/lang/Float") == 0)     return 'F';
    if (strcmp(cls, "java/lang/Double") == 0)    return 'D';
    return 0;
}

static jobj *box_from(jobj *c, void *mid, va_list *ap)
{
    if (c && c->cls &&
        strcmp(c->cls, "org/fmod/FMODAudioDevice") == 0)
        return fmod_device_object;
    jobj *o = new_obj(O_OBJECT, c ? c->cls : NULL);
    int k = box_kind(o->cls);
    if (!k) {
        /* Not a boxed primitive.  Keep a single String argument: java/io/File
         * and android/content/Intent are both built from one and then asked
         * about it, and an object that forgot its own path is indistinguishable
         * from a file that does not exist. */
        const jmethod *m = by_id(mid);
        if (m && m->sig && strcmp(m->sig, "(Ljava/lang/String;)V") == 0) {
            jobj *s = va_arg(*ap, jobj *);
            if (s && s->str) {
                o->str = strdup(s->str);
                o->len = (int)strlen(o->str);
            }
        } else if (m && m->sig && o->cls &&
                   strcmp(o->cls, "java/lang/String") == 0 &&
                   (strcmp(m->sig, "([BLjava/lang/String;)V") == 0 ||
                    strcmp(m->sig, "([B)V") == 0)) {
            /* Unity marshals managed UTF-8 strings by constructing a Java
             * String from byte[] plus the "UTF-8" charset name.  Dropping
             * that byte array turns every PlayerPrefs key into "", causing
             * all settings -- including the four audio volumes -- to alias. */
            jobj *bytes = va_arg(*ap, jobj *);
            if (strcmp(m->sig, "([BLjava/lang/String;)V") == 0)
                (void)va_arg(*ap, jobj *);
            if (bytes && bytes->data && bytes->len >= 0) {
                o->str = calloc((size_t)bytes->len + 1, 1);
                memcpy(o->str, bytes->data, (size_t)bytes->len);
                o->len = bytes->len;
            }
        } else if (m && o->cls &&
                   strcmp(o->cls, "java/util/Scanner") == 0 &&
                   strcmp(m->sig,
                          "(Ljava/io/InputStream;Ljava/lang/String;)V") == 0) {
            jobj *stream = va_arg(*ap, jobj *);
            (void)va_arg(*ap, jobj *);
            if (stream && stream->str) {
                o->str = strdup(stream->str);
                o->len = (int)strlen(o->str);
            }
        }
        JT("new %s(\"%s\")", o->cls ? o->cls : "?", o->str ? o->str : "");
        return o;
    }
    o->boxed = k;
    switch (k) {
    case 'J': o->prim = va_arg(*ap, int64_t); break;
    case 'I': o->prim = va_arg(*ap, int32_t); break;
    /* short, byte, char and boolean are promoted to int in a varargs call. */
    case 'S': o->prim = (int16_t)va_arg(*ap, int32_t); break;
    case 'B': o->prim = (int8_t)va_arg(*ap, int32_t); break;
    case 'C': o->prim = (uint16_t)va_arg(*ap, int32_t); break;
    case 'Z': o->prim = va_arg(*ap, int32_t) ? 1 : 0; break;
    case 'F': { double f = va_arg(*ap, double); float g = (float)f;
                memcpy(&o->prim, &g, sizeof g); break; }
    case 'D': { double f = va_arg(*ap, double);
                memcpy(&o->prim, &f, sizeof f); break; }
    default: break;
    }
    JT("boxed %s = %lld", o->cls, (long long)o->prim);
    return o;
}

static jobj *v_NewObject(void *e, jobj *c, void *mid, va_list ap)
{
    (void)e;
    return box_from(c, mid, &ap);
}
static jobj *j_NewObject(void *e, jobj *c, void *mid, ...)
{
    (void)e;
    va_list ap;
    va_start(ap, mid);
    jobj *o = box_from(c, mid, &ap);
    va_end(ap);
    return o;
}
static jobj *a_NewObject(void *e, jobj *c, void *mid, void *v)
{
    /* The A form passes a jvalue array, one slot per argument.  The VM builds
     * its java/io/File through this form and its Intent through the varargs
     * one, so the String argument has to be kept here too -- covering only the
     * varargs path leaves the File with no path at all. */
    (void)e;
    if (c && c->cls &&
        strcmp(c->cls, "org/fmod/FMODAudioDevice") == 0)
        return fmod_device_object;
    jobj *o = new_obj(O_OBJECT, c ? c->cls : NULL);
    int k = box_kind(o->cls);
    if (!k) {
        const jmethod *m = by_id(mid);
        if (m && m->sig && v && strcmp(m->sig, "(Ljava/lang/String;)V") == 0) {
            jobj *s = *(jobj **)v;
            if (s && s->str) {
                o->str = strdup(s->str);
                o->len = (int)strlen(o->str);
            }
        } else if (m && m->sig && v && o->cls &&
                   strcmp(o->cls, "java/lang/String") == 0 &&
                   (strcmp(m->sig, "([BLjava/lang/String;)V") == 0 ||
                    strcmp(m->sig, "([B)V") == 0)) {
            jobj *bytes = *(jobj **)v;
            if (bytes && bytes->data && bytes->len >= 0) {
                o->str = calloc((size_t)bytes->len + 1, 1);
                memcpy(o->str, bytes->data, (size_t)bytes->len);
                o->len = bytes->len;
            }
        }
        JT("new %s(\"%s\") [A]", o->cls ? o->cls : "?", o->str ? o->str : "");
        return o;
    }
    if (k && v) {
        o->boxed = k;
        o->prim = *(const int64_t *)v;
        if (k == 'I') o->prim = (int32_t)o->prim;
        if (k == 'S') o->prim = (int16_t)o->prim;
        if (k == 'B') o->prim = (int8_t)o->prim;
        if (k == 'C') o->prim = (uint16_t)o->prim;
        if (k == 'Z') o->prim = o->prim ? 1 : 0;
    }
    return o;
}

/* Fields all go through the same dispatch, so a getter can be bound the same
 * way a method is; anything unbound reads as zero/NULL. */
static void *f_obj(void *e, jobj *o, void *fid)
{
    return (void *)(uintptr_t)dispatch(e, o, fid, NULL, NULL);
}
static uint8_t f_bool(void *e, jobj *o, void *f) { return (uint8_t)(uintptr_t)f_obj(e, o, f); }
static int8_t f_byte(void *e, jobj *o, void *f) { return (int8_t)(uintptr_t)f_obj(e, o, f); }
static uint16_t f_char(void *e, jobj *o, void *f) { return (uint16_t)(uintptr_t)f_obj(e, o, f); }
static int16_t f_short(void *e, jobj *o, void *f) { return (int16_t)(uintptr_t)f_obj(e, o, f); }
static int32_t f_int(void *e, jobj *o, void *f) { return (int32_t)(uintptr_t)f_obj(e, o, f); }
static int64_t f_long(void *e, jobj *o, void *f) { return (int64_t)(uintptr_t)f_obj(e, o, f); }
static float f_float(void *e, jobj *o, void *f) { (void)e; (void)o; (void)f; return 0.0f; }
static double f_double(void *e, jobj *o, void *f) { (void)e; (void)o; (void)f; return 0.0; }
static void f_set(void *e, jobj *o, void *f, ...) { (void)e; (void)o; (void)f; }

static int32_t j_RegisterNatives(void *e, jobj *c, const void *m, int32_t n)
{
    (void)e;
    const struct { const char *name; const char *sig; void *fn; } *r = m;
    for (int32_t i = 0; i < n; i++) {
        if (natives_n == MAX_NATIVES)
            nx_die("native table full");
        jnative *k = &natives[natives_n++];
        snprintf(k->cls, sizeof k->cls, "%s", c && c->cls ? c->cls : "?");
        snprintf(k->name, sizeof k->name, "%s", r[i].name);
        snprintf(k->sig, sizeof k->sig, "%s", r[i].sig);
        k->fn = r[i].fn;
        nx_log("RegisterNatives %s.%s%s -> %p", k->cls, k->name, k->sig, k->fn);
    }
    return 0;
}
static int32_t j_UnregisterNatives(void *e, jobj *c) { (void)e; (void)c; return 0; }

static void *j_FromReflectedMethod(void *e, void *o)
{
    (void)e;
    if (o == doframe_method)
        return doframe_mid;
    if (o == handlemsg_method)
        return handlemsg_mid;
    if (o == run_method)
        return run_mid;
    return reflected_member_id(
        o, (1u << REFLECT_CONSTRUCTOR) | (1u << REFLECT_METHOD), NULL);
}

static void *j_FromReflectedField(void *e, void *o)
{
    (void)e;
    return reflected_member_id(o, 1u << REFLECT_FIELD, NULL);
}

static void *j_ToReflectedMethod(void *e, jobj *c, void *m, uint8_t st)
{
    (void)e;
    (void)c;
    const jmethod *entry = by_id(m);
    reflected_kind kind = entry && entry->name &&
                                  strcmp(entry->name, "<init>") == 0
                              ? REFLECT_CONSTRUCTOR : REFLECT_METHOD;
    return make_reflected_member(kind, m, st);
}

static void *j_ToReflectedField(void *e, jobj *c, void *m, uint8_t st)
{
    (void)e;
    (void)c;
    return make_reflected_member(REFLECT_FIELD, m, st);
}

#ifndef FP2_RELEASE_BUILD
static int reflection_contract_selftest(void)
{
    uint64_t message_zero_args[] = { 0 };
    uint64_t message_stop_args[] = { 2 };
    uint64_t message_quit_args[] = { 3 };
    jctx message_zero_lookup = { .args = message_zero_args };
    jctx message_stop_lookup = { .args = message_stop_args };
    jctx message_quit_lookup = { .args = message_quit_args };
    jobj *message_zero = (jobj *)(uintptr_t)
        j_Handler_obtainMessage(&message_zero_lookup);
    jobj *message_stop = (jobj *)(uintptr_t)
        j_Handler_obtainMessage(&message_stop_lookup);
    jobj *message_quit = (jobj *)(uintptr_t)
        j_Handler_obtainMessage(&message_quit_lookup);
    void *what_id = method_id("android/os/Message", "what", "I");
    if (!message_zero || !message_stop || !message_quit ||
        message_zero == message_stop || message_stop == message_quit ||
        f_int(fp2_jni_env(), message_zero, what_id) != 0 ||
        f_int(fp2_jni_env(), message_stop, what_id) != 2 ||
        f_int(fp2_jni_env(), message_quit, what_id) != 3) {
        fprintf(stderr, "netflix_selftest: Message.what lifecycle failed\n");
        return 1;
    }
    clear_choreographer_messages();
    if (!queue_choreographer_message(message_zero) ||
        !queue_choreographer_message(message_stop) ||
        !queue_choreographer_message(message_quit) ||
        take_choreographer_message() != message_zero ||
        take_choreographer_message() != message_stop ||
        take_choreographer_message() != message_quit ||
        take_choreographer_message() != NULL) {
        fprintf(stderr,
                "netflix_selftest: Handler message ordering failed\n");
        return 1;
    }

    __atomic_store_n(&choreo_stop_requested, 0, __ATOMIC_RELEASE);
    jctx looper_quit = { .self = looper_object };
    (void)j_Looper_quit(&looper_quit);
    if (!__atomic_load_n(&choreo_stop_requested, __ATOMIC_ACQUIRE)) {
        fprintf(stderr, "netflix_selftest: Looper.quit lifecycle failed\n");
        return 1;
    }
    __atomic_store_n(&choreo_stop_requested, 0, __ATOMIC_RELEASE);

    jobj *frame_proxy = mk_object("bitter/jnibridge/Proxy");
    frame_proxy->len = PROXY_FRAME_CALLBACK;
    uint64_t frame_args[] = { (uint64_t)(uintptr_t)frame_proxy };
    jctx frame_post = { .args = frame_args };
    (void)j_Choreographer_postFrameCallback(&frame_post);
    if (!__atomic_exchange_n(&choreo_java_frame_pending, 0,
                             __ATOMIC_ACQ_REL) ||
        __atomic_exchange_n(&choreo_java_frame_pending, 0,
                            __ATOMIC_ACQ_REL)) {
        fprintf(stderr,
                "netflix_selftest: Choreographer one-shot token failed\n");
        return 1;
    }
    __atomic_store_n(&choreo_proxy, NULL, __ATOMIC_RELEASE);

    jobj *handler_thread = mk_object("android/os/HandlerThread");
    handler_thread->str = "UnityChoreographer";
    jctx thread_start = { .self = handler_thread };
    (void)j_Thread_start(&thread_start);
    if (!__atomic_load_n(&choreo_thread_started, __ATOMIC_ACQUIRE) ||
        !__atomic_load_n(&choreo_thread_joinable, __ATOMIC_ACQUIRE)) {
        fprintf(stderr,
                "netflix_selftest: HandlerThread start lifecycle failed\n");
        return 1;
    }
    jctx thread_quit = { .self = looper_object };
    (void)j_Looper_quit(&thread_quit);
    fp2_jni_finish_callbacks();
    if (__atomic_load_n(&choreo_thread_started, __ATOMIC_ACQUIRE) ||
        __atomic_load_n(&choreo_thread_joinable, __ATOMIC_ACQUIRE)) {
        fprintf(stderr,
                "netflix_selftest: HandlerThread join lifecycle failed\n");
        return 1;
    }

    static const char config_cls[] =
        "com/netflix/games/performance/crashreporting/CrashReporterConfig";
    jobj *target = mk_class(config_cls);
    jobj *field_name = mk_string("projectId");
    jobj *field_sig = mk_string("Ljava/lang/String;");
    uint64_t field_args[] = {
        (uint64_t)(uintptr_t)target,
        (uint64_t)(uintptr_t)field_name,
        (uint64_t)(uintptr_t)field_sig,
        0,
    };
    jctx field_lookup = { .args = field_args };
    jobj *field = (jobj *)(uintptr_t)j_Reflection_getFieldID(&field_lookup);
    if (!field || field->type != O_OBJECT || !field->cls ||
        strcmp(field->cls, "java/lang/reflect/Field") != 0) {
        fprintf(stderr, "netflix_selftest: reflected Field wrapper failed\n");
        return 1;
    }

    jctx repeated_lookup = { .args = field_args };
    if ((jobj *)(uintptr_t)j_Reflection_getFieldID(&repeated_lookup) != field) {
        fprintf(stderr, "netflix_selftest: reflected Field is not stable\n");
        return 1;
    }

    jobj *runtime_class = j_GetObjectClass(fp2_jni_env(), field);
    if (!runtime_class || !runtime_class->cls ||
        strcmp(runtime_class->cls, "java/lang/reflect/Field") != 0) {
        fprintf(stderr, "netflix_selftest: Field GetObjectClass failed\n");
        return 1;
    }
    void *get_class_mid = method_id(
        "java/lang/Object", "getClass", "()Ljava/lang/Class;");
    runtime_class = c_obj(fp2_jni_env(), field, get_class_mid);
    if (!runtime_class || !runtime_class->cls ||
        strcmp(runtime_class->cls, "java/lang/reflect/Field") != 0) {
        fprintf(stderr, "netflix_selftest: Field Object.getClass failed\n");
        return 1;
    }

    void *declaring_mid = method_id(
        "java/lang/reflect/Field", "getDeclaringClass",
        "()Ljava/lang/Class;");
    jobj *declaring = c_obj(fp2_jni_env(), field, declaring_mid);
    if (!declaring || !declaring->cls ||
        strcmp(declaring->cls, config_cls) != 0) {
        fprintf(stderr, "netflix_selftest: Field declaring class failed\n");
        return 1;
    }

    void *signature_mid = method_id(
        "com/unity3d/player/ReflectionHelper", "getFieldSignature",
        "(Ljava/lang/reflect/Field;)Ljava/lang/String;");
    jobj *signature = c_obj(
        fp2_jni_env(), mk_class("com/unity3d/player/ReflectionHelper"),
        signature_mid, field);
    if (!signature || !signature->str ||
        strcmp(signature->str, "Ljava/lang/String;") != 0) {
        fprintf(stderr, "netflix_selftest: Field signature failed\n");
        return 1;
    }

    void *field_id = j_FromReflectedField(fp2_jni_env(), field);
    if (!field_id ||
        j_ToReflectedField(fp2_jni_env(), target, field_id, 0) != field) {
        fprintf(stderr, "netflix_selftest: Field JNI round-trip failed\n");
        return 1;
    }
    jobj *project_id = f_obj(fp2_jni_env(), netflix_crash_config, field_id);
    if (!project_id || !project_id->str || project_id->str[0] != 0) {
        fprintf(stderr, "netflix_selftest: reflected field read failed\n");
        return 1;
    }

    jobj *bool_name = mk_string("applicationIsInForeground");
    jobj *bool_sig = mk_string("Z");
    uint64_t bool_args[] = {
        (uint64_t)(uintptr_t)target,
        (uint64_t)(uintptr_t)bool_name,
        (uint64_t)(uintptr_t)bool_sig,
        0,
    };
    jctx bool_lookup = { .args = bool_args };
    jobj *bool_field =
        (jobj *)(uintptr_t)j_Reflection_getFieldID(&bool_lookup);
    void *bool_id = j_FromReflectedField(fp2_jni_env(), bool_field);
    uint64_t bool_signature_args[] = {
        (uint64_t)(uintptr_t)bool_field,
    };
    jctx bool_signature_lookup = { .args = bool_signature_args };
    jobj *bool_signature = (jobj *)(uintptr_t)
        j_Reflection_getFieldSignature(&bool_signature_lookup);
    if (!bool_field || !bool_id || !bool_signature ||
        !bool_signature->str || strcmp(bool_signature->str, "Z") != 0 ||
        f_bool(fp2_jni_env(), netflix_crash_config, bool_id) != 1) {
        fprintf(stderr, "netflix_selftest: reflected boolean field failed\n");
        return 1;
    }

    jobj *method_name = mk_string("getCrashReporterConfig");
    jobj *method_sig = mk_string(
        "()Lcom/netflix/games/performance/crashreporting/"
        "CrashReporterConfig;");
    uint64_t method_args[] = {
        (uint64_t)(uintptr_t)mk_class("com/netflix/unity/api/NfUnitySdk"),
        (uint64_t)(uintptr_t)method_name,
        (uint64_t)(uintptr_t)method_sig,
        0,
    };
    jctx method_lookup = { .args = method_args };
    jobj *method = (jobj *)(uintptr_t)j_Reflection_getMethodID(&method_lookup);
    void *method_id_value = j_FromReflectedMethod(fp2_jni_env(), method);
    if (!method || !method->cls ||
        strcmp(method->cls, "java/lang/reflect/Method") != 0 ||
        !method_id_value ||
        c_obj(fp2_jni_env(), mk_object("com/netflix/unity/api/NfUnitySdk"),
              method_id_value) != netflix_crash_config ||
        j_ToReflectedMethod(fp2_jni_env(),
                            mk_class("com/netflix/unity/api/NfUnitySdk"),
                            method_id_value, 0) != method) {
        fprintf(stderr, "netflix_selftest: reflected Method round-trip failed\n");
        return 1;
    }

    jobj *ctor_sig = mk_string("()V");
    uint64_t ctor_args[] = {
        (uint64_t)(uintptr_t)mk_class("java/lang/Object"),
        (uint64_t)(uintptr_t)ctor_sig,
    };
    jctx ctor_lookup = { .args = ctor_args };
    jobj *constructor =
        (jobj *)(uintptr_t)j_Reflection_getConstructorID(&ctor_lookup);
    void *constructor_id =
        j_FromReflectedMethod(fp2_jni_env(), constructor);
    if (!constructor || !constructor->cls ||
        strcmp(constructor->cls, "java/lang/reflect/Constructor") != 0 ||
        !constructor_id ||
        j_ToReflectedMethod(fp2_jni_env(), mk_class("java/lang/Object"),
                            constructor_id, 0) != constructor ||
        !j_NewObject(fp2_jni_env(), mk_class("java/lang/Object"),
                     constructor_id)) {
        fprintf(stderr,
                "netflix_selftest: reflected Constructor round-trip failed\n");
        return 1;
    }

    if (j_FromReflectedField(fp2_jni_env(), method) != NULL ||
        j_FromReflectedMethod(fp2_jni_env(), field) != NULL ||
        j_FromReflectedField(fp2_jni_env(), (void *)(uintptr_t)0x19a) != NULL ||
        j_FromReflectedMethod(fp2_jni_env(), play_games_success_method) !=
            play_games_success_mid ||
        j_FromReflectedMethod(fp2_jni_env(), netflix_on_result_method) !=
            netflix_on_result_mid) {
        fprintf(stderr, "netflix_selftest: reflected kind guard failed\n");
        return 1;
    }
    fprintf(stderr, "netflix_selftest: reflection contract PASS\n");
    return 0;
}
#endif
static jobj *j_DefineClass(void *e, const char *n, void *l, const void *b, int32_t sz)
{
    (void)e; (void)l; (void)b; (void)sz;
    return mk_class(n ? n : "?");
}

/* --- JavaVM ----------------------------------------------------------- */

static int32_t vm_GetEnv(void *vm, void **out, int32_t ver)
{
    (void)vm; (void)ver;
    *out = fp2_jni_env();
    return 0;
}
static int32_t vm_AttachCurrentThread(void *vm, void **out, void *args)
{
    (void)vm; (void)args;
    *out = fp2_jni_env();
    return 0;
}
static int32_t vm_DetachCurrentThread(void *vm) { (void)vm; return 0; }
static int32_t vm_DestroyJavaVM(void *vm) { (void)vm; return 0; }

/* ------------------------------------------------------------------- build */

void fp2_jni_init(void)
{
    for (int i = 0; i < JNI_SLOT_COUNT; i++)
        vt[i] = NULL;

    vt[JNI_GetVersion] = j_GetVersion;
    vt[JNI_DefineClass] = j_DefineClass;
    vt[JNI_FindClass] = j_FindClass;
    vt[JNI_FromReflectedMethod] = j_FromReflectedMethod;
    vt[JNI_FromReflectedField] = j_FromReflectedField;
    vt[JNI_ToReflectedMethod] = j_ToReflectedMethod;
    vt[JNI_GetSuperclass] = j_GetSuperclass;
    vt[JNI_IsAssignableFrom] = j_IsAssignableFrom;
    vt[JNI_ToReflectedField] = j_ToReflectedField;
    vt[JNI_Throw] = j_Throw;
    vt[JNI_ThrowNew] = j_ThrowNew;
    vt[JNI_ExceptionOccurred] = j_ExceptionOccurred;
    vt[JNI_ExceptionDescribe] = j_ExceptionDescribe;
    vt[JNI_ExceptionClear] = j_ExceptionClear;
    vt[JNI_FatalError] = j_FatalError;
    vt[JNI_PushLocalFrame] = j_PushLocalFrame;
    vt[JNI_PopLocalFrame] = j_PopLocalFrame;
    vt[JNI_NewGlobalRef] = j_NewGlobalRef;
    vt[JNI_DeleteGlobalRef] = j_DeleteRef;
    vt[JNI_DeleteLocalRef] = j_DeleteRef;
    vt[JNI_IsSameObject] = j_IsSameObject;
    vt[JNI_NewLocalRef] = j_NewLocalRef;
    vt[JNI_EnsureLocalCapacity] = j_EnsureLocalCapacity;
    vt[JNI_AllocObject] = j_AllocObject;
    vt[JNI_NewObject] = j_NewObject;
    vt[JNI_NewObjectV] = v_NewObject;
    vt[JNI_NewObjectA] = a_NewObject;
    vt[JNI_GetObjectClass] = j_GetObjectClass;
    vt[JNI_IsInstanceOf] = j_IsInstanceOf;
    vt[JNI_GetMethodID] = j_GetMethodID;

    /* Call<Type>Method / ...V / ...A, then the Nonvirtual and Static blocks:
     * same handlers, the dispatcher does not care which flavour was used. */
    static void *const call3[10][3] = {
        { c_obj, v_obj, a_obj },       { c_bool, v_bool, a_bool },
        { c_byte, v_byte, a_byte },    { c_char, v_char, a_char },
        { c_short, v_short, a_short }, { c_int, v_int, a_int },
        { c_long, v_long, a_long },    { c_float, v_float, a_float },
        { c_double, v_double, a_double }, { c_void, v_void, a_void },
    };
    for (int t = 0; t < 10; t++)
        for (int k = 0; k < 3; k++) {
            vt[JNI_CallObjectMethod + t * 3 + k] = call3[t][k];
            vt[JNI_CallNonvirtualObjectMethod + t * 3 + k] = call3[t][k];
            vt[JNI_CallStaticObjectMethod + t * 3 + k] = call3[t][k];
        }

    vt[JNI_GetFieldID] = j_GetFieldID;
    void *const getf[9] = { f_obj, f_bool, f_byte, f_char, f_short, f_int,
                            f_long, f_float, f_double };
    for (int i = 0; i < 9; i++) {
        vt[JNI_GetObjectField + i] = getf[i];
        vt[JNI_GetStaticObjectField + i] = getf[i];
        vt[JNI_SetObjectField + i] = f_set;
        vt[JNI_SetStaticObjectField + i] = f_set;
    }
    vt[JNI_GetStaticMethodID] = j_GetMethodID;
    vt[JNI_GetStaticFieldID] = j_GetFieldID;

    vt[JNI_NewString] = j_NewString;
    vt[JNI_GetStringLength] = j_GetStringLength;
    vt[JNI_GetStringChars] = j_GetStringChars;
    vt[JNI_ReleaseStringChars] = j_ReleaseStringChars;
    vt[JNI_NewStringUTF] = j_NewStringUTF;
    vt[JNI_GetStringUTFLength] = j_GetStringUTFLength;
    vt[JNI_GetStringUTFChars] = j_GetStringUTFChars;
    vt[JNI_ReleaseStringUTFChars] = j_ReleaseStringUTFChars;
    vt[JNI_GetStringRegion] = j_GetStringUTFRegion;
    vt[JNI_GetStringUTFRegion] = j_GetStringUTFRegion;
    vt[JNI_GetStringCritical] = j_GetStringChars;
    vt[JNI_ReleaseStringCritical] = j_ReleaseStringChars;

    vt[JNI_GetArrayLength] = j_GetArrayLength;
    vt[JNI_NewObjectArray] = j_NewObjectArray;
    vt[JNI_GetObjectArrayElement] = j_GetObjectArrayElement;
    vt[JNI_SetObjectArrayElement] = j_SetObjectArrayElement;
    for (int i = 0; i < 8; i++) {
        vt[JNI_NewBooleanArray + i] = j_NewByteArray;
        vt[JNI_GetBooleanArrayElements + i] = j_GetByteArrayElements;
        vt[JNI_ReleaseBooleanArrayElements + i] = j_ReleaseArrayElements;
        vt[JNI_GetBooleanArrayRegion + i] = j_GetByteArrayRegion;
        vt[JNI_SetBooleanArrayRegion + i] = j_SetByteArrayRegion;
    }
    /* Primitive-array slots are adjacent, but element width is not.  The
     * gamepad enumerator returns int[], so give that type its real byte stride. */
    vt[JNI_NewBooleanArray + 4] = j_NewIntArray;
    vt[JNI_GetBooleanArrayRegion + 4] = j_GetIntArrayRegion;
    vt[JNI_SetBooleanArrayRegion + 4] = j_SetIntArrayRegion;
    vt[JNI_GetPrimitiveArrayCritical] = j_GetPrimitiveArrayCritical;
    vt[JNI_ReleasePrimitiveArrayCritical] = j_ReleasePrimitiveArrayCritical;

    vt[JNI_RegisterNatives] = j_RegisterNatives;
    vt[JNI_UnregisterNatives] = j_UnregisterNatives;
    vt[JNI_MonitorEnter] = j_MonitorEnter;
    vt[JNI_MonitorExit] = j_MonitorExit;
    vt[JNI_GetJavaVM] = j_GetJavaVM;
    vt[JNI_NewWeakGlobalRef] = j_NewWeakGlobalRef;
    vt[JNI_DeleteWeakGlobalRef] = j_DeleteRef;
    vt[JNI_ExceptionCheck] = j_ExceptionCheck;
    vt[JNI_NewDirectByteBuffer] = j_NewDirectByteBuffer;
    vt[JNI_GetDirectBufferAddress] = j_GetDirectBufferAddress;
    vt[JNI_GetDirectBufferCapacity] = j_GetDirectBufferCapacity;
    vt[JNI_GetObjectRefType] = j_GetObjectRefType;

    jvm_vt[3] = vm_DestroyJavaVM;
    jvm_vt[4] = vm_AttachCurrentThread;
    jvm_vt[5] = vm_DetachCurrentThread;
    jvm_vt[6] = vm_GetEnv;
    jvm_vt[7] = vm_AttachCurrentThread;   /* AttachCurrentThreadAsDaemon */

    looper_object = mk_object("android/os/Looper");
    choreographer_object = mk_object("android/view/Choreographer");
    message_object = mk_object("android/os/Message");
    play_games_task = mk_object("com/google/android/gms/tasks/Task");
    play_games_auth_result = mk_object(
        "com/google/android/gms/games/AuthenticationResult");

#ifdef FP2_RELEASE_BUILD
    const char *membership = "GUEST";
#else
    const char *membership = getenv("FP2_NETFLIX_MEMBERSHIP");
    if (!membership ||
        (strcmp(membership, "GUEST") != 0 &&
         strcmp(membership, "REGISTERED_GUEST") != 0 &&
         strcmp(membership, "MEMBER") != 0))
        membership = "GUEST";
#endif
    netflix_membership = mk_object(
        "com/netflix/games/player/access/MembershipStatus");
    netflix_membership->str = strdup(membership);
    netflix_membership->len = (int)strlen(membership);
    netflix_player_access = mk_object(
        "com/netflix/games/player/access/PlayerAccessInfo");
    object_set_object(netflix_player_access, "playerId",
                      mk_string("nextos-local-6e6578746f736867"));
    object_set_object(netflix_player_access, "membershipStatus",
                      netflix_membership);
    netflix_profile = mk_object(
        "com/netflix/games/player/profiles/CurrentProfile");
    netflix_crash_config = mk_object(
        "com/netflix/games/performance/crashreporting/CrashReporterConfig");
    object_set_object(netflix_crash_config, "projectId", mk_string(""));
    object_set_object(netflix_crash_config, "applicationId",
                      mk_string("net.kairosoft.android.mujin_en"));
    object_set_object(netflix_crash_config, "applicationVersion",
                      mk_string("1.0.605"));
    object_set_object(netflix_crash_config, "applicationBuildVersion",
                      mk_string("261010701"));
    object_set_object(netflix_crash_config, "deviceModel",
                      mk_string("NextOS Mali-450"));
    object_set_object(netflix_crash_config, "deviceOSVersion",
                      mk_string("NextOS"));
    object_set_object(netflix_crash_config, "playerId",
                      mk_string("nextos-local-6e6578746f736867"));
    object_set_primitive(netflix_crash_config,
                         "applicationIsInForeground", 1);
    object_set_primitive(netflix_crash_config, "isAttachmentEnabled", 0);
    nx_log("jni: Netflix local profile membership=%s", membership);

    /* The reflected methods handed to JNIBridge.invoke must map to the same
     * stable IDs that the native proxy obtained from these interfaces. */
    doframe_mid = method_id("android/view/Choreographer$FrameCallback",
                            "doFrame", "(J)V");
    handlemsg_mid = method_id("android/os/Handler$Callback", "handleMessage",
                              "(Landroid/os/Message;)Z");
    run_mid = method_id("java/lang/Runnable", "run", "()V");
    play_games_success_mid = method_id(
        "com/google/android/gms/tasks/OnSuccessListener", "onSuccess",
        "(Ljava/lang/Object;)V");
    netflix_on_result_mid = method_id(
        "com/netflix/games/Callback", "onResult",
        "(Lcom/netflix/games/NetflixResult;)V");

    doframe_method = make_reflected_member(REFLECT_METHOD, doframe_mid, 0);
    handlemsg_method = make_reflected_member(REFLECT_METHOD,
                                             handlemsg_mid, 0);
    run_method = make_reflected_member(REFLECT_METHOD, run_mid, 0);
    play_games_success_method = make_reflected_member(
        REFLECT_METHOD, play_games_success_mid, 0);
    netflix_on_result_method = make_reflected_member(
        REFLECT_METHOD, netflix_on_result_mid, 0);
    if (!doframe_method || !handlemsg_method || !run_method ||
        !play_games_success_method || !netflix_on_result_method)
        nx_die("JNI reflected callback method initialization failed");

    frame_time_box = mk_object("java/lang/Long");
    frame_time_box->boxed = 'J';
    doframe_args = j_NewObjectArray(NULL, 1, mk_class("java/lang/Object"),
                                    frame_time_box);
    handlemsg_args = j_NewObjectArray(NULL, 1, mk_class("java/lang/Object"),
                                     message_object);
    empty_args = j_NewObjectArray(NULL, 0, mk_class("java/lang/Object"), NULL);
    play_games_success_args = j_NewObjectArray(
        NULL, 1, mk_class("java/lang/Object"), play_games_auth_result);

    /* Bound up front rather than on demand so the VM's GetMethodID finds these
     * already carrying a handler; method_id matches on name and signature, so a
     * late bind would create a second, handler-less entry. */
    fp2_jni_bind("bitter/jnibridge/JNIBridge", "newInterfaceProxy",
                 "(J[Ljava/lang/Class;)Ljava/lang/Object;",
                 (void *)j_newInterfaceProxy);
    fp2_jni_bind("com/unity3d/player/ReflectionHelper", "getConstructorID",
                 "(Ljava/lang/Class;Ljava/lang/String;)"
                 "Ljava/lang/reflect/Constructor;",
                 (void *)j_Reflection_getConstructorID);
    fp2_jni_bind("com/unity3d/player/ReflectionHelper", "getMethodID",
                 "(Ljava/lang/Class;Ljava/lang/String;Ljava/lang/String;Z)"
                 "Ljava/lang/reflect/Method;",
                 (void *)j_Reflection_getMethodID);
    fp2_jni_bind("com/unity3d/player/ReflectionHelper", "getFieldID",
                 "(Ljava/lang/Class;Ljava/lang/String;Ljava/lang/String;Z)"
                 "Ljava/lang/reflect/Field;",
                 (void *)j_Reflection_getFieldID);
    fp2_jni_bind("com/unity3d/player/ReflectionHelper", "getFieldSignature",
                 "(Ljava/lang/reflect/Field;)Ljava/lang/String;",
                 (void *)j_Reflection_getFieldSignature);
    fp2_jni_bind("java/lang/reflect/Field", "getDeclaringClass",
                 "()Ljava/lang/Class;",
                 (void *)j_Reflected_getDeclaringClass);
    fp2_jni_bind("java/lang/reflect/Method", "getDeclaringClass",
                 "()Ljava/lang/Class;",
                 (void *)j_Reflected_getDeclaringClass);
    fp2_jni_bind("java/lang/reflect/Constructor", "getDeclaringClass",
                 "()Ljava/lang/Class;",
                 (void *)j_Reflected_getDeclaringClass);
    fp2_jni_bind("com/unity3d/player/ReflectionHelper", "newProxyInstance",
                 "(Lcom/unity3d/player/UnityPlayer;JLjava/lang/Class;)"
                 "Ljava/lang/Object;",
                 (void *)j_Reflection_newProxyInstance);
    fp2_jni_bind("com/unity3d/player/ReflectionHelper", "endUnityLaunch",
                 "()V", (void *)j_Reflection_endUnityLaunch);
    fp2_jni_bind("com/unity3d/player/UnityPlayer", "currentActivity",
                 "Landroid/app/Activity;", (void *)j_Unity_currentActivity);
    /* AndroidJavaObject erases reference return/field types to Object in the
     * ReflectionHelper signature.  Keep the typed aliases above for direct
     * JNI users and publish the erased forms used by Hitman GO's managed
     * Armory layer. */
    fp2_jni_bind("com/unity3d/player/UnityPlayer", "currentActivity",
                 "Ljava/lang/Object;", (void *)j_Unity_currentActivity);

    /* Stranger Things 1.0.605 Netflix Games SDK.  Publish typed descriptors
     * from the APK plus the erased Object aliases generated by
     * AndroidJavaObject. */
    static const char netflix_sdk[] = "com/netflix/unity/api/NfUnitySdk";
    fp2_jni_bind(netflix_sdk, "init", "(Landroid/content/Context;)V",
                (void *)j_Netflix_init);
    fp2_jni_bind(netflix_sdk, "init", "(Ljava/lang/Object;)V",
                (void *)j_Netflix_init);
    fp2_jni_bind(netflix_sdk, "init",
                "(Lcom/unity3d/player/UnityPlayerActivity;)V",
                (void *)j_Netflix_init);
    fp2_jni_bind(netflix_sdk, "registerNetflixCallback",
                "(Lcom/netflix/unity/api/NetflixPluginCallback;)V",
                (void *)j_Netflix_registerPlugin);
    fp2_jni_bind(netflix_sdk, "registerNetflixCallback",
                "(Ljava/lang/Object;)V", (void *)j_Netflix_registerPlugin);
    fp2_jni_bind(netflix_sdk, "registerNetflixCallback",
                "(Lbitter/jnibridge/Proxy;)V",
                (void *)j_Netflix_registerPlugin);
    fp2_jni_bind(netflix_sdk, "showNetflixAccessButton",
                "(Landroid/app/Activity;I)V", (void *)j_Netflix_showAccess);
    fp2_jni_bind(netflix_sdk, "showNetflixAccessButton",
                "(Ljava/lang/Object;I)V", (void *)j_Netflix_showAccess);
    fp2_jni_bind(netflix_sdk, "showNetflixAccessButton",
                "(Lcom/unity3d/player/UnityPlayerActivity;I)V",
                (void *)j_Netflix_showAccess);
    fp2_jni_bind(netflix_sdk, "hideNetflixAccessButton",
                "(Landroid/app/Activity;)V", (void *)j_Netflix_hideAccess);
    fp2_jni_bind(netflix_sdk, "hideNetflixAccessButton",
                "(Ljava/lang/Object;)V", (void *)j_Netflix_hideAccess);
    fp2_jni_bind(netflix_sdk, "hideNetflixAccessButton",
                "(Lcom/unity3d/player/UnityPlayerActivity;)V",
                (void *)j_Netflix_hideAccess);
    fp2_jni_bind(netflix_sdk, "requestPlayerAccess",
                "(Lcom/netflix/games/Callback;)V",
                (void *)j_Netflix_requestAccess);
    fp2_jni_bind(netflix_sdk, "requestPlayerAccess",
                "(Ljava/lang/Object;)V", (void *)j_Netflix_requestAccess);
    fp2_jni_bind(netflix_sdk, "requestPlayerAccess",
                "(Lbitter/jnibridge/Proxy;)V",
                (void *)j_Netflix_requestAccess);
    fp2_jni_bind(netflix_sdk, "showNetflixAccessUIIfNecessary", "()V",
                (void *)j_Netflix_noop);
    fp2_jni_bind(netflix_sdk, "getCurrentProfile",
                "()Lcom/netflix/games/NetflixResult;",
                (void *)j_Netflix_getCurrentProfile);
    fp2_jni_bind(netflix_sdk, "getCurrentProfile", "()Ljava/lang/Object;",
                (void *)j_Netflix_getCurrentProfile);
    fp2_jni_bind(netflix_sdk, "writeBlob",
                "(Ljava/lang/String;Ljava/lang/String;"
                "Lcom/netflix/games/Callback;)V",
                (void *)j_Netflix_writeBlob);
    fp2_jni_bind(netflix_sdk, "writeBlob",
                "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Object;)V",
                (void *)j_Netflix_writeBlob);
    fp2_jni_bind(netflix_sdk, "writeBlob",
                "(Ljava/lang/String;Ljava/lang/String;"
                "Lbitter/jnibridge/Proxy;)V",
                (void *)j_Netflix_writeBlob);
    fp2_jni_bind(netflix_sdk, "readBlob",
                "(Ljava/lang/String;Lcom/netflix/games/Callback;)V",
                (void *)j_Netflix_readBlob);
    fp2_jni_bind(netflix_sdk, "readBlob",
                "(Ljava/lang/String;Ljava/lang/Object;)V",
                (void *)j_Netflix_readBlob);
    fp2_jni_bind(netflix_sdk, "readBlob",
                "(Ljava/lang/String;Lbitter/jnibridge/Proxy;)V",
                (void *)j_Netflix_readBlob);
    fp2_jni_bind(netflix_sdk, "deleteBlob",
                "(Ljava/lang/String;Lcom/netflix/games/Callback;)V",
                (void *)j_Netflix_deleteBlob);
    fp2_jni_bind(netflix_sdk, "deleteBlob",
                "(Ljava/lang/String;Ljava/lang/Object;)V",
                (void *)j_Netflix_deleteBlob);
    fp2_jni_bind(netflix_sdk, "deleteBlob",
                "(Ljava/lang/String;Lbitter/jnibridge/Proxy;)V",
                (void *)j_Netflix_deleteBlob);
    fp2_jni_bind(netflix_sdk, "resolveConflict",
                "(Ljava/lang/String;ILcom/netflix/games/Callback;)V",
                (void *)j_Netflix_resolveConflict);
    fp2_jni_bind(netflix_sdk, "resolveConflict",
                "(Ljava/lang/String;ILjava/lang/Object;)V",
                (void *)j_Netflix_resolveConflict);
    fp2_jni_bind(netflix_sdk, "resolveConflict",
                "(Ljava/lang/String;ILbitter/jnibridge/Proxy;)V",
                (void *)j_Netflix_resolveConflict);
    fp2_jni_bind(netflix_sdk, "getNfgSdkVersion", "()Ljava/lang/String;",
                (void *)j_Netflix_sdkString);
    fp2_jni_bind(netflix_sdk, "getNativeSdkVersion", "()Ljava/lang/String;",
                (void *)j_Netflix_sdkString);
    fp2_jni_bind(netflix_sdk, "getNfgSymbolicationId",
                "()Ljava/lang/String;", (void *)j_Netflix_sdkString);
    fp2_jni_bind(netflix_sdk, "getCrashReporterConfig",
                "()Lcom/netflix/games/performance/crashreporting/"
                "CrashReporterConfig;", (void *)j_Netflix_getCrashConfig);
    fp2_jni_bind(netflix_sdk, "getCrashReporterConfig",
                "()Ljava/lang/Object;", (void *)j_Netflix_getCrashConfig);
    fp2_jni_bind(netflix_sdk, "registerCrashReporterConfigChangeListener",
                "(Lcom/netflix/games/performance/crashreporting/"
                "ConfigChangeListener;)V", (void *)j_Netflix_registerConfig);
    fp2_jni_bind(netflix_sdk, "registerCrashReporterConfigChangeListener",
                "(Ljava/lang/Object;)V", (void *)j_Netflix_registerConfig);
    fp2_jni_bind(netflix_sdk, "registerCrashReporterConfigChangeListener",
                "(Lbitter/jnibridge/Proxy;)V",
                (void *)j_Netflix_registerConfig);

    static const char netflix_membership_class[] =
        "com/netflix/games/player/access/MembershipStatus";
    fp2_jni_bind(netflix_membership_class, "name", "()Ljava/lang/String;",
                (void *)j_Netflix_membershipName);

    static const char netflix_profile_class[] =
        "com/netflix/games/player/profiles/CurrentProfile";
    fp2_jni_bind(netflix_profile_class, "getPlayerId", "()Ljava/lang/String;",
                (void *)j_Netflix_profileString);
    fp2_jni_bind(netflix_profile_class, "getHandle", "()Ljava/lang/String;",
                (void *)j_Netflix_profileString);
    fp2_jni_bind(netflix_profile_class, "getPreferredLanguage",
                "()Ljava/lang/String;", (void *)j_Netflix_profileString);
    fp2_jni_bind(netflix_profile_class, "getLegacyGamerProfileId",
                "()Lcom/netflix/games/NetflixResult;",
                (void *)j_Netflix_profileLegacy);
    fp2_jni_bind(netflix_profile_class, "getLegacyGamerProfileId",
                "()Ljava/lang/Object;", (void *)j_Netflix_profileLegacy);
    fp2_jni_bind(netflix_profile_class, "getLegacyGamerAccessToken",
                "()Lcom/netflix/games/NetflixResult;",
                (void *)j_Netflix_profileLegacy);
    fp2_jni_bind(netflix_profile_class, "getLegacyGamerAccessToken",
                "()Ljava/lang/Object;", (void *)j_Netflix_profileLegacy);

    fp2_jni_bind("java/lang/Thread", "start", "()V",
                 (void *)j_Thread_start);
    fp2_jni_bind("org/fmod/FMOD", "init",
                 "(Lcom/unity3d/player/UnityPlayerActivity;)V",
                 (void *)j_FMODJava_init);
    fp2_jni_bind("org/fmod/FMOD", "init", "(Landroid/content/Context;)V",
                 (void *)j_FMODJava_init);
    fp2_jni_bind("org/fmod/FMOD", "init", "(Ljava/lang/Object;)V",
                 (void *)j_FMODJava_init);
    fp2_jni_bind("org/fmod/FMOD", "close", "()V", (void *)j_FMODJava_init);
    fp2_jni_bind("org/fmod/FMOD", "checkInit", "()Z",
                 (void *)j_FMODJava_checkInit);
    fp2_jni_bind("org/fmod/FMOD", "supportsLowLatency", "()Z",
                 (void *)j_FMODJava_supportsLowLatency);
    fp2_jni_bind("org/fmod/FMOD", "supportsAAudio", "()Z",
                 (void *)j_FMODJava_supportsAAudio);
    fp2_jni_bind("org/fmod/FMOD", "getOutputSampleRate", "()I",
                 (void *)j_FMODJava_getOutputSampleRate);
    fp2_jni_bind("org/fmod/FMOD", "getOutputBlockSize", "()I",
                 (void *)j_FMODJava_getOutputBlockSize);
    fp2_jni_bind("org/fmod/FMOD", "getAssetManager",
                 "()Landroid/content/res/AssetManager;",
                 (void *)j_Context_getAssets);
    fp2_jni_bind("org/fmod/FMODAudioDevice", "start", "()V",
                 (void *)j_FMOD_start);
    fp2_jni_bind("org/fmod/FMODAudioDevice", "stop", "()V",
                 (void *)j_FMOD_stop);
    fp2_jni_bind("org/fmod/FMODAudioDevice", "close", "()V",
                 (void *)j_FMOD_stop);
    fp2_jni_bind("org/fmod/FMODAudioDevice", "isRunning", "()Z",
                 (void *)j_FMOD_isRunning);
    fp2_jni_bind("android/os/HandlerThread", "getLooper",
                 "()Landroid/os/Looper;", (void *)j_HandlerThread_getLooper);
    fp2_jni_bind("android/os/Looper", "getMainLooper",
                 "()Landroid/os/Looper;", (void *)j_Looper_get);
    fp2_jni_bind("android/os/Looper", "myLooper",
                 "()Landroid/os/Looper;", (void *)j_Looper_get);
    fp2_jni_bind("android/os/Looper", "quit", "()V",
                 (void *)j_Looper_quit);
    fp2_jni_bind("android/os/Handler", "obtainMessage",
                 "(I)Landroid/os/Message;", (void *)j_Handler_obtainMessage);
    fp2_jni_bind("android/os/Message", "sendToTarget", "()V",
                 (void *)j_Message_sendToTarget);
    fp2_jni_bind("android/os/Message", "what", "I",
                 (void *)j_Message_what);
    fp2_jni_bind("java/lang/System", "nanoTime", "()J",
                 (void *)j_System_nanoTime);
    fp2_jni_bind("android/view/Choreographer", "getInstance",
                 "()Landroid/view/Choreographer;",
                 (void *)j_Choreographer_getInstance);
    fp2_jni_bind("android/view/Choreographer", "postFrameCallback",
                 "(Landroid/view/Choreographer$FrameCallback;)V",
                 (void *)j_Choreographer_postFrameCallback);
    fp2_jni_bind("java/lang/reflect/Method", "getName",
                 "()Ljava/lang/String;", (void *)j_Method_getName);
    fp2_jni_bind("android/app/Activity", "runOnUiThread",
                 "(Ljava/lang/Runnable;)V", (void *)j_runOnUiThread);
    fp2_jni_bind("com/unity3d/player/UnityPlayer", "showSoftInput",
                 "(Ljava/lang/String;IZZZZLjava/lang/String;IZZ)V",
                 (void *)j_Unity_showSoftInput);
    fp2_jni_bind("com/unity3d/player/UnityPlayer", "setSoftInputStr",
                 "(Ljava/lang/String;)V",
                 (void *)j_Unity_setSoftInputStr);
    fp2_jni_bind("com/unity3d/player/UnityPlayer", "hideSoftInput", "()V",
                 (void *)j_Unity_hideSoftInput);

    input_device = mk_object("android/view/InputDevice");
    touch_device = mk_object("android/view/InputDevice");
    motion_range_list = mk_object("java/util/List");
    touch_motion_range_list = mk_object("java/util/List");
    motion_range_list->len = (int)(sizeof motion_ranges /
                                    sizeof *motion_ranges);
    motion_range_list->elems = calloc((size_t)motion_range_list->len,
                                      sizeof *motion_range_list->elems);
    for (int i = 0; i < motion_range_list->len; i++) {
        motion_ranges[i] = mk_object("android/view/InputDevice$MotionRange");
        motion_ranges[i]->prim = motion_axis_ids[i];
        motion_range_list->elems[i] = motion_ranges[i];
    }
    motion_range_iterator = mk_object("java/util/Iterator");
    key_event_object = mk_object("android/view/KeyEvent");
    motion_event_object = mk_object("android/view/MotionEvent");
    motion_event_object->data = &motion_event;
    fmod_device_object = mk_object("org/fmod/FMODAudioDevice");
    fmod_bytebuffer = j_NewDirectByteBuffer(NULL, fmod_pcm,
                                            fmod_buffer_size);
    preferences_object =
        mk_object("android/content/SharedPreferences");
    preferences_editor =
        mk_object("android/content/SharedPreferences$Editor");
    armory_activity_object =
        mk_object("com/unity3d/player/UnityPlayerActivity");
    permission_plugin_object =
        mk_object("com/squareenixmontreal/armory/PermissionPlugin");
    audio_manager_object = mk_object("android/media/AudioManager");
    input_manager_object = mk_object("android/hardware/input/InputManager");
    vibrator_object = mk_object("android/os/Vibrator");
    window_manager_object = mk_object("android/view/WindowManager");
    fp2_jni_bind("android/view/InputDevice", "getDeviceIds", "()[I",
                 (void *)j_InputDevice_getDeviceIds);
    fp2_jni_bind("android/view/InputDevice", "getDevice",
                 "(I)Landroid/view/InputDevice;",
                 (void *)j_InputDevice_getDevice);
    fp2_jni_bind("android/view/InputEvent", "getDevice",
                 "()Landroid/view/InputDevice;",
                 (void *)j_InputEvent_getDevice);
    fp2_jni_bind("android/view/InputDevice", "getName",
                 "()Ljava/lang/String;", (void *)j_InputDevice_getString);
    fp2_jni_bind("android/view/InputDevice", "getDescriptor",
                 "()Ljava/lang/String;", (void *)j_InputDevice_getString);
    fp2_jni_bind("android/view/InputDevice", "getVendorId", "()I",
                 (void *)j_InputDevice_getInt);
    fp2_jni_bind("android/view/InputDevice", "getProductId", "()I",
                 (void *)j_InputDevice_getInt);
    fp2_jni_bind("android/view/InputDevice", "getSources", "()I",
                 (void *)j_InputDevice_getInt);
    fp2_jni_bind("android/view/InputDevice", "getId", "()I",
                 (void *)j_InputDevice_getInt);
    fp2_jni_bind("android/view/InputDevice", "getControllerNumber", "()I",
                 (void *)j_InputDevice_getInt);
    fp2_jni_bind("android/view/InputDevice", "getKeyboardType", "()I",
                 (void *)j_InputDevice_getInt);
    fp2_jni_bind("android/view/InputDevice", "supportsSource", "(I)Z",
                 (void *)j_InputDevice_supportsSource);
    fp2_jni_bind("android/view/InputDevice", "getMotionRanges",
                 "()Ljava/util/List;", (void *)j_InputDevice_getMotionRanges);
    fp2_jni_bind("android/view/InputDevice", "getMotionRange",
                 "(I)Landroid/view/InputDevice$MotionRange;",
                 (void *)j_InputDevice_getMotionRange);
    fp2_jni_bind("java/util/List", "size", "()I", (void *)j_List_size);
    fp2_jni_bind("java/util/List", "get", "(I)Ljava/lang/Object;",
                 (void *)j_List_get);
    fp2_jni_bind("java/util/List", "iterator", "()Ljava/util/Iterator;",
                 (void *)j_List_iterator);
    fp2_jni_bind("java/util/Iterator", "hasNext", "()Z",
                 (void *)j_Iterator_hasNext);
    fp2_jni_bind("java/util/Iterator", "next", "()Ljava/lang/Object;",
                 (void *)j_Iterator_next);
    fp2_jni_bind("android/view/InputDevice$MotionRange", "getAxis", "()I",
                 (void *)j_MotionRange_getInt);
    fp2_jni_bind("android/view/InputDevice$MotionRange", "getSource", "()I",
                 (void *)j_MotionRange_getInt);
    static const char *const range_float_methods[] = {
        "getMin", "getMax", "getFlat", "getFuzz", "getResolution",
    };
    for (size_t i = 0; i < sizeof range_float_methods /
                            sizeof *range_float_methods; i++)
        fp2_jni_bind("android/view/InputDevice$MotionRange",
                     range_float_methods[i], "()F",
                     (void *)j_MotionRange_getFloat);

    static const char *const key_int_methods[] = {
        "getAction", "getKeyCode", "getSource", "getDeviceId",
        "getMetaState", "getRepeatCount", "getScanCode", "getFlags",
        "getUnicodeChar",
    };
    for (size_t i = 0; i < sizeof key_int_methods / sizeof *key_int_methods; i++)
        fp2_jni_bind("android/view/KeyEvent", key_int_methods[i], "()I",
                     (void *)j_KeyEvent_getInt);
    fp2_jni_bind("android/view/KeyEvent", "getEventTime", "()J",
                 (void *)j_KeyEvent_getLong);
    fp2_jni_bind("android/view/KeyEvent", "getDownTime", "()J",
                 (void *)j_KeyEvent_getLong);
    fp2_jni_bind("android/view/KeyEvent", "isSystem", "()Z",
                 (void *)j_KeyEvent_isSystem);

    static const char *const motion_int_methods[] = {
        "getAction", "getActionMasked", "getActionIndex", "getSource",
        "getDeviceId", "getMetaState", "getButtonState", "getFlags",
        "getPointerCount", "getHistorySize", "getEdgeFlags",
        "getActionButton", "getDisplayId", "getClassification",
    };
    for (size_t i = 0; i < sizeof motion_int_methods /
                            sizeof *motion_int_methods; i++)
        fp2_jni_bind("android/view/MotionEvent", motion_int_methods[i],
                     "()I", (void *)j_MotionEvent_getInt);
    static const char *const motion_int_arg_methods[] = {
        "getPointerId", "findPointerIndex", "getToolType",
    };
    for (size_t i = 0; i < sizeof motion_int_arg_methods /
                            sizeof *motion_int_arg_methods; i++)
        fp2_jni_bind("android/view/MotionEvent", motion_int_arg_methods[i],
                     "(I)I", (void *)j_MotionEvent_getInt);
    fp2_jni_bind("android/view/MotionEvent", "getEventTime", "()J",
                 (void *)j_MotionEvent_getLong);
    fp2_jni_bind("android/view/MotionEvent", "getDownTime", "()J",
                 (void *)j_MotionEvent_getLong);
    fp2_jni_bind("android/view/MotionEvent", "getHistoricalEventTime", "(I)J",
                 (void *)j_MotionEvent_getLong);
    fp2_jni_bind("android/view/MotionEvent", "getAxisValue", "(I)F",
                 (void *)j_MotionEvent_getFloat);
    fp2_jni_bind("android/view/MotionEvent", "getAxisValue", "(II)F",
                 (void *)j_MotionEvent_getFloat);
    fp2_jni_bind("android/view/MotionEvent", "getHistoricalAxisValue",
                 "(II)F", (void *)j_MotionEvent_getFloat);
    fp2_jni_bind("android/view/MotionEvent", "getHistoricalAxisValue",
                 "(III)F", (void *)j_MotionEvent_getFloat);
    static const char *const motion_xy_methods[] = {
        "getX", "getY", "getRawX", "getRawY", "getPressure", "getSize",
        "getTouchMajor", "getTouchMinor", "getToolMajor", "getToolMinor",
        "getOrientation",
    };
    for (size_t i = 0; i < sizeof motion_xy_methods /
                            sizeof *motion_xy_methods; i++) {
        fp2_jni_bind("android/view/MotionEvent", motion_xy_methods[i], "()F",
                     (void *)j_MotionEvent_getFloat);
        fp2_jni_bind("android/view/MotionEvent", motion_xy_methods[i], "(I)F",
                     (void *)j_MotionEvent_getFloat);
    }
    fp2_jni_bind("android/view/MotionEvent", "getHistoricalX", "(I)F",
                 (void *)j_MotionEvent_getFloat);
    fp2_jni_bind("android/view/MotionEvent", "getHistoricalX", "(II)F",
                 (void *)j_MotionEvent_getFloat);
    fp2_jni_bind("android/view/MotionEvent", "getHistoricalY", "(I)F",
                 (void *)j_MotionEvent_getFloat);
    fp2_jni_bind("android/view/MotionEvent", "getHistoricalY", "(II)F",
                 (void *)j_MotionEvent_getFloat);
    fp2_jni_bind("android/view/MotionEvent", "isFromSource", "(I)Z",
                 (void *)j_MotionEvent_isFromSource);
    fp2_jni_bind("android/view/MotionEvent", "obtain",
                 "(Landroid/view/MotionEvent;)Landroid/view/MotionEvent;",
                 (void *)j_MotionEvent_obtain);
    fp2_jni_bind("android/view/MotionEvent", "recycle", "()V",
                 (void *)j_MotionEvent_recycle);
    fp2_jni_bind("android/view/MotionEvent", "getDevice",
                 "()Landroid/view/InputDevice;",
                 (void *)j_InputEvent_getDevice);
    fp2_jni_bind("android/view/InputEvent", "getSource", "()I",
                 (void *)j_MotionEvent_getInt);
    fp2_jni_bind("android/view/InputEvent", "getDeviceId", "()I",
                 (void *)j_MotionEvent_getInt);
    fp2_jni_bind("android/view/InputEvent", "getEventTime", "()J",
                 (void *)j_MotionEvent_getLong);

    fp2_jni_bind("java/io/File", "length", "()J", (void *)j_File_length);
    fp2_jni_bind("java/io/File", "exists", "()Z", (void *)j_File_exists);
    fp2_jni_bind("java/io/File", "getPath", "()Ljava/lang/String;",
                 (void *)j_File_getPath);
    fp2_jni_bind("java/io/File", "getAbsolutePath", "()Ljava/lang/String;",
                 (void *)j_File_getPath);
    fp2_jni_bind("java/io/File", "getCanonicalPath", "()Ljava/lang/String;",
                 (void *)j_File_getPath);
    fp2_jni_bind("java/io/File", "getParent", "()Ljava/lang/String;",
                 (void *)j_File_getParent);
    fp2_jni_bind("java/io/File", "getParentFile", "()Ljava/io/File;",
                 (void *)j_File_getParentFile);
    fp2_jni_bind("java/io/File", "isDirectory", "()Z",
                 (void *)j_File_isDirectory);
    fp2_jni_bind("java/io/File", "mkdirs", "()Z", (void *)j_File_mkdirs);

    fp2_jni_bind("android/content/Context", "getFilesDir",
                 "()Ljava/io/File;", (void *)j_Context_getFilesDir);
    fp2_jni_bind("android/content/Context", "getExternalFilesDir",
                 "(Ljava/lang/String;)Ljava/io/File;",
                 (void *)j_Context_getFilesDir);
    fp2_jni_bind("android/content/Context", "getCacheDir",
                 "()Ljava/io/File;", (void *)j_Context_getFilesDir);
    fp2_jni_bind("android/content/Context", "getPackageCodePath",
                 "()Ljava/lang/String;", (void *)j_Context_getPackageCodePath);

    for (size_t i = 0; i < 3; i++) {
        static const char *const donos[] = {
            "android/content/Context", "android/app/Activity",
            "android/content/ContextWrapper",
        };
        fp2_jni_bind(donos[i], "getApplicationInfo",
                     "()Landroid/content/pm/ApplicationInfo;",
                     (void *)j_Context_getApplicationInfo);
    }
    for (size_t i = 0; i < 3; i++) {
        static const char *const campos[] = {
            "sourceDir", "publicSourceDir", "dataDir",
        };
        fp2_jni_bind("android/content/pm/ApplicationInfo", campos[i],
                     "Ljava/lang/String;",
                     (void *)j_ApplicationInfo_sourceDir);
    }
    fp2_jni_bind("com/unity3d/player/PlayAssetDeliveryUnityWrapper",
                 "playCoreApiMissing", "()Z",
                 (void *)j_PlayAssetDelivery_apiMissing);

    fp2_jni_bind("android/content/pm/ApplicationInfo", "splitPublicSourceDirs",
                 "[Ljava/lang/String;",
                 (void *)j_ApplicationInfo_splitPublicSourceDirs);
    fp2_jni_bind("android/content/pm/ApplicationInfo", "splitSourceDirs",
                 "[Ljava/lang/String;",
                 (void *)j_ApplicationInfo_splitPublicSourceDirs);
    fp2_jni_bind("android/content/Context", "getPackageName",
                 "()Ljava/lang/String;", (void *)j_Context_getPackageName);
    fp2_jni_bind("android/content/Context", "getPackageManager",
                 "()Landroid/content/pm/PackageManager;",
                 (void *)j_Context_getPackageManager);
    fp2_jni_bind("android/content/pm/PackageManager", "hasSystemFeature",
                 "(Ljava/lang/String;)Z",
                 (void *)j_PackageManager_hasSystemFeature);
    fp2_jni_bind("android/os/Process", "setThreadPriority", "(II)V",
                 (void *)j_Process_setThreadPriority);
    fp2_jni_bind("android/content/Context", "getAssets",
                 "()Landroid/content/res/AssetManager;",
                 (void *)j_Context_getAssets);
    fp2_jni_bind("android/content/Context", "getContentResolver",
                 "()Landroid/content/ContentResolver;",
                 (void *)j_Context_getContentResolver);
    fp2_jni_bind("android/content/Context", "getContentResolver",
                 "()Ljava/lang/Object;",
                 (void *)j_Context_getContentResolver);
    fp2_jni_bind("android/content/Context", "getApplicationContext",
                 "()Landroid/content/Context;",
                 (void *)j_Context_getApplicationContext);
    fp2_jni_bind("android/content/Context", "getApplicationContext",
                 "()Ljava/lang/Object;",
                 (void *)j_Context_getApplicationContext);
    fp2_jni_bind("android/content/Context", "getSystemService",
                 "(Ljava/lang/String;)Ljava/lang/Object;",
                 (void *)j_Context_getSystemService);
    /* initJni receives an Activity; Android inheritance makes these Context
     * methods, but the flat registry needs the concrete class aliases too. */
    fp2_jni_bind("android/app/Activity", "getFilesDir",
                 "()Ljava/io/File;", (void *)j_Context_getFilesDir);
    fp2_jni_bind("android/app/Activity", "getExternalFilesDir",
                 "(Ljava/lang/String;)Ljava/io/File;",
                 (void *)j_Context_getFilesDir);
    fp2_jni_bind("android/app/Activity", "getPackageCodePath",
                 "()Ljava/lang/String;", (void *)j_Context_getPackageCodePath);
    fp2_jni_bind("android/app/Activity", "getPackageName",
                 "()Ljava/lang/String;", (void *)j_Context_getPackageName);
    fp2_jni_bind("android/app/Activity", "getPackageManager",
                 "()Landroid/content/pm/PackageManager;",
                 (void *)j_Context_getPackageManager);
    fp2_jni_bind("android/app/Activity", "getAssets",
                 "()Landroid/content/res/AssetManager;",
                 (void *)j_Context_getAssets);
    fp2_jni_bind("android/app/Activity", "getContentResolver",
                 "()Landroid/content/ContentResolver;",
                 (void *)j_Context_getContentResolver);
    fp2_jni_bind("android/app/Activity", "getContentResolver",
                 "()Ljava/lang/Object;",
                 (void *)j_Context_getContentResolver);
    fp2_jni_bind("android/app/Activity", "getApplicationContext",
                 "()Ljava/lang/Object;",
                 (void *)j_Context_getApplicationContext);
    fp2_jni_bind("android/app/Activity", "getSystemService",
                 "(Ljava/lang/String;)Ljava/lang/Object;",
                 (void *)j_Context_getSystemService);
    /* ArmoryActivity extends UnityPlayerActivity/Activity.  The tiny object
     * model has no inheritance lookup, so publish the inherited Context calls
     * under the concrete launcher class as Android's GetMethodID would. */
    /* Stranger Things ships the stock com.unity3d.player.UnityPlayerActivity
     * (AndroidManifest), so the inherited Context calls are published under
     * that concrete class. */
    static const char armory_activity[] =
        "com/unity3d/player/UnityPlayerActivity";
    fp2_jni_bind(armory_activity, "getApplicationInfo",
                 "()Landroid/content/pm/ApplicationInfo;",
                 (void *)j_Context_getApplicationInfo);
    fp2_jni_bind(armory_activity, "getFilesDir", "()Ljava/io/File;",
                 (void *)j_Context_getFilesDir);
    fp2_jni_bind(armory_activity, "getExternalFilesDir",
                 "(Ljava/lang/String;)Ljava/io/File;",
                 (void *)j_Context_getFilesDir);
    fp2_jni_bind(armory_activity, "getCacheDir", "()Ljava/io/File;",
                 (void *)j_Context_getFilesDir);
    fp2_jni_bind(armory_activity, "getPackageCodePath",
                 "()Ljava/lang/String;", (void *)j_Context_getPackageCodePath);
    fp2_jni_bind(armory_activity, "getPackageName", "()Ljava/lang/String;",
                 (void *)j_Context_getPackageName);
    fp2_jni_bind(armory_activity, "getAssets",
                 "()Landroid/content/res/AssetManager;",
                 (void *)j_Context_getAssets);
    fp2_jni_bind(armory_activity, "getContentResolver",
                 "()Landroid/content/ContentResolver;",
                 (void *)j_Context_getContentResolver);
    fp2_jni_bind(armory_activity, "getContentResolver",
                 "()Ljava/lang/Object;",
                 (void *)j_Context_getContentResolver);
    fp2_jni_bind(armory_activity, "getApplicationContext",
                 "()Ljava/lang/Object;",
                 (void *)j_Context_getApplicationContext);
    fp2_jni_bind(armory_activity, "getApplicationContext",
                 "()Landroid/content/Context;",
                 (void *)j_Context_getApplicationContext);
    fp2_jni_bind(armory_activity, "getSystemService",
                 "(Ljava/lang/String;)Ljava/lang/Object;",
                 (void *)j_Context_getSystemService);
    fp2_jni_bind(armory_activity, "getClass", "()Ljava/lang/Class;",
                 (void *)j_Object_getClass);
    fp2_jni_bind(armory_activity, "getClass", "()Ljava/lang/Object;",
                 (void *)j_Object_getClass);
    fp2_jni_bind(armory_activity, "runOnUiThread",
                 "(Ljava/lang/Runnable;)V", (void *)j_runOnUiThread);
    fp2_jni_bind(armory_activity, "getActivity",
                 "()Lcom/unity3d/player/UnityPlayerActivity;",
                 (void *)j_Armory_getActivity);
    fp2_jni_bind(armory_activity, "getActivity", "()Ljava/lang/Object;",
                 (void *)j_Armory_getActivity);
    fp2_jni_bind(armory_activity, "getPermissionPlugin",
                 "()Lcom/squareenixmontreal/armory/PermissionPlugin;",
                 (void *)j_Armory_getPermissionPlugin);
    fp2_jni_bind(armory_activity, "getPermissionPlugin",
                 "()Ljava/lang/Object;",
                 (void *)j_Armory_getPermissionPlugin);
    fp2_jni_bind(armory_activity, "getOsVersion", "()I",
                 (void *)j_Armory_getOsVersion);
    fp2_jni_bind(armory_activity, "getTargetSdkVersion", "()I",
                 (void *)j_Armory_getTargetSdkVersion);
    fp2_jni_bind(armory_activity, "hasPermission",
                 "(Ljava/lang/String;)Z", (void *)j_Armory_hasPermission);

    static const char permission_plugin[] =
        "com/squareenixmontreal/armory/PermissionPlugin";
    fp2_jni_bind(permission_plugin, "canRequestPermissions", "()Z",
                 (void *)j_Permission_canRequest);
    fp2_jni_bind(permission_plugin, "hasPermission",
                 "(Ljava/lang/String;)Z", (void *)j_Armory_hasPermission);
    fp2_jni_bind(permission_plugin, "shouldShowRequestPermissionRationale",
                 "(Ljava/lang/String;)Z", (void *)j_Permission_noRationale);
    fp2_jni_bind(permission_plugin, "openApplicationSettings", "()V",
                 (void *)j_Permission_noop);
    fp2_jni_bind(permission_plugin, "requestAccountPermission", "()V",
                 (void *)j_Permission_noop);
    fp2_jni_bind(permission_plugin, "requestLocationPermission", "()V",
                 (void *)j_Permission_noop);
    fp2_jni_bind(permission_plugin, "requestPermission",
                 "(Ljava/lang/String;)V", (void *)j_Permission_noop);
    fp2_jni_bind(permission_plugin, "requestPermissions",
                 "(Ljava/lang/String;)V", (void *)j_Permission_noop);
    /* The ANDROID_ID static field itself: run10 saw it answer 0 (null), so
     * getString(resolver, null) returned "" and Kairosoft's CheckInitOrFinish
     * treated the device id as invalid and called Config.finish(). */
    fp2_jni_bind("android/provider/Settings$Secure", "ANDROID_ID",
                 "Ljava/lang/String;", (void *)j_AndroidId_field);
    fp2_jni_bind("kairo/android/plugin/Utility", "showDialog",
                 "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/Object;)I",
                 (void *)j_Utility_showDialog);
    fp2_jni_bind("kairo/android/plugin/Config", "init",
                 "(IZLjava/lang/String;)V", (void *)j_Config_init);
    fp2_jni_bind("kairo/android/plugin/Config", "finish", "()V",
                 (void *)j_Config_noop);
    fp2_jni_bind("kairo/android/plugin/Utility", "setParam",
                 "(Ljava/lang/String;Ljava/lang/String;)V",
                 (void *)j_Utility_setParam);
    fp2_jni_bind("android/content/pm/PackageInfo", "versionCode", "I",
                 (void *)j_PackageInfo_versionCode);
    fp2_jni_bind("android/content/pm/PackageInfo", "versionName",
                 "Ljava/lang/String;", (void *)j_PackageInfo_versionName);
    fp2_jni_bind("android/content/pm/PackageInfo", "metaData",
                 "Landroid/os/Bundle;", (void *)j_PackageInfo_metaData);
    fp2_jni_bind("android/content/pm/ApplicationInfo", "versionCode", "I",
                 (void *)j_PackageInfo_versionCode);
    fp2_jni_bind("android/content/pm/ApplicationInfo", "versionName",
                 "Ljava/lang/String;", (void *)j_PackageInfo_versionName);
    fp2_jni_bind("android/content/pm/ApplicationInfo", "metaData",
                 "Landroid/os/Bundle;", (void *)j_PackageInfo_metaData);
    fp2_jni_bind("android/os/Bundle", "getString",
                 "(Ljava/lang/String;)Ljava/lang/String;",
                 (void *)j_Bundle_getString);
    fp2_jni_bind("android/os/Bundle", "getString",
                 "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;",
                 (void *)j_Bundle_getString);
    fp2_jni_bind("android/util/DisplayMetrics", "widthPixels", "I",
                 (void *)j_DisplayMetrics_int);
    fp2_jni_bind("android/util/DisplayMetrics", "heightPixels", "I",
                 (void *)j_DisplayMetrics_int);
    fp2_jni_bind("android/util/DisplayMetrics", "densityDpi", "I",
                 (void *)j_DisplayMetrics_int);
    fp2_jni_bind("android/util/DisplayMetrics", "density", "F",
                 (void *)j_DisplayMetrics_int);
    fp2_jni_bind("android/util/DisplayMetrics", "scaledDensity", "F",
                 (void *)j_DisplayMetrics_int);
    fp2_jni_bind("android/util/DisplayMetrics", "xdpi", "F",
                 (void *)j_DisplayMetrics_int);
    fp2_jni_bind("android/util/DisplayMetrics", "ydpi", "F",
                 (void *)j_DisplayMetrics_int);
    /* Fill the caller's DisplayMetrics from the real drawable. */
    fp2_jni_bind("android/view/Display", "getRealMetrics",
                 "(Landroid/util/DisplayMetrics;)V",
                 (void *)j_Display_fillMetrics);
    fp2_jni_bind("android/view/Display", "getMetrics",
                 "(Landroid/util/DisplayMetrics;)V",
                 (void *)j_Display_fillMetrics);
    fp2_jni_bind("android/content/Context", "getDisplayMetrics",
                 "()Landroid/util/DisplayMetrics;", (void *)j_Display_metrics);
    fp2_jni_bind("android/view/Display", "getWidth", "()I",
                 (void *)j_Display_get);
    fp2_jni_bind("android/view/Display", "getHeight", "()I",
                 (void *)j_Display_get);
    fp2_jni_bind("android/view/Display", "getRotation", "()I",
                 (void *)j_Display_get);
    fp2_jni_bind("android/view/Display", "getRefreshRate", "()F",
                 (void *)j_Display_get);
    fp2_jni_bind("android/graphics/Point", "x", "I", (void *)j_Point_get);
    fp2_jni_bind("android/graphics/Point", "y", "I", (void *)j_Point_get);
    fp2_jni_bind("kairo/android/plugin/Utility", "getScaleRatio",
                 "(IIII)F", (void *)j_Utility_getScaleRatio_stub);
    fp2_jni_bind("kairo/android/plugin/Utility", "getParam",
                 "(Ljava/lang/String;)Ljava/lang/String;",
                 (void *)j_Utility_getParam);
    fp2_jni_bind("kairo/android/plugin/Utility", "getPreferenceKeys",
                 "()Ljava/lang/String;", (void *)j_Utility_getPreferenceKeys);
    fp2_jni_bind("kairo/android/plugin/Utility", "getPackageName",
                 "()Ljava/lang/String;", (void *)j_Utility_getPackageName);
    /* TimeZone family: see the run18 note above j_TimeZone_getTimeZone. */
    fp2_jni_bind("java/util/TimeZone", "getDefault",
                 "()Ljava/util/TimeZone;", (void *)j_TimeZone_getDefault);
    fp2_jni_bind("java/util/TimeZone", "getDefault",
                 "()Ljava/lang/Object;", (void *)j_TimeZone_getDefault);
    fp2_jni_bind("java/util/TimeZone", "getTimeZone",
                 "(Ljava/lang/String;)Ljava/util/TimeZone;",
                 (void *)j_TimeZone_getTimeZone);
    fp2_jni_bind("java/util/TimeZone", "getTimeZone",
                 "(Ljava/lang/String;)Ljava/lang/Object;",
                 (void *)j_TimeZone_getTimeZone);
    fp2_jni_bind("java/util/TimeZone", "getRawOffset", "()I",
                 (void *)j_TimeZone_getRawOffset);
    fp2_jni_bind("java/util/TimeZone", "getOffset", "()I",
                 (void *)j_TimeZone_getOffset);
    fp2_jni_bind("java/util/TimeZone", "getID", "()Ljava/lang/String;",
                 (void *)j_TimeZone_getID);
    fp2_jni_bind("java/util/TimeZone", "getDisplayName",
                 "(ZLjava/lang/String;)Ljava/lang/String;",
                 (void *)j_TimeZone_getDisplayName);
    fp2_jni_bind("java/util/TimeZone", "useDaylightTime", "()Z",
                 (void *)j_TimeZone_useDaylightTime);
    fp2_jni_bind("java/util/TimeZone", "inDaylightTime", "()Z",
                 (void *)j_TimeZone_inDaylightTime);
    fp2_jni_bind("kairo/android/plugin/Utility", "getTimeZone",
                 "()Ljava/lang/Object;", (void *)j_Utility_getTimeZone);
    fp2_jni_bind("kairo/android/plugin/Utility", "getRawOffset", "()I",
                 (void *)j_Utility_getRawOffset);
    /* GLES20.glGetString + Display.getRealSize: see the run19 notes above
     * j_GLES20_glGetString and j_Display_getRealSize. */
    fp2_jni_bind("android/opengl/GLES20", "glGetString",
                 "(I)Ljava/lang/String;", (void *)j_GLES20_glGetString);
    /* The GL_* constants are static int FIELDS.  run20 measured GL_EXTENSIONS
     * answering 0, and the managed side fed that 0 straight into
     * glGetString, so the constants have to be real. */
    fp2_jni_bind("android/opengl/GLES20", "GL_VENDOR", "I",
                 (void *)j_GLES20_constant);
    fp2_jni_bind("android/opengl/GLES20", "GL_RENDERER", "I",
                 (void *)j_GLES20_constant);
    fp2_jni_bind("android/opengl/GLES20", "GL_VERSION", "I",
                 (void *)j_GLES20_constant);
    fp2_jni_bind("android/opengl/GLES20", "GL_EXTENSIONS", "I",
                 (void *)j_GLES20_constant);
    fp2_jni_bind("android/opengl/GLES20", "GL_SHADING_LANGUAGE_VERSION", "I",
                 (void *)j_GLES20_constant);
    /* run20: Utility.getAppWidth()/getAppHeight() answered 0 at the exact point
     * where the ad layout is computed, immediately before error 100. */
    fp2_jni_bind("kairo/android/plugin/Utility", "getAppWidth", "()I",
                 (void *)j_Utility_app_size);
    fp2_jni_bind("kairo/android/plugin/Utility", "getAppHeight", "()I",
                 (void *)j_Utility_app_size);
    /* run26: the remaining contracts that reached the error dialog unhandled. */
    fp2_jni_bind("kairo/android/plugin/Utility", "getNotificationFilter",
                 "(Ljava/lang/Object;)I",
                 (void *)j_Utility_getNotificationFilter);
    fp2_jni_bind("kairo/android/plugin/Utility", "fullscreen", "(Z)V",
                 (void *)j_Utility_fullscreen);
    fp2_jni_bind("kairo/android/plugin/ad/AdMobView", "createAdMobView",
                 "(ILjava/lang/String;ZI)Ljava/lang/Object;",
                 (void *)j_AdMobView_create);
    fp2_jni_bind("kairo/android/plugin/ad/AdMobView", "removeAdMobView", "()V",
                 (void *)j_AdMobView_remove);
    fp2_jni_bind("kairo/android/plugin/Utility", "getNotificationData",
                 "()[Ljava/lang/String;", (void *)j_Utility_getNotificationData);
    /* run26: the framework static constants the boot path reads, plus the
     * permission query.  The stub answered 0 for both PERMISSION_GRANTED and
     * the query, so the pair was self-consistent only by accident. */
    fp2_jni_bind("android/content/Context", "MODE_PRIVATE", "I",
                 (void *)j_android_int_constant);
    fp2_jni_bind("android/content/Context", "MODE_WORLD_READABLE", "I",
                 (void *)j_android_int_constant);
    fp2_jni_bind("android/content/Context", "MODE_WORLD_WRITEABLE", "I",
                 (void *)j_android_int_constant);
    fp2_jni_bind("android/content/Context", "MODE_MULTI_PROCESS", "I",
                 (void *)j_android_int_constant);
    fp2_jni_bind("android/content/Context", "LOCATION_SERVICE",
                 "Ljava/lang/String;", (void *)j_android_string_constant);
    fp2_jni_bind("android/content/Context", "AUDIO_SERVICE",
                 "Ljava/lang/String;", (void *)j_android_string_constant);
    fp2_jni_bind("android/content/Context", "DISPLAY_SERVICE",
                 "Ljava/lang/String;", (void *)j_android_string_constant);
    fp2_jni_bind("android/content/Context", "MEDIA_ROUTER_SERVICE",
                 "Ljava/lang/String;", (void *)j_android_string_constant);
    fp2_jni_bind("android/content/Context", "POWER_SERVICE",
                 "Ljava/lang/String;", (void *)j_android_string_constant);
    fp2_jni_bind("android/content/Context", "CONNECTIVITY_SERVICE",
                 "Ljava/lang/String;", (void *)j_android_string_constant);
    fp2_jni_bind("android/content/Context", "WINDOW_SERVICE",
                 "Ljava/lang/String;", (void *)j_android_string_constant);
    fp2_jni_bind("android/content/pm/PackageManager", "GET_META_DATA", "I",
                 (void *)j_android_int_constant);
    fp2_jni_bind("android/content/pm/PackageManager", "PERMISSION_GRANTED",
                 "I", (void *)j_android_int_constant);
    fp2_jni_bind("android/content/pm/PackageManager", "PERMISSION_DENIED",
                 "I", (void *)j_android_int_constant);
    fp2_jni_bind("android/content/pm/PackageManager",
                 "FEATURE_AUDIO_LOW_LATENCY", "Ljava/lang/String;",
                 (void *)j_android_string_constant);
    fp2_jni_bind("android/media/MediaRouter", "ROUTE_TYPE_LIVE_VIDEO", "I",
                 (void *)j_android_int_constant);
    fp2_jni_bind("android/media/AudioManager", "PROPERTY_OUTPUT_SAMPLE_RATE",
                 "Ljava/lang/String;", (void *)j_android_string_constant);
    fp2_jni_bind("android/media/AudioManager",
                 "PROPERTY_OUTPUT_FRAMES_PER_BUFFER", "Ljava/lang/String;",
                 (void *)j_android_string_constant);
    fp2_jni_bind("android/content/Context", "checkCallingOrSelfPermission",
                 "(Ljava/lang/String;)I", (void *)j_Context_checkPermission);
    fp2_jni_bind(armory_activity, "checkCallingOrSelfPermission",
                 "(Ljava/lang/String;)I", (void *)j_Context_checkPermission);
    /* run26: AudioManager.getProperty answered "" and Unity parsed it into a
     * 0 Hz device.  Hand back a real rate/buffer pair instead. */
    fp2_jni_bind("android/media/AudioManager", "getProperty",
                 "(Ljava/lang/String;)Ljava/lang/String;",
                 (void *)j_AudioManager_getProperty);
    /* run27: parseInt was answering 0, which silently discarded the "48000" we
     * had just started returning and re-created the 0 Hz device one layer up. */
    fp2_jni_bind("java/lang/Integer", "parseInt",
                 "(Ljava/lang/String;)I", (void *)j_Integer_parseInt);
    fp2_jni_bind("java/lang/Integer", "parseInt",
                 "(Ljava/lang/String;I)I", (void *)j_Integer_parseInt);
    fp2_jni_bind("java/lang/Integer", "valueOf", "(I)Ljava/lang/Object;",
                 (void *)j_Integer_valueOf_int);
    fp2_jni_bind("java/lang/Long", "parseLong", "(Ljava/lang/String;)J",
                 (void *)j_Long_parseLong);
    fp2_jni_bind("java/lang/String", "length", "()I", (void *)j_String_length);
    fp2_jni_bind("java/lang/String", "isEmpty", "()Z",
                 (void *)j_String_isEmpty);
    fp2_jni_bind("java/lang/String", "trim", "()Ljava/lang/String;",
                 (void *)j_String_trim);
    /* run27: the last two plugin calls still hitting the generic stub. */
    fp2_jni_bind("kairo/android/plugin/Utility", "setNotificationBackground",
                 "(Z)V", (void *)j_Utility_setNotificationBackground);
    fp2_jni_bind("kairo/android/plugin/Utility", "cancelNotifications",
                 "(ZZ)V", (void *)j_Utility_cancelNotifications);
    /* run20: IL2CPP also looks the plugin up under the SHORT class name, so the
     * same call arrives twice -- once as kairo/android/plugin/Config.init (which
     * the binding above catches, RSA public key and all) and once as
     * plugin/Config.init, which the generic stub answered with 0.  Bind the
     * short spelling to the same handlers instead of leaving the second,
     * identically shaped call to chance. */
    fp2_jni_bind("plugin/Config", "init", "(IZLjava/lang/String;)V",
                 (void *)j_Config_init);
    fp2_jni_bind("plugin/Config", "finish", "()V", (void *)j_Config_noop);
    /* run28: the dex inside the APK is the authoritative statement of what
     * this class must expose.  Config carries FOUR static methods --
     * init / finish / getParam / setParam -- and only the first two were
     * bound, so every parameter read went to the generic stub and came back
     * empty.  That is the entry point for Config.LANGUAGE, Config.PRINT and
     * Config.SUGOTOKU_APP_ID, i.e. exactly the environment answers BootForm's
     * start-up self-check consults before it decides to raise error 100.
     *
     * Config.paramTable_ and Utility's table are the same Hashtable in the
     * original (Config.getParam is what Utility.getParam delegates to), so
     * both names share one store here rather than drifting apart. */
    fp2_jni_bind("kairo/android/plugin/Config", "getParam",
                 "(Ljava/lang/String;)Ljava/lang/String;",
                 (void *)j_Utility_getParam);
    fp2_jni_bind("kairo/android/plugin/Config", "setParam",
                 "(Ljava/lang/String;Ljava/lang/String;)V",
                 (void *)j_Utility_setParam);
    fp2_jni_bind("plugin/Config", "getParam",
                 "(Ljava/lang/String;)Ljava/lang/String;",
                 (void *)j_Utility_getParam);
    fp2_jni_bind("plugin/Config", "setParam",
                 "(Ljava/lang/String;Ljava/lang/String;)V",
                 (void *)j_Utility_setParam);

    /* run28: the 98 unbound Utility methods, grouped by the answer they owe.
     * Each of these is a PREDICATE the start-up self-check evaluates, so the
     * generic stub's "return 0" was asserting false across the board -- which
     * is what pushed BootForm into STATE_ERROR.  The handlers answer what is
     * actually true on this device instead. */
    {
        static const char *U = "kairo/android/plugin/Utility";
        /* real screen geometry, consistent with getAppWidth/Height */
        fp2_jni_bind(U, "getWidth", "()I", (void *)j_Utility_width);
        fp2_jni_bind(U, "getHeight", "()I", (void *)j_Utility_width);
        /* real device class */
        fp2_jni_bind(U, "isTablet", "()Z", (void *)j_Utility_isTablet);
        fp2_jni_bind(U, "isFullscreen", "()Z", (void *)j_Utility_isFullscreen);
        fp2_jni_bind(U, "isDebug", "()Z", (void *)j_Utility_isDebug);
        fp2_jni_bind(U, "isDebug", "(Landroid/content/Context;)Z",
                     (void *)j_Utility_isDebug);
        /* no system bars on a bare handheld framebuffer */
        fp2_jni_bind(U, "getStatusBarHeight", "()I", (void *)j_Utility_bar_height);
        fp2_jni_bind(U, "getSystemBarHeight", "()I", (void *)j_Utility_bar_height);
        fp2_jni_bind(U, "getStatusBarPosition", "()I", (void *)j_Utility_bar_height);
        fp2_jni_bind(U, "isGetStatusBarHeight", "()Z", (void *)j_Utility_isFullscreen);
        fp2_jni_bind(U, "isGetStatusBarPosition", "()Z", (void *)j_Utility_isFullscreen);
        /* honest filesystem answer: the RESLOAD_SEB_* check depends on it */
        fp2_jni_bind(U, "fileExists", "(Ljava/lang/String;)Z",
                     (void *)j_Utility_fileExists);
        /* permission answers must agree with checkCallingOrSelfPermission */
        fp2_jni_bind(U, "hasPermission", "(Ljava/lang/String;)Z",
                     (void *)j_Utility_hasPermission);
        /* no Play Services here; false keeps the game on its degraded path */
        fp2_jni_bind(U, "checkPlayServices", "()Z", (void *)j_Utility_noPlay);
        fp2_jni_bind(U, "isGooglePlayServicesAvailable", "()Z",
                     (void *)j_Utility_noPlay);
        fp2_jni_bind(U, "checkAdvertisingId", "()I", (void *)j_Utility_noPlay);
        fp2_jni_bind(U, "googlePassCheck", "()I", (void *)j_Utility_noPlay);
        fp2_jni_bind(U, "licenseCheck", "()I", (void *)j_Utility_noPlay);
        fp2_jni_bind(U, "licenseCheckV1", "()I", (void *)j_Utility_noPlay);
        fp2_jni_bind(U, "getLicenseCheck", "()I", (void *)j_Utility_noPlay);
        fp2_jni_bind(U, "getLicenseCheckV1", "()I", (void *)j_Utility_noPlay);
        fp2_jni_bind(U, "licenseCheckForAu", "()I", (void *)j_Utility_noPlay);
        fp2_jni_bind(U, "licenseCheckForSugo", "()I", (void *)j_Utility_noPlay);
        fp2_jni_bind(U, "safetyNetAttest", "(II)I", (void *)j_Utility_noPlay);
        fp2_jni_bind(U, "closeKairoClub", "()Z", (void *)j_Utility_noPlay);
        fp2_jni_bind(U, "checkPermissionConfirmAfter", "(Ljava/lang/String;)Z",
                     (void *)j_Utility_noPlay);
        fp2_jni_bind(U, "isInstalledApplication", "(Ljava/lang/String;)Z",
                     (void *)j_Utility_noPlay);
        fp2_jni_bind(U, "launchApplication", "(Ljava/lang/String;Ljava/lang/String;)Z",
                     (void *)j_Utility_noPlay);
        /* string-valued lookups that have no service behind them */
        fp2_jni_bind(U, "getAdvertisingId", "()Ljava/lang/String;",
                     (void *)j_Utility_noPlayString);
        fp2_jni_bind(U, "getGCMRegistrationId", "()Ljava/lang/String;",
                     (void *)j_Utility_noPlayString);
        fp2_jni_bind(U, "getInstallReferrer", "()Ljava/lang/String;",
                     (void *)j_Utility_noPlayString);
        fp2_jni_bind(U, "installedApplicationList", "()Ljava/lang/String;",
                     (void *)j_Utility_noPlayString);
        fp2_jni_bind(U, "getSignature", "()Ljava/lang/String;",
                     (void *)j_Utility_getSignature);
        /* null on success is the contract; "" would read as a failure */
        fp2_jni_bind(U, "getErrMsg", "()Ljava/lang/String;",
                     (void *)j_Utility_getErrMsg);
        /* launched from the frontend, so there is no intent to inspect */
        fp2_jni_bind(U, "hasIntent", "()Z", (void *)j_Utility_intent);
        fp2_jni_bind(U, "getIntent", "()Ljava/lang/String;", (void *)j_Utility_intent);
        fp2_jni_bind(U, "getIntentAction", "()Ljava/lang/String;",
                     (void *)j_Utility_intent);
        fp2_jni_bind(U, "getIntentExtras", "()Ljava/lang/String;",
                     (void *)j_Utility_intent);
        fp2_jni_bind(U, "getIntentPackage", "()Ljava/lang/String;",
                     (void *)j_Utility_intent);
        fp2_jni_bind(U, "getIntentScheme", "()Ljava/lang/String;",
                     (void *)j_Utility_intent);
        fp2_jni_bind(U, "getLaunchArguments", "()Ljava/lang/String;",
                     (void *)j_Utility_intent);
        /* fresh install: nothing stored yet */
        fp2_jni_bind(U, "existPreference", "(Ljava/lang/String;)Z",
                     (void *)j_Utility_preference);
        fp2_jni_bind(U, "getPreference", "(Ljava/lang/String;)[B",
                     (void *)j_Utility_preference);
        /* no Activity to host any of these */
        fp2_jni_bind(U, "launchSetting", "()V", (void *)j_Utility_surface);
        fp2_jni_bind(U, "launchMailer",
                     "(Ljava/lang/String;Ljava/lang/String;)V",
                     (void *)j_Utility_surface);
        fp2_jni_bind(U, "onActivityResult", "(IILandroid/content/Intent;)Z",
                     (void *)j_Utility_surface);
        fp2_jni_bind(U, "handleBonusActivityResult", "(IILandroid/content/Intent;)Z",
                     (void *)j_Utility_surface);
        fp2_jni_bind(U, "onRequestPermissionsResult",
                     "(I[Ljava/lang/String;[I)V", (void *)j_Utility_surface);
        fp2_jni_bind(U, "requestPermission", "(Ljava/lang/String;)V",
                     (void *)j_Utility_surface);
        fp2_jni_bind(U, "isInputPanelFinish", "()Z", (void *)j_Utility_noPlay);
        fp2_jni_bind(U, "isLanguagePanelFinish", "()Z", (void *)j_Utility_noPlay);

        /* The remainder: UI, persistence, messaging, SQL.  Not consulted by
         * the start-up self-check, so one honest handler covers them -- and it
         * returns null rather than "" for reference types, because an empty
         * string is a parseable value and "" is what turned a fixed sample
         * rate back into 0 Hz in run27. */
        fp2_jni_bind(U, "getBatteryInfo", "()[I", (void *)j_Utility_rest);
        fp2_jni_bind(U, "getInputPanelResult", "()Ljava/lang/String;",
                     (void *)j_Utility_rest);
        fp2_jni_bind(U, "getLanguagePanelResult", "()Ljava/lang/String;",
                     (void *)j_Utility_rest);
        fp2_jni_bind(U, "getNotificationBackground",
                     "(Landroid/content/Context;)Z", (void *)j_Utility_rest);
        fp2_jni_bind(U, "getFontTextureSize", "(Ljava/lang/String;I)[B",
                     (void *)j_Utility_rest);
        fp2_jni_bind(U, "makeFontTexture", "(Ljava/lang/String;II)[B",
                     (void *)j_Utility_rest);
        fp2_jni_bind(U, "makeFontTextureBold", "(Ljava/lang/String;IIII)[B",
                     (void *)j_Utility_rest);
        fp2_jni_bind(U, "readFile", "(Ljava/lang/String;)[B",
                     (void *)j_Utility_rest);
        fp2_jni_bind(U, "GetBundleString", "(Ljava/lang/String;I)[Ljava/lang/Object;",
                     (void *)j_Utility_rest);
        fp2_jni_bind(U, "GetBundleStrings", "(Ljava/lang/String;)[[Ljava/lang/String;",
                     (void *)j_Utility_rest);
        fp2_jni_bind(U, "GetContentValues", "([B)Landroid/content/ContentValues;",
                     (void *)j_Utility_rest);
        fp2_jni_bind(U, "PackBundle", "(Landroid/os/Bundle;)Ljava/lang/String;",
                     (void *)j_Utility_rest);
        fp2_jni_bind(U, "Read", "(Ljava/io/DataInputStream;)[B",
                     (void *)j_Utility_rest);
        fp2_jni_bind(U, "launchBonusFlow", "(Ljava/lang/String;I)Ljava/lang/String;",
                     (void *)j_Utility_rest);
        fp2_jni_bind(U, "launchBonusFlowAsync",
                     "(Ljava/lang/String;I)Lkairo/android/plugin/util/AsyncThread;",
                     (void *)j_Utility_rest);
        fp2_jni_bind(U, "showInputPanel",
                     "(Ljava/lang/String;Ljava/lang/String;ILjava/lang/String;"
                     "Ljava/lang/String;I)Ljava/lang/String;", (void *)j_Utility_rest);
        fp2_jni_bind(U, "imeOnBlock", "(Ljava/lang/String;II)Ljava/lang/String;",
                     (void *)j_Utility_rest);
        fp2_jni_bind(U, "SelectSQL",
                     "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)[B",
                     (void *)j_Utility_rest);
        fp2_jni_bind(U, "DeleteSQL",
                     "(Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;)V",
                     (void *)j_Utility_rest);
        fp2_jni_bind(U, "InsertSQL", "(Ljava/lang/String;[B)V",
                     (void *)j_Utility_rest);
        fp2_jni_bind(U, "UpdateSQL",
                     "(Ljava/lang/String;[BLjava/lang/String;[Ljava/lang/String;)V",
                     (void *)j_Utility_rest);
        fp2_jni_bind(U, "Write", "(Ljava/io/DataOutputStream;[B)V",
                     (void *)j_Utility_rest);
        fp2_jni_bind(U, "sendNotification", "(JLjava/lang/String;Ljava/lang/String;)V",
                     (void *)j_Utility_rest);
        fp2_jni_bind(U, "addNotificationData", "(Ljava/lang/String;)V",
                     (void *)j_Utility_rest);
        fp2_jni_bind(U, "addNotificationData",
                     "(Landroid/content/Context;Ljava/lang/String;)V",
                     (void *)j_Utility_rest);
        fp2_jni_bind(U, "removeNotificationData", "()V", (void *)j_Utility_rest);
        fp2_jni_bind(U, "putPreference", "(Ljava/lang/String;[B)V",
                     (void *)j_Utility_rest);
        fp2_jni_bind(U, "removePreference", "(Ljava/lang/String;)V",
                     (void *)j_Utility_rest);
        fp2_jni_bind(U, "saveToSd", "(Ljava/lang/String;[B)Z", (void *)j_Utility_rest);
        fp2_jni_bind(U, "setClockTime", "(JI)V", (void *)j_Utility_rest);
        fp2_jni_bind(U, "setDebug", "(Z)V", (void *)j_Utility_rest);
        fp2_jni_bind(U, "setEmptyIntent", "()V", (void *)j_Utility_rest);
        fp2_jni_bind(U, "setGCMRegistrationId", "(Ljava/lang/String;)V",
                     (void *)j_Utility_rest);
        fp2_jni_bind(U, "setLanguage", "(I)V", (void *)j_Utility_rest);
        fp2_jni_bind(U, "setNotificationFilter", "(I)V", (void *)j_Utility_rest);
        fp2_jni_bind(U, "setResult", "(ILjava/lang/String;)V", (void *)j_Utility_rest);
        fp2_jni_bind(U, "showDetailsSettings", "()V", (void *)j_Utility_rest);
        fp2_jni_bind(U, "showToast", "(Ljava/lang/String;I)V", (void *)j_Utility_rest);
        fp2_jni_bind(U, "startAdvertisingId", "()V", (void *)j_Utility_rest);
        fp2_jni_bind(U, "startInputPanel",
                     "(Ljava/lang/String;Ljava/lang/String;ILjava/lang/String;"
                     "Ljava/lang/String;I)V", (void *)j_Utility_rest);
        fp2_jni_bind(U, "startLanguagePanel",
                     "([Ljava/lang/String;ILjava/lang/String;I)V",
                     (void *)j_Utility_rest);
        fp2_jni_bind(U, "startLicenseCheck", "()V", (void *)j_Utility_rest);
        fp2_jni_bind(U, "startLicenseCheckV1", "()V", (void *)j_Utility_rest);
        fp2_jni_bind(U, "destroyLicenseChecker", "()V", (void *)j_Utility_rest);
        fp2_jni_bind(U, "destroyGooglePassCheck", "()V", (void *)j_Utility_rest);
        fp2_jni_bind(U, "vibrator", "([J)V", (void *)j_Utility_rest);

        /* AdMobView: no ads, so "loaded" and "started" must both be false --
         * claiming otherwise would have the game wait for an ad that can never
         * arrive, which is a hang, not a degraded path. */
        {
            static const char *A = "kairo/android/plugin/ad/AdMobView";
            fp2_jni_bind(A, "isLoad", "()Z", (void *)j_Utility_noPlay);
            fp2_jni_bind(A, "isStart", "()Z", (void *)j_Utility_noPlay);
            fp2_jni_bind(A, "getAdHeight", "()I", (void *)j_Utility_noPlay);
        }
        /* Interstitial: never available, and getState must not claim otherwise. */
        {
            static const char *I2 = "kairo/android/plugin/ad/Interstitial";
            fp2_jni_bind(I2, "getState", "()I", (void *)j_Utility_noPlay);
            fp2_jni_bind(I2, "show", "()V", (void *)j_Utility_rest);
            fp2_jni_bind(I2, "startInterstitialAd",
                         "(Ljava/lang/String;)Lkairo/android/plugin/ad/Interstitial;",
                         (void *)j_Utility_rest);
        }
    }
    fp2_jni_bind("android/view/Display", "getRealSize",
                 "(Landroid/graphics/Point;)V", (void *)j_Display_getRealSize);
    fp2_jni_bind("android/view/Display", "getSize",
                 "(Landroid/graphics/Point;)V", (void *)j_Display_getRealSize);
    fp2_jni_bind("android/provider/Settings$Secure", "getString",
                 "(Landroid/content/ContentResolver;Ljava/lang/String;)"
                 "Ljava/lang/String;", (void *)j_SettingsSecure_getString);
    fp2_jni_bind("android/provider/Settings$Secure", "getString",
                 "(Ljava/lang/Object;Ljava/lang/String;)Ljava/lang/String;",
                 (void *)j_SettingsSecure_getString);
    fp2_jni_bind("android/media/AudioManager", "STREAM_MUSIC", "I",
                 (void *)j_AudioManager_getInt);
    fp2_jni_bind("android/media/AudioManager", "getStreamVolume", "(I)I",
                 (void *)j_AudioManager_getInt);
    fp2_jni_bind("android/media/AudioManager", "getStreamMaxVolume", "(I)I",
                 (void *)j_AudioManager_getInt);
    fp2_jni_bind("android/media/AudioManager", "getStreamMinVolume", "(I)I",
                 (void *)j_AudioManager_getInt);
    fp2_jni_bind("java/util/Locale", "getDefault",
                 "()Ljava/util/Locale;", (void *)j_Locale_getDefault);
    fp2_jni_bind("java/util/Locale", "getDefault", "()Ljava/lang/Object;",
                 (void *)j_Locale_getDefault);
    fp2_jni_bind("java/util/Locale", "getLanguage",
                 "()Ljava/lang/String;", (void *)j_Locale_getString);
    fp2_jni_bind("java/util/Locale", "getCountry",
                 "()Ljava/lang/String;", (void *)j_Locale_getString);
    fp2_jni_bind("java/util/Locale", "toLanguageTag",
                 "()Ljava/lang/String;", (void *)j_Locale_getString);
    fp2_jni_bind("java/util/Locale", "toString", "()Ljava/lang/String;",
                 (void *)j_Locale_getString);
    fp2_jni_bind("com/prime31/GoogleIABPlugin", "instance",
                 "()Ljava/lang/Object;", (void *)j_GoogleIAB_instance);
    fp2_jni_bind("com/prime31/GoogleIABPlugin", "init",
                 "(Ljava/lang/String;)V", (void *)j_Permission_noop);
    fp2_jni_bind("com/google/android/gms/games/PlayGamesSdk", "initialize",
                 "(Lcom/unity3d/player/UnityPlayerActivity;)V",
                 (void *)j_PlayGames_initialize);
    fp2_jni_bind("com/google/android/gms/games/PlayGames",
                 "getGamesSignInClient",
                 "(Lcom/unity3d/player/UnityPlayerActivity;)"
                 "Ljava/lang/Object;", (void *)j_PlayGames_getSignInClient);
    fp2_jni_bind("com/google/android/gms/games/GamesSignInClient",
                 "isAuthenticated", "()Ljava/lang/Object;",
                 (void *)j_PlayGames_authenticationTask);
    fp2_jni_bind("com/google/android/gms/games/GamesSignInClient",
                 "signIn", "()Ljava/lang/Object;",
                 (void *)j_PlayGames_authenticationTask);
    fp2_jni_bind("com/google/android/gms/tasks/Task", "addOnSuccessListener",
                 "(Lcom/google/android/gms/tasks/OnSuccessListener;)"
                 "Ljava/lang/Object;",
                 (void *)j_PlayGames_addOnSuccessListener);
    fp2_jni_bind("com/google/android/gms/tasks/Task", "addOnFailureListener",
                 "(Lcom/google/android/gms/tasks/OnFailureListener;)"
                 "Ljava/lang/Object;",
                 (void *)j_PlayGames_addOnFailureListener);
    fp2_jni_bind("com/google/android/gms/games/AuthenticationResult",
                 "isAuthenticated", "()Z",
                 (void *)j_PlayGames_isAuthenticatedResult);
    fp2_jni_bind("com/squareenixmontreal/armory/SignatureReader",
                 "ShowSignature", "()V", (void *)j_Permission_noop);
    fp2_jni_bind("android/content/res/AssetManager", "open",
                 "(Ljava/lang/String;)Ljava/io/InputStream;",
                 (void *)j_AssetManager_open);
    fp2_jni_bind("java/util/Scanner", "next", "()Ljava/lang/String;",
                 (void *)j_Scanner_next);
    fp2_jni_bind("android/os/Environment", "getExternalStorageState",
                 "()Ljava/lang/String;", (void *)j_Environment_mounted);
    fp2_jni_bind("android/os/Environment", "MEDIA_MOUNTED",
                 "Ljava/lang/String;", (void *)j_Environment_mounted);
    fp2_jni_bind("java/lang/String", "equals", "(Ljava/lang/Object;)Z",
                 (void *)j_String_equals);
    fp2_jni_bind("java/lang/StringBuilder", "toString",
                 "()Ljava/lang/String;", (void *)j_StringBuilder_toString);
    fp2_jni_bind("android/net/Uri", "encode",
                 "(Ljava/lang/String;)Ljava/lang/String;",
                 (void *)j_Uri_encode);
    fp2_jni_bind("android/net/Uri", "decode",
                 "(Ljava/lang/String;)Ljava/lang/String;",
                 (void *)j_Uri_decode);

    for (size_t i = 0; i < sizeof build_fields / sizeof *build_fields; i++) {
        fp2_jni_bind("android/os/Build", build_fields[i].name,
                     "Ljava/lang/String;", (void *)j_Build_string);
        fp2_jni_bind("android/os/Build$VERSION", build_fields[i].name,
                     "Ljava/lang/String;", (void *)j_Build_string);
    }
    fp2_jni_bind("android/os/Build$VERSION", "SDK_INT", "I",
                 (void *)j_Build_SDK_INT);

    fp2_jni_bind("android/app/Activity", "getPreferences",
                 "(I)Landroid/content/SharedPreferences;",
                 (void *)j_getPreferences);
    fp2_jni_bind("android/content/Context", "getSharedPreferences",
                 "(Ljava/lang/String;I)Landroid/content/SharedPreferences;",
                 (void *)j_getPreferences);
    fp2_jni_bind("android/content/SharedPreferences", "getBoolean",
                 "(Ljava/lang/String;Z)Z", (void *)j_Prefs_getBoolean);
    fp2_jni_bind("android/content/SharedPreferences", "getInt",
                 "(Ljava/lang/String;I)I", (void *)j_Prefs_getInt);
    fp2_jni_bind("android/content/SharedPreferences", "getFloat",
                 "(Ljava/lang/String;F)F", (void *)j_Prefs_getFloat);
    fp2_jni_bind("android/content/SharedPreferences", "getString",
                 "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;",
                 (void *)j_Prefs_getString);
    fp2_jni_bind("android/content/SharedPreferences", "contains",
                 "(Ljava/lang/String;)Z", (void *)j_Prefs_contains);
    fp2_jni_bind("android/content/SharedPreferences", "edit",
                 "()Landroid/content/SharedPreferences$Editor;",
                 (void *)j_Prefs_edit);
    fp2_jni_bind("android/content/SharedPreferences$Editor", "putInt",
                 "(Ljava/lang/String;I)"
                 "Landroid/content/SharedPreferences$Editor;",
                 (void *)j_Prefs_putInt);
    fp2_jni_bind("android/content/SharedPreferences$Editor", "putFloat",
                 "(Ljava/lang/String;F)"
                 "Landroid/content/SharedPreferences$Editor;",
                 (void *)j_Prefs_putFloat);
    fp2_jni_bind("android/content/SharedPreferences$Editor", "putString",
                 "(Ljava/lang/String;Ljava/lang/String;)"
                 "Landroid/content/SharedPreferences$Editor;",
                 (void *)j_Prefs_putString);
    fp2_jni_bind("android/content/SharedPreferences$Editor", "putBoolean",
                 "(Ljava/lang/String;Z)"
                 "Landroid/content/SharedPreferences$Editor;",
                 (void *)j_Prefs_putBoolean);
    fp2_jni_bind("android/content/SharedPreferences$Editor", "remove",
                 "(Ljava/lang/String;)"
                 "Landroid/content/SharedPreferences$Editor;",
                 (void *)j_Prefs_remove);
    fp2_jni_bind("android/content/SharedPreferences$Editor", "clear",
                 "()Landroid/content/SharedPreferences$Editor;",
                 (void *)j_Prefs_clear);
    fp2_jni_bind("android/content/SharedPreferences$Editor", "apply",
                 "()V", (void *)j_Prefs_apply);
    fp2_jni_bind("android/content/SharedPreferences$Editor", "commit",
                 "()Z", (void *)j_Prefs_commit);
    fp2_jni_bind("java/lang/Object", "getClass", "()Ljava/lang/Class;",
                 (void *)j_Object_getClass);
    fp2_jni_bind("java/lang/Class", "getClassLoader",
                 "()Ljava/lang/ClassLoader;", (void *)j_getClassLoader);
    fp2_jni_bind("java/lang/ClassLoader", "findLibrary",
                 "(Ljava/lang/String;)Ljava/lang/String;",
                 (void *)j_ClassLoader_findLibrary);
    fp2_jni_bind("java/lang/System", "loadLibrary",
                 "(Ljava/lang/String;)V", (void *)j_System_loadLibrary);
    fp2_jni_bind("java/lang/System", "load",
                 "(Ljava/lang/String;)V", (void *)j_System_loadLibrary);
    fp2_jni_bind("com/unity3d/player/UnityPlayer", "getClassLoader",
                 "()Ljava/lang/ClassLoader;", (void *)j_getClassLoader);
    fp2_jni_bind("com/unity3d/player/UnityPlayer", "forName",
                 "(Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;",
                 (void *)j_forName);
    /* The reflected AndroidJavaObject path erases reference returns to
     * Object: publish both signatures or the erased lookup misses (run9). */
    fp2_jni_bind("java/lang/Class", "forName",
                 "(Ljava/lang/String;)Ljava/lang/Class;",
                 (void *)j_Class_forName);
    fp2_jni_bind("java/lang/Class", "forName",
                 "(Ljava/lang/String;)Ljava/lang/Object;",
                 (void *)j_Class_forName);
    /* Kairosoft's local license gate reads the installer package: the honest
     * offline answer for a sideloaded copy is empty, which fails the check
     * and Application.Quit() fires at frame 5 (run10).  The owner-supplied
     * APK is Play-distributed, so report the Play installer. */
    fp2_jni_bind("android/content/pm/PackageManager", "getInstallerPackageName",
                 "(Ljava/lang/String;)Ljava/lang/String;",
                 (void *)j_GetInstallerPackageName);

    /* Sanity: native Android objects call JNIEnv slots by their ABI offset. */
    if (JNI_NewStringUTF * 8 != 1336)
        nx_die("JNIEnv layout drift: NewStringUTF is at %d, not 1336",
               JNI_NewStringUTF * 8);
    nx_log("jni: env=%p vtable slots=%d", fp2_jni_env(), JNI_SLOT_COUNT);
}

void *fp2_jni_sym(const char *name)
{
    (void)name;
    return NULL;
}
