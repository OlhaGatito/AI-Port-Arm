/* SPDX-License-Identifier: GPL-3.0-only */
#include "rl.h"

#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

static char g_root[512];

typedef struct { char kind[20]; char name[128]; int idx; } SoundDef;
static SoundDef g_defs[128];
static int g_ndefs;

void res_init(const char *root)
{
    snprintf(g_root, sizeof(g_root), "%s", root);
    char path[600];
    if (!res_resolve(path, sizeof(path), "assets/script/sounddef.txt")) {
        rl_log("WARN sounddef.txt not found under %s", root);
        return;
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        rl_log("WARN sounddef.txt unreadable");
        return;
    }
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char k[16], n[96];
        int i;
        if (sscanf(line, "%15[^,],EXTERNAL,%95[^,],%d", k, n, &i) == 3) {
            if (g_ndefs < (int)(sizeof(g_defs) / sizeof(g_defs[0]))) {
                SoundDef *d = &g_defs[g_ndefs++];
                snprintf(d->kind, sizeof(d->kind), "%s", k);
                snprintf(d->name, sizeof(d->name), "%s", n);
                d->idx = i;
            }
        }
    }
    fclose(f);
    rl_log("sounddef entries=%d", g_ndefs);
}

const char *res_root(void) { return g_root; }

static int path_exists(const char *p)
{
    struct stat st;
    return stat(p, &st) == 0;
}

static int file_exists(const char *p)
{
    struct stat st;
    return stat(p, &st) == 0 && S_ISREG(st.st_mode);
}

/* Match `rel` with every path component compared case-insensitively, one
 * directory level at a time, so a mis-cased leaf (se03x.ogg) and a mis-cased
 * directory both resolve. */
static int resolve_ci(char *out, size_t n, const char *rel)
{
    char cur[512];
    snprintf(cur, sizeof(cur), "%s", g_root);
    const char *p = rel;
    while (*p) {
        const char *slash = strchr(p, '/');
        size_t len = slash ? (size_t)(slash - p) : strlen(p);
        if (!len) { p++; continue; }
        char want[256];
        snprintf(want, sizeof(want), "%.*s", (int)len, p);
        char best[256];
        int found = 0;
        if (snprintf(out, n, "%s/%s", cur, want) >= (int)n) return 0;
        if (path_exists(out)) {
            snprintf(cur, sizeof(cur), "%s", out);
            found = 1;
        } else {
            DIR *d = opendir(cur);
            struct dirent *e;
            if (!d) return 0;
            while ((e = readdir(d))) {
                size_t el = strlen(e->d_name);
                if (el == len && !strncasecmp(e->d_name, want, el)) {
                    snprintf(best, sizeof(best), "%s", e->d_name);
                    found = 1;
                    break;
                }
            }
            closedir(d);
            if (!found) return 0;
            char nxt[512];
            if (snprintf(nxt, sizeof(nxt), "%s/%s", cur, best) >= (int)sizeof(nxt)) return 0;
            snprintf(cur, sizeof(cur), "%s", nxt);
        }
        if (!slash) break;
        p = slash + 1;
    }
    if (snprintf(out, n, "%s", cur) >= (int)n) return 0;
    return path_exists(out);
}

/* Absolute path of a character's asset directory (a directory, not a file). */
static int dir_path(char *out, size_t n, int bust_id)
{
    char dir[300];
    res_dir(dir, sizeof(dir), bust_id);
    snprintf(out, n, "%s/%s", g_root, dir);
    if (path_exists(out)) return 1;
    return resolve_ci(out, n, dir) && path_exists(out);
}

int res_resolve(char *out, size_t n, const char *rel)
{
    if (snprintf(out, n, "%s/%s", g_root, rel) >= (int)n) return 0;
    if (file_exists(out)) return 1;
    if (resolve_ci(out, n, rel) && file_exists(out)) return 1;
    out[0] = 0;
    return 0;
}

int res_bg(char *out, size_t n, int id)
{
    char rel[160];
    if (id >= 501)
        snprintf(rel, sizeof(rel), "assets/background/event_%02d.jpg", id - 500);
    else
        snprintf(rel, sizeof(rel), "assets/background/bg_%03d.jpg", id);
    if (res_resolve(out, n, rel)) return 1;
    rl_log("MISS bg id=%d (%s)", id, rel);
    return 0;
}

/* bustId = charIndex + 40*(variant-1), proven against the 31 ids in the scripts. */
int res_dir(char *out, size_t n, int bust_id)
{
    int ch = (bust_id - 1) % 40 + 1;
    snprintf(out, n, "assets/%02d", ch);
    return 1;
}

int res_bust(char *out, size_t n, int bust_id)
{
    int ch = (bust_id - 1) % 40 + 1;
    int variant = (bust_id - 1) / 40 + 1;
    char rel[160];
    snprintf(rel, sizeof(rel), "assets/%02d/body_%02d.png", ch, variant);
    if (res_resolve(out, n, rel)) return 1;
    rl_log("MISS bust id=%d (%s)", bust_id, rel);
    return 0;
}

int res_part(char *out, size_t n, int bust_id, const char *kind, int idx)
{
    int ch = (bust_id - 1) % 40 + 1;
    char rel[160];
    snprintf(rel, sizeof(rel), "assets/%02d/%d_%s.png", ch, idx, kind);
    if (res_resolve(out, n, rel)) return 1;
    /* The chibi cast (13, 14, 15, 17, 21) ships one whole-face crop per
     * expression instead of separate brow/eye/mouth layers. Their scripts move
     * all three indices together, so the eye index is the face selector. */
    if (!strcmp(kind, "eye")) {
        snprintf(rel, sizeof(rel), "assets/%02d/%d_face.png", ch, idx);
        if (res_resolve(out, n, rel)) return 1;
    }
    return 0;
}

/* red.png / shadow.png / black.png: per-character face overlays that share the
 * brow/eye/mouth canvas, so they anchor exactly like a face part. Most cast
 * members ship none of them, so a miss is normal and stays quiet. */
int res_overlay(char *out, size_t n, int bust_id, const char *name)
{
    int ch = (bust_id - 1) % 40 + 1;
    char rel[160];
    snprintf(rel, sizeof(rel), "assets/%02d/%s.png", ch, name);
    if (res_resolve(out, n, rel)) return 1;
    out[0] = 0;
    return 0;
}

static int digits_have(const char *s, int v){
    char c;
    if (v < 0 || v > 9) return 0;
    c = (char)('0' + v);
    return s && strchr(s, c) != NULL;
}

/* acce_NN[_tear<digits>|_tabaco<digits>].png: the digit list is the set of
 * expression indices the variant is drawn for (tears key on mouth, the
 * cigarette on emo); the unsuffixed file is the default. Rule inferred from the
 * file names, to be confirmed on screen. */
int res_acce(char *out, size_t n, int bust_id, int k, int mouth, int emo)
{
    char path[600], rel[900], prefix[24];
    static const char *sufs[3] = { "_tear", "_tabaco", "" };
    if (!dir_path(path, sizeof(path), bust_id)) return 0;
    snprintf(prefix, sizeof(prefix), "acce_%02d", k);
    DIR *d = opendir(path);
    if (!d) return 0;
    int hit = 0;
    for (int pass = 0; pass < 3 && !hit; pass++) {
        size_t pl = strlen(prefix), sl = strlen(sufs[pass]);
        int key = pass == 0 ? mouth : pass == 1 ? emo : -1;
        struct dirent *e;
        rewinddir(d);
        while ((e = readdir(d))) {
            size_t el = strlen(e->d_name);
            if (el < 4 || strcasecmp(e->d_name + el - 4, ".png")) continue;
            if (strncasecmp(e->d_name, prefix, pl)) continue;
            size_t midl = el - 4 - pl;
            if (sl) {
                char digits[32];
                if (midl <= sl) continue;
                if (strncasecmp(e->d_name + pl, sufs[pass], sl)) continue;
                snprintf(digits, sizeof(digits), "%.*s", (int)(midl - sl),
                         e->d_name + pl + sl);
                if (!digits_have(digits, key)) continue;
            } else if (midl) {
                continue;
            }
            snprintf(rel, sizeof(rel), "%s/%s", path, e->d_name);
            if (file_exists(rel)) {
                snprintf(out, n, "%s", rel);
                hit = 1;
            }
        }
    }
    closedir(d);
    return hit;
}

int res_sound(char *out, size_t n, const char *kind, int id)
{
    for (int i = 0; i < g_ndefs; i++) {
        if (g_defs[i].idx == id && !strcasecmp(g_defs[i].kind, kind)) {
            char rel[256];
            snprintf(rel, sizeof(rel), "files/sound/%s.ogg", g_defs[i].name);
            if (res_resolve(out, n, rel)) return 1;
            rl_log("MISS %s id=%d (%s)", kind, id, rel);
            return 0;
        }
    }
    rl_log("MISS %s id=%d not in sounddef", kind, id);
    return 0;
}

int res_voice(char *out, size_t n, const char *vid)
{
    char rel[256];
    snprintf(rel, sizeof(rel), "files/voice/%s.ogg", vid);
    if (res_resolve(out, n, rel)) return 1;
    rl_log("MISS voice %s", rel);
    return 0;
}

int res_script(char *out, size_t n, const char *base)
{
    char rel[256];
    snprintf(rel, sizeof(rel), "assets/script/%s.txt", base);
    if (res_resolve(out, n, rel)) return 1;
    rl_log("MISS script %s", rel);
    return 0;
}
