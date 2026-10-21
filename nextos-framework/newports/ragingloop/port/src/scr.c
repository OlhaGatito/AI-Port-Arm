/* SPDX-License-Identifier: GPL-3.0-only
 * Script loader: the game's scripts are plain UTF-8 text, one `#Cmd(args)` per
 * line; a bare line is message text. Labels are `#L(name)` and the names are
 * localised strings, so the label table is a string->line map.
 * If/Else/EndIf are pre-linked here so the runner never needs a stack.
 */
#include "rl.h"

#define PARSE_STACK_MAX 64

static char *xstrdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

void scr_free(RlScript *s)
{
    if (!s) return;
    for (int i = 0; i < s->n; i++) {
        for (int j = 0; j < s->lines[i].argc; j++) free(s->lines[i].argv[j]);
        free(s->lines[i].text);
    }
    free(s->lines);
    for (int i = 0; i < s->nl; i++) free(s->labels[i].name);
    free(s->labels);
    free(s->buf);
    free(s);
}

static int split_args(char *s, char **argv, int max)
{
    int n = 0;
    char *p = s;
    if (!*p) return 0;
    while (n < max - 1) {
        char *comma = strchr(p, ',');
        if (!comma) {
            argv[n++] = p;
            return n;
        }
        *comma = 0;
        argv[n++] = p;
        p = comma + 1;
    }
    argv[n++] = p;                       /* the tail stays one arg past the cap */
    return n;
}

static void add_label(RlScript *s, const char *name, int line)
{
    for (int i = 0; i < s->nl; i++) {
        if (!strcmp(s->labels[i].name, name)) {
            s->labels[i].line = line;    /* redefinition wins, as in the APK */
            return;
        }
    }
    s->labels = realloc(s->labels, (size_t)(s->nl + 1) * sizeof(*s->labels));
    if (!s->labels) return;
    s->labels[s->nl].name = xstrdup(name);
    s->labels[s->nl].line = line;
    s->nl++;
}

int scr_label_line(const RlScript *s, const char *name)
{
    if (!s || !name) return -1;
    for (int i = 0; i < s->nl; i++)
        if (!strcmp(s->labels[i].name, name)) return s->labels[i].line;
    return -1;
}

RlScript *scr_load(const char *abspath, char *err, size_t errn)
{
    FILE *f = fopen(abspath, "rb");
    if (!f) {
        snprintf(err, errn, "open failed: %s", abspath);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0 || len > 32 * 1024 * 1024) {
        snprintf(err, errn, "bad size %ld", len);
        fclose(f);
        return NULL;
    }
    char *buf = malloc((size_t)len + 1);
    if (!buf) { snprintf(err, errn, "OOM"); fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)len, f);
    fclose(f);
    buf[got] = 0;
    if (got >= 3 && (unsigned char)buf[0] == 0xEF && (unsigned char)buf[1] == 0xBB
        && (unsigned char)buf[2] == 0xBF) memmove(buf, buf + 3, got - 2);

    RlScript *s = calloc(1, sizeof(*s));
    if (!s) { free(buf); snprintf(err, errn, "OOM"); return NULL; }
    s->buf = buf;
    const char *base = strrchr(abspath, '/');
    base = base ? base + 1 : abspath;
    snprintf(s->fname, sizeof(s->fname), "%.*s", (int)(sizeof(s->fname) - 1), base);

    int cap = 1024;
    s->lines = calloc((size_t)cap, sizeof(*s->lines));
    if (!s->lines) { scr_free(s); snprintf(err, errn, "OOM"); return NULL; }

    char *p = buf;
    while (*p) {
        char *nl = strchr(p, '\n');
        if (nl) *nl = 0;
        size_t l = strlen(p);
        while (l && (p[l - 1] == '\r' || p[l - 1] == ' ' || p[l - 1] == '\t')) p[--l] = 0;
        if (l) {
            if (s->n >= cap) {
                int nc = cap * 2;
                RlLine *nl2 = realloc(s->lines, (size_t)nc * sizeof(*s->lines));
                if (!nl2) break;
                memset(nl2 + cap, 0, (size_t)(nc - cap) * sizeof(*nl2));
                s->lines = nl2;
                cap = nc;
            }
            RlLine *ln = &s->lines[s->n];
            memset(ln, 0, sizeof(*ln));
            ln->endif = -1;
            ln->elsenext = -1;
            if (p[0] == '#') {
                char *lp = p + 1;
                char *open = strchr(lp, '(');
                char *close = open ? strrchr(lp, ')') : NULL;
                ln->kind = RL_CMD;
                if (open && close && close > open) {
                    size_t nl2 = (size_t)(open - lp);
                    if (nl2 >= sizeof(ln->name)) nl2 = sizeof(ln->name) - 1;
                    memcpy(ln->name, lp, nl2);
                    ln->name[nl2] = 0;
                    *close = 0;
                    ln->argc = split_args(open + 1, ln->argv, RL_ARG_MAX);
                    for (int i = 0; i < ln->argc; i++) ln->argv[i] = xstrdup(ln->argv[i]);
                    *close = ')';
                } else {
                    snprintf(ln->name, sizeof(ln->name), "%s", lp);
                    char *paren = strchr(ln->name, '(');
                    if (paren) *paren = 0;
                }
                if (!strcmp(ln->name, "L") && ln->argc >= 1)
                    add_label(s, ln->argv[0], s->n);
            } else {
                ln->kind = RL_TEXT;
                ln->text = xstrdup(p);
                snprintf(ln->name, sizeof(ln->name), "text");
            }
            s->n++;
        }
        if (!nl) break;
        p = nl + 1;
    }

    /* Pre-link If / Else / EndIf. */
    int stack[PARSE_STACK_MAX];
    int sp = 0;
    for (int i = 0; i < s->n; i++) {
        RlLine *ln = &s->lines[i];
        if (ln->kind != RL_CMD) continue;
        if (!strcmp(ln->name, "If")) {
            if (sp < PARSE_STACK_MAX) stack[sp++] = i;
        } else if (!strcmp(ln->name, "Else")) {
            if (sp) s->lines[stack[sp - 1]].endif = i;
            ln->elsenext = sp ? stack[sp - 1] : -1;
        } else if (!strcmp(ln->name, "EndIf")) {
            if (sp) s->lines[stack[--sp]].elsenext = i;
        }
    }
    if (sp) rl_log("%s: %d unterminated #If", s->fname, sp);
    return s;
}
