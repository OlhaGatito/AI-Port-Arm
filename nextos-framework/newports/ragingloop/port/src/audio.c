/* SPDX-License-Identifier: GPL-3.0-only
 * Audio: libvorbisfile decoding into an SDL software mixer.
 *
 * BGM and voice are streamed (a chapter's music runs to several minutes and
 * this device has 1 GB of RAM), sound effects are short enough to decode once
 * into a small pool of pre-loaded slots.
 *
 * The interpreter thread only ever mutates mixer state under
 * SDL_LockAudioDevice(), because the callback decodes on its own thread.
 */
#include "rl.h"

#include <vorbis/vorbisfile.h>

#define MIX_RATE 44100
#define SE_SLOTS 3
#define SE_MAX_FRAMES (10u * MIX_RATE)      /* 10 s ceiling per effect */

typedef struct {
    OggVorbis_File vf;
    int open;
    int active;
    int loop;
    float vol;
    float target;
    float ramp_left;                         /* samples over which vol slides */
    int ch;                                  /* the file's own channel count */
} Stream;

static struct {
    int16_t *pcm;                            /* interleaved stereo */
    unsigned frames, pos;
    int active;
} g_se[SE_SLOTS];

static Stream g_bgm, g_voc;
static SDL_AudioDeviceID g_dev;
static char g_bgm_path[300];
static unsigned g_cb_frames;                 /* frames per callback */
static Uint32 g_mix_calls, g_stream_frames;

static float g_se_vol = 0.9f;
static float g_bgm_gain = 1.0f, g_se_gain = 1.0f;
static float g_voc_gain = 1.0f;      /* the 语音音量 setting */
static float g_voc_spk = 1.0f;       /* this line's 角色语音 setting */

void aud_set_gain(float bgm, float se, float voice)
{
    g_bgm_gain = bgm < 0.f ? 0.f : bgm > 1.f ? 1.f : bgm;
    g_se_gain = se < 0.f ? 0.f : se > 1.f ? 1.f : se;
    g_voc_gain = voice < 0.f ? 0.f : voice > 1.f ? 1.f : voice;
}

static int16_t clamp16(int v)
{
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return (int16_t)v;
}

static void stream_close(Stream *st)
{
    st->active = 0;
    if (st->open) { ov_clear(&st->vf); st->open = 0; }
}

/* Decode `frames` sample-frames and add them into the S16 output.
 * ov_read() returns the file's own channel layout - it does not upmix - so a
 * mono stream must be expanded here.  7568 of the 7581 voice files and 14 of
 * the 49 sound effects are mono: reading them as stereo makes every line play
 * at double speed. */
static void mix_stream(Stream *st, int16_t *out, int frames, float mul)
{
    const int bpf = st->ch > 1 ? 4 : 2;      /* bytes per sample-frame from ov_read */
    while (st->active && frames > 0) {
        int want = frames > 512 ? 512 : frames;
        int16_t tmp[512 * 2];
        long got = ov_read(&st->vf, (char *)tmp, want * bpf, 0, 2, 1, NULL);
        if (got <= 0) {
            if (st->loop && ov_raw_seek(&st->vf, 0) == 0) continue;
            stream_close(st);
            return;
        }
        int nf = (int)(got / bpf);
        float step = st->ramp_left > 0.f ? (st->target - st->vol) / st->ramp_left : 0.f;
        for (int i = 0; i < nf; i++) {
            st->vol += step;
            if (step) st->ramp_left -= 1.f;
            if (st->ramp_left <= 0.f) { st->vol = st->target; st->ramp_left = 0.f; }
            float v = st->vol * mul;
            int16_t l = tmp[st->ch > 1 ? 2 * i : i];
            int16_t r = st->ch > 1 ? tmp[2 * i + 1] : l;
            out[2 * i]     = clamp16(out[2 * i]     + (int)(l * v));
            out[2 * i + 1] = clamp16(out[2 * i + 1] + (int)(r * v));
        }
        g_stream_frames += (unsigned)nf;
        out += nf * 2;
        frames -= nf;
    }
}

static void mix_se(int16_t *out, int frames)
{
    const float v = g_se_vol * g_se_gain;
    for (int s = 0; s < SE_SLOTS; s++) {
        if (!g_se[s].active) continue;
        int16_t *p = g_se[s].pcm;
        unsigned pos = g_se[s].pos;
        int i = 0;
        for (; i < frames && pos < g_se[s].frames; i++, pos++) {
            out[2 * i]     = clamp16(out[2 * i]     + (int)(p[2 * pos] * v));
            out[2 * i + 1] = clamp16(out[2 * i + 1] + (int)(p[2 * pos + 1] * v));
        }
        g_se[s].pos = pos;
        if (pos >= g_se[s].frames) g_se[s].active = 0;
    }
}

static void mix_cb(void *ud, Uint8 *dst, int len)
{
    (void)ud;
    int frames = len / 4;
    memset(dst, 0, (size_t)len);
    mix_stream(&g_bgm, (int16_t *)dst, frames, g_bgm_gain);
    mix_stream(&g_voc, (int16_t *)dst, frames, g_voc_gain * g_voc_spk);
    mix_se((int16_t *)dst, frames);
    g_mix_calls++;
    g_cb_frames = (unsigned)frames;
}

void aud_init(void)
{
    if (getenv("RL_AUDIO") && !atoi(getenv("RL_AUDIO"))) {
        rl_log("audio: disabled by RL_AUDIO=0");
        return;
    }
    SDL_AudioSpec want, got;
    SDL_zero(want);
    want.freq = MIX_RATE;
    want.format = AUDIO_S16LSB;
    want.channels = 2;
    want.samples = 1024;
    want.callback = mix_cb;
    g_dev = SDL_OpenAudioDevice(NULL, 0, &want, &got, 0);
    if (!g_dev) {
        rl_log("audio: open failed: %s", SDL_GetError());
        return;
    }
    rl_log("audio: %dHz %dch %dspp callback", got.freq, got.channels, got.samples);
    SDL_PauseAudioDevice(g_dev, 0);
}

static void lock(void)   { if (g_dev) SDL_LockAudioDevice(g_dev); }
static void unlock(void) { if (g_dev) SDL_UnlockAudioDevice(g_dev); }

/* Open `path` on a stream, optionally replacing what is already there. */
static int stream_open(Stream *st, const char *path, int loop, float fade_ms)
{
    OggVorbis_File nf;
    if (ov_fopen(path, &nf) != 0) {
        rl_log("audio: ov_fopen failed: %s", path);
        return 0;
    }
    stream_close(st);
    st->vf = nf;
    st->open = 1;
    st->active = 1;
    st->loop = loop;
    const vorbis_info *vi = ov_info(&nf, -1);
    st->ch = vi && vi->channels > 0 ? vi->channels : 2;
    if (rl_verbose) rl_log("audio open ch=%d rate=%d", st->ch, vi ? (int)vi->rate : 0);
    st->vol = fade_ms > 0 ? 0.f : 1.f;
    st->target = 1.f;
    st->ramp_left = fade_ms > 0 ? fade_ms * (MIX_RATE / 1000.f) : 0.f;
    return 1;
}

void aud_bgm(const char *path, int fade_ms)
{
    if (!g_dev || !path) return;
    if (g_bgm_path[0] && !strcmp(g_bgm_path, path) && g_bgm.active) return;
    lock();
    if (stream_open(&g_bgm, path, 1, fade_ms > 0 ? (float)fade_ms : 0.f)) {
        snprintf(g_bgm_path, sizeof(g_bgm_path), "%s", path);
        rl_log("BGM play %s fade=%dms", path, fade_ms);
    } else {
        g_bgm_path[0] = 0;
    }
    unlock();
}

void aud_fade_bgm(int ms)
{
    if (!g_dev || !g_bgm.active) return;
    lock();
    g_bgm.target = 0.f;
    g_bgm.ramp_left = ms * (MIX_RATE / 1000.f);
    g_bgm.loop = 0;
    unlock();
}

void aud_stop_bgm(void)
{
    if (!g_dev) return;
    lock();
    if (g_bgm.active) rl_log("BGM stop %s", g_bgm_path);
    stream_close(&g_bgm);
    g_bgm_path[0] = 0;
    unlock();
}

void aud_voice(const char *path, float gain)
{
    if (!g_dev || !path) return;
    lock();
    g_voc_spk = gain < 0.f ? 0.f : gain;
    stream_open(&g_voc, path, 0, 0.f);
    unlock();
}

/* Decode a short effect once and keep it in a slot; the game fires the same
 * effects repeatedly, so re-reading from the card every time is the costlier
 * option here. */
static int16_t *decode_all(const char *path, unsigned *frames_out)
{
    OggVorbis_File vf;
    if (ov_fopen(path, &vf) != 0) return NULL;
    const vorbis_info *vi = ov_info(&vf, -1);
    int ch = vi && vi->channels > 0 ? vi->channels : 2;
    int bpf = ch > 1 ? 4 : 2;
    unsigned cap = 8192, n = 0;
    int16_t *pcm = malloc((size_t)cap * 2 * sizeof(int16_t));
    if (!pcm) { ov_clear(&vf); return NULL; }
    for (;;) {
        if (n + 512 > cap) {
            if (n >= SE_MAX_FRAMES) break;
            unsigned nc = cap * 2;
            int16_t *np = realloc(pcm, (size_t)nc * 2 * sizeof(int16_t));
            if (!np) break;
            pcm = np;
            cap = nc;
        }
        int16_t tmp[512 * 2];
        long got = ov_read(&vf, (char *)tmp, 512 * bpf, 0, 2, 1, NULL);
        if (got <= 0) break;
        unsigned nf = (unsigned)(got / bpf);
        for (unsigned i = 0; i < nf; i++) {
            pcm[(size_t)(n + i) * 2]     = tmp[ch > 1 ? (size_t)2 * i : i];
            pcm[(size_t)(n + i) * 2 + 1] = ch > 1 ? tmp[2 * i + 1] : tmp[i];
        }
        n += nf;
    }
    ov_clear(&vf);
    if (!n) { free(pcm); return NULL; }
    *frames_out = n;
    return pcm;
}

void aud_se(const char *path)
{
    if (!g_dev || !path) return;
    lock();
    int slot = -1;
    for (int i = 0; i < SE_SLOTS; i++) if (!g_se[i].active) { slot = i; break; }
    if (slot < 0) {
        /* all busy: reuse the one closest to its end */
        slot = 0;
        for (int i = 1; i < SE_SLOTS; i++)
            if (g_se[i].pos > g_se[slot].pos) slot = i;
    }
    if (!g_se[slot].pcm) {
        unsigned n = 0;
        g_se[slot].pcm = decode_all(path, &n);
        g_se[slot].frames = n;
        if (!g_se[slot].pcm) { unlock(); return; }
    }
    g_se[slot].pos = 0;
    g_se[slot].active = 1;
    unlock();
}

void aud_shutdown(void)
{
    if (!g_dev) return;
    SDL_CloseAudioDevice(g_dev);
    g_dev = 0;
    for (int i = 0; i < SE_SLOTS; i++) { free(g_se[i].pcm); g_se[i].pcm = NULL; }
}

void aud_stats(void)
{
    rl_log("audio: callbacks=%u spp=%u decoded=%u frames bgm=%d voice=%d",
           g_mix_calls, g_cb_frames, g_stream_frames, g_bgm.active, g_voc.active);
}
