/* SPDX-License-Identifier: GPL-3.0-only
 * Raging Loop — native AArch64 ADV interpreter, entry point and runner.
 *
 * Everything is drawn into a 1280x720 target texture (the native size of every
 * background CG and the coordinate space the scripts address) and then scaled
 * into a letterboxed rect on the panel, so the script's x/y numbers can be used
 * verbatim without a second coordinate transform.
 */
#include "rl.h"

#include <errno.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define TARGET_FPS 30

int rl_verbose = 0;
static FILE *g_logfile;

void rl_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    fflush(stderr);
    if (g_logfile) {
        va_start(ap, fmt);
        vfprintf(g_logfile, fmt, ap);
        fprintf(g_logfile, "\n");
        fflush(g_logfile);
        va_end(ap);
    }
    va_end(ap);
}

/* ---- machine state ------------------------------------------------------ */

typedef struct {
    RlScript *s;
    int pc;
} Frame;

typedef struct {
    int active, from_x, from_y, to_x, to_y, left;   /* #ImgBustXY slide */
} SlotMove;

static SDL_Window *g_win;
static SDL_Renderer *g_winr;
static SDL_Texture *g_target;
static SDL_Rect g_dst;                 /* where the 1280x720 target lands */
static int g_win_w, g_win_h;
static RlView g_v;
static int g_c[RL_FLAG_MAX], g_s[RL_FLAG_MAX];
static int g_cmp;                      /* register loaded by #CmpC/#CmpS */
static Frame g_stack[8];
static int g_sp;
static int g_wait;                     /* #Wait frames remaining */
static int g_await;                    /* stopped at #MesEnd, needs a key */
static int g_select;                   /* options from the pending #Select */
static int g_sel_block;                /* interpretation stops while selecting */
static char g_sel_label[16][128];      /* #Select args are literal display text */
static char g_sel_cond[16][32];
static int g_sel_n, g_sel_lim;
static int g_sel_cur;
static int g_quit;
static int g_shown_lines;
static char g_disp[8192];                /* the revealed prefix of g_v.text that is drawn */
static int g_reveal_bytes;               /* bytes of g_v.text made visible so far */
static int g_reveal_acc;                 /* fixed-point: 30 units = one second */
static int g_text_cps = 20;              /* RL_TEXT_CPS: characters revealed per second */
static SlotMove g_move[RL_SLOT_MAX];
static Uint32 g_t0;
static Uint32 g_frames;
static int g_maxframes;
static int g_autop;                  /* RL_AUTOP: frames to hold a message before advancing */
static int g_await_ct;
static int g_sel_ct;                   /* RL_AUTOP dwell frames on a #Select */
static int g_save_toast;               /* frames left to show the saved notice */

/* ---- option screen ------------------------------------------------------ */
/* The original keeps these as ints in shared_prefs/RL.xml (every one shipped at
 * the middle value 2) and draws them as a five-position slider: opt_bar_01 is
 * 小/大, opt_bar_02 慢/快, opt_bar_03 浅/深. See docs/ANALYSIS.md §2.7. */
enum { OPT_CPS, OPT_AUTO, OPT_FONTPX, OPT_SHADE, OPT_BGM, OPT_SE, OPT_VOICE, OPT_N };
static int g_opt[OPT_N] = { 2, 2, 2, 2, 2, 2, 2 };
static const int g_opt_cps[5]    = { 8, 14, 20, 28, 40 };
static const int g_opt_auto[5]   = { 120, 90, 60, 36, 20 };  /* dwell frames at 30 fps */
static const int g_opt_fontpx[5] = { 28, 31, 34, 38, 42 };
static const int g_opt_shade[5]  = { 0x50, 0x8C, 0xC8, 0xE4, 0xF4 };
static const float g_opt_gain[5] = { 0.f, .3f, .6f, .85f, 1.f };
static const char *g_opt_name[OPT_N] = {
    "文本推送速度", "文本自动推进速度", "字体大小", "文本框浓度",
    "音乐音量", "音效音量", "语音音量"
};
static const char *g_opt_bar[OPT_N] = {
    "assets/opt_bar_02.png", "assets/opt_bar_02.png", "assets/opt_bar_01.png",
    "assets/opt_bar_03.png", "assets/opt_bar_01.png", "assets/opt_bar_01.png",
    "assets/opt_bar_01.png"
};

/* 角色语音 — the original's fourth settings tab (opt_tl04 literally reads
 * 角色语音). Its rows are opt_cha_l01..18: 380x140 art with the portrait and the
 * Chinese name already baked in, so nothing here needs our font. Each row cycles
 * three states, and b10_h/i/j_on.png are those three states — 无 / 中 / 大 —
 * which is why they are 150x80 while every other b10_* button is 320x80.
 *
 * The row order is the art's own, not the scripts': #MesSta(voiceId, speakerId)
 * carries speakerId 1..19, and the voice file's own suffix is that same number
 * (7579 of 7581 lines agree), so g_sid_row is the table between them. Derived in
 * docs/ANALYSIS.md §2.11. speakerId 18/19 are the two ？？ voices and share 其他. */
#define VO_ROW_N 18
static const unsigned char g_sid_row[20] = {
    0, 1, 13, 17, 9, 14, 6, 7, 8, 5, 4, 10, 15, 11, 16, 12, 2, 3, 18, 18
};
static int g_vo[VO_ROW_N] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
/* 中 is the shipped default, so it must be unity; 大 is a boost, hence the clip
 * guard in the mixer. The original's curve is not recoverable from its assets. */
static const float g_vo_gain[3] = { 0.f, 1.f, 1.4f };
static const char *const g_vo_btn[3] = {
    "assets/b10_h_on.png", "assets/b10_i_on.png", "assets/b10_j_on.png"
};
static int g_vo_cur;                 /* row under the cursor on the 角色语音 page */
static int g_vo_dbg;                 /* row the last voiced line used; verbose probe only */

/* Row order follows the original's cfg_L00..12 where the port has a
 * counterpart: ウィンドウ非表示 → ログ表示 → セーブ … タイトルへ戻る. */
enum { M_RESUME, M_OPT, M_AUTO, M_SKIP, M_JUMPSEL, M_HIDE, M_LOG, M_CHART, M_SAVE, M_LOAD,
       M_TITLE, MENU_N };
static const char *g_menu_name[MENU_N] = {
    "继续游戏", "选项设置", "自动推进", "快进跳过", "跳到下个选项",
    "隐藏文本框", "回想日志", "章节图表", "保存进度", "读取进度", "返回标题"
};
static int g_menu;                 /* 0 closed, 1 quick list, 2 option page, 3 voice rows, 4 slot list, 5 chart, 6 key config */
static int g_menu_cur, g_opt_cur;
static int g_slot_cur;               /* row under the cursor on the 存读档 page */
static int g_slot_mode;              /* 0 = 保存进度, 1 = 读取进度 */
static char g_slot_msg[48];          /* one-line refusal shown on the slot page */
static int g_slot_test = -1;         /* RL_SAVE_SLOT: slot index for the test write */
static int g_slot_at;                /* RL_SAVE_AT: frame to fire that test write */
static int g_fire_at, g_fire_val;    /* RL_FIRE: frame + button slot of a test binding press */

/* ---- 按键配置 -------------------------------------------------------------
 * The original lets an external keyboard drive the in-game menu: shared_prefs
 * holds KEYCONFIG_NUM plus a sparse list of KEYCONFIG_MENU_<n> (one of the 13
 * KEY_MENU_* action names) / KEYCONFIG_KEYCODE_<n> (an Android KeyEvent code)
 * pairs, all shipped empty (NUM=0). Its screen art is key_w_00_off/on row
 * plates with a b_key_off/on/gray "KEY" keycap, framed by keycursor_1..4.
 *
 * A handheld has no keyboard, so this port keeps the *feature* and swaps the
 * carrier: a menu action can be bound to a spare pad button. L1 and SELECT are
 * not offered - they are the built-in menu/escape keys, and taking them away
 * could trap a player in an overlay. M_RESUME is not offered either: binding
 * "close a menu that is closed" does nothing. */
enum { K_NONE, K_A, K_B, K_X, K_Y, K_R1, K_START, KEY_V_N };
static const char *g_key_name[KEY_V_N] = {
    "无", "A", "B", "X", "Y", "R1", "START"
};
#define KEY_BTN_N (KEY_V_N - 1)        /* bindable buttons, index = value - 1 */
static int g_key[MENU_N];              /* action row -> KEY_V_N value, all 无 */
static int g_key_cur;                  /* row under the cursor on the 按键配置 page */
static int g_auto_on, g_skip_on, g_box_hidden;
static int g_jump_sel;             /* 快进 until the next #Select (see tick) */
static int g_jump_ct;

/* Title screen. The button art carries its own Chinese text, and tb03 ships a
 * _gray variant, so the original itself dims entries the player cannot use
 * yet — a greyed row here is faithful, not a placeholder. */
#define TITLE_N 5
static const char *g_title_btn[TITLE_N] = {
    "assets/tb01", "assets/tb02", "assets/tb03", "assets/tb04", "assets/tb05"
};
static const int g_title_ok[TITLE_N] = { 1, 1, 0, 0, 1 };
static int g_title, g_title_cur;
static const char *g_script;

/* 回想日志 — the original's ログ表示 page (cfg_L04).
 *
 * Its art is a set of empty plates. f_select 450x540 and f_bar_select 40x540
 * form the scene column, f_maintxt 440x530 the text column, and 450+40+440 =
 * 930 centres in 1280 with a 175 px margin either side. f_maintxt carries
 * hairlines at y=79 and y=449, i.e. header / body / footer zones, which is what
 * a scene title above a paged body and a page counter below it implies. None of
 * the six plates has a glyph baked into it, so every character here is ours.
 * The evidence, and why the port groups entries by #L() node, is in
 * docs/ANALYSIS.md §2.8 and §2.9. */
#define LOG_MAX   256            /* messages kept; the corpus averages 20 chars */
#define LOG_DROP  128            /* released at once when the buffer fills */
#define LOG_TEXT  1024
#define LOG_FILE  "ragingloop.log"
#define LGR_X     175            /* scene column */
#define LGT_X     665            /* text column, 175 + 450 + 40 */
#define LG_Y      90
#define LGT_Y     (LG_Y + 5)     /* the 530-tall plate centred on the 540 one */
#define LG_ROWS   13             /* scene rows inside the 540-tall plate */
#define LG_ROW_H  40
#define LGT_HDR_H 79             /* f_maintxt's own hairline, see above */
#define LGT_BOT   449            /* ... and its second one */
#define LGT_PX    26             /* backlog body size, independent of 字体大小 */

typedef struct {
    char node[64];               /* nearest enclosing #L() title */
    char name[48];
    char text[LOG_TEXT];
} RlLogMsg;

static RlLogMsg g_log[LOG_MAX];
static int g_log_n, g_log_open, g_log_cur, g_log_pg;
static int g_backlog_at;           /* RL_BACKLOG: open the panel at this frame */
static int g_reset_at;             /* RL_RESET: run 返回标题 at this frame */
static char g_log_node[64];        /* #L() the interpreter last walked over */

static void advance(int dir);
static void reveal_reset(void);
static void reveal_finish(void);
static void draw_log(void);
static void log_push(void);
static void log_flush(void);
static void log_read_disk(void);
static void grp_build(void);
static int grp_pages(int gi, int *starts, int cap);
static void backlog_show(void);
static void game_reset(void);

/* ---- view helpers ------------------------------------------------------- */

static void slot_clear(int i)
{
    if (i < 1 || i >= RL_SLOT_MAX) return;
    memset(&g_v.slot[i], 0, sizeof(RlSlot));
}

static RlSlot *slot_get(int i)
{
    if (i < 1 || i >= RL_SLOT_MAX) return NULL;
    return &g_v.slot[i];
}

static int argi(const RlLine *l, int i, int dflt)
{
    if (i >= l->argc || !l->argv[i] || !*l->argv[i]) return dflt;
    return atoi(l->argv[i]);
}

/* ---- drawing ------------------------------------------------------------ */

static void draw_sprite(const char *path, int x, int y, int w, int h, int alpha)
{
    if (!path || !*path) return;
    SDL_Texture *t = img_get(g_winr, path, NULL, NULL);
    if (!t) return;
    if (alpha < 100) SDL_SetTextureAlphaMod(t, (Uint8)(255 * alpha / 100));
    SDL_Rect d = { x, y, w, h };
    SDL_RenderCopy(g_winr, t, NULL, &d);
}

/* Every layer of a bust - the body, the brow/eye/mouth crops, the accessories -
 * is a crop of the same body canvas anchored at its top-left, so each one draws
 * at the slot origin with its own pixel size times the slot scale. Confirmed by
 * compositing assets/01 (568x911 body, 371x256 face crops) and assets/02
 * (396x630 / 272x265): the crops land exactly in the sockets only at (0,0). */
static void draw_slot_layer(const char *path, const RlSlot *sl, int ox, int oy)
{
    int w = 0, h = 0;
    SDL_Texture *t = img_get(g_winr, path, &w, &h);
    if (!t || w <= 0 || h <= 0) return;
    SDL_Rect d = { ox, oy, w * sl->scale / 100, h * sl->scale / 100 };
    SDL_SetTextureAlphaMod(t, (Uint8)(255 * sl->alpha / 100));
    SDL_RenderCopy(g_winr, t, NULL, &d);
}

static void render_bust(int i)
{
    RlSlot *sl = slot_get(i);
    if (!sl || !sl->used) return;
    char path[600];
    if (!res_bust(path, sizeof(path), sl->bustId)) return;

    /* #ImgBustAdv's y is the vertical centre of the body canvas in most calls and
     * its top edge when arg4 is 15 - fitted over the whole corpus: that split puts
     * the head above the message box in 99.7% / 99.0% of the two populations, while
     * any single anchor reaches at most 79%. */
    int bw = 0, bh = 0;
    if (!img_get(g_winr, path, &bw, &bh)) return;
    int oy = sl->anchor ? sl->y : sl->y - bh * sl->scale / 200;
    draw_slot_layer(path, sl, sl->x, oy);

    static const char *kinds[3] = { "brow", "eye", "mouth" };
    int idx[3] = { sl->brow, sl->eye, sl->mouth };
    for (int k = 0; k < 3; k++) {
        if (idx[k] < 0) continue;
        if (res_part(path, sizeof(path), sl->bustId, kinds[k], idx[k]))
            draw_slot_layer(path, sl, sl->x, oy);
    }
    /* ImgFaceAdv arg4 selects the face overlay; arg1 of ImgItem selects an
     * accessory. They are independent switches, so both live on the slot. */
    static const char *overs[4] = { "", "red", "shadow", "black" };
    if (sl->faceover >= 1 && sl->faceover <= 3 &&
        res_overlay(path, sizeof(path), sl->bustId, overs[sl->faceover]))
        draw_slot_layer(path, sl, sl->x, oy);
    for (int k = 1; k <= 8; k++)
        if (sl->item[k] &&
            res_acce(path, sizeof(path), sl->bustId, k, sl->mouth, sl->emo))
            draw_slot_layer(path, sl, sl->x, oy);
}

static void draw_bar(int x, int y, int w, int h, Uint32 argb)
{
    SDL_SetRenderDrawBlendMode(g_winr, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_winr, (Uint8)(argb >> 16), (Uint8)(argb >> 8),
                           (Uint8)argb, (Uint8)(argb >> 24));
    SDL_Rect r = { x, y, w, h };
    SDL_RenderFillRect(g_winr, &r);
}

#define BOX_X 40
#define BOX_Y 496
#define BOX_W 1200
#define BOX_H 190
#define TEXT_X 92
#define TEXT_Y 546
static int g_text_px = 34;             /* 字体大小 option; opt_apply() owns it */
static int g_name_px = 30;
static int g_box_a = 0xC8;             /* 文本框浓度 option */

static void draw_ui_a(const char *rel, int x, int y, int w, int h, int alpha)
{
    char path[600];
    if (!res_resolve(path, sizeof(path), rel)) {
        rl_log("MISS ui %s", rel);
        return;
    }
    SDL_Texture *t = img_get(g_winr, path, NULL, NULL);
    if (!t) return;
    SDL_SetTextureAlphaMod(t, (Uint8)(255 * alpha / 100));
    SDL_Rect d = { x, y, w, h };
    SDL_RenderCopy(g_winr, t, NULL, &d);
}

static void draw_ui(const char *rel, int x, int y, int w, int h)
{
    draw_ui_a(rel, x, y, w, h, 100);
}

/* txt_draw wraps rather than clips, so a one-line cell has to be cut here.
 * `cap` is the size of s; the ellipsis only lands when there is room for it. */
static void clip_w(char *s, size_t cap, int px, int maxw)
{
    int ell = txt_measure("…", px);
    if (txt_measure(s, px) <= maxw) return;
    char *q = s + strlen(s);
    while (q > s) {
        do { q--; } while (q > s && ((*q & 0xC0) == 0x80));
        *q = 0;
        if (txt_measure(s, px) + ell <= maxw) break;
    }
    size_t len = strlen(s);
    if (len + 3 < cap) {
        memcpy(s + len, "\xE2\x80\xA6", 3);
        s[len + 3] = 0;
    }
}

/* The quick list and the option page. The row art is the original's own
 * (opt_w_00_off/on pills, opt_bar_0* five-position sliders with a bar.png
 * handle); only the labels are re-set in our font, because the shipped
 * cfg_L00..12 textures bake in Japanese text while this build reads Chinese. */
/* The slot list needs the save-code helpers below it, so its view lives there. */
static void draw_slot_page(void);
static void draw_chart(void);

static void draw_menu(void)
{
    draw_bar(0, 0, RL_W, RL_H, 0xB4000000);
    if (g_menu == 1) {
        /* Row height follows MENU_N: the plate is a stretched 9-slice, so a
         * shorter row costs nothing and keeps the last entry on screen. */
        const int rw = 600, top = 12, x = (RL_W - rw) / 2;
        const int gap = (RL_H - 46 - top) / MENU_N;
        const int rh = gap - 4;
        for (int i = 0; i < MENU_N; i++) {
            int y = top + i * gap;
            draw_ui(i == g_menu_cur ? "assets/opt_w_00_on.png"
                                    : "assets/opt_w_00_off.png", x, y, rw, rh);
            char lab[192];
            const char *tail = NULL;
            if (i == M_AUTO) tail = g_auto_on ? "ON" : "OFF";
            else if (i == M_SKIP) tail = g_skip_on ? "ON" : "OFF";
            else if (i == M_HIDE) tail = g_box_hidden ? "ON" : "OFF";
            if (tail) snprintf(lab, sizeof(lab), "%s  %s", g_menu_name[i], tail);
            else snprintf(lab, sizeof(lab), "%s", g_menu_name[i]);
            txt_draw(g_winr, lab, x + (rw - txt_measure(lab, 30)) / 2,
                     y + (rh - 30) / 2, 30, 0xFFFFFFFF, 0, 0);
        }
    } else if (g_menu == 2) {
        /* gap 74 keeps the last row clear of the hint line: the slider art is 80
         * tall but its ink only spans y22..63, so a 6 px overlap is transparent. */
        const int bx = 660, bw = 460, bh = 80, gap = 74, top = 22;
        for (int i = 0; i < OPT_N; i++) {
            int y = top + i * gap;
            if (i == g_opt_cur) draw_ui("assets/opt_w_00_on.png", 150, y - 8, 1000, 92);
            txt_draw(g_winr, g_opt_name[i], 190, y + 26, 30, 0xFFFFFFFF, 0, 0);
            draw_ui(g_opt_bar[i], bx, y, bw, bh);
            draw_ui("assets/bar.png", bx + 80 + g_opt[i] * 70, y + 20, 20, 40);
        }
        /* The two sub-pages are plain rows, not sliders. */
        for (int j = 0; j < 2; j++) {
            int y = top + (OPT_N + j) * gap;
            if (g_opt_cur == OPT_N + j)
                draw_ui("assets/opt_w_00_on.png", 150, y - 8, 1000, 66);
            txt_draw(g_winr, j ? "按键配置" : "角色语音", 190, y + 10, 30, 0xFFFFFFFF, 0, 0);
            txt_draw(g_winr, "A 进入", 1000, y + 14, 24, 0xFFBFBFBF, 0, 0);
        }
    } else if (g_menu == 6) {
        /* The original's own key-config plates: key_w_00_off/on (420x100) with a
         * b_key_* keycap at the row's right - blue when a button is bound, grey
         * for 无. The keycap's "KEY" text is baked in, so the bound name goes to
         * its left rather than over it. */
        const int pw = 660, ph = 60, gap = 66, top = 14;
        const int x = (RL_W - pw) / 2;
        for (int i = M_OPT; i < MENU_N; i++) {
            int y = top + (i - M_OPT) * gap;
            draw_ui(i - M_OPT == g_key_cur ? "assets/key_w_00_on.png"
                                           : "assets/key_w_00_off.png", x, y, pw, ph);
            txt_draw(g_winr, g_menu_name[i], x + 20, y + (ph - 28) / 2, 28, 0xFFFFFFFF, 0, 0);
            draw_ui(g_key[i] ? "assets/b_key_on.png" : "assets/b_key_gray.png",
                    x + pw - 62, y + (ph - 54) / 2, 54, 54);
            const char *bn = g_key_name[g_key[i]];
            txt_draw(g_winr, bn, x + pw - 76 - txt_measure(bn, 28), y + (ph - 28) / 2, 28,
                     g_key[i] ? 0xFF7FE8FF : 0xFF8C8C8C, 0, 0);
        }
    } else if (g_menu == 3) {
        /* opt_cha_l01..18 are 380x140 *list rows* (portrait + full name baked in,
         * no reserved gap), and the 无/中/大 control is a separate element to the
         * right - so this page is a scrolling one-column list, not a grid: a grid
         * would either cover the name with the button or make up/down jump rows
         * diagonally while left/right is taken by the value. */
        const int vis = 5, ph = 118, pitch = 124, pw = 320, bw = 126, bh = 67;
        const int cellw = pw + 12 + bw;
        const int x0 = (RL_W - cellw) / 2;
        int top = g_vo_cur - vis / 2;
        if (top < 0) top = 0;
        if (top > VO_ROW_N - vis) top = VO_ROW_N - vis;
        for (int i = top; i < top + vis && i < VO_ROW_N; i++) {
            int y = 40 + (i - top) * pitch;
            char rel[48];
            if (i == g_vo_cur)
                draw_ui("assets/opt_w_00_on.png", x0 - 8, y - 5, cellw + 16, ph + 10);
            snprintf(rel, sizeof(rel), "assets/opt_cha_l%02d.png", i + 1);
            draw_ui(rel, x0, y, pw, ph);
            draw_ui(g_vo_btn[g_vo[i]], x0 + pw + 12, y + (ph - bh) / 2, bw, bh);
        }
        {
            char cnt[24];
            snprintf(cnt, sizeof(cnt), "%d / %d", g_vo_cur + 1, VO_ROW_N);
            txt_draw(g_winr, cnt, RL_W - 90, 14, 24, 0xFFBFBFBF, 0, 0);
        }
    } else if (g_menu == 5) {
        draw_chart();
    } else {
        draw_slot_page();
    }
    txt_draw(g_winr, g_menu == 1 ? "方向键 选择   A 决定   B 关闭菜单"
                    : g_menu == 2 ? "方向键 选择   左右 调整   A/B 返回"
                    : g_menu == 3 ? "方向键 选择   左右 无/中/大   A/B 返回"
                    : g_menu == 6 ? "方向键 选择   左右 换键   A/B 返回"
                    : g_menu == 5 ? ""
                    : "方向键 选择槽位   A 记录/读取   B 返回",
             24, RL_H - 38, 24, 0xFFBFBFBF, 0, 0);
}

/* title_back.jpg is already 1280x720 with the logo and the three talismans
 * baked into it. title_talisman_flash.png is deliberately not drawn: its sheet
 * carries no placement hint and neither correlation measure converges, so the
 * pulse would land visibly off the baked talismans.
 *
 * The phone UI is touch-driven, so tb0N_on is the pressed look and there is no
 * focus state in the original art. A pad needs one, so the cursor is carried by
 * opacity: the focused row is the only fully opaque one. */
static void draw_title(void)
{
    draw_ui("assets/title_back.jpg", 0, 0, RL_W, RL_H);

    for (int i = 0; i < TITLE_N; i++) {
        char rel[64];
        int y = 306 + i * 74;
        if (!g_title_ok[i]) {
            if (i == 2) draw_ui_a("assets/tb03_gray.png", 24, y, 240, 70, 45);
            else {
                snprintf(rel, sizeof(rel), "%s_off.png", g_title_btn[i]);
                draw_ui_a(rel, 24, y, 240, 70, 25);
            }
        } else {
            snprintf(rel, sizeof(rel), "%s_%s.png", g_title_btn[i],
                     i == g_title_cur ? "on" : "off");
            draw_ui_a(rel, 24, y, 240, 70, i == g_title_cur ? 100 : 45);
        }
    }
    txt_draw(g_winr, "方向键 选择   A 决定", 24, RL_H - 34, 24, 0xFFBFBFBF, 0, 0);
}

static void render_frame(void)
{
    SDL_SetRenderTarget(g_winr, g_target);

    if (g_title) {
        SDL_SetRenderDrawColor(g_winr, 0, 0, 0, 255);
        SDL_RenderClear(g_winr);
        draw_title();
        if (g_menu) draw_menu();     /* 设定 opens the same option page */
    } else {
        if (g_v.has_color) {
            SDL_SetRenderDrawColor(g_winr, (Uint8)(g_v.bgcolor >> 16),
                                   (Uint8)(g_v.bgcolor >> 8), (Uint8)g_v.bgcolor, 255);
            SDL_RenderClear(g_winr);
        } else {
            SDL_SetRenderDrawColor(g_winr, 0, 0, 0, 255);
            SDL_RenderClear(g_winr);
        }
        if (g_v.bg[0]) draw_sprite(g_v.bg, 0, 0, RL_W, RL_H, 100);

        for (int i = 1; i < RL_SLOT_MAX; i++) render_bust(i);

        /* The box is hidden under a menu page: the 存读档 list runs to y=602,
         * i.e. straight through BOX_Y=496, and the dim layer is translucent, so
         * leaving it up made the last two rows unreadable. Port judgement - the
         * original's save UI is a Java view we cannot read, not a finding. */
        if (g_v.mesvis && !g_box_hidden && !g_menu && (g_v.name[0] || g_v.text[0])) {
            draw_bar(BOX_X, BOX_Y, BOX_W, BOX_H, ((Uint32)g_box_a) << 24);
            if (g_v.name[0]) {
                draw_bar(BOX_X + 24, BOX_Y - 46, txt_measure(g_v.name, g_name_px) + 56,
                         52, ((Uint32)g_box_a) << 24);
                txt_draw(g_winr, g_v.name, BOX_X + 52, BOX_Y - 40, g_name_px,
                         0xFFFFD24A, 0, 0);
            }
            txt_draw(g_winr, g_disp, TEXT_X, TEXT_Y, g_text_px, 0xFFFFFFFF,
                     BOX_W - (TEXT_X - BOX_X) * 2, g_text_px + 16);
        }

        if (g_select) {
            draw_bar(300, 180, 680, 60 * g_sel_n + 40, 0xD8000000);
            for (int i = 0; i < g_sel_n; i++) {
                int yy = 210 + i * 60;
                if (i == g_sel_cur) draw_bar(320, yy - 8, 640, 52, 0x50FFFFFF);
                int dim = g_sel_lim && g_sel_cond[i][0] && !strcmp(g_sel_cond[i], "0");
                txt_draw(g_winr, g_sel_label[i], 350, yy, 32,
                         dim ? 0xFF808080 : 0xFFFFFFFF, 580, 0);
            }
        }

        if (g_v.fade > 0) {
            int a = (int)(g_v.fade * 255);
            draw_bar(0, 0, RL_W, RL_H, ((Uint32)a << 24));
        }

        if (g_menu) draw_menu();
        if (g_log_open) draw_log();
    }

    if (g_save_toast)
        txt_draw(g_winr, "已保存", RL_W - 130, 24, 28, 0xFFFFFFFF, 0, 0);

    SDL_SetRenderTarget(g_winr, NULL);
    SDL_SetRenderDrawColor(g_winr, 0, 0, 0, 255);
    SDL_RenderClear(g_winr);
    SDL_RenderCopy(g_winr, g_target, NULL, &g_dst);
    SDL_RenderPresent(g_winr);
}

/* ---- interpreter -------------------------------------------------------- */

static int cur_line(RlLine **out)
{
    if (g_sp == 0) return 0;
    Frame *f = &g_stack[g_sp - 1];
    if (!f->s || f->pc < 0 || f->pc >= f->s->n) return 0;
    *out = &f->s->lines[f->pc];
    return 1;
}

static int jump_label(const char *name)
{
    Frame *f = &g_stack[g_sp - 1];
    int l = scr_label_line(f->s, name);
    if (l < 0) {
        rl_log("MISS label %s in %s", name, f->s->fname);
        return 0;
    }
    f->pc = l;
    return 1;
}

static int load_script(const char *base)
{
    char path[600];
    if (!res_script(path, sizeof(path), base)) return 0;
    char err[256];
    RlScript *s = scr_load(path, err, sizeof(err));
    if (!s) { rl_log("script %s: %s", path, err); return 0; }
    rl_log("script %s lines=%d labels=%d", s->fname, s->n, s->nl);
    if (g_sp >= (int)(sizeof(g_stack) / sizeof(g_stack[0]))) {
        rl_log("script stack overflow");
        scr_free(s);
        return 0;
    }
    g_stack[g_sp].s = s;
    g_stack[g_sp].pc = 0;
    g_sp++;
    return 1;
}

/* ---- save / resume ------------------------------------------------------
 * #Save() marks the four checkpoints the original engine autosaved at; its save
 * menu lives in the Java front-end and has no script commands, so an
 * interpreter port can own the position but not the menu.  A snapshot is
 * therefore written at every checkpoint, refreshed on a throttle at message
 * ends (the payload sits on exFAT, so write frequency is the cost to control),
 * written once more when the window closes, and read back at launch.
 * The file is plain text under $RL_ROOT/saves so the port stays removable with
 * its own directory. */

#define SAVE_FILE "ragingloop.save"
#define SAVE_STEP_MS 20000            /* message-end autosave throttle */
/* The original keeps four autos (assets/autosave01..04.png) and a set of manual
 * slots; the manual count is not recoverable from the APK (the save UI is
 * obfuscated to single-letter classes and the shipped RL.xml carries no slot
 * keys - saves are files under FILE_PATH, listed with File.listFiles()), so 10
 * is this port's choice: one page, numbered 01..10 like chrt_sp_no. */
#define SAVE_AUTO_N 4
#define SAVE_MAN_N 10
#define SAVE_SLOT_N (SAVE_AUTO_N + SAVE_MAN_N)

static Uint32 g_save_at;              /* ticks of the last write, 0 = none yet */
static int g_write_ok = 1;            /* 0 under RL_NOSAVE: reads still work, so a
                                       * headless resume probe cannot lie to us. */

static int save_dir(char *out, size_t n)
{
    /* not "save": the payload ships the original shared_prefs tree there */
    snprintf(out, n, "%s/saves", res_root());
    if (mkdir(out, 0777) == 0 || errno == EEXIST) return 1;
    rl_log("save: mkdir %s: %s", out, strerror(errno));
    return 0;
}

/* File name of list slot i: 0..3 are the autos, 4..13 the manual slots. The
 * live checkpoint the game resumes from keeps its historical name and is not a
 * row in this list - every autosave refreshes it *and* rotates into an auto. */
static void slot_leaf(char *out, size_t n, int i)
{
    if (i < SAVE_AUTO_N) snprintf(out, n, "auto_%d.save", i + 1);
    else snprintf(out, n, "slot_%02d.save", i - SAVE_AUTO_N + 1);
}

static void slot_label(char *out, size_t n, int i)
{
    if (i < SAVE_AUTO_N) snprintf(out, n, "自动 %d", i + 1);
    else snprintf(out, n, "槽 %02d", i - SAVE_AUTO_N + 1);
}

static void slot_full(char *out, size_t n, const char *dir, const char *leaf)
{
    snprintf(out, n, "%s/%s", dir, leaf);
}

/* Oldest-auto (or first empty auto) wins the next rotation, so no rotation
 * index has to be persisted anywhere. */
static int auto_pick(const char *dir)
{
    time_t oldest = 0;
    int pick = 0;
    for (int i = 0; i < SAVE_AUTO_N; i++) {
        char leaf[40], path[680];
        struct stat st;
        slot_leaf(leaf, sizeof(leaf), i);
        slot_full(path, sizeof(path), dir, leaf);
        if (stat(path, &st) != 0) return i;            /* an empty slot is oldest */
        if (!oldest || st.st_mtime < oldest) { oldest = st.st_mtime; pick = i; }
    }
    return pick;
}

static int copy_file(const char *from, const char *to)
{
    char buf[8192];
    size_t got;
    FILE *fi = fopen(from, "rb"), *fo;
    if (!fi) return 0;
    fo = fopen(to, "wb");
    if (!fo) { fclose(fi); return 0; }
    while ((got = fread(buf, 1, sizeof(buf), fi)) > 0)
        if (fwrite(buf, 1, got, fo) != got) { fclose(fi); fclose(fo); return 0; }
    fclose(fi);
    fclose(fo);
    return 1;
}

/* Background paths are stored relative to the port root so a save still loads
 * after the directory moves or the case of a component differs. */
static void str_cpy(char *dst, size_t n, const char *src)
{
    size_t len = strlen(src);
    if (n) {
        if (len > n - 1) len = n - 1;
        memcpy(dst, src, len);
        dst[len] = 0;
    }
}

static void path_rel(const char *abs, char *out, size_t n)
{
    const char *root = res_root();
    size_t rl = strlen(root);
    if (root[0] && abs[0] && !strncmp(abs, root, rl) && abs[rl] == '/')
        str_cpy(out, n, abs + rl + 1);
    else
        str_cpy(out, n, abs);
}

static void esc_write(FILE *f, const char *s)
{
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p == '\\') fputs("\\\\", f);
        else if (*p == '\n') fputs("\\n", f);
        else fputc(*p, f);
    }
}

static void esc_read(const char *in, char *out, size_t n)
{
    size_t j = 0;
    for (const char *p = in; *p && j + 1 < n; p++) {
        if (p[0] == '\\' && p[1]) {
            p++;
            out[j++] = *p == 'n' ? '\n' : *p;
        } else {
            out[j++] = *p;
        }
    }
    out[j] = 0;
}

static void kv_str(FILE *f, const char *key, const char *val)
{
    fprintf(f, "%s ", key);
    esc_write(f, val);
    fputc('\n', f);
}

static int save_write(const char *why, const char *leaf)
{
    if (!g_write_ok || !g_sp || g_select) return 0;
    char dir[600], to[680], tmp[680];
    if (!save_dir(dir, sizeof(dir))) return 0;
    slot_full(to, sizeof(to), dir, leaf);
    snprintf(tmp, sizeof(tmp), "%s.tmp", to);
    FILE *f = fopen(tmp, "wb");
    if (!f) {
        rl_log("save: create %s: %s", tmp, strerror(errno));
        g_save_at = SDL_GetTicks();      /* a full card must not be retried every line */
        return 0;
    }
    fprintf(f, "RLSAVE 2\n");
    for (int i = 0; i < g_sp; i++)
        fprintf(f, "frame %s %d\n", g_stack[i].s->fname, g_stack[i].pc);
    for (int i = 0; i < RL_FLAG_MAX; i++) if (g_c[i]) fprintf(f, "c %d %d\n", i, g_c[i]);
    for (int i = 0; i < RL_FLAG_MAX; i++) if (g_s[i]) fprintf(f, "s %d %d\n", i, g_s[i]);
    char rel[600];
    path_rel(g_v.bg, rel, sizeof(rel));
    kv_str(f, "bg", rel);
    fprintf(f, "color %d %u\n", g_v.has_color, g_v.bgcolor);
    for (int i = 1; i < RL_SLOT_MAX; i++) {
        const RlSlot *sl = &g_v.slot[i];
        if (!sl->used) continue;
        int items = 0;
        for (int k = 1; k <= 8; k++) if (sl->item[k]) items |= 1 << (k - 1);
        fprintf(f, "slot %d %d %d %d %d %d %d %d %d %d %d %d %d\n", i, sl->bustId,
                sl->x, sl->y, sl->scale, sl->alpha, sl->anchor, sl->brow, sl->eye,
                sl->mouth, sl->emo, sl->faceover, items);
    }
    kv_str(f, "name", g_v.name);
    kv_str(f, "text", g_v.text);
    /* List preview, kept separate from text: the box is legitimately empty when
     * a save lands on a non-dialogue beat, and reusing text for the restore
     * path would repaint a stale line into the box on load. */
    kv_str(f, "prev", g_v.text[0] ? g_v.text
                     : (g_log_n ? g_log[g_log_n - 1].text : ""));
    fprintf(f, "view %d %d %g %g\n", g_v.mesvis, g_v.mescolor, g_v.fade, g_v.fade_to);
    fprintf(f, "state %d %d %d\n", g_wait, g_await, g_cmp);
    fclose(f);
    if (rename(tmp, to) != 0) {
        rl_log("save: rename %s: %s", to, strerror(errno));
        remove(tmp);
        return 0;
    }
    g_save_at = SDL_GetTicks();
    g_save_toast = 45;
    rl_log("save (%s) at %s:%d", why, g_stack[g_sp - 1].s->fname,
           g_stack[g_sp - 1].pc);
    /* The autos also feed the 存档 list, so the player can jump back past the
     * single live checkpoint. Byte copy: the live file was just fsync-renamed. */
    if (!strcmp(why, "autosave")) {
        char leaf[40], src[680], dst[680];
        int i = auto_pick(dir);
        slot_leaf(leaf, sizeof(leaf), i);
        slot_full(src, sizeof(src), dir, SAVE_FILE);
        slot_full(dst, sizeof(dst), dir, leaf);
        if (copy_file(src, dst)) rl_log("auto: rotated into %s", leaf);
        else rl_log("auto: rotate to %s failed: %s", leaf, strerror(errno));
    }
    return 1;
}

static int save_load_leaf(const char *leaf)
{
    char dir[600], to[680];
    if (!save_dir(dir, sizeof(dir))) return 0;
    slot_full(to, sizeof(to), dir, leaf);
    FILE *f = fopen(to, "rb");
    if (!f) return 0;
    char line[17408];
    if (!fgets(line, sizeof(line), f) || strncmp(line, "RLSAVE ", 7)) {
        rl_log("save: bad header in %s", to);
        fclose(f);
        return 0;
    }
    /* v1 kept one accessory index and no face overlay; the player's existing
     * checkpoint is that shape, so read it and promote it rather than drop it. */
    int ver = atoi(line + 7);
    char bases[8][96];
    int pcs[8], depth = 0;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        int a, b, c, d, e, g, h, ii, jj, kk, ll, mm, nn;
        if (!strncmp(line, "frame ", 6)) {
            char nm[96];
            int pc;
            if (depth < (int)(sizeof(bases) / sizeof(bases[0]))
                && sscanf(line + 6, "%95s %d", nm, &pc) == 2) {
                str_cpy(bases[depth], sizeof(bases[0]), nm);
                pcs[depth] = pc;
                depth++;
            }
        } else if (!strncmp(line, "bg ", 3)) {
            char rel[600];
            esc_read(line + 3, rel, sizeof(rel));
            g_v.bg[0] = 0;
            if (rel[0] == '/') {
                if (access(rel, R_OK) == 0) str_cpy(g_v.bg, sizeof(g_v.bg), rel);
            } else {
                res_resolve(g_v.bg, sizeof(g_v.bg), rel);
            }
        } else if (!strncmp(line, "color ", 6)) {
            if (sscanf(line + 6, "%d %u", &a, (unsigned *)&b) == 2) {
                g_v.has_color = a;
                g_v.bgcolor = (uint32_t)(unsigned)b;
            }
        } else if (!strncmp(line, "slot ", 5)
                   && sscanf(line + 5, "%d %d %d %d %d %d %d %d %d %d %d %d %d",
                             &a, &b, &c, &d, &e, &g, &h, &ii, &jj, &kk, &ll, &mm,
                             &nn) >= 12) {
            if (a >= 1 && a < RL_SLOT_MAX) {
                RlSlot *sl = &g_v.slot[a];
                memset(sl, 0, sizeof(*sl));
                sl->used = 1;
                sl->bustId = b; sl->x = c; sl->y = d; sl->scale = e; sl->alpha = g;
                sl->anchor = h; sl->brow = ii; sl->eye = jj; sl->mouth = kk;
                if (ver == 1) {                  /* v1: field 12 was one acce index */
                    sl->emo = ll;
                    if (mm >= 1 && mm <= 8) sl->item[mm] = 1;
                } else {
                    sl->emo = ll; sl->faceover = mm;
                    for (int k = 1; k <= 8; k++) if (nn & (1 << (k - 1))) sl->item[k] = 1;
                }
            }
        } else if (!strncmp(line, "name ", 5)) {
            esc_read(line + 5, g_v.name, sizeof(g_v.name));
        } else if (!strncmp(line, "text ", 5)) {
            esc_read(line + 5, g_v.text, sizeof(g_v.text));
        } else if (!strncmp(line, "view ", 5) && sscanf(line + 5, "%d %d %g %g", &a, &b,
                                                        &g_v.fade, &g_v.fade_to) == 4) {
            g_v.mesvis = a;
            g_v.mescolor = b;
        } else if (!strncmp(line, "state ", 6) && sscanf(line + 6, "%d %d %d", &a, &b, &c) == 3) {
            g_wait = a;
            g_await = b;
            g_cmp = c;
        } else if (sscanf(line, "c %d %d", &a, &b) == 2 && a >= 0 && a < RL_FLAG_MAX) {
            g_c[a] = b;
        } else if (sscanf(line, "s %d %d", &a, &b) == 2 && a >= 0 && a < RL_FLAG_MAX) {
            g_s[a] = b;
        }
    }
    fclose(f);
    if (!depth) {
        rl_log("save: %s holds no script frame", to);
        return 0;
    }
    while (g_sp) { scr_free(g_stack[--g_sp].s); }
    for (int i = 0; i < depth; i++) {
        char base[96];
        str_cpy(base, sizeof(base), bases[i]);
        size_t bl = strlen(base);
        if (bl > 4 && !strcmp(base + bl - 4, ".txt")) base[bl - 4] = 0;
        if (!load_script(base)) {
            rl_log("save: resume dropped, %s missing", bases[i]);
            /* unwind the frames already restored, or the caller's fresh start
             * would be pushed on top of a half-restored stack */
            while (g_sp) { scr_free(g_stack[--g_sp].s); }
            return 0;
        }
        Frame *fr = &g_stack[g_sp - 1];
        if (pcs[i] < 0 || pcs[i] >= fr->s->n) {
            rl_log("save: pc %d out of range in %s (%d lines), restart of script",
                   pcs[i], bases[i], fr->s->n);
            pcs[i] = 0;
        }
        fr->pc = pcs[i];
    }
    memset(g_move, 0, sizeof(g_move));
    reveal_finish();
    rl_log("save: resumed %s:%d depth=%d", g_stack[g_sp - 1].s->fname,
           g_stack[g_sp - 1].pc, depth);
    return 1;
}

static int save_load(void)
{
    return save_load_leaf(SAVE_FILE);
}

/* What the 存档 list shows for one slot, read without touching the VM: the
 * original labels each row with its own day stamp and a text preview (dex
 * strings saveDay / saveDat / saveNotify), so the port reads the same three
 * things back out of the snapshot it already writes. */
typedef struct {
    int used;
    char when[20];      /* local mtime, "MM-DD HH:MM" */
    char script[40];    /* top script frame, basename without .txt */
    int pc;
    char preview[64];   /* saved message text, markup stripped, cut to 20 glyphs */
} SlotInfo;

static void preview_cut(char *s)
{
    /* Drop the engine's inline %xx markup and its line breaks - the row is one
     * line, and a saved "\n" would otherwise spill over the next slot. Then
     * keep at most 20 UTF-8 glyphs so the row stays inside the plate. */
    char *w = s;
    for (char *p = s; *p; p++) {
        if (*p == '\n') continue;
        if (p[0] == '%' && p[1]) { p++; continue; }
        *w++ = *p;
    }
    *w = 0;
    int keep = 0;
    for (char *p = s; *p;) {
        unsigned char c = (unsigned char)*p;
        int adv = c < 0x80 ? 1 : (c < 0xE0 ? 2 : (c < 0xF0 ? 3 : 4));
        keep++;
        if (keep > 20) { *p = 0; break; }
        p += adv;
    }
}

static int slot_probe(int idx, SlotInfo *si)
{
    char dir[600], leaf[40], path[680];
    memset(si, 0, sizeof(*si));
    if (!save_dir(dir, sizeof(dir))) return 0;
    slot_leaf(leaf, sizeof(leaf), idx);
    slot_full(path, sizeof(path), dir, leaf);
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    si->used = 1;
    {
        struct tm tm;
        localtime_r(&st.st_mtime, &tm);
        strftime(si->when, sizeof(si->when), "%m-%d %H:%M", &tm);
    }
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    char line[17408];
    int have_prev = 0;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (!strncmp(line, "frame ", 6)) {
            char nm[96];
            int pc = 0;
            if (sscanf(line + 6, "%95s %d", nm, &pc) >= 1) {
                str_cpy(si->script, sizeof(si->script), nm);
                si->pc = pc;
                size_t l = strlen(si->script);
                if (l > 4 && !strcmp(si->script + l - 4, ".txt")) si->script[l - 4] = 0;
            }
        } else if (!strncmp(line, "prev ", 5)) {
            char raw[256];
            esc_read(line + 5, raw, sizeof(raw));
            str_cpy(si->preview, sizeof(si->preview), raw);
            preview_cut(si->preview);
            have_prev = 1;
        } else if (!strncmp(line, "text ", 5) && !have_prev) {
            /* slots written before the prev field exist still preview */
            char raw[256];
            esc_read(line + 5, raw, sizeof(raw));
            str_cpy(si->preview, sizeof(si->preview), raw);
            preview_cut(si->preview);
        }
    }
    fclose(f);
    return 1;
}

/* 存读档 page. Row art is the original's dialog button set (b02a 读取 /
 * b02b 记录 / b02c 覆盖) and the four autos carry their own 60x60 badges
 * (autosave01..04.png). */
static void draw_slot_page(void)
{
    const int vis = 6, rw = 1000, rh = 80, rx = (RL_W - rw) / 2, pitch = 92;
    int top = g_slot_cur - vis / 2;
    if (top < 0) top = 0;
    if (top > SAVE_SLOT_N - vis) top = SAVE_SLOT_N - vis;
    txt_draw(g_winr, g_slot_mode ? "读取进度" : "保存进度", rx, 14, 30,
             0xFFFFFFFF, 0, 0);
    for (int i = top; i < top + vis && i < SAVE_SLOT_N; i++) {
        int y = 62 + (i - top) * pitch;
        int tx = rx + (i < SAVE_AUTO_N ? 84 : 16);
        char lab[24], when[20], script[40], prev[64];
        SlotInfo si;
        slot_probe(i, &si);
        slot_label(lab, sizeof(lab), i);
        /* Non-cursor rows get the off plate too, like the quick list: without
         * it their text sat directly on the scene and lost contrast. */
        draw_ui(i == g_slot_cur ? "assets/opt_w_00_on.png"
                                : "assets/opt_w_00_off.png", rx, y, rw, rh);
        if (i < SAVE_AUTO_N) {
            char icon[40];
            snprintf(icon, sizeof(icon), "assets/autosave0%d.png", i + 1);
            draw_ui(icon, rx + 10, y + 10, 60, 60);
        }
        txt_draw(g_winr, lab, tx, y + 6, 26,
                 si.used ? 0xFFFFFFFF : 0xFF8C8C8C, 150, 0);
        if (!si.used) {
            txt_draw(g_winr, "空槽", tx + 170, y + 10, 22, 0xFF8C8C8C, 0, 0);
        } else {
            str_cpy(when, sizeof(when), si.when);
            str_cpy(script, sizeof(script), si.script);
            str_cpy(prev, sizeof(prev), si.preview);
            clip_w(script, sizeof(script), 22, 250);
            clip_w(prev, sizeof(prev), 22, 600);
            txt_draw(g_winr, when, tx + 170, y + 10, 22, 0xFFBFBFBF, 0, 0);
            txt_draw(g_winr, script, tx + 340, y + 10, 22, 0xFFBFBFBF, 0, 0);
            txt_draw(g_winr, prev, tx, y + 44, 22, 0xFFFFFFFF, 0, 0);
        }
        /* Which button shows is what A will do, the same split the original
         * makes between 记录 and 覆盖. Autos stay read-only by design. */
        if (g_slot_mode) {
            if (si.used) draw_ui("assets/b02a_on.png", rx + rw - 190, y + 9, 160, 62);
        } else if (i >= SAVE_AUTO_N) {
            draw_ui(si.used ? "assets/b02c_on.png" : "assets/b02b_on.png",
                    rx + rw - 190, y + 9, 160, 62);
        }
    }
    {
        char cnt[24];
        snprintf(cnt, sizeof(cnt), "%d / %d", g_slot_cur + 1, SAVE_SLOT_N);
        txt_draw(g_winr, cnt, RL_W - 90, 14, 24, 0xFFBFBFBF, 0, 0);
        if (g_slot_msg[0])
            txt_draw(g_winr, g_slot_msg, rx, RL_H - 74, 24, 0xFFFFD080, 0, 0);
    }
}

/* ---- chapter chart (#26) --------------------------------------------------
 * The branch map ships as data: assets/script/C00_01.csv .. C07_01.csv. A file
 * is a grid - node lines alternate with a numeric line, and one line can hold
 * several comma separated columns, i.e. story lanes running in parallel. Each
 * node cell is 7 pipe separated fields:
 *
 *   5月12日(月)|scenario02_victim|Ｑ．是否在茅厕过夜|L03L08L19L20|select|2|1
 *   date          script             title               flagset   type  out b
 *
 * `title` is the same string the script writes with #L(title) (verified: every
 * Clist0N.txt line is a "script<TAB>title" pair taken from those labels), so a
 * node's visited state is derivable at runtime. `flagset` lists the fNN flags
 * the node depends on - the conditions of its own #SelectLimited options, which
 * is how Ｑ．是否在茅厕过夜 ends up carrying exactly f20&f19&f8&f3.
 *
 * What the numeric line means is NOT resolved: it holds one number per outgoing
 * branch (normal/end contribute 1, a select contributes one per branch, e.g.
 * "1,5,4,3" under a normal + a 3-way select), and the values are 5/4/3/2/1 -
 * consistent with connector geometry in a 60 px grid, but nothing in the APK
 * says so. The port therefore lays nodes out by (row, column), which is the
 * grid the CSV already encodes, and derives parent/child links from column
 * ranges instead of from that line.
 */
#define CHART_CH   8
#define CHART_MAX  320
#define CHART_KID  4

typedef struct {
    char date[24];
    char script[40];
    char title[72];
    int type;                       /* 0 normal, 1 select, 2 end */
    int out;                        /* field 5: the declared out-degree */
    int ch, row, col;
    int kid[CHART_KID];
    int nkid;
} ChartNode;

static ChartNode g_cnode[CHART_MAX];
static int g_cnode_n;
static int g_crow0[CHART_CH], g_crn[CHART_CH];   /* nodes per chapter, contiguous */
static Uint32 g_seen[(CHART_MAX + 31) / 32];
static int g_chart_quiet;                          /* suppress the per-node log line */
static int g_chapter;                            /* tab under the cursor */
static int g_chart_cur;                          /* node index under the cursor */
static int g_chart_top;                          /* first visible row */

static int chart_is_seen(int i)
{
    return (g_seen[i / 32] >> (i % 32)) & 1u;
}

static void chart_see(int i)
{
    g_seen[i / 32] |= 1u << (i % 32);
}

/* Mark the node the interpreter just walked into. `fname` is the script's
 * basename with .txt, `title` the #L() argument. Returns 1 when this call is
 * what lit the node up, 0 when it was already lit or no node matched. */
static int chart_mark(const char *fname, const char *title)
{
    char base[40];
    size_t bl;
    if (!g_cnode_n || !title || !*title) return 0;
    str_cpy(base, sizeof(base), fname ? fname : "");
    bl = strlen(base);
    if (bl > 4 && !strcmp(base + bl - 4, ".txt")) base[bl - 4] = 0;
    for (int i = 0; i < g_cnode_n; i++) {
        if (strcmp(g_cnode[i].title, title)) continue;
        if (base[0] && strcmp(g_cnode[i].script, base)) continue;
        if (chart_is_seen(i)) return 0;
        if (!g_chart_quiet)
            rl_log("chart: seen %s/%s (ch%d r%d c%d)",
                   g_cnode[i].script, g_cnode[i].title,
                   g_cnode[i].ch, g_cnode[i].row, g_cnode[i].col);
        chart_see(i);
        return 1;
    }
    return 0;
}

/* One CSV: '|' fields inside a cell, ',' between lanes. Lines with no '|' are
 * the numeric rows and only advance the row counter when a node line follows. */
static int chart_load_one(int ch)
{
    char rel[64], path[600], line[2048];
    int row = -1;

    snprintf(rel, sizeof(rel), "assets/script/C%02d_01.csv", ch);
    if (!res_resolve(path, sizeof(path), rel)) {
        rl_log("MISS chart %s", rel);
        return 0;
    }
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f)) {
        char *cells[8], *c, *nl;
        int nc = 0, had_node = 0;
        line[strcspn(line, "\r\n")] = 0;
        if (!strchr(line, '|')) continue;           /* the numeric line */
        row++;
        nl = line;
        while (nc < 8) {
            c = strchr(nl, ',');
            cells[nc++] = nl;
            if (!c) break;
            *c = 0;
            nl = c + 1;
        }
        for (int i = 0; i < nc; i++) {
            char *fld[8], *p = cells[i];
            int nf = 0;
            ChartNode *nd;
            while (nf < 8) {
                c = strchr(p, '|');
                fld[nf++] = p;
                if (!c) break;
                *c = 0;
                p = c + 1;
            }
            if (nf < 7) continue;                   /* prev|/next| link cells */
            if (g_cnode_n >= CHART_MAX) {
                rl_log("chart: %s truncated at %d nodes", rel, CHART_MAX);
                fclose(f);
                return row + 1;
            }
            nd = &g_cnode[g_cnode_n];
            memset(nd, 0, sizeof(*nd));
            str_cpy(nd->date, sizeof(nd->date), fld[0]);
            str_cpy(nd->script, sizeof(nd->script), fld[1]);
            str_cpy(nd->title, sizeof(nd->title), fld[2]);
            nd->type = !strcmp(fld[4], "select") ? 1 : (!strcmp(fld[4], "end") ? 2 : 0);
            nd->out = atoi(fld[5]);
            nd->ch = ch;
            nd->row = row;
            nd->col = i;
            had_node = 1;
            g_cnode_n++;
        }
        if (!had_node) row--;                       /* a lane-only line, no row used */
    }
    fclose(f);
    return row + 1;
}

/* The numeric line under each row is a per-edge connector-style list (1 straight,
 * 5 first branch, 3 last branch, 2/4 a middle one - it names a chrt_line0N
 * sprite, never a target), so the grid plus field 5 stays the link source. */
/* Hand the next row's cells out left to right, each parent taking the number of
 * children it declares in field 5. The older rule - "the cells from my column up
 * to my sibling's" - disagreed with the declared degree on 10 of the 254 nodes;
 * this leaves 6 short, all explained: three chapters hand their last row to the
 * next CSV (the `next|` line names those children) and three rows declare an
 * edge that skips a row. No node is left without a parent either way. */
static void chart_link(void)
{
    for (int c = 0; c < CHART_CH; c++) {
        int end = g_crow0[c] + g_crn[c];
        int i = g_crow0[c];
        while (i < end) {
            int j = i, k;
            for (j = i; j < end && g_cnode[j].row == g_cnode[i].row; j++) {}
            k = j;                    /* the parser appends by row, then by column */
            for (int p = i; p < j; p++) {
                ChartNode *q = &g_cnode[p];
                while (q->nkid < q->out && q->nkid < CHART_KID && k < end &&
                       g_cnode[k].row == q->row + 1)
                    q->kid[q->nkid++] = k++;
            }
            i = j;
        }
    }
}

static void chart_load(void)
{
    int at = 0;
    g_cnode_n = 0;
    for (int c = 0; c < CHART_CH; c++) {
        chart_load_one(c);
        g_crow0[c] = at;
        g_crn[c] = g_cnode_n - at;
        at = g_cnode_n;
    }
    chart_link();
    /* Six nodes take fewer children than they declare: three chapters hand their
     * last row to the next CSV (the `next|` line names those), three rows declare
     * an edge that skips a row. More than six means the parse broke, not the CSV. */
    int bad = 0;
    for (int i = 0; i < g_cnode_n; i++)
        if (g_cnode[i].nkid != g_cnode[i].out) bad++;
    rl_log("chart: %d nodes over %d chapters, degree short %d", g_cnode_n, CHART_CH, bad);
}

static int chart_now_node(void);

/* Put the cursor on the node the player is standing in, so opening the chart
 * answers "where am I" without a scroll. */
static void chart_focus(void)
{
    int now = chart_now_node();
    if (now >= 0) {
        g_chapter = g_cnode[now].ch;
        g_chart_cur = now;
        return;
    }
    for (int i = 0; i < g_cnode_n; i++)
        if (g_cnode[i].ch == g_chapter) { g_chart_cur = i; return; }
    g_chart_cur = 0;
}

/* chrt_line01/02 are 60x60 seamless 10px bars (vertical ink x25..35 over the
 * full height, horizontal ink y25..35 over the full width), so tiling them 1:1
 * gives the original's connector weight; a solid rect read as a hairline. */
static void chart_tile(const char *rel, int x, int y, int len, int vert,
                       Uint32 rgb, int alpha)
{
    char path[600];
    SDL_Texture *t;
    if (len <= 0) return;
    if (!res_resolve(path, sizeof(path), rel)) {
        rl_log("MISS ui %s", rel);
        return;
    }
    t = img_get(g_winr, path, NULL, NULL);
    if (!t) return;
    SDL_SetTextureColorMod(t, (Uint8)(rgb >> 16), (Uint8)(rgb >> 8), (Uint8)rgb);
    SDL_SetTextureAlphaMod(t, (Uint8)alpha);
    for (int o = 0; o < len; o += 60) {
        int n = len - o > 60 ? 60 : len - o;
        SDL_Rect s = { 0, 0, vert ? 60 : n, vert ? n : 60 };
        SDL_Rect d = { vert ? x : x + o, vert ? y + o : y, vert ? 60 : n, vert ? n : 60 };
        SDL_RenderCopy(g_winr, t, &s, &d);
    }
    SDL_SetTextureColorMod(t, 255, 255, 255);
    SDL_SetTextureAlphaMod(t, 255);
}

static void draw_chart(void)
{
    /* chrt_l00_* is 370x100 with a "▶ 详细分支" tab baked into its bottom-left
     * corner (ink up to x=130), so the plate keeps the source aspect and the
     * date goes to the free bottom-right; pw=280 puts that tab edge at x=98.
     * Connectors are solid rects, not chrt_line: that art is a 440x30 horizontal
     * bar and squashing it into a 30px-wide vertical run rendered as "==". */
    const int pw = 280, ph = 76, colw = 292, pitch = 104, x0 = 56, y0 = 104;
    const int visr = (RL_H - y0 - 44) / pitch;
    static const char *num[] = { "一", "二", "三", "四", "五", "六", "七", "八" };
    int maxrow = 0, seen = 0;
    int now = chart_now_node();
    char lab[48];

    for (int i = g_crow0[g_chapter]; i < g_crow0[g_chapter] + g_crn[g_chapter]; i++) {
        if (g_cnode[i].row > maxrow) maxrow = g_cnode[i].row;
        if (chart_is_seen(i)) seen++;
    }
    if (g_chart_top > maxrow - visr + 1) g_chart_top = maxrow - visr + 1;
    if (g_chart_top < 0) g_chart_top = 0;
    /* Keep the cursor on screen after a chapter switch. */
    if (g_crn[g_chapter] > 0) {
        int cr = g_cnode[g_chart_cur].row;
        if (cr < g_chart_top) g_chart_top = cr;
        else if (cr > g_chart_top + visr - 1) g_chart_top = cr - visr + 1;
        if (g_chart_top < 0) g_chart_top = 0;
    }

    txt_draw(g_winr, "章节图表", 20, 14, 30, 0xFFFFFFFF, 0, 0);
    for (int c = 0; c < CHART_CH; c++) {
        int tx = 190 + c * 104;
        snprintf(lab, sizeof(lab), "第%s章", num[c]);
        txt_draw(g_winr, lab, tx, 18, c == g_chapter ? 28 : 22,
                 c == g_chapter ? 0xFFFFFFFF : 0xFF8C8C8C, 0, 0);
    }
    snprintf(lab, sizeof(lab), "%d / %d 已见", seen, g_crn[g_chapter]);
    txt_draw(g_winr, lab, RL_W - 190, 18, 22, 0xFFBFBFBF, 0, 0);

    /* Connectors first, so the plates sit on top of them. */
    for (int i = g_crow0[g_chapter]; i < g_crow0[g_chapter] + g_crn[g_chapter]; i++) {
        ChartNode *p = &g_cnode[i];
        int py = y0 + (p->row - g_chart_top) * pitch;
        for (int k = 0; k < p->nkid; k++) {
            ChartNode *q = &g_cnode[p->kid[k]];
            int qy = y0 + (q->row - g_chart_top) * pitch;
            int ax = x0 + p->col * colw + pw / 2, bx = x0 + q->col * colw + pw / 2;
            int seen = chart_is_seen(i) && chart_is_seen(p->kid[k]);
            int col = seen ? 0x2080E0 : 0xFFFFFF, alp = seen ? 255 : 80;
            int mid, y1, y2;
            if (py + ph > RL_H || qy < y0 - pitch) continue;
            mid = py + ph + (pitch - ph) / 2;
            y1 = py + ph;
            y2 = qy;
            chart_tile("assets/chrt_line01.png", ax - 30, y1, mid - y1, 1, col, alp);
            if (bx != ax)
                chart_tile("assets/chrt_line02.png", ax < bx ? ax : bx, mid - 30,
                           ax < bx ? bx - ax : ax - bx, 0, col, alp);
            chart_tile("assets/chrt_line01.png", bx - 30, mid, y2 - mid, 1, col, alp);
        }
    }
    for (int i = g_crow0[g_chapter]; i < g_crow0[g_chapter] + g_crn[g_chapter]; i++) {
        ChartNode *p = &g_cnode[i];
        int x = x0 + p->col * colw, y = y0 + (p->row - g_chart_top) * pitch;
        int on = chart_is_seen(i);
        char ttl[80];
        int dw;
        if (y + ph < y0 - 10 || y > RL_H - 30) continue;
        if (i == g_chart_cur) draw_ui("assets/opt_w_00_on.png", x - 8, y - 6, pw + 16, ph + 12);
        draw_ui_a(on ? "assets/chrt_l00_on.png" : "assets/chrt_l00_off.png",
                  x, y, pw, ph, on ? 255 : 165);
        str_cpy(ttl, sizeof(ttl), p->title);
        clip_w(ttl, sizeof(ttl), 20, pw - 24);
        txt_draw(g_winr, ttl, x + 12, y + 10, 20,
                 p->type == 2 ? 0xFFFFD080 : (on ? 0xFFFFFFFF : 0xFF8C8C8C), pw - 24, 0);
        /* C00's tutorial row carries the literal "none" in the date column —
         * the original shows no date there, not the word. */
        dw = (!p->date[0] || !strcmp(p->date, "none")) ? 0 : txt_measure(p->date, 16);
        if (dw)
            txt_draw(g_winr, p->date, x + pw - 12 - dw, y + 46, 16,
                     on ? 0xFFD8D8D8 : 0xFF7F7F7F, 0, 0);
        if (p->type == 1)
            txt_draw(g_winr, "分支", x + 106, y + 46, 16, 0xFF9FD0FF, 0, 0);
        /* "where am I": the #L() the interpreter last walked over, in the script
         * it is standing in now. chrt_nowpoint01 is 40x40 with ink only at
         * x 9..30 / y 8..36 (blue 18,116,162) => a marker, not a plate. */
        if (i == now)
            draw_ui("assets/chrt_nowpoint01.png", x - 44, y + ph / 2 - 20, 40, 40);
    }
    snprintf(lab, sizeof(lab), "方向键 换章/换节点   B 返回");
    txt_draw(g_winr, lab, 20, RL_H - 34, 22, 0xFFBFBFBF, 0, 0);
}

/* Node the player is standing in, or -1: the current #L() title matched against
 * the open script. Recomputed per draw because a #L() can fire under the chart. */
static int chart_now_node(void)
{
    char base[40];
    size_t bl;
    if (!g_sp || !g_log_node[0]) return -1;
    str_cpy(base, sizeof(base), g_stack[g_sp - 1].s->fname);
    bl = strlen(base);
    if (bl > 4 && !strcmp(base + bl - 4, ".txt")) base[bl - 4] = 0;
    for (int i = 0; i < g_cnode_n; i++) {
        if (!strcmp(g_cnode[i].title, g_log_node) &&
            (!base[0] || !strcmp(g_cnode[i].script, base)))
            return i;
    }
    return -1;
}

/* Bring the story up: continue the player's save when there is one, otherwise
 * start the default scenario. Returns 1 on resume, 0 on fresh start, -1 on a
 * load failure the caller cannot recover from. */
static int game_begin(int prefer_resume)
{
    char spath[600];
    if (prefer_resume && save_load()) {
        log_read_disk();
        return 1;
    }
    /* a fresh start reads as a fresh backlog, not as last playthrough's */
    g_log_n = 0;
    g_log_node[0] = 0;
    if (!res_script(spath, sizeof(spath), g_script)) {
        rl_log("start script not found: %s", g_script);
        return -1;
    }
    char err[256];
    RlScript *s0 = scr_load(spath, err, sizeof(err));
    if (!s0) { rl_log("parse failed: %s", err); return -1; }
    rl_log("start %s lines=%d labels=%d", s0->fname, s0->n, s0->nl);
    g_stack[g_sp].s = s0;
    g_stack[g_sp].pc = 0;
    g_sp++;
    return 0;
}

/* "f4&f20&d31" / "-1" -> 1 when every referenced C flag is set. */
static int cond_ok(const char *c)
{
    if (!c || !*c || !strcmp(c, "-1")) return 1;
    char buf[64];
    snprintf(buf, sizeof(buf), "%s", c);
    char *save = NULL;
    for (char *tok = strtok_r(buf, "&", &save); tok; tok = strtok_r(NULL, "&", &save)) {
        if (*tok != 'f' && *tok != 'd') return 1;
        int id = atoi(tok + 1);
        if (id < 0 || id >= RL_FLAG_MAX || g_c[id] == 0) return 0;
    }
    return 1;
}

static int if_true(const RlLine *l)
{
    const char *op = l->argc > 0 ? l->argv[0] : "==";
    int k = argi(l, 1, 0);
    if (!strcmp(op, "==")) return g_cmp == k;
    if (!strcmp(op, "!=")) return g_cmp != k;
    if (!strcmp(op, ">=")) return g_cmp >= k;
    if (!strcmp(op, "<=")) return g_cmp <= k;
    if (!strcmp(op, ">"))  return g_cmp > k;
    if (!strcmp(op, "<"))  return g_cmp < k;
    rl_log("unknown #If op '%s'", op);
    return g_cmp == k;
}

static void step_transition(void)
{
    if (g_v.fade == g_v.fade_to) return;
    float d = 0.12f;
    if (g_v.fade < g_v.fade_to) {
        g_v.fade += d;
        if (g_v.fade > g_v.fade_to) g_v.fade = g_v.fade_to;
    } else {
        g_v.fade -= d;
        if (g_v.fade < g_v.fade_to) g_v.fade = g_v.fade_to;
    }
}

static void set_bg(int id)
{
    char path[600];
    if (res_bg(path, sizeof(path), id)) {
        snprintf(g_v.bg, sizeof(g_v.bg), "%s", path);
        g_v.has_color = 0;
    } else {
        g_v.bg[0] = 0;
    }
}

static void run_cmd(RlLine *l)
{
    const char *n = l->name;
    Frame *f = &g_stack[g_sp - 1];

    if (!strcmp(n, "ImgBack")) { set_bg(argi(l, 0, 0)); return; }
    if (!strcmp(n, "ImgBackOff")) { g_v.bg[0] = 0; return; }
    if (!strcmp(n, "BgColor")) {
        g_v.bgcolor = ((Uint32)argi(l, 0, 0) << 16) | ((Uint32)argi(l, 1, 0) << 8)
            | (Uint32)argi(l, 2, 0);
        g_v.has_color = 1;
        return;
    }
    if (!strcmp(n, "WipeIn")) { g_v.fade_to = 0.f; return; }
    if (!strcmp(n, "WipeOut")) { g_v.fade_to = 1.f; return; }
    if (!strcmp(n, "ImgBustAdv")) {
        RlSlot *sl = slot_get(argi(l, 0, 0));
        if (!sl) return;
        sl->used = 1;
        sl->bustId = argi(l, 1, 1);
        sl->x = argi(l, 2, 0);
        sl->y = argi(l, 3, 0);
        sl->anchor = argi(l, 4, 0) == 15 ? 1 : 0;
        if (sl->scale == 0) sl->scale = 100;
        if (sl->alpha == 0) sl->alpha = 100;
        return;
    }
    if (!strcmp(n, "ImgBustXY")) {
        /* #ImgBustXY(slot, ms, x, y, 0) - slide the layer to x,y over ms. */
        RlSlot *sl = slot_get(argi(l, 0, 0));
        if (!sl) return;
        int ms = argi(l, 1, 0);
        SlotMove *m = &g_move[argi(l, 0, 0)];
        if (ms > 0) {
            m->active = 1;
            m->from_x = sl->x;
            m->from_y = sl->y;
            m->to_x = argi(l, 2, sl->x);
            m->to_y = argi(l, 3, sl->y);
            m->left = ms / 33;
            if (m->left < 1) m->left = 1;
        } else {
            sl->x = argi(l, 2, sl->x);
            sl->y = argi(l, 3, sl->y);
        }
        return;
    }
    if (!strcmp(n, "ImgScale")) {
        RlSlot *sl = slot_get(argi(l, 0, 0));
        if (sl) sl->scale = argi(l, 1, 100);
        return;
    }
    if (!strcmp(n, "ImgAlpha")) {
        RlSlot *sl = slot_get(argi(l, 0, 0));
        if (sl) sl->alpha = argi(l, 1, 100);
        return;
    }
    if (!strcmp(n, "ImgBustOffAdv")) { slot_clear(argi(l, 0, 0)); return; }
    if (!strcmp(n, "ImgFaceAdv")) {
        RlSlot *sl = slot_get(argi(l, 0, 0));
        if (!sl) return;
        sl->brow = argi(l, 1, -1);
        sl->eye = argi(l, 2, -1);
        sl->mouth = argi(l, 3, -1);
        sl->faceover = argi(l, 4, 0);
        sl->emo = argi(l, 5, -1);
        return;
    }
    if (!strcmp(n, "ImgItem")) {
        RlSlot *sl = slot_get(argi(l, 0, 0));
        int k = argi(l, 1, 0);
        if (sl && k >= 1 && k <= 8) sl->item[k] = argi(l, 2, 0) ? 1 : 0;
        return;
    }
    if (!strcmp(n, "MesWin")) { g_v.mesvis = argi(l, 0, 0) == 0 ? 1 : 0; return; }
    if (!strcmp(n, "MesColor")) { g_v.mescolor = argi(l, 0, 0); return; }
    if (!strcmp(n, "MesName")) {
        if (l->argc >= 1 && l->argv[0] && *l->argv[0]) {
            const char *a = l->argv[0];
            size_t o = 0;
            for (const char *q = a; *q; q++) {
                if (q[0] == '@' && q[1] == 's') {
                    if (o + 3 < sizeof(g_v.name)) {
                        g_v.name[o++] = (char)0xE3;
                        g_v.name[o++] = (char)0x80;
                        g_v.name[o++] = (char)0x80;   /* U+3000 ideographic space */
                    }
                    q++;
                    continue;
                }
                if (o + 4 < sizeof(g_v.name)) g_v.name[o++] = *q;
            }
            g_v.name[o] = 0;
        } else g_v.name[0] = 0;
        return;
    }
    if (!strcmp(n, "MesSta")) {
        g_v.text[0] = 0;
        reveal_reset();
        g_shown_lines = 0;
        if (l->argc >= 1 && l->argv[0] && *l->argv[0]) {
            int sid = argi(l, 1, 0);
            int row = (sid >= 1 && sid <= 19) ? g_sid_row[sid] : VO_ROW_N;
            char vp[600];
            g_vo_dbg = row;
            if (res_voice(vp, sizeof(vp), l->argv[0]))
                aud_voice(vp, g_vo_gain[g_vo[row - 1]]);
        }
        g_await = 0;
        return;
    }
    if (!strcmp(n, "MesEnd")) {
        if (rl_verbose) rl_log("MesEnd textlen=%zu lines=%d", strlen(g_v.text), g_shown_lines);
        g_await = 1;
        g_await_ct = 0;
        log_push();
        /* every message end is a safe place to leave off, so it is also where an
         * ADV keeps its checkpoint - throttled because the card is exFAT */
        if (!g_save_at || SDL_GetTicks() - g_save_at > SAVE_STEP_MS) save_write("autosave", SAVE_FILE);
        return;
    }
    if (!strcmp(n, "Bgm")) {
        char sp[600];
        if (res_sound(sp, sizeof(sp), "BGM", argi(l, 0, 0))) aud_bgm(sp, 0);
        return;
    }
    if (!strcmp(n, "BgmIn")) {
        char sp[600];
        if (res_sound(sp, sizeof(sp), "BGM", argi(l, 0, 0))) aud_bgm(sp, 2000);
        return;
    }
    if (!strcmp(n, "BgmOut")) { aud_fade_bgm(1000); return; }
    if (!strcmp(n, "BgmStop")) { aud_stop_bgm(); return; }
    if (!strcmp(n, "Se")) {
        char sp[600];
        if (res_sound(sp, sizeof(sp), "SE", argi(l, 0, 0))) aud_se(sp);
        return;
    }
    if (!strcmp(n, "Wait")) { g_wait = argi(l, 0, 0); return; }
    if (!strcmp(n, "CmpC")) { g_cmp = argi(l, 0, 0) < RL_FLAG_MAX ? g_c[argi(l, 0, 0)] : 0; return; }
    if (!strcmp(n, "CmpS")) { g_cmp = argi(l, 0, 0) < RL_FLAG_MAX ? g_s[argi(l, 0, 0)] : 0; return; }
    if (!strcmp(n, "FlagCON") || !strcmp(n, "FlagSON")) {
        int id = argi(l, 0, -1);
        if (id >= 0 && id < RL_FLAG_MAX) {
            if (n[4] == 'C') g_c[id] = 1; else g_s[id] = 1;
        }
        return;
    }
    if (!strcmp(n, "FlagCOFF") || !strcmp(n, "FlagSOFF")) {
        int id = argi(l, 0, -1);
        if (id >= 0 && id < RL_FLAG_MAX) {
            if (n[4] == 'C') g_c[id] = 0; else g_s[id] = 0;
        }
        return;
    }
    if (!strcmp(n, "If")) {
        if (!if_true(l)) {
            int t = l->endif >= 0 ? l->endif : l->elsenext;
            f->pc = t + 1;
            return;
        }
        return;
    }
    if (!strcmp(n, "Else")) {
        int ifi = l->elsenext;
        if (ifi >= 0 && ifi < f->s->n) f->pc = f->s->lines[ifi].elsenext + 1;
        return;
    }
    if (!strcmp(n, "Save")) { save_write("#Save", SAVE_FILE); return; }
    if (!strcmp(n, "L")) {
        /* The backlog groups by story node, and #L(name) is where the node's own
         * title comes from — the same strings the C0x_yy.csv chapter tables use. */
        if (l->argc >= 1 && l->argv[0] && *l->argv[0]) {
            str_cpy(g_log_node, sizeof(g_log_node), l->argv[0]);
            chart_mark(g_sp ? g_stack[g_sp - 1].s->fname : "", l->argv[0]);
        }
        return;
    }
    if (!strcmp(n, "EndIf") || !strcmp(n, "data")
        || !strcmp(n, "Analytics") || !strcmp(n, "MesType") || !strcmp(n, "Flash")
        || !strcmp(n, "Vib") || !strcmp(n, "Quake") || !strcmp(n, "ImgAnim")
        || !strcmp(n, "Staff")) {
        if (rl_verbose) rl_log("cmd %s(%s) ignored", n, l->argc ? l->argv[0] : "");
        return;
    }
    if (!strcmp(n, "Select")) {
        g_select = 1;
        g_sel_lim = 0;
        g_sel_n = 0;
        g_sel_block = 1;
        int want = argi(l, 0, 0);
        if (want > (int)(sizeof(g_sel_label) / sizeof(g_sel_label[0])))
            want = (int)(sizeof(g_sel_label) / sizeof(g_sel_label[0]));
        for (int i = 1; i < l->argc && g_sel_n < want; i++) {
            snprintf(g_sel_label[g_sel_n], sizeof(g_sel_label[0]), "%s", l->argv[i]);
            g_sel_cond[g_sel_n][0] = 0;
            g_sel_n++;
        }
        g_sel_cur = 0;
        return;
    }
    if (!strcmp(n, "SelectLimited")) {
        g_select = 1;
        g_sel_lim = 1;
        g_sel_block = 1;
        g_sel_n = 0;
        int want = argi(l, 0, 0);
        int cap = (int)(sizeof(g_sel_label) / sizeof(g_sel_label[0]));
        if (want > cap) want = cap;
        for (int i = 1; i + 1 < l->argc && g_sel_n < want; i += 2) {
            snprintf(g_sel_label[g_sel_n], sizeof(g_sel_label[0]), "%s", l->argv[i]);
            snprintf(g_sel_cond[g_sel_n], sizeof(g_sel_cond[0]), "%s", l->argv[i + 1]);
            if (!cond_ok(g_sel_cond[g_sel_n])) g_sel_cond[g_sel_n][0] = '0';
            g_sel_n++;
        }
        g_sel_cur = 0;
        while (g_sel_cur < g_sel_n - 1 && g_sel_cond[g_sel_cur][0] == '0') g_sel_cur++;
        return;
    }
    if (!strcmp(n, "JumpC")) {
        int idx = argi(l, 0, -1);
        if (!g_select && idx == g_cmp) jump_label(l->argv[1] ? l->argv[1] : "");
        return;
    }
    if (!strcmp(n, "Jump")) { jump_label(l->argv[0] ? l->argv[0] : ""); return; }
    if (!strcmp(n, "JumpF")) {
        load_script(l->argv[0] ? l->argv[0] : "");
        if (g_sp && l->argc > 1 && l->argv[1] && *l->argv[1]) jump_label(l->argv[1]);
        return;
    }
    if (!strcmp(n, "Ret")) {
        if (g_sp > 1) { RlScript *dead = g_stack[--g_sp].s; scr_free(dead); }
        else g_quit = 1;
        return;
    }
    if (!strcmp(n, "End")) { rl_log("#End in %s", f->s->fname); g_quit = 1; return; }
    if (!strcmp(n, "Movie")) {
        rl_log("#Movie(%s) not implemented: waiting for a key", l->argc ? l->argv[0] : "");
        g_v.bg[0] = 0;
        g_v.has_color = 1;
        g_v.bgcolor = 0;
        g_await = 1;
        return;
    }
    rl_log("TODO cmd #%s(%s) at %s:%d", n, l->argv[0] ? l->argv[0] : "", f->s->fname, f->pc);
}

/* One-shot framebuffer dump. The handheld has no ffmpeg/kmsgrab, so the loader
 * reads back its own 1280x720 target when RL_SNAP names a file; this is how the
 * picture gets verified without asking for the screen to be photographed. */
static void save_snapshot(void)
{
    const char *path = getenv("RL_SNAP");
    const char *dir = getenv("RL_SNAP_DIR");
    int every = getenv("RL_SNAP_EVERY") ? atoi(getenv("RL_SNAP_EVERY")) : 0;
    int at = getenv("RL_SNAP_AT") ? atoi(getenv("RL_SNAP_AT")) : 60;
    char buf[600];
    if (dir && *dir && every > 0) {
        if (g_frames % (Uint32)every) return;
        snprintf(buf, sizeof(buf), "%s/shot%04u_f%u.bmp", dir,
                 g_frames / (Uint32)every, g_frames);
        path = buf;
    } else if (path && *path) {
        if (g_frames != (Uint32)at) return;
    } else {
        return;
    }
    Uint32 *px = malloc((size_t)RL_W * RL_H * 4);
    if (!px) return;
    SDL_SetRenderTarget(g_winr, g_target);
    int rc = SDL_RenderReadPixels(g_winr, NULL, SDL_PIXELFORMAT_ARGB8888, px,
                                  RL_W * 4);
    SDL_SetRenderTarget(g_winr, NULL);
    if (rc == 0) {
        SDL_Surface *s = SDL_CreateRGBSurfaceWithFormatFrom(px, RL_W, RL_H, 32,
                                                           RL_W * 4,
                                                           SDL_PIXELFORMAT_ARGB8888);
        if (s) {
            if (SDL_SaveBMP(s, path) != 0) rl_log("snapshot failed: %s", SDL_GetError());
            else rl_log("snapshot written %s", path);
            SDL_FreeSurface(s);
        }
    } else {
        rl_log("RenderReadPixels failed: %s", SDL_GetError());
    }
    free(px);
}

/* ---- text reveal ---------------------------------------------------------
 * The original engine types a message out one character at a time and the
 * scripts carry no timing for it, so the rate is a port setting (RL_TEXT_CPS).
 * g_v.text holds the whole message as soon as its lines arrive; only its first
 * g_reveal_bytes bytes are drawn, through g_disp. */

static int utf8_len(const char *p)
{
    unsigned char c = (unsigned char)*p;
    if (c >= 0xF0) return 4;
    if (c >= 0xE0) return 3;
    if (c >= 0xC0) return 2;
    return 1;
}

/* Length of the markup token at p (%coN %coe %hoN %hoe %dot(..) %r), or 0.
 * A token is revealed in one step, so the cut can never land inside "%co2" and
 * make txt_draw print the raw tag for a frame. */
static int markup_len(const char *p)
{
    if (p[0] != '%') return 0;
    if (!strncmp(p, "%dot(", 5)) {
        const char *e = strchr(p, ')');
        return e ? (int)(e - p + 1) : 5;
    }
    if (!strncmp(p, "%co", 3) || !strncmp(p, "%ho", 3)) return 4;
    if (!strncmp(p, "%r", 2)) return 2;
    return 0;
}

static void reveal_sync(void)
{
    size_t len = strlen(g_v.text);
    if ((size_t)g_reveal_bytes > len) g_reveal_bytes = (int)len;
    memcpy(g_disp, g_v.text, (size_t)g_reveal_bytes);
    g_disp[g_reveal_bytes] = 0;
}

static void reveal_reset(void)
{
    g_reveal_bytes = 0;
    g_reveal_acc = 0;
    reveal_sync();
}

static int reveal_pending(void)
{
    return g_reveal_bytes < (int)strlen(g_v.text);
}

static void reveal_step(void)
{
    g_reveal_acc += g_text_cps;
    while (g_reveal_acc >= 30 && reveal_pending()) {
        g_reveal_acc -= 30;
        int tk = markup_len(g_v.text + g_reveal_bytes);
        g_reveal_bytes += tk ? tk : utf8_len(g_v.text + g_reveal_bytes);
    }
    if (g_reveal_acc > 60) g_reveal_acc = 60;
    reveal_sync();
}

static void reveal_finish(void)
{
    g_reveal_bytes = (int)strlen(g_v.text);
    g_reveal_acc = 0;
    reveal_sync();
}

static void tick(void)
{
    RlLine *l;
    if (!cur_line(&l)) {
        rl_log("quit: script ended (pc out of range)");
        g_quit = 1;
        return;
    }
    Frame *f = &g_stack[g_sp - 1];

    if (g_jump_sel && g_select) g_jump_sel = 0;   /* the choice we were skipping to */
    step_transition();
    reveal_step();
    if (g_save_toast) g_save_toast--;
    for (int i = 1; i < RL_SLOT_MAX; i++) {
        SlotMove *m = &g_move[i];
        if (!m->active) continue;
        RlSlot *sl = &g_v.slot[i];
        if (m->left <= 0) { sl->x = m->to_x; sl->y = m->to_y; m->active = 0; continue; }
        int t = m->left;
        sl->x = m->to_x + (sl->x - m->to_x) * (t - 1) / t;
        sl->y = m->to_y + (sl->y - m->to_y) * (t - 1) / t;
        m->left--;
    }
    if (g_wait > 0) { g_wait--; return; }
    if (g_v.fade != g_v.fade_to) return;
    if (g_await) {
        /* dwell frames before the message turns by itself: the harness knob
         * wins, then 快进, then the 自动推进 setting */
        int dwell = g_autop ? g_autop
                : g_skip_on ? 2
                : g_auto_on ? g_opt_auto[g_opt[OPT_AUTO]] : 0;
        if (g_jump_sel) {
            if (g_select) g_jump_sel = 0;
            else if (++g_jump_ct > 30 * 600) {
                g_jump_sel = 0;
                rl_log("skip-to-choice: gave up after 600 s");
            } else dwell = 2;
        }
        if (reveal_pending()) {
            if (!dwell) return;
            reveal_finish();
        }
        if (dwell && ++g_await_ct >= dwell) { g_await_ct = 0; advance(0); }
        else return;
    }
    if (g_select && g_sel_block) {
        if (g_autop > 0 && ++g_sel_ct >= g_autop) { g_sel_ct = 0; advance(0); }
        else return;
    }

    if (l->kind == RL_TEXT) {
        size_t cur = strlen(g_v.text);
        if (cur + strlen(l->text) + 2 < sizeof(g_v.text)) {
            if (cur) g_v.text[cur++] = '\n';
            memcpy(g_v.text + cur, l->text, strlen(l->text) + 1);
        }
        g_shown_lines++;
        if (rl_verbose) rl_log("text line total=%zu", strlen(g_v.text));
        f->pc++;
        return;
    }
    f->pc++;
    run_cmd(l);
}

/* ---- options and the in-game menu --------------------------------------- */

#define OPT_FILE "ragingloop.cfg"

static void opt_apply(void)
{
    g_text_cps = g_opt_cps[g_opt[OPT_CPS]];
    g_text_px  = g_opt_fontpx[g_opt[OPT_FONTPX]];
    g_name_px  = g_text_px - 4;
    g_box_a    = g_opt_shade[g_opt[OPT_SHADE]];
    aud_set_gain(g_opt_gain[g_opt[OPT_BGM]], g_opt_gain[g_opt[OPT_SE]],
                 g_opt_gain[g_opt[OPT_VOICE]]);
    if (rl_verbose)
        rl_log("opt: cps=%d auto=%d font=%d shade=%02x bgm=%d se=%d vo=%d",
               g_text_cps, g_opt_auto[g_opt[OPT_AUTO]], g_text_px, g_box_a,
               g_opt[OPT_BGM], g_opt[OPT_SE], g_opt[OPT_VOICE]);
}

static void opt_save(void)
{
    char dir[600], to[600], tmp[640];
    if (!g_write_ok) return;
    if (!save_dir(dir, sizeof(dir))) return;
    snprintf(to, sizeof(to), "%s/%s", dir, OPT_FILE);
    snprintf(tmp, sizeof(tmp), "%s.tmp", to);
    FILE *f = fopen(tmp, "wb");
    if (!f) { rl_log("opt: create %s: %s", tmp, strerror(errno)); return; }
    for (int i = 0; i < OPT_N; i++) fprintf(f, "opt %d %d\n", i, g_opt[i]);
    for (int i = 0; i < VO_ROW_N; i++) fprintf(f, "vo %d %d\n", i, g_vo[i]);
    /* 按键配置: assign on load, and only the bound rows are written, so the
     * default of "nothing bound" needs no line at all. */
    for (int i = 0; i < MENU_N; i++)
        if (g_key[i]) fprintf(f, "key %d %d\n", i, g_key[i]);
    /* Chapter-chart visited bits, keyed by node index in g_cnode. Only non-zero
     * words are written, so a fresh cfg stays as small as before.
     * Index-keyed means a changed CSV can move a bit onto a different node -
     * accepted: the chart is a memory aid, not save state. */
    for (int i = 0; i < (int)(sizeof(g_seen) / sizeof(g_seen[0])); i++)
        if (g_seen[i]) fprintf(f, "seen %d %08x\n", i, g_seen[i]);
    fprintf(f, "flags %d %d %d\n", g_auto_on, g_skip_on, g_box_hidden);
    fclose(f);
    if (rename(tmp, to) != 0) { rl_log("opt: rename %s: %s", to, strerror(errno)); remove(tmp); }
}

static void opt_load(void)
{
    char dir[600], to[600], line[256];
    if (!save_dir(dir, sizeof(dir))) return;
    snprintf(to, sizeof(to), "%s/%s", dir, OPT_FILE);
    FILE *f = fopen(to, "rb");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        int a, b;
        unsigned bits;
        if (sscanf(line, "opt %d %d", &a, &b) == 2 && a >= 0 && a < OPT_N && b >= 0 && b <= 4)
            g_opt[a] = b;
        else if (sscanf(line, "vo %d %d", &a, &b) == 2 && a >= 0 && a < VO_ROW_N && b >= 0 && b <= 2)
            g_vo[a] = b;
        else if (sscanf(line, "key %d %d", &a, &b) == 2 && a >= 0 && a < MENU_N &&
                   b > K_NONE && b < KEY_V_N)
            g_key[a] = b;
        else if (sscanf(line, "seen %d %x", &a, &bits) == 2 && a >= 0 &&
                 a < (int)(sizeof(g_seen) / sizeof(g_seen[0])))
            /* OR, not assign: game_begin() already backfilled the chart from the
             * backlog, and this runs after it. */
            g_seen[a] |= (Uint32)bits;
        else if (sscanf(line, "flags %d %d %d", &a, &b, &g_box_hidden) == 3) {
            g_auto_on = a != 0;
            g_skip_on = b != 0;
        }
    }
    fclose(f);
    rl_log("opt: loaded %s", to);
}

static void menu_close(void)
{
    g_menu = 0;
    opt_save();
}

/* Run one quick-list action. Split out of menu_confirm so a 按键配置 binding can
 * fire the same row from the story view, with the menu closed. */
static void menu_act(int row)
{
    switch (row) {
    case M_RESUME: menu_close(); break;
    case M_OPT: g_menu = 2; g_opt_cur = 0; break;
    case M_AUTO: g_auto_on = !g_auto_on; break;
    case M_SKIP: g_skip_on = !g_skip_on; break;
    case M_JUMPSEL:
        if (!g_select) { g_jump_sel = 1; g_jump_ct = 0; }
        menu_close();
        break;
    case M_HIDE: g_box_hidden = !g_box_hidden; break;
    case M_LOG:
        g_menu = 0;
        backlog_show();
        break;
    case M_CHART:
        if (!g_cnode_n) break;
        g_menu = 5;
        chart_focus();
        break;
    case M_SAVE:
    case M_LOAD:
        g_menu = 4;
        g_slot_mode = row == M_LOAD;
        g_slot_msg[0] = 0;
        break;
    case M_TITLE: menu_close(); game_reset(); break;
    default: break;
    }
    /* A toggle leaves the list open, so the flag is saved here; the callers that
     * fire an action from the story view save it themselves. */
    if (g_menu == 1) opt_save();
}

static void menu_confirm(void)
{
    menu_act(g_menu_cur);
}

/* Fire the first action bound to pad-button slot i (a K_A..K_START value - 1).
 * Returns 1 when a binding consumed the press, so the caller skips its own
 * handling of that button. */
static int key_fire_slot(int i)
{
    for (int r = M_OPT; r < MENU_N; r++) {
        if (g_key[r] != i + 1) continue;
        rl_log("key: %s -> %s", g_key_name[i + 1], g_menu_name[r]);
        menu_act(r);
        opt_save();
        return 1;
    }
    return 0;
}

static void opt_adjust(int delta)
{
    int v = g_opt[g_opt_cur] + delta;
    if (v < 0) v = 0;
    if (v > 4) v = 4;
    if (v == g_opt[g_opt_cur]) return;
    g_opt[g_opt_cur] = v;
    opt_apply();
    opt_save();
}

/* Overlay navigation. vy is the up/down axis, hx the left/right one; both are
 * -1/0/+1. Returns 1 when the input belonged to the menu, so the caller skips
 * the ADV advance for it. */
static int menu_nav(int vy, int hx, int confirm, int back)
{
    if (g_menu == 1) {
        if (back) { menu_close(); return 1; }
        if (confirm) { menu_confirm(); return 1; }
        if (vy) { g_menu_cur = (g_menu_cur + vy + MENU_N) % MENU_N; return 1; }
        return 0;
    }
    if (g_menu == 4) {
        char leaf[40], lab[24];
        SlotInfo si;
        if (back) { g_menu = 1; g_slot_msg[0] = 0; return 1; }
        if (vy) {
            g_slot_cur = (g_slot_cur + vy + SAVE_SLOT_N) % SAVE_SLOT_N;
            g_slot_msg[0] = 0;
            return 1;
        }
        if (!confirm) return 0;
        slot_leaf(leaf, sizeof(leaf), g_slot_cur);
        slot_label(lab, sizeof(lab), g_slot_cur);
        slot_probe(g_slot_cur, &si);
        if (!g_slot_mode) {
            /* Autos belong to the engine's own rotation; letting the player
             * overwrite one from here would only lose the newest of them. */
            if (g_slot_cur < SAVE_AUTO_N) {
                snprintf(g_slot_msg, sizeof(g_slot_msg), "%s 由游戏写入，请选 槽 01..%02d",
                         lab, SAVE_MAN_N);
                return 1;
            }
            if (!save_write("slot", leaf)) {
                snprintf(g_slot_msg, sizeof(g_slot_msg), "写入 %s 失败", lab);
                return 1;
            }
            log_flush();
            snprintf(g_slot_msg, sizeof(g_slot_msg), "已%s到 %s",
                     si.used ? "覆盖" : "记录", lab);
            return 1;
        }
        if (!si.used) { snprintf(g_slot_msg, sizeof(g_slot_msg), "%s 是空的", lab); return 1; }
        if (save_load_leaf(leaf)) {
            rl_log("load: %s from %s", leaf, si.script);
            menu_close();
        } else {
            snprintf(g_slot_msg, sizeof(g_slot_msg), "读取 %s 失败", lab);
        }
        return 1;
    }
    if (g_menu == 5) {
        int lo = g_crow0[g_chapter], hi = lo + g_crn[g_chapter];
        if (back || confirm) { g_menu = 1; return 1; }
        if (vy) {
            if (hi <= lo) return 1;
            g_chart_cur += vy;
            if (g_chart_cur < lo) g_chart_cur = hi - 1;
            if (g_chart_cur >= hi) g_chart_cur = lo;
            return 1;
        }
        if (hx) {
            g_chapter = (g_chapter + hx + CHART_CH) % CHART_CH;
            if (g_crow0[g_chapter] < g_crow0[g_chapter] + g_crn[g_chapter])
                g_chart_cur = g_crow0[g_chapter];
            return 1;
        }
        return 0;
    }
    if (g_menu == 3) {
        if (back || confirm) { g_menu = 2; opt_save(); return 1; }
        if (vy) { g_vo_cur = (g_vo_cur + vy + VO_ROW_N) % VO_ROW_N; return 1; }
        if (hx) {
            int v = g_vo[g_vo_cur] + hx;
            if (v < 0) v = 2;
            if (v > 2) v = 0;
            g_vo[g_vo_cur] = v;
            opt_save();
            return 1;
        }
        return 0;
    }
    if (g_menu == 6) {
        /* Rows are the bindable actions, i.e. the quick list without 继续游戏. */
        if (back || confirm) { g_menu = 2; return 1; }
        if (vy) {
            g_key_cur = (g_key_cur + vy + MENU_N - M_OPT) % (MENU_N - M_OPT);
            return 1;
        }
        if (hx) {
            int act = g_key_cur + M_OPT;
            int v = g_key[act] + hx;
            if (v < K_NONE) v = KEY_V_N - 1;
            if (v >= KEY_V_N) v = K_NONE;
            g_key[act] = v;
            opt_save();
            return 1;
        }
        return 0;
    }
    if (g_menu == 2) {
        if (back) { g_menu = g_title ? 0 : 1; return 1; }
        if (confirm) {
            if (g_opt_cur == OPT_N) { g_menu = 3; g_vo_cur = 0; }
            else if (g_opt_cur == OPT_N + 1) { g_menu = 6; g_key_cur = 0; }
            else g_menu = g_title ? 0 : 1;
            return 1;
        }
        if (vy) { g_opt_cur = (g_opt_cur + vy + OPT_N + 2) % (OPT_N + 2); return 1; }
        if (hx && g_opt_cur < OPT_N) opt_adjust(hx);
        return 1;
    }
    return 0;
}

/* Title navigation. Greyed entries are stepped over, the way the original
 * skips an unlock-gated button. */
static void title_nav(int vy, int confirm)
{
    if (vy) {
        int i = g_title_cur;
        for (int k = 0; k < TITLE_N; k++) {
            i = (i + vy + TITLE_N) % TITLE_N;
            if (g_title_ok[i]) break;
        }
        g_title_cur = i;
        return;
    }
    if (!confirm) return;
    switch (g_title_cur) {
    case 0: {
        int r = game_begin(1);
        if (r < 0) { g_quit = 1; return; }
        g_title = 0;
        rl_log("title: %s", r ? "continue" : "new game");
        break;
    }
    case 1: g_menu = 2; g_opt_cur = 0; break;
    case 4: g_quit = 1; break;
    default: break;
    }
}

/* ---- 回想日志 ------------------------------------------------------------ */

static void log_push(void)
{
    if (!g_v.text[0]) return;
    if (g_log_n == LOG_MAX) {
        memmove(g_log, g_log + LOG_DROP,
                sizeof(g_log[0]) * (size_t)(LOG_MAX - LOG_DROP));
        g_log_n -= LOG_DROP;
    }
    RlLogMsg *e = &g_log[g_log_n++];
    str_cpy(e->node, sizeof(e->node), g_log_node);
    str_cpy(e->name, sizeof(e->name), g_v.name);
    str_cpy(e->text, sizeof(e->text), g_v.text);
}

/* Consecutive messages sharing one #L() node, i.e. the unit the original's
 * scene list shows. A node visited twice yields two groups, because they are
 * two different passages of the story. */
typedef struct { int first, n; char title[64]; } RlGrp;
static RlGrp g_grp[LOG_MAX];
static int g_grp_n, g_grp_stamp;

static void grp_build(void)
{
    if (g_grp_stamp == g_log_n) return;
    g_grp_stamp = g_log_n;
    g_grp_n = 0;
    for (int i = 0; i < g_log_n; i++) {
        const char *t = g_log[i].node[0] ? g_log[i].node : "未命名段落";
        if (!g_grp_n || strcmp(g_grp[g_grp_n - 1].title, t)) {
            if (g_grp_n >= LOG_MAX) break;
            str_cpy(g_grp[g_grp_n].title, sizeof(g_grp[g_grp_n].title), t);
            g_grp[g_grp_n].first = i;
            g_grp[g_grp_n].n = 0;
            g_grp_n++;
        }
        g_grp[g_grp_n - 1].n++;
    }
}

/* Open on the passage the player is in, which is the last group. */
static void backlog_show(void)
{
    grp_build();
    g_log_cur = g_grp_n ? g_grp_n - 1 : 0;
    g_log_pg = 0;
    g_log_open = 1;
    if (rl_verbose)
        for (int i = 0; i < g_grp_n; i++) {
            int starts[LOG_MAX];
            rl_log("group %d: %d msgs %d pages [%s]", i, g_grp[i].n,
                   grp_pages(i, starts, LOG_MAX), g_grp[i].title);
        }
}

/* Lines a string occupies after txt_draw's wrap. For CJK-only text the engine
 * breaks anywhere, so ceil(width/wrap) is the count its layout produces. */
static int wrap_lines(const char *s, int px, int maxw)
{
    char seg[LOG_TEXT];
    int total = 0;
    const char *p = s;
    for (;;) {
        const char *q = strchr(p, '\n');
        size_t len = q ? (size_t)(q - p) : strlen(p);
        if (len >= sizeof(seg)) len = sizeof(seg) - 1;
        memcpy(seg, p, len);
        seg[len] = 0;
        int w = txt_measure(seg, px);
        int lines = maxw > 0 ? (w + maxw - 1) / maxw : 1;
        total += lines > 0 ? lines : 1;
        if (!q) break;
        p = q + 1;
    }
    return total;
}

/* Fill the text column from message `start` of group `gi`; when draw is 0 this
 * only measures. Returns the first message that did not fit, i.e. the next
 * page's start. */
static int log_body(int gi, int start, int draw)
{
    const int x = LGT_X + 24, w = 440 - 48;
    const int lh = LGT_PX + 12;
    const int bottom = LGT_Y + LGT_BOT - 10;
    const int top_y = LGT_Y + LGT_HDR_H + 10;
    int end = g_grp[gi].first + g_grp[gi].n;
    int y = top_y;
    int i = start;
    for (; i < end; i++) {
        int has_nm = g_log[i].name[0] != 0;
        int tl = wrap_lines(g_log[i].text, LGT_PX, w);
        int need = (tl + (has_nm ? 1 : 0)) * lh + 8;
        if (y + need > bottom) break;
        if (draw) {
            int yy = y;
            if (has_nm) {
                txt_draw(g_winr, g_log[i].name, x, yy, LGT_PX - 2, 0xFFFFD24A, w, 0);
                yy += lh;
            }
            txt_draw(g_winr, g_log[i].text, x, yy, LGT_PX, 0xFFFFFFFF, w, lh);
        }
        y += need;
    }
    /* A page must always consume at least one message, or a passage longer
     * than the column would make 下一页 a no-op. */
    return i > start ? i : start + 1;
}

/* Page starts of a group, so 左右翻页 and the footer's 第 i/N 页 agree. */
static int grp_pages(int gi, int *starts, int cap)
{
    int n = 0;
    int i = g_grp[gi].first, end = g_grp[gi].first + g_grp[gi].n;
    while (i < end && n < cap) {
        starts[n++] = i;
        int nx = log_body(gi, i, 0);
        i = nx > i ? nx : i + 1;
    }
    return n;
}

static void draw_log(void)
{
    grp_build();
    draw_bar(0, 0, RL_W, RL_H, 0xE6000000);
    draw_ui("assets/f_select.png", LGR_X, LG_Y, 450, 540);
    draw_ui("assets/f_bar_select.png", LGR_X + 450, LG_Y, 40, 540);
    draw_ui("assets/f_maintxt.png", LGT_X, LGT_Y, 440, 530);

    if (!g_grp_n) {
        txt_draw(g_winr, "还没有可以回想的记录", LGR_X + 24, LG_Y + 24, 26,
                 0xFFBFBFBF, 400, 0);
        txt_draw(g_winr, "B 返回", 24, RL_H - 34, 24, 0xFFBFBFBF, 0, 0);
        return;
    }

    int top = g_log_cur - LG_ROWS + 1;
    if (top > g_grp_n - LG_ROWS) top = g_grp_n - LG_ROWS;
    if (top < 0) top = 0;

    for (int r = 0; r < LG_ROWS; r++) {
        int gi = top + r;
        if (gi >= g_grp_n) break;
        int y = LG_Y + 16 + r * LG_ROW_H;
        int cur = gi == g_log_cur;
        if (cur) draw_bar(LGR_X + 8, y - 6, 434, LG_ROW_H - 2, 0x40FFFFFF);
        char lab[160];
        snprintf(lab, sizeof(lab), "%d. %s", gi + 1, g_grp[gi].title);
        clip_w(lab, sizeof(lab), 24, 410);
        txt_draw(g_winr, lab, LGR_X + 16, y, 24,
                 cur ? 0xFFFFFFFF : 0xFF9FB0C8, 0, 0);
    }

    int vis = g_grp_n < LG_ROWS ? g_grp_n : LG_ROWS;
    int th = (540 - 20) * vis / g_grp_n;
    if (th < 24) th = 24;
    if (th > 540 - 20) th = 540 - 20;
    int span = g_grp_n - vis;
    int ty = LG_Y + 10 + (span > 0 ? (540 - 20 - th) * top / span : 0);
    draw_bar(LGR_X + 450 + 10, ty, 20, th, 0x80FFFFFF);

    char head[192];
    snprintf(head, sizeof(head), "%s", g_grp[g_log_cur].title);
    clip_w(head, sizeof(head), 28, 392);
    txt_draw(g_winr, head, LGT_X + 24, LGT_Y + 24, 28, 0xFFFFD24A, 0, 0);

    int starts[LOG_MAX];
    int np = grp_pages(g_log_cur, starts, LOG_MAX);
    if (g_log_pg >= np) g_log_pg = np - 1;
    if (g_log_pg < 0) g_log_pg = 0;
    log_body(g_log_cur, starts[g_log_pg], 1);

    char foot[96];
    snprintf(foot, sizeof(foot), "第 %d / %d 页　共 %d 段", g_log_pg + 1, np, g_grp_n);
    clip_w(foot, sizeof(foot), 22, 392);
    txt_draw(g_winr, foot, LGT_X + 24, LGT_Y + LGT_BOT + 18, 22, 0xFFBFBFBF, 0, 0);

    txt_draw(g_winr, "上下 选段　左右 翻列表　A 下一页　B 返回", 24, RL_H - 34, 24,
             0xFFBFBFBF, 0, 0);
}

static void log_nav(int vy, int hx, int confirm, int back)
{
    grp_build();
    if (back) { g_log_open = 0; g_log_pg = 0; return; }
    if (!g_grp_n) return;
    if (vy) {
        g_log_cur += vy;
    } else if (hx) {
        g_log_cur += hx * LG_ROWS;
    } else if (confirm) {
        int starts[LOG_MAX];
        int np = grp_pages(g_log_cur, starts, LOG_MAX);
        if (g_log_pg + 1 < np) { g_log_pg++; return; }
        g_log_cur++;
        g_log_pg = 0;
    } else {
        return;
    }
    if (g_log_cur < 0) g_log_cur = 0;
    if (g_log_cur >= g_grp_n) g_log_cur = g_grp_n - 1;
    g_log_pg = 0;
}

/* The backlog is only written where the player expects a disk touch: an
 * explicit save, a trip back to the title, and process exit. The autosave
 * throttle is far too hot for a 300 KB record on exFAT. */
static void log_flush(void)
{
    char dir[600], to[700], tmp[720];
    if (!g_write_ok || !g_log_n) return;
    if (!save_dir(dir, sizeof(dir))) return;
    snprintf(to, sizeof(to), "%s/%s", dir, LOG_FILE);
    snprintf(tmp, sizeof(tmp), "%s.tmp", to);
    FILE *f = fopen(tmp, "wb");
    if (!f) { rl_log("log: create %s failed", tmp); return; }
    fprintf(f, "RLOG 1\n");
    for (int i = 0; i < g_log_n; i++) {
        fputs("node ", f); esc_write(f, g_log[i].node); fputc('\n', f);
        fputs("name ", f); esc_write(f, g_log[i].name); fputc('\n', f);
        fputs("text ", f); esc_write(f, g_log[i].text); fputc('\n', f);
    }
    fclose(f);
    if (rename(tmp, to) != 0) {
        rl_log("log: rename %s: %s", to, strerror(errno));
        remove(tmp);
        return;
    }
    rl_log("log: %d entries written", g_log_n);
}

static void log_read_disk(void)
{
    char dir[600], to[700], line[4200];
    char node[64] = "", name[48] = "";
    g_log_n = 0;
    if (!save_dir(dir, sizeof(dir))) return;
    snprintf(to, sizeof(to), "%s/%s", dir, LOG_FILE);
    FILE *f = fopen(to, "rb");
    if (!f) return;
    if (!fgets(line, sizeof(line), f) || strncmp(line, "RLOG 1", 6)) {
        rl_log("log: bad header in %s", LOG_FILE);
        fclose(f);
        return;
    }
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (!strncmp(line, "node ", 5)) {
            esc_read(line + 5, node, sizeof(node));
        } else if (!strncmp(line, "name ", 5)) {
            esc_read(line + 5, name, sizeof(name));
        } else if (!strncmp(line, "text ", 5) && g_log_n < LOG_MAX) {
            RlLogMsg *e = &g_log[g_log_n++];
            str_cpy(e->node, sizeof(e->node), node);
            str_cpy(e->name, sizeof(e->name), name);
            esc_read(line + 5, e->text, sizeof(e->text));
        }
    }
    fclose(f);
    str_cpy(g_log_node, sizeof(g_log_node), node);
    rl_log("log: %d entries resumed", g_log_n);
    /* A backlog written before the chart existed still says which scenes were
     * read: every entry carries its #L() title, and the 254 titles are unique
     * across all eight CSVs (measured), so the backlog alone identifies them.
     * Without this the player opens a map that is empty until they replay. */
    {
        int lit = 0;
        g_chart_quiet = 1;
        for (int i = 0; i < g_log_n; i++)
            if (g_log[i].node[0] && chart_mark("", g_log[i].node)) lit++;
        g_chart_quiet = 0;
        if (lit) rl_log("chart: %d nodes backfilled from the backlog", lit);
    }
}

/* 返回标题: drop the story, keep the checkpoint. 阅读 then resumes exactly
 * where the player left off, so this is a way out, not a way back to zero. */
static void game_reset(void)
{
    log_flush();
    while (g_sp) scr_free(g_stack[--g_sp].s);
    memset(&g_v, 0, sizeof(g_v));
    g_v.mesvis = 1;
    g_v.fade = 1.f;
    g_v.fade_to = 1.f;
    for (int i = 0; i < RL_SLOT_MAX; i++) {
        g_v.slot[i].scale = 100;
        g_v.slot[i].alpha = 100;
    }
    g_wait = g_await = g_cmp = 0;
    g_select = g_sel_block = g_sel_n = g_sel_cur = 0;
    g_shown_lines = 0;
    g_jump_sel = g_jump_ct = 0;
    g_log_n = g_grp_n = g_grp_stamp = 0;
    g_log_cur = g_log_pg = 0;
    g_log_node[0] = 0;
    reveal_reset();
    aud_stop_bgm();
    g_title = 1;
    g_title_cur = 0;
    rl_log("back to title");
}

/* ---- input -------------------------------------------------------------- */

static SDL_GameController *g_pad;


static void advance(int dir)
{
    /* the first key on a still-typing message finishes it rather than moving on */
    if (dir == 0 && reveal_pending()) { reveal_finish(); return; }
    if (g_select) {
        Frame *f = &g_stack[g_sp - 1];
        if (dir != 0) {
            int n = g_sel_n;
            int i = g_sel_cur;
            for (int k = 0; k < n; k++) {
                i = (i + dir + n) % n;
                if (g_sel_cond[i][0] != '0') break;
            }
            g_sel_cur = i;
            return;
        }
        /* confirmed: run the matching entry of the following #JumpC table */
        RlScript *s = f->s;
        int sel = g_sel_cur;
        g_select = 0;
        g_sel_block = 0;
        for (int i = f->pc; i < s->n; i++) {
            RlLine *ln = &s->lines[i];
            if (ln->kind != RL_CMD || strcmp(ln->name, "JumpC")) break;
            if (ln->argc >= 1 && atoi(ln->argv[0]) == sel) {
                rl_log("#Select -> option %d label %s", sel, ln->argv[1] ? ln->argv[1] : "?");
                jump_label(ln->argv[1] ? ln->argv[1] : "");
                return;
            }
        }
        rl_log("#Select in %s: no JumpC entry for option %d", s->fname, sel);
        return;
    }
    /* at a message end: the next key clears it and continues */
    g_await = 0;
    g_v.text[0] = 0;
    reveal_reset();
}

static void handle_key(SDL_Event *e)
{
    int k = e->key.keysym.sym;
    if (g_log_open) {
        log_nav((k == SDLK_UP || k == SDLK_w) ? -1 : (k == SDLK_DOWN || k == SDLK_s) ? 1 : 0,
                (k == SDLK_LEFT || k == SDLK_a) ? -1 : (k == SDLK_RIGHT || k == SDLK_d) ? 1 : 0,
                k == SDLK_RETURN || k == SDLK_SPACE || k == SDLK_e,
                k == SDLK_ESCAPE || k == SDLK_q || k == SDLK_BACKSPACE);
        return;
    }
    if (g_menu) {
        int vy = (k == SDLK_UP || k == SDLK_w) ? -1 : (k == SDLK_DOWN || k == SDLK_s) ? 1 : 0;
        int hx = (k == SDLK_LEFT || k == SDLK_a) ? -1 : (k == SDLK_RIGHT || k == SDLK_d) ? 1 : 0;
        menu_nav(vy, hx, k == SDLK_RETURN || k == SDLK_SPACE || k == SDLK_e,
                 k == SDLK_ESCAPE || k == SDLK_q || k == SDLK_BACKSPACE);
        return;
    }
    if (g_title) {
        int vy = (k == SDLK_UP || k == SDLK_w) ? -1 : (k == SDLK_DOWN || k == SDLK_s) ? 1 : 0;
        title_nav(vy, k == SDLK_RETURN || k == SDLK_SPACE || k == SDLK_e);
        return;
    }
    if (k == SDLK_m || k == SDLK_TAB) { g_menu = 1; g_menu_cur = 0; return; }
    if (k == SDLK_ESCAPE || k == SDLK_q) { rl_log("quit: key %d", k); g_quit = 1; return; }
    if (g_select) {
        if (k == SDLK_UP || k == SDLK_w) advance(-1);
        else if (k == SDLK_DOWN || k == SDLK_s) advance(1);
        else if (k == SDLK_RETURN || k == SDLK_SPACE || k == SDLK_e) advance(0);
        return;
    }
    if (k == SDLK_RETURN || k == SDLK_SPACE || k == SDLK_e || k == SDLK_RIGHT) {
        advance(0);
    } else if (k == SDLK_LEFT) {
        /* no reverse step: the interpreter only runs forward. 回想日志 (L1 ->
         * 回想日志) is the way to read back what already happened. */
    } else if (k == SDLK_HOME) {
        rl_verbose = !rl_verbose;
    }
}

static void handle_pad(void)
{
    static Uint8 p_a, p_sel, p_up, p_dn, p_left, p_right, p_l1, p_b;
    static Uint8 p_btn[KEY_BTN_N];
    if (!g_pad) return;
#define PBTN(id) SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_##id)
#define EDGE(cur, prev) (cur && !prev)
    Uint8 a = PBTN(A), b = PBTN(B), sel = PBTN(BACK);
    Uint8 up = PBTN(DPAD_UP), dn = PBTN(DPAD_DOWN);
    Uint8 lf = PBTN(DPAD_LEFT), rt = PBTN(DPAD_RIGHT);
    Uint8 l1 = PBTN(LEFTSHOULDER);
    /* The 按键配置 pool, in g_key value order (K_A..K_START). L1 and SELECT are
     * deliberately absent: they stay the escape keys whatever the player maps. */
    Uint8 btn[KEY_BTN_N] = { PBTN(A), PBTN(B), PBTN(X), PBTN(Y),
                             PBTN(RIGHTSHOULDER), PBTN(START) };

    if (g_log_open) {
        log_nav(EDGE(up, p_up) ? -1 : EDGE(dn, p_dn) ? 1 : 0,
                EDGE(lf, p_left) ? -1 : EDGE(rt, p_right) ? 1 : 0,
                EDGE(a, p_a), EDGE(b, p_b) || EDGE(l1, p_l1) || EDGE(sel, p_sel));
    } else if (g_menu) {
        menu_nav(EDGE(up, p_up) ? -1 : EDGE(dn, p_dn) ? 1 : 0,
                 EDGE(lf, p_left) ? -1 : EDGE(rt, p_right) ? 1 : 0,
                 EDGE(a, p_a), EDGE(b, p_b) || EDGE(l1, p_l1) || EDGE(sel, p_sel));
    } else if (g_title) {
        title_nav(EDGE(up, p_up) ? -1 : EDGE(dn, p_dn) ? 1 : 0, EDGE(a, p_a));
    } else {
        /* L1 is the menu key: the original opens it from an on-screen touch
         * button, which a handheld has no use for. SELECT still quits outright
         * so a stuck overlay can never trap the player. */
        int fired = 0;
        if (EDGE(l1, p_l1)) { g_menu = 1; g_menu_cur = 0; }
        else if (EDGE(sel, p_sel)) { rl_log("quit: pad SELECT"); g_quit = 1; }
        else if (g_select) {
            if (EDGE(up, p_up)) advance(-1);
            if (EDGE(dn, p_dn)) advance(1);
            if (EDGE(a, p_a)) advance(0);
        } else {
            /* A bound button owns its press, so a mapped A fires the action
             * without also advancing the text. Bindings stay silent during a
             * #Select, where the d-pad and A belong to the choice. */
            for (int i = 0; i < KEY_BTN_N && !fired; i++)
                if (EDGE(btn[i], p_btn[i])) fired = key_fire_slot(i);
            if (!fired && EDGE(a, p_a)) advance(0);
        }
    }
    p_a = a; p_b = b; p_sel = sel; p_up = up; p_dn = dn;
    p_left = lf; p_right = rt; p_l1 = l1;
    memcpy(p_btn, btn, sizeof p_btn);
#undef PBTN
#undef EDGE
}

/* ---- setup -------------------------------------------------------------- */

static void compute_dst(int w, int h)
{
    g_win_w = w;
    g_win_h = h;
    float s = (float)w / RL_W;
    float sy = (float)h / RL_H;
    if (sy < s) s = sy;
    int dw = (int)(RL_W * s), dh = (int)(RL_H * s);
    g_dst.x = (w - dw) / 2;
    g_dst.y = (h - dh) / 2;
    g_dst.w = dw;
    g_dst.h = dh;
    rl_log("target %dx%d -> dst %dx%d at (%d,%d) on %dx%d", RL_W, RL_H, dw, dh,
           g_dst.x, g_dst.y, w, h);
}

int main(int argc, char **argv)
{
    const char *root = getenv("RL_ROOT");
    const char *script = getenv("RL_SCRIPT");
    const char *font = getenv("RL_FONT");
    const char *logf = getenv("RL_LOG");
    int script_forced = script != NULL;
    (void)argc; (void)argv;
    if (getenv("RL_VERBOSE")) rl_verbose = 1;
    if (getenv("RL_NOSAVE")) g_write_ok = 0;
    if (!root) root = ".";
    if (!script) script = "scenario01_start";
    if (logf) g_logfile = fopen(logf, "ab");

    rl_log("ragingloop loader starting root=%s script=%s", root, script);
    res_init(root);
    img_init();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        rl_log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    const char *driver = SDL_GetCurrentVideoDriver();
    SDL_version ver;
    SDL_GetVersion(&ver);
    rl_log("sdl %d.%d.%d driver=%s", ver.major, ver.minor, ver.patch,
           driver ? driver : "?");

    unsigned flags = SDL_WINDOW_SHOWN;
    const char *fs = getenv("RL_FULLSCREEN");
    if (!fs || atoi(fs) != 0) flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    int wq = 1024, hq = 768;
    g_win = SDL_CreateWindow("Raging Loop", SDL_WINDOWPOS_UNDEFINED,
                             SDL_WINDOWPOS_UNDEFINED, wq, hq, flags);
    if (!g_win) {
        /* kmsdrm can refuse fullscreen on some compositors; retry windowed */
        rl_log("fullscreen window failed (%s), retrying windowed", SDL_GetError());
        g_win = SDL_CreateWindow("Raging Loop", SDL_WINDOWPOS_UNDEFINED,
                                 SDL_WINDOWPOS_UNDEFINED, wq, hq, SDL_WINDOW_SHOWN);
    }
    if (!g_win) { rl_log("no window: %s", SDL_GetError()); return 2; }
    SDL_GetWindowSize(g_win, &wq, &hq);
    g_winr = SDL_CreateRenderer(g_win, -1, SDL_RENDERER_ACCELERATED
                                | SDL_RENDERER_PRESENTVSYNC);
    if (!g_winr) {
        rl_log("accelerated renderer failed (%s), trying software", SDL_GetError());
        g_winr = SDL_CreateRenderer(g_win, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!g_winr) { rl_log("no renderer: %s", SDL_GetError()); return 3; }
    SDL_RendererInfo ri;
    if (SDL_GetRendererInfo(g_winr, &ri) == 0)
        rl_log("renderer='%s' flags=0x%x maxtexture=%dx%d", ri.name, ri.flags,
               ri.max_texture_width, ri.max_texture_height);
    compute_dst(wq, hq);

    g_target = SDL_CreateTexture(g_winr, SDL_PIXELFORMAT_ABGR8888,
                                 SDL_TEXTUREACCESS_TARGET, RL_W, RL_H);
    if (!g_target) {
        rl_log("target texture failed (%s)", SDL_GetError());
        g_target = SDL_CreateTexture(g_winr, SDL_PIXELFORMAT_ABGR8888,
                                     SDL_TEXTUREACCESS_STREAMING, RL_W, RL_H);
    }
    if (!g_target) { rl_log("no render target: %s", SDL_GetError()); return 4; }
    SDL_SetTextureBlendMode(g_target, SDL_BLENDMODE_BLEND);

    g_pad = SDL_GameControllerOpen(0);
    if (g_pad) rl_log("pad='%s'", SDL_GameControllerName(g_pad));
    else rl_log("no game controller: %s", SDL_GetError());

    if (font && *font) {
        txt_init(g_winr, font);
    } else {
        char cand[600];
        static const char *tries[] = {
            "fonts/NotoSansSC-Regular.ttf",
            "../PortMaster/resources/NotoSansSC-Regular.ttf",
        };
        for (unsigned i = 0; i < sizeof(tries) / sizeof(tries[0]) && !txt_ready(); i++) {
            if (res_resolve(cand, sizeof(cand), tries[i])) txt_init(g_winr, cand);
        }
        if (!txt_ready()) rl_log("no CJK font found; set RL_FONT");
    }

    aud_init();

    g_v.fade = 1.f;
    g_v.fade_to = 1.f;
    g_v.mesvis = 1;
    for (int i = 0; i < RL_SLOT_MAX; i++) { g_v.slot[i].scale = 100; g_v.slot[i].alpha = 100; }

    g_script = script;
    /* Before game_begin(): the first #L() of a resumed script has to find the
     * node table already loaded, or the opening scenes never light up. */
    chart_load();
    /* The original boots into its title screen. The headless harness and the
     * pacing/overlay shots drive the story directly, so those env knobs imply
     * "skip the title" on a dev host only. */
    if (!script_forced && !getenv("RL_NEW") && !getenv("RL_MENU") &&
        !getenv("RL_AUTOP") && !getenv("RL_BACKLOG") && !getenv("RL_RESET"))
        g_title = 1;
    if (getenv("RL_TITLE")) g_title = atoi(getenv("RL_TITLE")) != 0;
    if (!g_title) {
        /* Continuing where the player stopped is the save feature; a harness
         * that names a script or asks for RL_NEW wants a fresh start instead. */
        if (game_begin(!script_forced && !getenv("RL_NEW")) < 0) return 5;
    } else {
        rl_log("title screen up");
    }

    g_t0 = SDL_GetTicks();
    g_maxframes = getenv("RL_MAXFRAMES") ? atoi(getenv("RL_MAXFRAMES")) : 0;
    g_autop = getenv("RL_AUTOP") ? atoi(getenv("RL_AUTOP")) : 0;
    if (g_autop) rl_log("autopilot on: %d frames per message", g_autop);
    opt_load();
    opt_apply();
    if (getenv("RL_TEXT_CPS")) {
        int c = atoi(getenv("RL_TEXT_CPS"));
        if (c >= 1 && c <= 200) g_text_cps = c;
    }
    if (getenv("RL_MENU")) {
        int m = atoi(getenv("RL_MENU"));
        if (m >= 1 && m <= 6) g_menu = m;
        /* The chart page normally opens through menu_confirm(), which is where
         * the cursor gets focused; RL_MENU bypasses it, so focus here. */
        if (g_menu == 5) chart_focus();
    }
    /* Chapter tabs are switched by left/right, which the dummy driver cannot
     * send, so a headless shot of any chapter needs this direct set. */
    if (getenv("RL_CHAPTER")) {
        int c = atoi(getenv("RL_CHAPTER"));
        if (c >= 0 && c < CHART_CH && g_crn[c] > 0) {
            g_chapter = c;
            g_chart_cur = g_crow0[c];
        }
    }
    /* Same reason as RL_SAVE_SLOT: move the chart cursor without key input. */
    if (getenv("RL_CHART_NODE")) {
        int k = atoi(getenv("RL_CHART_NODE"));
        if (k >= 0 && k < g_cnode_n) g_chart_cur = k;
    }
    /* RL_BIND="动作行=键值,…" pre-assigns 按键配置, so the page can be shot and
     * the fire path exercised without a pad. Same range checks as the cfg read. */
    {
        const char *b = getenv("RL_BIND");
        while (b && *b) {
            int r = 0, v = 0, used = 0;
            if (sscanf(b, "%d=%d%n", &r, &v, &used) < 2) break;
            if (r >= M_OPT && r < MENU_N && v > K_NONE && v < KEY_V_N) g_key[r] = v;
            b += used;
            while (*b == ',') b++;
        }
    }
    /* RL_FIRE="键值@帧" presses one bindable button from inside the run: it calls
     * the same key_fire_slot() the pad edge does, which is the only way to prove
     * the binding path cannot fault with the menu closed. */
    if (getenv("RL_FIRE")) {
        int v = 0, at = 0;
        if (sscanf(getenv("RL_FIRE"), "%d@%d", &v, &at) == 2 &&
            v > K_NONE && v < KEY_V_N && at > 0) {
            g_fire_val = v;
            g_fire_at = at;
        }
    }
    /* The dummy video driver cannot deliver an A press, so the slot layer is
     * driven directly here: the mode for the page, the slot index and the
     * frame to write from (the write itself fires in the loop, see RL_SAVE_AT). */
    if (getenv("RL_SLOTMODE")) g_slot_mode = atoi(getenv("RL_SLOTMODE")) ? 1 : 0;
    /* Slot index for the frame-triggered write below; -1 means no test write. */
    g_slot_test = getenv("RL_SAVE_SLOT") ? atoi(getenv("RL_SAVE_SLOT")) : -1;
    if (g_slot_test < 0 || g_slot_test >= SAVE_SLOT_N) g_slot_test = -1;
    if (getenv("RL_SAVE_AT")) g_slot_at = atoi(getenv("RL_SAVE_AT"));
    if (getenv("RL_LOAD_SLOT")) {
        int i = atoi(getenv("RL_LOAD_SLOT"));
        char leaf[40];
        if (i >= 0 && i < SAVE_SLOT_N) {
            slot_leaf(leaf, sizeof(leaf), i);
            rl_log("slot-test: load %s %s", leaf,
                   save_load_leaf(leaf) ? "ok" : "failed");
        }
    }
    if (getenv("RL_BACKLOG")) g_backlog_at = atoi(getenv("RL_BACKLOG"));
    if (getenv("RL_RESET")) g_reset_at = atoi(getenv("RL_RESET"));
    rl_log("text reveal: %d chars/s", g_text_cps);
    Uint32 next = SDL_GetTicks();
    const int frame_ms = 1000 / TARGET_FPS;
    while (!g_quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) { rl_log("quit: SDL_QUIT from the compositor"); g_quit = 1; }
            else if (e.type == SDL_KEYDOWN) handle_key(&e);
            else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                compute_dst(e.window.data1, e.window.data2);
        }
        handle_pad();
        if (g_backlog_at && g_frames == (Uint32)g_backlog_at) {
            g_backlog_at = 0;
            backlog_show();
            /* Same reason RL_MENU=2 exists: the dummy driver cannot feed us key
             * events, so a screenshot of a chosen group/page needs a direct entry. */
            const char *at = getenv("RL_LOG_AT");
            if (at && *at) {
                int gi = atoi(at);
                const char *dot = strchr(at, '.');
                g_log_cur = (gi >= 0 && gi < g_grp_n) ? gi : g_log_cur;
                if (dot) g_log_pg = atoi(dot + 1);
            }
            rl_log("RL_BACKLOG: panel up at frame %u with %d messages, %d groups",
                   g_frames, g_log_n, g_grp_n);
        }
        /* 返回标题 frees the script stack, so it needs a headless entry point
         * too; a fault here would otherwise only ever show up in the player's
         * own session. */
        if (g_reset_at && g_frames == (Uint32)g_reset_at) {
            g_reset_at = 0;
            rl_log("RL_RESET: returning to title at frame %u", g_frames);
            game_reset();
        }
        /* A slot written before the loop only ever holds pc=0, which proves the
         * file format but not that a mid-scene position survives. This fires
         * inside the run so the write/load pair carries a real stack. */
        if (g_slot_at && g_frames == (Uint32)g_slot_at) {
            char leaf[40];
            g_slot_at = 0;
            slot_leaf(leaf, sizeof(leaf), g_slot_test < 0 ? SAVE_AUTO_N : g_slot_test);
            rl_log("slot-test: write %s at frame %u %s", leaf, g_frames,
                   save_write("slot-test", leaf) ? "ok" : "failed");
        }
        if (!g_menu && !g_title && !g_log_open) tick();
        /* One synthetic 按键配置 press, gated exactly like the pad edge is. */
        if (g_fire_at && g_frames == (Uint32)g_fire_at) {
            g_fire_at = 0;
            if (!g_menu && !g_title && !g_log_open && !g_select) {
                rl_log("RL_FIRE: pressing %s at frame %u", g_key_name[g_fire_val],
                       g_frames);
                key_fire_slot(g_fire_val - 1);
            } else {
                rl_log("RL_FIRE: skipped, story view not active");
            }
        }
        render_frame();
        g_frames++;
        save_snapshot();
        if (g_maxframes && g_frames >= (Uint32)g_maxframes) {
            rl_log("RL_MAXFRAMES=%d reached", g_maxframes);
            break;
        }
        if (rl_verbose && g_frames % 300 == 0) {
            rl_log("t=%us frames=%u cache=%ldB pc=%s:%d fade=%.2f missing=%d voice=r%d",
                   (unsigned)((SDL_GetTicks() - g_t0) / 1000), g_frames,
                   img_cache_bytes(), g_sp ? g_stack[g_sp - 1].s->fname : "-",
                   g_sp ? g_stack[g_sp - 1].pc : 0, g_v.fade, txt_missing(), g_vo_dbg);
            img_stats();
            txt_stats();
            aud_stats();
        }
        next += frame_ms;
        Uint32 now = SDL_GetTicks();
        if (now < next) SDL_Delay(next - now);
        else if (now - next > 500) next = now;
    }

    Uint32 el = (SDL_GetTicks() - g_t0) / 1000;
    rl_log("exit after %u frames in %us (%.1f fps), cache=%ldB",
           g_frames, el, el ? (double)g_frames / (double)el : 0.0,
           img_cache_bytes());
    save_write("quit", SAVE_FILE);
    log_flush();
    opt_save();
    aud_stop_bgm();
    aud_shutdown();
    while (g_sp) { scr_free(g_stack[--g_sp].s); }
    SDL_DestroyTexture(g_target);
    SDL_DestroyRenderer(g_winr);
    SDL_DestroyWindow(g_win);
    SDL_Quit();
    if (g_logfile) fclose(g_logfile);
    return 0;
}
