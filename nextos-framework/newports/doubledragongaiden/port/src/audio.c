/*
 * audio.c -- Unity audio output on NextOS (Double Dragon Gaiden).
 *
 * This title carries no audio engine of its own: Unity 2021.3's built-in
 * stack drives the output, on Android through OpenSL ES (and optionally
 * AAudio) which libunity reaches by dlopen at runtime.  bionic.c hands out
 * fake handles for "libOpenSLES.so" / "libaaudio.so", my_dlsym resolves the
 * entry points here, and the facades below feed one shared SDL queued device.
 *
 * The device core is the 60parsecs lineage: backpressure with a ceiling, the
 * device lives for the whole process, and every facade view writes through
 * sf_audio_device_write().  For the first runs the OpenSL ES side reports
 * FEATURE_UNSUPPORTED so the game boots silent by design instead of failing
 * somewhere inside libunity; the real OpenSL bridge lands once the picture
 * is up and the dlsym trace shows exactly which interfaces it binds.
 */

#define _GNU_SOURCE
#include <SDL2/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>

#include "nx_elf.h"
#include "sf.h"

static SDL_AudioDeviceID output;
static int out_channels;
static int out_rate;
static Uint32 queue_target;
static int audio_subsystem;
static unsigned long blocks_written;
static int trace;

static void device_destroy(void);

static int silent_driver(const char *name)
{
    return !name || strcmp(name, "dummy") == 0 || strcmp(name, "disk") == 0;
}

static int pcm_peak_s16(const int16_t *samples, int count)
{
    int peak = 0;
    for (int i = 0; i < count; i++) {
        int value = samples[i];
        if (value < 0)
            value = value == -32768 ? 32768 : -value;
        if (value > peak)
            peak = value;
    }
    return peak;
}

static int ensure_subsystem(void)
{
    if (audio_subsystem)
        return 1;
    const char *requested = getenv("SF_AUDIO_DRIVER");
    if (requested && *requested)
        setenv("SDL_AUDIODRIVER", requested, 1);
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "[audio] SDL audio init failed: %s\n", SDL_GetError());
        return 0;
    }
    audio_subsystem = 1;
    trace = getenv("SF_AUDIO_TRACE") != NULL;

    fprintf(stderr, "[audio] SDL drivers:");
    for (int i = 0, n = SDL_GetNumAudioDrivers(); i < n; i++) {
        const char *name = SDL_GetAudioDriver(i);
        if (name)
            fprintf(stderr, " %s", name);
    }
    fprintf(stderr, "\n[audio] backend selected: %s\n",
            SDL_GetCurrentAudioDriver() ? SDL_GetCurrentAudioDriver()
                                        : "(none)");
    return 1;
}

/* Shared SDL playback device.  Format must match across callers to reuse:
 * reopening per stream would drop the other facade's queued audio. */
int sf_audio_device_init(int channels, int rate, int frames, int buffers)
{
    if (getenv("SF_NO_AUDIO"))
        return 0;
    if (!ensure_subsystem())
        return 0;
    if (silent_driver(SDL_GetCurrentAudioDriver()))
        return 0;
    if (channels < 1 || channels > 2 || rate < 8000 || rate > 192000) {
        fprintf(stderr, "[audio] formato recusado: %d canais, %d Hz\n",
                channels, rate);
        return 0;
    }
    if (frames < 128)
        frames = 128;
    if (frames > 4096)
        frames = 4096;
    if (buffers < 2)
        buffers = 2;
    if (buffers > 16)
        buffers = 16;

    if (output && out_channels == channels && out_rate == rate) {
        blocks_written = 0;
        queue_target = (Uint32)frames * (Uint32)channels *
                       (Uint32)sizeof(int16_t) * (Uint32)buffers;
        SDL_PauseAudioDevice(output, 0);
        fprintf(stderr,
                "[audio] device init: reutilizando o dispositivo SDL "
                "(%d Hz x %d, frames=%d, buffers=%d)\n",
                rate, channels, frames, buffers);
        return 1;
    }

    device_destroy();

    SDL_AudioSpec want, have;
    memset(&want, 0, sizeof want);
    memset(&have, 0, sizeof have);
    want.freq = rate;
    want.format = AUDIO_S16SYS;
    want.channels = (Uint8)channels;
    want.samples = (Uint16)frames;

    output = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!output) {
        fprintf(stderr, "[audio] SDL_OpenAudioDevice falhou: %s\n",
                SDL_GetError());
        return 0;
    }
    out_channels = channels;
    out_rate = rate;
    blocks_written = 0;
    queue_target = (Uint32)frames * (Uint32)channels *
                   (Uint32)sizeof(int16_t) * (Uint32)buffers;
    SDL_PauseAudioDevice(output, 0);
    fprintf(stderr,
            "[audio] device init: %d Hz, %d canal(is), frames=%d, "
            "buffers=%d (SDL %d Hz/%d, driver=%s)\n",
            rate, channels, frames, buffers, have.freq, have.channels,
            SDL_GetCurrentAudioDriver() ? SDL_GetCurrentAudioDriver() : "?");
    return 1;
}

void sf_audio_device_write(const void *data, int bytes)
{
    if (!output || !data || bytes <= 0)
        return;
    /* Contrapressao com TETO: se a fila nao drenar (device sumindo, backend
     * travado) o chamador nao pode ficar preso para sempre. */
    for (int spin = 0; spin < 500 &&
                       SDL_GetQueuedAudioSize(output) >= queue_target; spin++)
        usleep(1000);
    if (SDL_QueueAudio(output, data, (Uint32)bytes) != 0) {
        fprintf(stderr, "[audio] SDL_QueueAudio falhou: %s\n", SDL_GetError());
        return;
    }
    blocks_written++;
    if (trace && (blocks_written <= 8 || blocks_written % 512 == 0))
        fprintf(stderr, "[audio] bloco %lu (%d bytes) peak=%d queued=%u\n",
                blocks_written, bytes,
                pcm_peak_s16(data, bytes / (int)sizeof(int16_t)),
                (unsigned)SDL_GetQueuedAudioSize(output));
}

/* Real teardown: only for a format change or process exit. */
static void device_destroy(void)
{
    if (!output)
        return;
    SDL_PauseAudioDevice(output, 1);
    SDL_ClearQueuedAudio(output);
    SDL_CloseAudioDevice(output);
    output = 0;
    blocks_written = 0;
}

void sf_audio_device_close(void)
{
    if (!output)
        return;
    blocks_written = 0;
    fprintf(stderr, "[audio] device close\n");
}

int sf_audio_started(void)
{
    return output != 0;
}

/* Unity owns the whole mix here (no Java-side pump as in the FMOD lineage):
 * the facades below are demand-driven by libunity, so there is nothing to
 * start up front. */
int sf_audio_start(void *env)
{
    (void)env;
    return 1;
}

void sf_audio_stop(void)
{
    device_destroy();
    if (audio_subsystem) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        audio_subsystem = 0;
    }
}

/* ------------------------------------------------------------------- AAudio
 *
 * Unity 2021.3 can route its output through AAudio when the system reports a
 * new enough API level (this port answers 33).  The facade below is the
 * proven 60parsecs bridge: real builder/stream state machines feeding the
 * shared SDL device, both in the data-callback and blocking-write modes.
 * Signatures and constants are the NDK's, because callers compare against
 * them by value.
 */

/* Values are the ones in the NDK's aaudio.h, not a local invention. */
enum {
    AA_OK = 0,
    AA_ERROR_BASE = -900,
    AA_ERROR_DISCONNECTED = AA_ERROR_BASE + 1,
    AA_ERROR_ILLEGAL_ARGUMENT = AA_ERROR_BASE + 2,
    AA_ERROR_INTERNAL = AA_ERROR_BASE + 3,
    AA_ERROR_INVALID_STATE = AA_ERROR_BASE + 5,
    AA_ERROR_UNAVAILABLE = AA_ERROR_BASE + 8,
    AA_ERROR_NO_FREE_HANDLES = AA_ERROR_BASE + 9,
    AA_ERROR_NO_MEMORY = AA_ERROR_BASE + 11,
    AA_ERROR_WOULD_BLOCK = AA_ERROR_BASE + 12,
    AA_ERROR_INVALID_HANDLE = AA_ERROR_BASE + 13,
    AA_ERROR_INVALID_RATE = AA_ERROR_BASE + 19,
    AA_ERROR_TIMEOUT = AA_ERROR_BASE + 17,
    AA_FORMAT_UNSPECIFIED = 0,
    AA_FORMAT_PCM_I16 = 1,
    AA_FORMAT_PCM_FLOAT = 2,
    AA_SHARING_MODE_EXCLUSIVE = 0,
    AA_SHARING_MODE_SHARED = 1,
    AA_PERFORMANCE_NONE = 10,
    AA_PERFORMANCE_POWER_SAVING = 11,
    AA_PERFORMANCE_LOW_LATENCY = 12,
    AA_STATE_UNINITIALIZED = 0,
    AA_STATE_UNKNOWN = 1,
    AA_STATE_OPEN = 2,
    AA_STATE_STARTING = 10,
    AA_STATE_RUNNING = 11,          /* AAUDIO_STATE_STARTED */
    AA_STATE_PAUSING = 12,
    AA_STATE_PAUSED = 13,
    AA_STATE_FLUSHING = 14,
    AA_STATE_FLUSHED = 15,
    AA_STATE_STOPPING = 16,
    AA_STATE_STOPPED = 17,
    AA_STATE_CLOSING = 18,
    AA_STATE_CLOSED = 19,
    AA_STATE_DISCONNECTED = 20,
    AA_CALLBACK_RESULT_CONTINUE = 0,
    AA_CALLBACK_RESULT_STOP = 1,
    AA_USAGE_MEDIA = 1,
    AA_CONTENT_TYPE_MUSIC = 2,
    AA_INPUT_PRESET_GENERIC = 1,
    AA_DIRECTION_OUTPUT = 1
};

typedef struct {
    int32_t sample_rate;
    int32_t channel_count;
    int32_t format;
    int32_t performance_mode;
    int32_t sharing_mode;
    int32_t usage;
    int32_t content_type;
    int32_t input_preset;
    int32_t direction;
    int32_t device_id;
    int32_t frames_per_data_callback;
    int32_t buffer_capacity;
    int32_t (*data_callback)(void *stream, void *user, void *audio,
                             int32_t frames);
    void *data_user;
    void (*error_callback)(void *stream, void *user, int32_t error);
    void *error_user;
} aa_builder;

typedef struct {
    int32_t magic;
    aa_builder req;
    int32_t state;
    int32_t burst;
    int32_t buffer_size;
    int64_t frames_written;
    int64_t frames_read;
    int64_t started_at_ns;
    void *callback_buffer;
    pthread_t pump;
    int pump_live;
    volatile int pump_stop;
} aa_stream;

#define AA_STREAM_MAGIC 0x50464141u   /* "AAPF" */
#define AA_MAX_STREAMS 4
static aa_stream aa_streams[AA_MAX_STREAMS];

static int aa_bytes_per_frame(int format)
{
    return format == AA_FORMAT_PCM_FLOAT ? 4 : 2;
}

/* SF_AATRACE prints every facade call with its arguments; that is what turns
 * "somewhere in here" into a line. */
static int aa_trace_on(void)
{
    static int enabled = -1;
    if (enabled < 0)
        enabled = getenv("SF_AATRACE") != NULL;
    return enabled;
}

#define AAT(...)                                                               \
    do {                                                                       \
        if (aa_trace_on()) {                                                   \
            fprintf(stderr, "[audio/aa] " __VA_ARGS__);                        \
            fputc('\n', stderr);                                               \
        }                                                                      \
    } while (0)

static int64_t aa_now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

/* "No sound" has two very different causes that look identical from outside: the
 * mixer is not handing us any block at all, or it is handing us blocks full of
 * silence (the game has nothing to play yet).  This counts blocks and the peak
 * s16 amplitude so a device run answers which one it is, every ~5 s and only
 * under SF_AATRACE. */
static void aa_flow_probe(const void *s16_data, size_t bytes)
{
    static unsigned long blocks;
    static int peak;
    if (!aa_trace_on() || !s16_data || bytes < 2)
        return;
    const int16_t *p = s16_data;
    size_t n = bytes / sizeof(int16_t);
    for (size_t i = 0; i < n; i++) {
        int v = p[i] < 0 ? -p[i] : p[i];
        if (v > peak)
            peak = v;
    }
    if (++blocks % 256 != 0)
        return;
    AAT("fluxo: %lu blocos, pico s16=%d (0 = silencio do mixer)", blocks, peak);
    peak = 0;
}

/* Hand one block of the stream's format to the shared SDL device, converting
 * float to s16 because sf_audio_device_write() paces the mixer through a device
 * opened as AUDIO_S16SYS.  The length handed on must be the converted size,
 * half the float byte count: passing the float byte count plays every block
 * with its second half uninitialized memory. */
static void aa_submit(aa_stream *s, const void *data, size_t bytes)
{
    if (!data || bytes == 0)
        return;
    if (s->req.format == AA_FORMAT_PCM_FLOAT) {
        size_t samples = bytes / sizeof(float);
        static int16_t *converted;
        static size_t converted_capacity;
        if (samples > converted_capacity) {
            free(converted);
            converted_capacity = samples;
            converted = malloc(converted_capacity * sizeof *converted);
            if (!converted) {
                converted_capacity = 0;
                return;
            }
        }
        const float *src = data;
        for (size_t i = 0; i < samples; i++) {
            float value = src[i] * 32768.0f;
            if (value > 32767.0f) value = 32767.0f;
            if (value < -32768.0f) value = -32768.0f;
            converted[i] = (int16_t)value;
        }
        aa_flow_probe(converted, samples * sizeof(int16_t));
        sf_audio_device_write(converted, (int)(samples * sizeof(int16_t)));
        return;
    }
    aa_flow_probe(data, bytes);
    sf_audio_device_write(data, (int)bytes);
}

static void *aa_pump(void *opaque)
{
    aa_stream *s = opaque;
    int32_t frames = s->req.frames_per_data_callback > 0
                       ? s->req.frames_per_data_callback : s->burst;
    size_t bytes = (size_t)frames
                   * (size_t)s->req.channel_count
                   * (size_t)aa_bytes_per_frame(s->req.format);
    void *buffer = calloc(1, bytes ? bytes : 1024);
    if (!buffer)
        return NULL;
    while (!s->pump_stop && s->state == AA_STATE_RUNNING) {
        if (!s->req.data_callback)
            break;
        int32_t result = s->req.data_callback(s, s->req.data_user, buffer,
                                             frames);
        if (result == AA_CALLBACK_RESULT_STOP)
            break;
        aa_submit(s, buffer, bytes);
        s->frames_written += frames;
        s->frames_read += frames;
    }
    free(buffer);
    return NULL;
}

static aa_builder *aa_builder_new(void)
{
    aa_builder *b = calloc(1, sizeof *b);
    if (!b)
        return NULL;
    b->sample_rate = 48000;
    b->channel_count = 2;
    b->format = AA_FORMAT_PCM_I16;
    b->performance_mode = AA_PERFORMANCE_LOW_LATENCY;
    b->sharing_mode = AA_SHARING_MODE_SHARED;
    b->usage = AA_USAGE_MEDIA;
    b->content_type = AA_CONTENT_TYPE_MUSIC;
    b->direction = AA_DIRECTION_OUTPUT;
    b->input_preset = AA_INPUT_PRESET_GENERIC;
    b->frames_per_data_callback = 0;
    b->buffer_capacity = 0;
    return b;
}

/* NDK signature: the builder comes back through the out-parameter and the
 * return value is an aaudio_result_t. */
int32_t AAudio_createStreamBuilder(aa_builder **out)
{
    if (!out) {
        AAT("createStreamBuilder(NULL) -> ILLEGAL_ARGUMENT");
        return AA_ERROR_ILLEGAL_ARGUMENT;
    }
    aa_builder *b = aa_builder_new();
    *out = b;
    AAT("createStreamBuilder(out=%p) -> builder %p", (void *)out, (void *)b);
    return b ? AA_OK : AA_ERROR_NO_MEMORY;
}

#define AA_BUILDER_INT(field, member)                                          \
    void AAudioStreamBuilder_set##field(aa_builder *b, int32_t value)           \
    {                                                                           \
        AAT("set" #field "(%p, %d)", (void *)b, value);                         \
        if (b)                                                                  \
            b->member = value;                                                  \
    }

AA_BUILDER_INT(SampleRate, sample_rate)
AA_BUILDER_INT(ChannelCount, channel_count)
AA_BUILDER_INT(Format, format)
AA_BUILDER_INT(PerformanceMode, performance_mode)
AA_BUILDER_INT(SharingMode, sharing_mode)
AA_BUILDER_INT(Usage, usage)
AA_BUILDER_INT(ContentType, content_type)
AA_BUILDER_INT(InputPreset, input_preset)
AA_BUILDER_INT(Direction, direction)
AA_BUILDER_INT(DeviceId, device_id)
AA_BUILDER_INT(FramesPerDataCallback, frames_per_data_callback)
AA_BUILDER_INT(BufferCapacityInFrames, buffer_capacity)

/* Channel mask is the newer way to ask for stereo; the count is what the rest
 * of this implementation needs, so derive it from the mask. */
void AAudioStreamBuilder_setChannelMask(aa_builder *b, int32_t mask)
{
    AAT("setChannelMask(%p, %d)", (void *)b, mask);
    if (!b)
        return;
    b->channel_count = mask >= 3 && mask <= 5 ? 2 : (mask == 1 ? 1 : 2);
}

int32_t AAudioStreamBuilder_setDataCallback(aa_builder *b, void *callback,
                                            void *user)
{
    AAT("setDataCallback(%p, cb=%p, user=%p)", (void *)b, callback, user);
    if (!b)
        return AA_ERROR_ILLEGAL_ARGUMENT;
    b->data_callback = (int32_t (*)(void *, void *, void *, int32_t))callback;
    b->data_user = user;
    return AA_OK;
}

int32_t AAudioStreamBuilder_setErrorCallback(aa_builder *b, void *callback,
                                             void *user)
{
    AAT("setErrorCallback(%p, cb=%p, user=%p)", (void *)b, callback, user);
    if (!b)
        return AA_ERROR_ILLEGAL_ARGUMENT;
    b->error_callback = (void (*)(void *, void *, int32_t))callback;
    b->error_user = user;
    return AA_OK;
}

int32_t AAudioStreamBuilder_openStream(aa_builder *b, void **out_stream)
{
    AAT("openStream(%p, out=%p) req: rate=%d ch=%d fmt=%d perf=%d share=%d "
        "frames_cb=%d cap=%d", (void *)b, (void *)out_stream,
        b ? b->sample_rate : -1, b ? b->channel_count : -1, b ? b->format : -1,
        b ? b->performance_mode : -1, b ? b->sharing_mode : -1,
        b ? b->frames_per_data_callback : -1, b ? b->buffer_capacity : -1);
    if (!b || !out_stream)
        return AA_ERROR_ILLEGAL_ARGUMENT;
    aa_stream *s = NULL;
    for (int i = 0; i < AA_MAX_STREAMS; i++) {
        if (aa_streams[i].magic != AA_STREAM_MAGIC) {
            s = &aa_streams[i];
            break;
        }
    }
    if (!s) {
        fprintf(stderr, "[audio] AAudio: sem slot de stream livre\n");
        return AA_ERROR_NO_MEMORY;
    }
    memset(s, 0, sizeof *s);
    s->magic = AA_STREAM_MAGIC;
    s->req = *b;
    if (s->req.sample_rate < 8000 || s->req.sample_rate > 192000)
        s->req.sample_rate = 48000;
    if (s->req.channel_count < 1 || s->req.channel_count > 2)
        s->req.channel_count = 2;
    if (s->req.format != AA_FORMAT_PCM_FLOAT)
        s->req.format = AA_FORMAT_PCM_I16;
    s->burst = s->req.sample_rate / 60;
    if (s->burst < 128)
        s->burst = 128;
    s->buffer_size = s->burst * 4;
    s->state = AA_STATE_OPEN;
    s->started_at_ns = 0;
    int frames = s->req.frames_per_data_callback > 0
                   ? s->req.frames_per_data_callback : s->burst;
    int buffers = s->req.buffer_capacity > 0 && frames > 0
                    ? s->req.buffer_capacity / frames : 4;
    *out_stream = s;
    fprintf(stderr,
            "[audio] AAudio.openStream: %d Hz, %d canais, formato %s, "
            "callback=%s burst=%d frames_cb=%d buffers=%d\n",
            s->req.sample_rate, s->req.channel_count,
            s->req.format == AA_FORMAT_PCM_FLOAT ? "float" : "s16",
            s->req.data_callback ? "sim" : "nao", s->burst, frames, buffers);
    if (!sf_audio_device_init(s->req.channel_count, s->req.sample_rate, frames,
                              buffers)) {
        AAT("openStream: SDL device init FAILED -> AAUDIO_ERROR_DISCONNECTED");
        s->magic = 0;
        *out_stream = NULL;
        return AA_ERROR_DISCONNECTED;
    }
    AAT("openStream -> stream %p (AAUDIO_OK)", (void *)s);
    return AA_OK;
}

int32_t AAudioStreamBuilder_delete(aa_builder *b)
{
    AAT("builder delete(%p)", (void *)b);
    free(b);
    return AA_OK;
}

#define AA_STREAM_INT(field, expr)                                             \
    int32_t AAudioStream_get##field(void *stream)                              \
    {                                                                          \
        aa_stream *s = stream;                                                 \
        int32_t value =                                                        \
            s && s->magic == AA_STREAM_MAGIC ? (int32_t)(expr) : 0;            \
        AAT("get" #field "(%p) -> %d", stream, value);                          \
        return value;                                                          \
    }

AA_STREAM_INT(SampleRate, s->req.sample_rate)
AA_STREAM_INT(ChannelCount, s->req.channel_count)
AA_STREAM_INT(Format, s->req.format)
AA_STREAM_INT(FramesPerBurst, s->burst)
AA_STREAM_INT(FramesPerDataCallback, s->req.frames_per_data_callback)
AA_STREAM_INT(BufferCapacityInFrames, s->burst * 16)
AA_STREAM_INT(BufferSizeInFrames, s->buffer_size)
AA_STREAM_INT(PerformanceMode, s->req.performance_mode)
AA_STREAM_INT(SharingMode, s->req.sharing_mode)
AA_STREAM_INT(Usage, s->req.usage)
AA_STREAM_INT(ContentType, s->req.content_type)
AA_STREAM_INT(InputPreset, s->req.input_preset)
AA_STREAM_INT(Direction, s->req.direction)
AA_STREAM_INT(DeviceId, s->req.device_id)
AA_STREAM_INT(XRunCount, 0)
AA_STREAM_INT(AudioApiVersion, 8)
AA_STREAM_INT(State, s->state)
AA_STREAM_INT(SessionId, 1)

int32_t AAudioStream_setBufferSizeInFrames(void *stream, int32_t frames)
{
    aa_stream *s = stream;
    AAT("setBufferSizeInFrames(%p, %d)", stream, frames);
    if (!s || s->magic != AA_STREAM_MAGIC)
        return AA_ERROR_ILLEGAL_ARGUMENT;
    s->buffer_size = frames;
    return frames;
}

int64_t AAudioStream_getFramesWritten(void *stream)
{
    aa_stream *s = stream;
    return s && s->magic == AA_STREAM_MAGIC ? s->frames_written : 0;
}

int64_t AAudioStream_getFramesRead(void *stream)
{
    aa_stream *s = stream;
    return s && s->magic == AA_STREAM_MAGIC ? s->frames_read : 0;
}

static int32_t aa_state_request(aa_stream *s, int32_t target)
{
    if (!s || s->magic != AA_STREAM_MAGIC)
        return AA_ERROR_ILLEGAL_ARGUMENT;
    s->state = target;
    if (target == AA_STATE_RUNNING && s->req.data_callback && !s->pump_live) {
        s->pump_stop = 0;
        s->started_at_ns = aa_now_ns();
        if (pthread_create(&s->pump, NULL, aa_pump, s) == 0)
            s->pump_live = 1;
        else
            fprintf(stderr, "[audio] AAudio: nao deu thread de callback\n");
    }
    if (target != AA_STATE_RUNNING && s->pump_live) {
        s->pump_stop = 1;
        pthread_join(s->pump, NULL);
        s->pump_live = 0;
    }
    return AA_OK;
}

int32_t AAudioStream_requestStart(void *stream)
{
    AAT("requestStart(%p)", stream);
    return aa_state_request(stream, AA_STATE_RUNNING);
}
int32_t AAudioStream_requestPause(void *stream)
{
    AAT("requestPause(%p)", stream);
    return aa_state_request(stream, AA_STATE_PAUSED);
}
int32_t AAudioStream_requestStop(void *stream)
{
    AAT("requestStop(%p)", stream);
    return aa_state_request(stream, AA_STATE_STOPPED);
}
int32_t AAudioStream_requestFlush(void *stream)
{
    AAT("requestFlush(%p)", stream);
    return aa_state_request(stream, AA_STATE_FLUSHED);
}

int32_t AAudioStream_write(void *stream, const void *data, int32_t frames,
                           int64_t timeout_ns)
{
    aa_stream *s = stream;
    (void)timeout_ns;
    if (!s || s->magic != AA_STREAM_MAGIC)
        return AA_ERROR_ILLEGAL_ARGUMENT;
    if (s->state != AA_STATE_RUNNING)
        return AA_ERROR_INVALID_STATE;
    size_t bytes = (size_t)frames * (size_t)s->req.channel_count
                   * (size_t)aa_bytes_per_frame(s->req.format);
    aa_submit(s, data, bytes);
    s->frames_written += frames;
    return frames;
}

int32_t AAudioStream_read(void *stream, void *data, int32_t frames,
                          int64_t timeout_ns)
{
    (void)stream; (void)data; (void)frames; (void)timeout_ns;
    /* Output-only device: nothing to capture. */
    return AA_ERROR_INVALID_STATE;
}

/* NDK form: the two outputs are separate int64 pointers, not a struct. */
int32_t AAudioStream_getTimestamp(void *stream, int32_t clock_id,
                                  int64_t *frame_position, int64_t *nano_time)
{
    aa_stream *s = stream;
    (void)clock_id;
    if (!s || s->magic != AA_STREAM_MAGIC || !frame_position || !nano_time)
        return AA_ERROR_ILLEGAL_ARGUMENT;
    if (s->state != AA_STATE_RUNNING)
        return AA_ERROR_INVALID_STATE;
    *frame_position = s->frames_read;
    *nano_time = s->started_at_ns ? aa_now_ns() : 0;
    return AA_OK;
}

/* Every state change here is synchronous, so the answer is the current state at
 * once; when the stream is still in the state the caller waits to leave, take a
 * short nap instead of spinning the caller's loop. */
int32_t AAudioStream_waitForStateChange(void *stream, int32_t input_state,
                                        int32_t *next_state,
                                        int64_t timeout_ns)
{
    aa_stream *s = stream;
    AAT("waitForStateChange(%p, in=%d, timeout=%lld)", stream, input_state,
        (long long)timeout_ns);
    if (!s || s->magic != AA_STREAM_MAGIC)
        return AA_ERROR_ILLEGAL_ARGUMENT;
    if (next_state && s->state != input_state) {
        *next_state = s->state;
        AAT("waitForStateChange -> immediate, state=%d", s->state);
        return AA_OK;
    }
    int64_t ns = timeout_ns;
    if (ns > 5000000LL)
        ns = 5000000LL;
    if (ns > 0) {
        struct timespec ts = { ns / 1000000000LL, ns % 1000000000LL };
        nanosleep(&ts, NULL);
    }
    if (next_state)
        *next_state = s->state;
    AAT("waitForStateChange -> timed out, state=%d", s->state);
    return AA_ERROR_TIMEOUT;
}

/* MMap is AAudio's exclusive-mode fast path and this bridge has none, so the
 * answer is "not used".  Single-argument form: callers pass the stream and
 * nothing else (measured on the 60parsecs box -- a two-parameter write
 * through "bool *" crashed on the stream magic). */
int32_t AAudioStream_isMMapUsed(void *stream)
{
    AAT("isMMapUsed(%p) -> 0", stream);
    return 0;
}

int32_t AAudioStream_close(void *stream)
{
    aa_stream *s = stream;
    AAT("close(%p)", stream);
    if (!s || s->magic != AA_STREAM_MAGIC)
        return AA_ERROR_ILLEGAL_ARGUMENT;
    aa_state_request(s, AA_STATE_STOPPED);
    free(s->callback_buffer);
    memset(s, 0, sizeof *s);
    sf_audio_device_close();
    return AA_OK;
}

int32_t AAudio_getLibraryVersion(void) { return 0x00010000; }

static const char *const aa_result_text[] = {
    "AAUDIO_OK", "AAUDIO_ERROR_DISCONNECTED", "AAUDIO_ERROR_ILLEGAL_ARGUMENT",
    "AAUDIO_ERROR_INTERNAL", "AAUDIO_ERROR_INVALID_STATE"
};

const char *AAudio_convertResultToText(int32_t result)
{
    if (result == AA_OK)
        return aa_result_text[0];
    if (result == AA_ERROR_DISCONNECTED)
        return aa_result_text[1];
    if (result == AA_ERROR_ILLEGAL_ARGUMENT)
        return aa_result_text[2];
    if (result == AA_ERROR_INVALID_STATE)
        return aa_result_text[4];
    return "AAUDIO_ERROR_UNKNOWN";
}

/* --------------------------------------------------------------- OpenSL ES
 *
 * Unity 2021.3's default Android audio output is OpenSL ES, reached by
 * dlopen("libOpenSLES.so") + dlsym("slCreateEngine"); the interface ID data
 * symbols (SL_IID_*) are fetched the same way.  Until the real bridge lands,
 * every entry point reports unsupported with a one-line log so the game boots
 * silent by design and the dlsym trace shows exactly what it binds.  The
 * return values are the Khronos SLresult numbers, not inventions.
 */
#define SL_RESULT_SUCCESS 0
#define SL_RESULT_FEATURE_UNSUPPORTED 12

static int sl_trace_on(void)
{
    static int enabled = -1;
    if (enabled < 0)
        enabled = getenv("SF_SLTRACE") != NULL;
    return enabled;
}

#define SLT(...)                                                               \
    do {                                                                       \
        if (sl_trace_on()) {                                                   \
            fprintf(stderr, "[audio/sl] " __VA_ARGS__);                        \
            fputc('\n', stderr);                                               \
        }                                                                      \
    } while (0)

/* slCreateEngine(eng_out, options, features).  The stub returns a null engine
 * with FEATURE_UNSUPPORTED: Unity logs "audio device init failed" and runs on
 * with the "no sound" fallback path instead of dereferencing anything. */
int32_t slCreateEngine(void **engine_out, void *options, void *features)
{
    SLT("slCreateEngine(out=%p, options=%p, features=%p) -> "
        "FEATURE_UNSUPPORTED (stub)",
        engine_out, options, features);
    if (engine_out)
        *engine_out = NULL;
    return SL_RESULT_FEATURE_UNSUPPORTED;
}

/* Interface ID data symbols: callers compare pointers against these.  They
 * resolve to distinct readable addresses so identity checks stay well
 * defined, but no object ever hands one of these interfaces out. */
static int sl_iid_placeholder;

const void *sl_iid_addr(const char *name)
{
    SLT("SL_IID request: %s", name);
    return &sl_iid_placeholder;
}

#define SL_IID_CASE(sym)                                                       \
    if (strcmp(name, #sym) == 0)                                               \
        return sl_iid_addr(#sym);

const void *sf_sl_iid_lookup(const char *name)
{
    SL_IID_CASE(SL_IID_ENGINE)
    SL_IID_CASE(SL_IID_ENGINECAPABILITIES)
    SL_IID_CASE(SL_IID_PLAY)
    SL_IID_CASE(SL_IID_BUFFERQUEUE)
    SL_IID_CASE(SL_IID_ANDROIDSIMPLEBUFFERQUEUE)
    SL_IID_CASE(SL_IID_OUTPUTMIX)
    SL_IID_CASE(SL_IID_VOLUME)
    SL_IID_CASE(SL_IID_PLAYBACKRATE)
    SL_IID_CASE(SL_IID_AUDIODECODERCAPABILITIES)
    SL_IID_CASE(SL_IID_AUDIOENCODER)
    SL_IID_CASE(SL_IID_AUDIOENCODERCAPABILITIES)
    SL_IID_CASE(SL_IID_ANDROIDCONFIGURATION)
    SL_IID_CASE(SL_IID_ANDROIDACOUSTICECHOCANCELLATION)
    SL_IID_CASE(SL_IID_ANDROIDAUTOMATICGAINCONTROL)
    SL_IID_CASE(SL_IID_ANDROIDNOISESUPPRESSION)
    SL_IID_CASE(SL_IID_PREFETCHSTATUS)
    SL_IID_CASE(SL_IID_SEEK)
    SL_IID_CASE(SL_IID_RECORD)
    SL_IID_CASE(SL_IID_METRICINFORMATION)
    return NULL;
}

/* slQueryNumSupportedEngineInterfaces / slQuerySupportedEngineInterfaces are
 * probed by some stacks before creating the engine; answer zero so the
 * unsupported verdict is consistent everywhere. */
int32_t slQueryNumSupportedEngineInterfaces(void)
{
    SLT("slQueryNumSupportedEngineInterfaces() -> 0");
    return 0;
}

int32_t slQuerySupportedEngineInterfaces(uint32_t index, void *iid)
{
    (void)index; (void)iid;
    SLT("slQuerySupportedEngineInterfaces(%u) -> FEATURE_UNSUPPORTED", index);
    return SL_RESULT_FEATURE_UNSUPPORTED;
}

static const nx_import aa_tab[] = {
    { "AAudio_createStreamBuilder",        (void *)AAudio_createStreamBuilder },
    { "AAudioStreamBuilder_setSampleRate",
      (void *)AAudioStreamBuilder_setSampleRate },
    { "AAudioStreamBuilder_setChannelCount",
      (void *)AAudioStreamBuilder_setChannelCount },
    { "AAudioStreamBuilder_setChannelMask",
      (void *)AAudioStreamBuilder_setChannelMask },
    { "AAudioStreamBuilder_setFormat",     (void *)AAudioStreamBuilder_setFormat },
    { "AAudioStreamBuilder_setPerformanceMode",
      (void *)AAudioStreamBuilder_setPerformanceMode },
    { "AAudioStreamBuilder_setSharingMode",
      (void *)AAudioStreamBuilder_setSharingMode },
    { "AAudioStreamBuilder_setUsage",      (void *)AAudioStreamBuilder_setUsage },
    { "AAudioStreamBuilder_setContentType",
      (void *)AAudioStreamBuilder_setContentType },
    { "AAudioStreamBuilder_setInputPreset",
      (void *)AAudioStreamBuilder_setInputPreset },
    { "AAudioStreamBuilder_setDirection",
      (void *)AAudioStreamBuilder_setDirection },
    { "AAudioStreamBuilder_setDeviceId",   (void *)AAudioStreamBuilder_setDeviceId },
    { "AAudioStreamBuilder_setFramesPerDataCallback",
      (void *)AAudioStreamBuilder_setFramesPerDataCallback },
    { "AAudioStreamBuilder_setBufferCapacityInFrames",
      (void *)AAudioStreamBuilder_setBufferCapacityInFrames },
    { "AAudioStreamBuilder_setDataCallback",
      (void *)AAudioStreamBuilder_setDataCallback },
    { "AAudioStreamBuilder_setErrorCallback",
      (void *)AAudioStreamBuilder_setErrorCallback },
    { "AAudioStreamBuilder_openStream",    (void *)AAudioStreamBuilder_openStream },
    { "AAudioStreamBuilder_delete",        (void *)AAudioStreamBuilder_delete },
    { "AAudioStream_getSampleRate",        (void *)AAudioStream_getSampleRate },
    { "AAudioStream_getChannelCount",      (void *)AAudioStream_getChannelCount },
    { "AAudioStream_getFormat",            (void *)AAudioStream_getFormat },
    { "AAudioStream_getFramesPerBurst",    (void *)AAudioStream_getFramesPerBurst },
    { "AAudioStream_getFramesPerDataCallback",
      (void *)AAudioStream_getFramesPerDataCallback },
    { "AAudioStream_getBufferCapacityInFrames",
      (void *)AAudioStream_getBufferCapacityInFrames },
    { "AAudioStream_getBufferSizeInFrames",
      (void *)AAudioStream_getBufferSizeInFrames },
    { "AAudioStream_setBufferSizeInFrames",
      (void *)AAudioStream_setBufferSizeInFrames },
    { "AAudioStream_getPerformanceMode",   (void *)AAudioStream_getPerformanceMode },
    { "AAudioStream_getSharingMode",       (void *)AAudioStream_getSharingMode },
    { "AAudioStream_getUsage",             (void *)AAudioStream_getUsage },
    { "AAudioStream_getContentType",       (void *)AAudioStream_getContentType },
    { "AAudioStream_getInputPreset",       (void *)AAudioStream_getInputPreset },
    { "AAudioStream_getDirection",         (void *)AAudioStream_getDirection },
    { "AAudioStream_getDeviceId",          (void *)AAudioStream_getDeviceId },
    { "AAudioStream_getXRunCount",         (void *)AAudioStream_getXRunCount },
    { "AAudioStream_getAudioApiVersion",   (void *)AAudioStream_getAudioApiVersion },
    { "AAudioStream_getState",             (void *)AAudioStream_getState },
    { "AAudioStream_getSessionId",         (void *)AAudioStream_getSessionId },
    { "AAudioStream_getFramesWritten",     (void *)AAudioStream_getFramesWritten },
    { "AAudioStream_getFramesRead",        (void *)AAudioStream_getFramesRead },
    { "AAudioStream_getTimestamp",         (void *)AAudioStream_getTimestamp },
    { "AAudioStream_requestStart",         (void *)AAudioStream_requestStart },
    { "AAudioStream_requestPause",         (void *)AAudioStream_requestPause },
    { "AAudioStream_requestStop",          (void *)AAudioStream_requestStop },
    { "AAudioStream_requestFlush",         (void *)AAudioStream_requestFlush },
    { "AAudioStream_waitForStateChange",   (void *)AAudioStream_waitForStateChange },
    { "AAudioStream_write",                (void *)AAudioStream_write },
    { "AAudioStream_read",                 (void *)AAudioStream_read },
    { "AAudioStream_isMMapUsed",           (void *)AAudioStream_isMMapUsed },
    { "AAudioStream_close",                (void *)AAudioStream_close },
    { "AAudio_getLibraryVersion",          (void *)AAudio_getLibraryVersion },
    { "AAudio_convertResultToText",        (void *)AAudio_convertResultToText },

    /* OpenSL ES: stubs until the real bridge lands (see section above). */
    { "slCreateEngine",                    (void *)slCreateEngine },
    { "slQueryNumSupportedEngineInterfaces",
      (void *)slQueryNumSupportedEngineInterfaces },
    { "slQuerySupportedEngineInterfaces",
      (void *)slQuerySupportedEngineInterfaces },
};

const nx_import *sf_audio_table(size_t *n)
{
    *n = sizeof aa_tab / sizeof *aa_tab;
    return aa_tab;
}

/* SL_IID_* are DATA symbols: my_dlsym must hand back addresses, not a table
 * of function pointers, so they get their own lookup used by bionic.c. */
const void *sf_sl_iid_symbol(const char *sym)
{
    if (!sym || strncmp(sym, "SL_IID_", 7) != 0)
        return NULL;
    return sf_sl_iid_lookup(sym);
}
