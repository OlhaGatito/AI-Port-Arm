/* SPDX-License-Identifier: GPL-3.0-only */
#include "rl.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "stb_image.h"

#define RL_TEX_DEFAULT_CAP (48u << 20)

typedef struct Entry {
    char path[600];
    SDL_Texture *tex;
    int w, h;
    unsigned bpp;
    Uint32 stamp;
    struct Entry *older, *newer;
} Entry;

static Entry g_head, g_tail;                       /* LRU sentinels */
static unsigned g_n, g_bytes;
static unsigned g_cap = RL_TEX_DEFAULT_CAP;
static Uint32 g_clock;
static unsigned g_hits, g_loads, g_evicts;

static void unlink_entry(Entry *e)
{
    e->older->newer = e->newer;
    e->newer->older = e->older;
}

static void push_front(Entry *e)
{
    e->newer = g_head.newer;
    e->older = &g_head;
    g_head.newer->older = e;
    g_head.newer = e;
}

static void evict_until(unsigned budget)
{
    while (g_bytes > budget && g_tail.older != &g_head) {
        Entry *victim = g_tail.older;
        unlink_entry(victim);
        SDL_DestroyTexture(victim->tex);
        g_bytes -= victim->bpp;
        g_n--;
        g_evicts++;
        free(victim);
    }
}

void img_init(void)
{
    const char *cap = getenv("RL_TEXCAP");
    if (cap) {
        long mb = strtol(cap, NULL, 10);
        if (mb >= 4 && mb <= 512) g_cap = (unsigned)mb << 20;
    }
    g_head.newer = &g_tail;
    g_tail.older = &g_head;
}

SDL_Texture *img_get(SDL_Renderer *r, const char *path, int *w, int *h)
{
    if (!path || !path[0]) return NULL;
    for (Entry *e = g_head.newer; e != &g_tail; e = e->newer) {
        if (!strcmp(e->path, path)) {
            g_hits++;
            unlink_entry(e);
            push_front(e);
            e->stamp = ++g_clock;
            if (w) *w = e->w;
            if (h) *h = e->h;
            return e->tex;
        }
    }
    int pw = 0, ph = 0, chn = 0;
    unsigned char *px = stbi_load(path, &pw, &ph, &chn, 4);
    if (!px) {
        rl_log("IMG decode failed: %s", path);
        return NULL;
    }
    SDL_Texture *tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_ABGR8888,
                                         SDL_TEXTUREACCESS_STATIC, pw, ph);
    if (!tex) {
        rl_log("IMG CreateTexture failed: %s", SDL_GetError());
        stbi_image_free(px);
        return NULL;
    }
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    if (SDL_UpdateTexture(tex, NULL, px, pw * 4) != 0) {
        rl_log("IMG UpdateTexture failed: %s", SDL_GetError());
        SDL_DestroyTexture(tex);
        stbi_image_free(px);
        return NULL;
    }
    stbi_image_free(px);

    Entry *e = calloc(1, sizeof(*e));
    if (!e) { SDL_DestroyTexture(tex); return NULL; }
    snprintf(e->path, sizeof(e->path), "%s", path);
    e->tex = tex;
    e->w = pw;
    e->h = ph;
    e->bpp = (unsigned)pw * (unsigned)ph * 4u;
    e->stamp = ++g_clock;
    push_front(e);
    g_n++;
    g_bytes += e->bpp;
    g_loads++;
    if (g_bytes > g_cap) evict_until(g_cap);
    if (w) *w = pw;
    if (h) *h = ph;
    return tex;
}

long img_cache_bytes(void)
{
    return (long)g_bytes;
}

void img_stats(void)
{
    rl_log("tex cache n=%u bytes=%u/%u loads=%u hits=%u evicts=%u",
           g_n, g_bytes, g_cap, g_loads, g_hits, g_evicts);
}
