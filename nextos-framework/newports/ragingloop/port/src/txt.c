/* SPDX-License-Identifier: GPL-3.0-only
 * Text: stb_truetype glyph atlas + CJK-aware line breaking + the script's
 * inline colour tags (%coN/%coe, %dot(), %hos/%hoe, %r).
 */
#include "rl.h"

#include <ctype.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define ATLAS_W 1024
#define ATLAS_H 1024
#define MAX_ATLAS 6
#define HASH_N 8192
#define LINE_MAX_CELLS 2048

typedef struct {
    Uint8 *cov;
    SDL_Texture *tex;
    int x, y, row_h;
    int dirty_y0, dirty_y1;
} Atlas;

typedef struct Glyph {
    Uint32 cp;
    int size;
    int w, h;                /* bitmap size, 0 for blank */
    int x0, y0;              /* offsets from pen / baseline, as stb reports them */
    int advance;
    int atlas;               /* -1 when nothing was rasterised */
    int ax, ay;              /* position inside the atlas */
    struct Glyph *next;
} Glyph;

typedef struct {
    Glyph *g;
    Uint32 rgb;
} Cell;

enum { TAG_NONE = 0, TAG_DOT, TAG_HOS };

static SDL_Renderer *g_r;
static stbtt_fontinfo g_font;
static unsigned char *g_map;
static size_t g_map_len;
static int g_ready;
static int g_asc, g_desc, g_gap;   /* vmetrics, font units */
static Atlas g_atl[MAX_ATLAS];
static int g_natlas;
static Glyph *g_hash[HASH_N];
static int g_bakes, g_missing;

static unsigned hash_of(Uint32 cp, int size)
{
    return (unsigned)((cp * 2654435761u) ^ ((unsigned)size * 40503u)) % HASH_N;
}

static void atlas_upload(Atlas *a)
{
    if (a->dirty_y1 <= a->dirty_y0) return;
    if (!a->tex) {
        a->tex = SDL_CreateTexture(g_r, SDL_PIXELFORMAT_ABGR8888,
                                   SDL_TEXTUREACCESS_STATIC, ATLAS_W, ATLAS_H);
        if (!a->tex) { rl_log("atlas create failed: %s", SDL_GetError()); return; }
        SDL_SetTextureBlendMode(a->tex, SDL_BLENDMODE_BLEND);
    }
    int rows = a->dirty_y1 - a->dirty_y0;
    Uint32 *px = malloc((size_t)ATLAS_W * rows * 4);
    if (!px) { rl_log("atlas upload OOM"); return; }
    for (int y = 0; y < rows; y++) {
        const Uint8 *src = a->cov + (size_t)(a->dirty_y0 + y) * ATLAS_W;
        Uint32 *dst = px + (size_t)y * ATLAS_W;
        for (int x = 0; x < ATLAS_W; x++)
            dst[x] = 0x00FFFFFFu | ((Uint32)src[x] << 24);
    }
    SDL_Rect area = { 0, a->dirty_y0, ATLAS_W, rows };
    if (SDL_UpdateTexture(a->tex, &area, px, ATLAS_W * 4) != 0)
        rl_log("atlas upload failed: %s", SDL_GetError());
    free(px);
    a->dirty_y0 = ATLAS_H;
    a->dirty_y1 = 0;
}

static Glyph *glyph_get(Uint32 cp, int size)
{
    unsigned h = hash_of(cp, size);
    for (Glyph *g = g_hash[h]; g; g = g->next)
        if (g->cp == cp && g->size == size) return g;

    Glyph *g = calloc(1, sizeof(*g));
    if (!g) return NULL;
    g->cp = cp;
    g->size = size;
    g->atlas = -1;
    g->next = g_hash[h];
    g_hash[h] = g;
    g_bakes++;
    if (!g_ready) return g;

    float scale = stbtt_ScaleForPixelHeight(&g_font, (float)size);
    int adv_units = 0, lsb = 0;
    stbtt_GetCodepointHMetrics(&g_font, (int)cp, &adv_units, &lsb);
    g->advance = (int)(adv_units * scale + 0.5f);

    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    stbtt_GetCodepointBitmapBoxSubpixel(&g_font, (int)cp, scale, scale, 0, 0,
                                        &x0, &y0, &x1, &y1);
    int gw = x1 - x0, gh = y1 - y0;
    if (gw <= 0 || gh <= 0 || gw > ATLAS_W - 2 || gh > ATLAS_H - 2) {
        if (cp != ' ' && stbtt_FindGlyphIndex(&g_font, (int)cp) == 0) g_missing++;
        return g;                        /* blank or unsupported: advance only */
    }
    if (stbtt_FindGlyphIndex(&g_font, (int)cp) == 0) g_missing++;

    /* shelf pack: run x, then wrap to a new row */
    Atlas *a = NULL;
    for (int i = 0; i < g_natlas; i++) {
        Atlas *c = &g_atl[i];
        if (c->x > 1) c->x += 0;
        if (c->x + gw <= ATLAS_W && c->y + gh <= ATLAS_H) { a = c; break; }
        if (c->y + c->row_h + 1 + gh <= ATLAS_H) {
            c->y += c->row_h + 1;
            c->x = 1;
            c->row_h = 0;
            if (c->x + gw <= ATLAS_W) { a = c; break; }
        }
    }
    if (!a) {
        if (g_natlas >= MAX_ATLAS) { rl_log("glyph atlas exhausted (cp=%U size=%d)", cp, size); return g; }
        a = &g_atl[g_natlas];
        memset(a, 0, sizeof(*a));
        a->cov = calloc(1, (size_t)ATLAS_W * ATLAS_H);
        if (!a->cov) { rl_log("OOM glyph atlas %d", g_natlas); return g; }
        a->x = a->y = 1;
        a->dirty_y0 = ATLAS_H;
        a->dirty_y1 = 0;
        g_natlas++;
    }
    stbtt_MakeCodepointBitmapSubpixel(&g_font,
                                      a->cov + (size_t)a->y * ATLAS_W + a->x,
                                      gw, gh, ATLAS_W, scale, scale, 0, 0, (int)cp);
    if (a->y < a->dirty_y0) a->dirty_y0 = a->y;
    if (a->y + gh > a->dirty_y1) a->dirty_y1 = a->y + gh;
    g->w = gw;
    g->h = gh;
    g->x0 = x0;
    g->y0 = y0;
    g->ax = a->x;
    g->ay = a->y;
    g->atlas = (int)(a - g_atl);
    if (gh > a->row_h) a->row_h = gh;
    a->x += gw + 1;
    return g;
}

void txt_init(SDL_Renderer *r, const char *path)
{
    g_r = r;
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        rl_log("FONT open failed: %s", path);
        return;
    }
    struct stat st;
    if (fstat(fd, &st) != 0 || st.st_size < 4096) {
        rl_log("FONT too small: %s", path);
        close(fd);
        return;
    }
    g_map_len = (size_t)st.st_size;
    g_map = mmap(NULL, g_map_len, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (g_map == MAP_FAILED) {
        rl_log("FONT mmap failed: %s", path);
        g_map = NULL;
        return;
    }
    int off = stbtt_GetFontOffsetForIndex(g_map, 0);
    if (off < 0 || !stbtt_InitFont(&g_font, g_map, off)) {
        rl_log("FONT init failed: %s (stb_truetype needs TrueType glyf outlines, not CFF)", path);
        return;
    }
    stbtt_GetFontVMetrics(&g_font, &g_asc, &g_desc, &g_gap);
    g_ready = 1;
    rl_log("font ok %s size=%zu vmetrics a=%d d=%d g=%d", path, g_map_len, g_asc, g_desc, g_gap);
}

int txt_ready(void) { return g_ready; }

int txt_missing(void) { return g_missing; }

void txt_stats(void)
{
    rl_log("txt: bakes=%d missing=%d atlases=%d", g_bakes, g_missing, g_natlas);
}

const char *txt_utf8_next(const char *s, unsigned *cp)
{
    unsigned char c = (unsigned char)*s;
    if (!c) { *cp = 0; return s; }
    if (c < 0x80) { *cp = c; return s + 1; }
    if ((c & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        *cp = ((unsigned)(c & 0x1F) << 6) | (unsigned)(s[1] & 0x3F);
        return s + 2;
    }
    if ((c & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        *cp = ((unsigned)(c & 0x0F) << 12) | ((unsigned)(s[1] & 0x3F) << 6)
            | (unsigned)(s[2] & 0x3F);
        return s + 3;
    }
    if ((c & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80
        && (s[3] & 0xC0) == 0x80) {
        *cp = ((unsigned)(c & 0x07) << 18) | ((unsigned)(s[1] & 0x3F) << 12)
            | ((unsigned)(s[2] & 0x3F) << 6) | (unsigned)(s[3] & 0x3F);
        return s + 4;
    }
    *cp = '?';
    return s + 1;
}

static int is_ascii_word(Uint32 cp)
{
    return cp < 0x80 && (isalnum((int)cp) || cp == '-' || cp == '\'' || cp == '.'
                         || cp == '&' || cp == '@');
}

/* A line may not begin with these. */
static int forbid_lead(Uint32 cp)
{
    static const Uint32 t[] = {
        0x3001, 0x3002, 0xFF0C, 0xFF0E, 0xFF1A, 0xFF1B, 0xFF01, 0xFF1F, 0xFF09,
        0x300B, 0x300D, 0x300F, 0xFF3D, 0xFF5D, 0x2019, 0x201D, 0x2026, 0x30FC,
        0x30FB, 0xFF3A, 0xFF05, 0xFF03, 0x3099, 0x309A, 0x00A1, 0x00BF, 0xFF10
    };
    for (unsigned i = 0; i < sizeof(t) / sizeof(t[0]); i++)
        if (cp == t[i]) return 1;
    return 0;
}

/* A line may not end with these. */
static int forbid_tail(Uint32 cp)
{
    static const Uint32 t[] = {
        0xFF08, 0x300A, 0x300C, 0x300E, 0xFF3B, 0xFF5B, 0x2018, 0x201C, 0xFF04
    };
    for (unsigned i = 0; i < sizeof(t) / sizeof(t[0]); i++)
        if (cp == t[i]) return 1;
    return 0;
}

/* Inline colour/emphasis tags. Returns the length consumed (0 if not a tag). */
static int tag_scan(const char *p, Uint32 base_rgb, Uint32 *rgb, int *tag)
{
    static const Uint32 pal[10] = {
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFD24A, 0xFFE9A040, 0xFF7FD4FF,
        0xFF9BE86E, 0xFFFF7A7A, 0xFFC79BFF, 0xFFFFFFFF, 0xFFBFBFBF
    };
    if (!strncmp(p, "%coe", 4) || !strncmp(p, "%hoe", 4)) {
        *rgb = base_rgb; *tag = TAG_NONE; return 4;
    }
    if (!strncmp(p, "%hos", 4)) { *rgb = 0xFFE9A040; *tag = TAG_HOS; return 4; }
    if (!strncmp(p, "%dot", 4) && p[4] == '(') { *rgb = 0xFFFFD24A; *tag = TAG_DOT; return 5; }
    if (!strncmp(p, "%co", 3) && p[3] >= '0' && p[3] <= '9') {
        int n = p[3] - '0';
        *rgb = pal[n < 10 ? n : 0];
        return 4;
    }
    if (!strncmp(p, "%r", 2)) return 2;
    return 0;
}

static void draw_cells(const Cell *line, int from, int to, int x, int baseline)
{
    Uint32 last = 0xFFFFFFFFu;
    int cur_atlas = -1;
    for (int i = from; i < to; i++) {
        const Cell *c = &line[i];
        Glyph *g = c->g;
        int adv = g->advance ? g->advance : g->w;
        if (g->w > 0 && g->h > 0 && g->atlas >= 0 && g->atlas < g_natlas) {
            Atlas *a = &g_atl[g->atlas];
            atlas_upload(a);          /* glyphs are baked during layout, just now */
            if (a->tex) {
                if (g->atlas != cur_atlas || c->rgb != last) {
                    SDL_SetTextureColorMod(a->tex, (Uint8)(c->rgb >> 16),
                                           (Uint8)(c->rgb >> 8), (Uint8)c->rgb);
                    cur_atlas = g->atlas;
                    last = c->rgb;
                }
                SDL_Rect src = { g->ax, g->ay, g->w, g->h };
                SDL_Rect dst = { x + g->x0, baseline + g->y0, g->w, g->h };
                SDL_RenderCopy(g_r, a->tex, &src, &dst);
            }
        }
        x += adv;
    }
}

/* y is the top of the first line box; the baseline is derived from the font. */
static void layout(const char *s, int x, int y, int px, Uint32 base_rgb,
                   int maxw, int line_px, int max_lines)
{
    static Cell line[LINE_MAX_CELLS];
    int np = 0, line_w = 0, last_break = 0, shown = 0;
    Uint32 rgb = base_rgb;
    int tag = TAG_NONE;
    float scale = g_ready ? stbtt_ScaleForPixelHeight(&g_font, (float)px) : 0.f;
    int asc = g_ready ? (int)(g_asc * scale + 0.5f) : px;
    int baseline = y + asc;
    const char *p = s;

    if (line_px <= 0) line_px = asc + (int)(-g_desc * scale + 0.5f) + 2;

    while (*p) {
        int tl = (*p == '%') ? tag_scan(p, base_rgb, &rgb, &tag) : 0;
        if (tl) { p += tl; continue; }
        unsigned cp;
        const char *nx = txt_utf8_next(p, &cp);
        if (!cp) break;
        if (tag == TAG_DOT && cp == ')') { rgb = base_rgb; tag = TAG_NONE; p = nx; continue; }
        if (cp == '\n') {
            draw_cells(line, 0, np, x, baseline);
            np = 0; line_w = 0; last_break = 0;
            baseline += line_px;
            shown++;
            p = nx;
            if (max_lines && shown >= max_lines) break;
            continue;
        }
        Glyph *g = glyph_get(cp, px);
        if (!g) { p = nx; continue; }
        int adv = g->advance ? g->advance : g->w;
        int can_break = 1;
        if (cp == ' ') can_break = 1;
        else if (np && is_ascii_word(line[np - 1].g->cp) && is_ascii_word(cp)) can_break = 0;
        else if (forbid_lead(cp)) can_break = 0;
        else if (np && forbid_tail(line[np - 1].g->cp)) can_break = 0;
        if (can_break && np) last_break = np;

        if (maxw > 0 && np && line_w + adv > maxw) {
            int cut = last_break > 0 ? last_break : np;
            draw_cells(line, 0, cut, x, baseline);
            int keep = np - cut;
            if (keep > 0) memmove(line, &line[cut], (size_t)keep * sizeof(line[0]));
            np = keep;
            line_w = 0;
            for (int i = 0; i < np; i++)
                line_w += line[i].g->advance ? line[i].g->advance : line[i].g->w;
            last_break = 0;
            baseline += line_px;
            shown++;
            if (max_lines && shown >= max_lines) break;
        }
        if (np < LINE_MAX_CELLS) {
            line[np].g = g;
            line[np].rgb = rgb;
            np++;
            line_w += adv;
        }
        p = nx;
    }
    if (np) draw_cells(line, 0, np, x, baseline);
}

void txt_draw(SDL_Renderer *r, const char *utf8, int x, int y, int px,
              uint32_t rgb, int maxw, int line_px)
{
    (void)r;
    if (!utf8 || !*utf8) return;
    layout(utf8, x, y, px, rgb, maxw, line_px, 0);
}

/* Width of the first logical line, tags removed. */
int txt_measure(const char *utf8, int px)
{
    int w = 0;
    Uint32 rgb = 0xFFFFFFFFu;
    int tag = TAG_NONE;
    const char *p = utf8;
    while (*p) {
        int tl = (*p == '%') ? tag_scan(p, 0xFFFFFFFFu, &rgb, &tag) : 0;
        if (tl) { p += tl; continue; }
        unsigned cp;
        const char *nx = txt_utf8_next(p, &cp);
        if (!cp) break;
        if (tag == TAG_DOT && cp == ')') { tag = TAG_NONE; p = nx; continue; }
        if (cp == '\n') break;
        if (cp == '@' && !strncmp(p, "@s", 2)) { p += 2; w += px / 3; continue; }
        Glyph *g = glyph_get(cp, px);
        if (g) w += g->advance ? g->advance : g->w;
        p = nx;
    }
    (void)rgb;
    return w;
}

int txt_height(int px, int lines)
{
    float scale = g_ready ? stbtt_ScaleForPixelHeight(&g_font, (float)px) : 0.f;
    int asc = g_ready ? (int)(g_asc * scale + 0.5f) : px;
    int dsc = g_ready ? (int)(-g_desc * scale + 0.5f) : px / 4;
    return asc + dsc + 2 + (lines > 1 ? (lines - 1) * (asc + dsc + 2) : 0);
}

void txt_flush_uploads(void)
{
    for (int i = 0; i < g_natlas; i++) atlas_upload(&g_atl[i]);
}
