/* SPDX-License-Identifier: GPL-3.0-only
 * Raging Loop (人狼村之谜) — native AArch64 ADV interpreter.
 * The Android original renders through a Java GLSurfaceView; there is no native
 * engine to load, so this is a re-implementation of the observable behaviour of
 * the script command set (see docs/ANALYSIS.md §2).
 */
#ifndef RL_RL_H
#define RL_RL_H

#include <SDL2/SDL.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RL_W 1280            /* asset-native render target (all CGs are 1280x720) */
#define RL_H 720
#define RL_SLOT_MAX 12       /* the corpus addresses slots 1..9 only */
#define RL_FLAG_MAX 2048     /* C and S namespaces are both indexed below this */
#define RL_ARG_MAX 12

enum { RL_CMD = 0, RL_TEXT = 1 };

typedef struct {
    int kind;
    char name[24];
    char *argv[RL_ARG_MAX];
    int argc;
    char *text;              /* RL_TEXT: raw line, UTF-8, may hold %tags */
    int endif;               /* RL_CMD If: index of Else (or -1) */
    int elsenext;            /* RL_CMD If: index after Else; EndIf index */
} RlLine;

typedef struct {
    char *name;
    int line;
} RlLabel;

typedef struct {
    RlLine *lines;
    int n;
    RlLabel *labels;
    int nl;
    char *buf;
    char fname[96];
} RlScript;

typedef struct {
    int used, bustId, x, y, scale, alpha;
    int anchor;              /* ImgBustAdv arg4: 0 = y is the canvas centre, 15 = y is its top */
    int brow, eye, mouth, emo;
    int faceover;            /* ImgFaceAdv arg4: 0 none, 1 red, 2 shadow, 3 black */
    int item[9];             /* ImgItem k=1..8: each toggles independently */
} RlSlot;

typedef struct {
    char bg[600];            /* absolute path, same width as the res_* buffers */
    uint32_t bgcolor;
    int has_color;
    RlSlot slot[RL_SLOT_MAX];
    char name[256];
    char text[8192];
    int mesvis, mescolor;
    float fade, fade_to;
} RlView;

/* res.c — asset addressing. Every id->path rule here is derived in
 * docs/ANALYSIS.md §2.2 from the constant tables baked into the APK's dex. */
void res_init(const char *root);
const char *res_root(void);
/* Absolute path for a tree-relative name; case-insensitive fallback, because
 * the payload mixes SE03x.ogg on disk against "SE03x" in sounddef.txt and the
 * roms partition is exFAT (case-insensitive, no symlinks). Returns 0 if absent. */
int res_resolve(char *out, size_t n, const char *rel);
int res_bg(char *out, size_t n, int id);
int res_bust(char *out, size_t n, int bust_id);
int res_dir(char *out, size_t n, int bust_id);      /* "assets/NN" */
int res_part(char *out, size_t n, int bust_id, const char *kind, int idx);
int res_overlay(char *out, size_t n, int bust_id, const char *name);
int res_acce(char *out, size_t n, int bust_id, int k, int mouth, int emo);
int res_sound(char *out, size_t n, const char *kind, int id);
int res_voice(char *out, size_t n, const char *vid);
int res_script(char *out, size_t n, const char *base);

/* img.c */
void img_init(void);
SDL_Texture *img_get(SDL_Renderer *r, const char *rel, int *w, int *h);
long img_cache_bytes(void);
void img_stats(void);

/* txt.c */
void txt_init(SDL_Renderer *r, const char *font_path);
int txt_ready(void);
int txt_missing(void);
void txt_stats(void);
int txt_measure(const char *utf8, int px);
int txt_height(int px, int lines);
void txt_draw(SDL_Renderer *r, const char *utf8, int x, int y, int px,
              uint32_t rgb, int maxw, int line_px);
void txt_flush_uploads(void);
const char *txt_utf8_next(const char *s, unsigned *cp);

/* scr.c */
RlScript *scr_load(const char *relpath, char *err, size_t errn);
void scr_free(RlScript *s);
int scr_label_line(const RlScript *s, const char *name);

/* audio.c */
void aud_init(void);
void aud_bgm(const char *path, int fade_ms);
void aud_fade_bgm(int ms);
void aud_stop_bgm(void);
void aud_se(const char *path);
/* gain is this line's speaker volume (角色语音), resolved by the caller from the
 * MesSta speakerId; 1.0 means "not turned down". */
void aud_voice(const char *path, float gain);
/* Option-screen gains (docs/ANALYSIS.md §2.7): BGM_VOLUME / SE_VOLUME /
 * VOICE_ALL drive the three streams. All three keys are real prefs in the
 * original's dex string table. */
void aud_set_gain(float bgm, float se, float voice);
void aud_shutdown(void);
void aud_stats(void);

/* main.c */
void rl_log(const char *fmt, ...);
extern int rl_verbose;

#endif /* RL_RL_H */
