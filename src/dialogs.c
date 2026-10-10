/* dialogs.c -- Genecyst dialogs, laid out after screenshots of the original:
 * Load ROM, Messages, Palette, Patterns, About, Directories, ROM patcher,
 * Set resolution, Input device, Genesis Timing. */
#include "dialogs.h"
#include "gui/gui.h"
#include "gui/font.h"
#include "paths.h"
#include "emu.h"
#include "pads.h"
#include <dirent.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

static DialogHost host;
void dlg_init(const DialogHost *h) { host = *h; }

static void win_setup(Window *w, const char *title, int x, int y, int ww, int hh,
                      void (*draw)(Window *, Canvas *),
                      int (*mouse)(Window *, int, int, int, int),
                      int (*key)(Window *, SDL_Keysym *, const char *))
{
    if (w->w) return;   /* already set up: keep the user's position */
    snprintf(w->title, sizeof w->title, "%s", title);
    w->x = x; w->y = y; w->w = ww; w->h = hh; w->closeable = 1;
    w->draw = draw; w->mouse = mouse; w->key = key;
}

/* right-aligned small label whose right edge is at window-relative rx */
static void label_r(Canvas *c, Window *w, int rx, int y, const char *s)
{
    cv_text_small(c, w->x + rx - font_small_width(s), w->y + y, s, C_TEXT);
}

/* ============================ Load ROM ==================================== */
typedef struct { char name[256]; int is_dir; } Entry;
static struct {
    Window win;
    ListBox lb;
    char dir[1024];
    Entry *entries;
    int count;
    Button load;
} lr;

static int has_rom_ext(const char *n)
{
    const char *d = strrchr(n, '.');
    if (!d) return 0;
    static const char *ext[] = {".bin", ".smd", ".gen", ".md", ".usa", ".jap", ".eur", ".zip", ".1", ".sms", ".gg", ".cue", ".iso", NULL};
    for (int i = 0; ext[i]; i++) if (!strcasecmp(d, ext[i])) return 1;
    return 0;
}

static int entry_cmp(const void *a, const void *b)
{
    const Entry *x = a, *y = b;
    if (x->is_dir != y->is_dir) return y->is_dir - x->is_dir;
    return strcasecmp(x->name, y->name);
}

static void lr_scan(void)
{
    free(lr.entries); lr.entries = NULL; lr.count = 0;
    int cap = 64;
    lr.entries = malloc(cap * sizeof(Entry));
#ifdef _WIN32
    /* drive letters like the original's [-C-] entries */
    unsigned long drives = GetLogicalDrives();
    for (int i = 0; i < 26; i++) if (drives & (1ul << i)) {
        if (lr.count == cap) { cap *= 2; lr.entries = realloc(lr.entries, cap * sizeof(Entry)); }
        snprintf(lr.entries[lr.count].name, 256, "[-%c-]", 'A' + i);
        lr.entries[lr.count].is_dir = 3;
        lr.count++;
    }
#endif
    DIR *d = opendir(lr.dir);
    if (!d) return;
    struct dirent *de;
    while ((de = readdir(d))) {
        if (!strcmp(de->d_name, ".")) continue;
        char full[2048];
        snprintf(full, sizeof full, "%s/%s", lr.dir, de->d_name);
        struct stat st;
        if (stat(full, &st)) continue;
        int is_dir = S_ISDIR(st.st_mode);
        if (!is_dir && !has_rom_ext(de->d_name)) continue;
        if (!is_dir && de->d_name[0] == '.') continue;
        if (lr.count == cap) { cap *= 2; lr.entries = realloc(lr.entries, cap * sizeof(Entry)); }
        snprintf(lr.entries[lr.count].name, 256, "%s", de->d_name);
        lr.entries[lr.count].is_dir = is_dir;
        lr.count++;
    }
    closedir(d);
    qsort(lr.entries, lr.count, sizeof(Entry), entry_cmp);
    lr.lb.count = lr.count; lr.lb.top = 0; lr.lb.sel = 0;
    for (int i = 0; i < lr.count; i++) if (!lr.entries[i].is_dir) { lr.lb.sel = i; break; }
    listbox_ensure_visible(&lr.lb);
}

static const char *lr_text(void *u, int i) { (void)u; return lr.entries[i].name; }
static int lr_icon(void *u, int i) { (void)u; return lr.entries[i].is_dir == 3 ? 3 : lr.entries[i].is_dir ? 2 : 1; }

static void lr_activate(void)
{
    if (lr.lb.sel < 0 || lr.lb.sel >= lr.count) return;
    Entry *e = &lr.entries[lr.lb.sel];
    char full[2048];
    if (e->is_dir == 3) {   /* drive */
        snprintf(lr.dir, sizeof lr.dir, "%c:/", e->name[2]);
        snprintf(paths.romdir, sizeof paths.romdir, "%s", lr.dir);
        lr_scan();
        return;
    }
    if (e->is_dir) {
        if (!strcmp(e->name, "..")) {
            char *s = strrchr(lr.dir, '/');
            if (s && s != lr.dir) *s = 0; else strcpy(lr.dir, "/");
        } else {
            if (!strcmp(lr.dir, "/")) snprintf(full, sizeof full, "/%s", e->name);
            else snprintf(full, sizeof full, "%s/%s", lr.dir, e->name);
            snprintf(lr.dir, sizeof lr.dir, "%.1023s", full);
        }
        snprintf(paths.romdir, sizeof paths.romdir, "%s", lr.dir);
        lr_scan();
        return;
    }
    snprintf(full, sizeof full, "%s/%s", lr.dir, e->name);
    gui_close_window(&lr.win);
    if (host.rom_picked) host.rom_picked(full);
}

static void lr_draw(Window *w, Canvas *c)
{
    window_frame_draw(w, c);
    char shown[1200];
    snprintf(shown, sizeof shown, "%s", lr.dir);
    int maxw = w->w - 16;
    while (font_text_width(shown) > maxw && strlen(shown) > 4) memmove(shown, shown + 1, strlen(shown));
    cv_text(c, w->x + 6, w->y + 16, shown, C_WHITE);
    listbox_draw(&lr.lb, c, w->x, w->y);
    cv_rect(c, w->x + lr.lb.x, w->y + lr.lb.y, lr.lb.w, lr.lb.h, C_GREEN);
    button_widget_draw(c, w->x, w->y, &lr.load);
}

static int lr_mouse(Window *w, int type, int mx, int my, int button)
{
    (void)w;
    int act = 0;
    if (button_widget_mouse(&lr.load, type, mx, my, button)) { lr_activate(); return 1; }
    listbox_mouse(&lr.lb, type, mx, my, button, &act);
    if (act) lr_activate();
    return 1;
}

static int lr_key(Window *w, SDL_Keysym *ks, const char *text)
{
    (void)w; (void)text;
    if (!ks) return 0;
    int act = 0;
    if (ks->sym == SDLK_BACKSPACE) { lr.lb.sel = 0; for (int i = 0; i < lr.count; i++) if (!strcmp(lr.entries[i].name, "..")) { lr.lb.sel = i; break; } lr_activate(); return 1; }
    if (listbox_key(&lr.lb, ks->sym, &act)) { if (act) lr_activate(); return 1; }
    if (ks->sym >= SDLK_a && ks->sym <= SDLK_z) {
        for (int k = 1; k <= lr.count; k++) {
            int i = (lr.lb.sel + k) % lr.count;
            if (tolower((unsigned char)lr.entries[i].name[0]) == ks->sym) { lr.lb.sel = i; listbox_ensure_visible(&lr.lb); break; }
        }
        return 1;
    }
    return 0;
}

void dlg_loadrom_open(void)
{
    if (!lr.dir[0]) snprintf(lr.dir, sizeof lr.dir, "%s", paths.romdir);
    Window *w = &lr.win;
    if (!w->w) {
        win_setup(w, "Load ROM", 22, 14, 182, 192, lr_draw, lr_mouse, lr_key);
        lr.lb.x = 5; lr.lb.y = 25; lr.lb.w = 172; lr.lb.h = 150; lr.lb.rowh = 10;
        lr.lb.text = lr_text; lr.lb.icon = lr_icon;
        lr.load = (Button){ (182 - 30) / 2, 192 - 14, 30, 11, "LOAD", 0, 1 };
    }
    lr_scan();
    gui_add_window(w);
}

/* ============================ Messages ==================================== */
static struct { Window win; ScrollBar sb; int follow; } mg;

static void mg_draw(Window *w, Canvas *c)
{
    window_frame_draw(w, c);
    int bx = w->x + 1, by = w->y + 13, bw = w->w - 14, bh = w->h - 14;
    cv_fill(c, bx, by, bw, bh, C_BLACK);
    int rows = (bh - 3) / 9;
    mg.sb.count = gui_message_count(); mg.sb.rows = rows;
    if (mg.follow) mg.sb.top = mg.sb.count - rows;
    scrollbar_clamp(&mg.sb);
    cv_clip(c, bx, by, bw, bh);
    for (int r = 0; r < rows; r++) {
        int i = mg.sb.top + r;
        if (i >= mg.sb.count) break;
        int age = mg.sb.count - 1 - i;
        cv_text_small_noshadow(c, bx + 2, by + 3 + r * 9, gui_message_line(age), age == 0 ? C_MSGGRAY : C_MSGGREEN);
    }
    cv_noclip(c);
    scrollbar_draw(c, w->x, w->y, &mg.sb, 1);
}

static void mg_after_scroll(void)
{
    scrollbar_clamp(&mg.sb);
    mg.follow = mg.sb.top >= mg.sb.count - mg.sb.rows;
}

static int mg_mouse(Window *w, int type, int mx, int my, int button)
{
    (void)w;
    if (type == SDL_MOUSEWHEEL) { mg.sb.top -= button * 3; mg_after_scroll(); return 1; }
    if (scrollbar_mouse(&mg.sb, type, mx, my, button)) mg_after_scroll();
    return 1;
}

static int mg_key(Window *w, SDL_Keysym *ks, const char *text)
{
    (void)w; (void)text;
    if (!ks) return 0;
    switch (ks->sym) {
    case SDLK_UP: mg.sb.top--; break;
    case SDLK_DOWN: mg.sb.top++; break;
    case SDLK_PAGEUP: mg.sb.top -= mg.sb.rows; break;
    case SDLK_PAGEDOWN: mg.sb.top += mg.sb.rows; break;
    case SDLK_HOME: mg.sb.top = 0; break;
    case SDLK_END: mg.sb.top = mg.sb.count; break;
    default: return 0;
    }
    mg_after_scroll();
    return 1;
}

void dlg_messages_open(void)
{
    Window *w = &mg.win;
    if (!w->w) {
        win_setup(w, "Messages", 21, 15, 252, 124, mg_draw, mg_mouse, mg_key);
        mg.sb = (ScrollBar){ 252 - 12, 13, 11, 124 - 14, 0, 0, 0, 0 };
        mg.follow = 1;
    }
    gui_add_window(w);
}

/* ============================ Palette ===================================== */
static Window pal_win;
static void pal_draw(Window *w, Canvas *c)
{
    window_frame_draw(w, c);
    for (int row = 0; row < 4; row++)
        for (int col = 0; col < 16; col++)
            cv_fill(c, w->x + 1 + col * 16, w->y + 13 + row * 16, 16, 16, emu.loaded ? emu_cram_color(row * 16 + col) : C_BLACK);
}
static int pal_mouse(Window *w, int type, int mx, int my, int button)
{
    (void)w;
    if (type == SDL_MOUSEBUTTONDOWN && button == SDL_BUTTON_LEFT && mx >= 1 && my >= 13 && mx < 257 && my < 77) {
        int col = (mx - 1) / 16, row = (my - 13) / 16;
        uint32_t rgb = emu_cram_color(row * 16 + col);
        gui_message("Palette %d colour %d: R%d G%d B%d", row, col, (rgb >> 16) / 36, ((rgb >> 8) & 255) / 36, (rgb & 255) / 36);
    }
    return 1;
}
void dlg_palette_open(void)
{
    win_setup(&pal_win, "Palette", 41, 35, 258, 80, pal_draw, pal_mouse, NULL);
    gui_add_window(&pal_win);
}

/* ============================ Patterns ==================================== */
static struct { Window win; ScrollBar sb; int pal; uint32_t buf[128 * 128]; } pt;
static void pt_draw(Window *w, Canvas *c)
{
    window_frame_draw(w, c);
    scrollbar_clamp(&pt.sb);
    for (int r = 0; r < 16; r++) {
        char a[8];
        snprintf(a, sizeof a, "%04X", (pt.sb.top + r) * 0x200);
        cv_text_small_noshadow(c, w->x + 2, w->y + 15 + r * 8, a, C_WHITE);
    }
    if (emu.loaded) emu_draw_tiles(pt.buf, 128, pt.sb.top * 16, 16, 16, pt.pal);
    else memset(pt.buf, 0, sizeof pt.buf);
    cv_blit(c, w->x + 22, w->y + 14, pt.buf, 128, 128, 128);
    scrollbar_draw(c, w->x, w->y, &pt.sb, 1);
}
static int pt_mouse(Window *w, int type, int mx, int my, int button)
{
    (void)w;
    if (type == SDL_MOUSEWHEEL) { pt.sb.top -= button * 2; scrollbar_clamp(&pt.sb); return 1; }
    if (scrollbar_mouse(&pt.sb, type, mx, my, button)) return 1;
    if (type == SDL_MOUSEBUTTONDOWN && button == SDL_BUTTON_LEFT && mx >= 22 && my >= 14 && mx < 150 && my < 142) {
        int tile = (pt.sb.top + (my - 14) / 8) * 16 + (mx - 22) / 8;
        gui_message("Tile %03X at VRAM %04X (palette %d)", tile, tile * 32, pt.pal);
    }
    return 1;
}
static int pt_key(Window *w, SDL_Keysym *ks, const char *text)
{
    (void)w; (void)text;
    if (!ks) return 0;
    switch (ks->sym) {
    case SDLK_UP: pt.sb.top--; break;
    case SDLK_DOWN: pt.sb.top++; break;
    case SDLK_PAGEUP: pt.sb.top -= 16; break;
    case SDLK_PAGEDOWN: pt.sb.top += 16; break;
    case SDLK_HOME: pt.sb.top = 0; break;
    case SDLK_END: pt.sb.top = 128; break;
    case SDLK_LEFT: pt.pal = (pt.pal + 3) & 3; gui_message("Patterns: palette %d", pt.pal); break;
    case SDLK_RIGHT: pt.pal = (pt.pal + 1) & 3; gui_message("Patterns: palette %d", pt.pal); break;
    default: return 0;
    }
    scrollbar_clamp(&pt.sb);
    return 1;
}
void dlg_patterns_open(void)
{
    Window *w = &pt.win;
    if (!w->w) {
        win_setup(w, "Patterns", 61, 55, 163, 144, pt_draw, pt_mouse, pt_key);
        pt.sb = (ScrollBar){ 149, 14, 12, 128, 128, 16, 0, 0 };
    }
    gui_add_window(w);
}

/* ============================ About ======================================= */
static Window about_win;
static void upper(char *s) { for (; *s; s++) *s = (char)toupper((unsigned char)*s); }
static void about_draw(Window *w, Canvas *c)
{
    window_frame_draw(w, c);
    int x = w->x + 6, y = w->y + 18;
    cv_text_small(c, x, y, "GENECYST SDL", C_TEXT);
    const char *ver = "VX.XX";
    cv_text_small(c, w->x + w->w - 6 - font_small_width(ver), y, ver, C_MSGGREEN);
    char comp[96], ver2[32];
    snprintf(ver2, sizeof ver2, "%s", __VERSION__);
    ver2[strcspn(ver2, " ")] = 0;
#if defined(__clang__)
    snprintf(comp, sizeof comp, "CLANG %s %s %s", ver2, __DATE__, __TIME__);
#else
    snprintf(comp, sizeof comp, "GCC %s %s %s", ver2, __DATE__, __TIME__);
#endif
    upper(comp);
    cv_text_small(c, x, y + 10, comp, C_TEXT);
    cv_text_small(c, x, y + 24, "COPYRIGHT (C) 1998 BLOODLUST SOFTWARE", C_TEXT);
    cv_text_small(c, x, y + 34, "REVIVAL 2026 - GENESIS PLUS GX CORE", C_GRAY3);
}
void dlg_about_open(void)
{
    win_setup(&about_win, "About", 21, 15, 222, 70, about_draw, NULL, NULL);
    gui_add_window(&about_win);
}

/* ============================ Directories ================================= */
static struct { Window win; TextEdit te[4]; Button ok; int focus; } dr;
static const char *dr_labels[4] = { "SAVE:", "PCX:", "LOG:", "PATCH:" };

static void dr_apply(void)
{
    char *dst[4] = { paths.savedir, paths.pcxdir, paths.logdir, paths.patdir };
    for (int i = 0; i < 4; i++) {
        const char *v = dr.te[i].buf;
        if (!strcmp(v, ".") || !*v) dst[i][0] = 0;
        else snprintf(dst[i], 1024, "%s", v);
    }
    gui_message("Directories set");
    gui_close_window(&dr.win);
}
static void dr_draw(Window *w, Canvas *c)
{
    window_frame_draw(w, c);
    for (int i = 0; i < 4; i++) {
        label_r(c, w, 39, 19 + 15 * i, dr_labels[i]);
        textedit_draw(c, w->x, w->y, &dr.te[i], dr.focus == i);
    }
    cv_fill(c, w->x + 1, w->y + w->h - 11, w->w - 2, 10, C_WINTITLE);   /* button band */
    button_widget_draw(c, w->x, w->y, &dr.ok);
}
static int dr_mouse(Window *w, int type, int mx, int my, int button)
{
    (void)w;
    if (button_widget_mouse(&dr.ok, type, mx, my, button)) { dr_apply(); return 1; }
    if (type == SDL_MOUSEBUTTONDOWN) for (int i = 0; i < 4; i++) if (textedit_hit(&dr.te[i], mx, my)) dr.focus = i;
    return 1;
}
static int dr_key(Window *w, SDL_Keysym *ks, const char *text)
{
    (void)w;
    if (ks && ks->sym == SDLK_TAB) { dr.focus = (dr.focus + ((ks->mod & KMOD_SHIFT) ? 3 : 1)) % 4; return 1; }
    int r = textedit_key(&dr.te[dr.focus], ks, text);
    if (r == 2) { dr_apply(); return 1; }
    return r;
}
void dlg_directories_open(void)
{
    Window *w = &dr.win;
    if (!w->w) {
        win_setup(w, "Directories", 21, 15, 181, 93, dr_draw, dr_mouse, dr_key);
        for (int i = 0; i < 4; i++) dr.te[i] = (TextEdit){ 45, 17 + 15 * i, 130, "", 0, 0, 0 };
        dr.ok = (Button){ 82, 82, 18, 10, "OK", 0, 1 };
    }
    const char *vals[4] = { paths.savedir, paths.pcxdir, paths.logdir, paths.patdir };
    for (int i = 0; i < 4; i++) textedit_set(&dr.te[i], vals[i][0] ? vals[i] : ".");
    dr.focus = 0;
    gui_add_window(w);
}

/* ============================ ROM patcher ================================= */
static struct { Window win; TextEdit code, name; Button add, remove, toggle, save; int focus, sel, top; } pc;
#define PC_ROWS 8

static void pc_add(void)
{
    if (!emu.loaded) { gui_message("No ROM loaded"); return; }
    if (!pc.code.buf[0]) return;
    int i = emu_patch_add(pc.code.buf, pc.name.buf);
    if (i >= 0) { pc.sel = i; textedit_set(&pc.code, ""); textedit_set(&pc.name, ""); gui_message("Patch added: %s", emu_patches[i].code); }
}
static void pc_clamp(void)
{
    if (pc.sel >= emu_npatches) pc.sel = emu_npatches - 1;
    if (pc.sel < 0) pc.sel = 0;
    if (pc.sel < pc.top) pc.top = pc.sel;
    if (pc.sel >= pc.top + PC_ROWS) pc.top = pc.sel - PC_ROWS + 1;
    if (pc.top < 0) pc.top = 0;
}
static void pc_draw(Window *w, Canvas *c)
{
    window_frame_draw(w, c);
    cv_text_small(c, w->x + 5, w->y + 17, "CODE:", C_TEXT);
    textedit_draw(c, w->x, w->y, &pc.code, pc.focus == 0);
    cv_text_small(c, w->x + 5, w->y + 30, "NAME:", C_TEXT);
    textedit_draw(c, w->x, w->y, &pc.name, pc.focus == 1);
    button_widget_draw(c, w->x, w->y, &pc.add);
    button_widget_draw(c, w->x, w->y, &pc.remove);
    button_widget_draw(c, w->x, w->y, &pc.toggle);
    button_widget_draw(c, w->x, w->y, &pc.save);
    int lx = w->x + 5, ly = w->y + 54, lw = 207, lh = 74;
    cv_fill(c, lx, ly, lw, lh, C_BLACK);
    cv_rect(c, lx, ly, lw, lh, pc.focus == 2 ? C_GREEN : C_BTN);
    pc_clamp();
    cv_clip(c, lx + 1, ly + 1, lw - 2, lh - 2);
    for (int r = 0; r < PC_ROWS; r++) {
        int i = pc.top + r;
        if (i >= emu_npatches) break;
        int ry = ly + 2 + r * 9;
        if (i == pc.sel) cv_fill(c, lx + 1, ry - 1, lw - 2, 9, C_GREEN);
        char line[96];
        snprintf(line, sizeof line, "%c %-11s %06X:%04X  %s", emu_patches[i].on ? '*' : ' ', emu_patches[i].code,
                 emu_patches[i].addr, emu_patches[i].val, emu_patches[i].name);
        cv_text_small_noshadow(c, lx + 4, ry + 1, line, i == pc.sel ? C_WHITE : C_TEXT);
    }
    cv_noclip(c);
}
static int pc_mouse(Window *w, int type, int mx, int my, int button)
{
    (void)w;
    if (button_widget_mouse(&pc.add, type, mx, my, button)) { pc_add(); return 1; }
    if (button_widget_mouse(&pc.remove, type, mx, my, button)) { emu_patch_remove(pc.sel); pc_clamp(); return 1; }
    if (button_widget_mouse(&pc.toggle, type, mx, my, button)) { emu_patch_toggle(pc.sel); return 1; }
    if (button_widget_mouse(&pc.save, type, mx, my, button)) { emu_patch_save(); return 1; }
    if (type == SDL_MOUSEBUTTONDOWN && button == SDL_BUTTON_LEFT) {
        if (textedit_hit(&pc.code, mx, my)) pc.focus = 0;
        else if (textedit_hit(&pc.name, mx, my)) pc.focus = 1;
        else if (mx >= 5 && my >= 54 && mx < 212 && my < 128) {
            pc.focus = 2;
            int i = pc.top + (my - 56) / 9;
            if (i >= 0 && i < emu_npatches) pc.sel = i;
        }
    }
    if (type == SDL_MOUSEWHEEL) { pc.top -= button; pc_clamp(); }
    return 1;
}
static int pc_key(Window *w, SDL_Keysym *ks, const char *text)
{
    (void)w;
    if (ks && ks->sym == SDLK_TAB) { pc.focus = (pc.focus + ((ks->mod & KMOD_SHIFT) ? 2 : 1)) % 3; return 1; }
    if (pc.focus < 2) {
        int r = textedit_key(pc.focus == 0 ? &pc.code : &pc.name, ks, text);
        if (r == 2) { pc_add(); return 1; }
        return r;
    }
    if (!ks) return 0;
    switch (ks->sym) {
    case SDLK_UP: pc.sel--; break;
    case SDLK_DOWN: pc.sel++; break;
    case SDLK_SPACE: case SDLK_RETURN: emu_patch_toggle(pc.sel); break;
    case SDLK_DELETE: case SDLK_BACKSPACE: emu_patch_remove(pc.sel); break;
    default: return 0;
    }
    pc_clamp();
    return 1;
}
void dlg_patcher_open(void)
{
    Window *w = &pc.win;
    if (!w->w) {
        win_setup(w, "ROM patcher", 41, 35, 217, 135, pc_draw, pc_mouse, pc_key);
        pc.code = (TextEdit){ 28, 15, 165, "", 0, 0, 15 };
        pc.name = (TextEdit){ 28, 28, 165, "", 0, 0, 39 };
        pc.add = (Button){ 15, 41, 23, 10, "ADD", 0, 1 };
        pc.remove = (Button){ 57, 41, 39, 10, "REMOVE", 0, 1 };
        pc.toggle = (Button){ 118, 41, 37, 10, "TOGGLE", 0, 1 };
        pc.save = (Button){ 172, 41, 29, 10, "SAVE", 0, 1 };
    }
    pc.focus = 0;
    gui_add_window(w);
}

/* ============================ Set resolution ============================== */
static struct { Window win; TextEdit tx, ty; Button ok; int focus; } sr;
static void sr_apply(void)
{
    int w = atoi(sr.tx.buf), h = atoi(sr.ty.buf);
    if (w < 256) w = 256;
    if (w > 1280) w = 1280;
    if (h < 200) h = 200;
    if (h > 1024) h = 1024;
    if (host.set_resolution) host.set_resolution(w, h);
    gui_close_window(&sr.win);
}
static void sr_draw(Window *w, Canvas *c)
{
    window_frame_draw(w, c);
    label_r(c, w, 44, 19, "X-RES:");
    textedit_draw(c, w->x, w->y, &sr.tx, sr.focus == 0);
    label_r(c, w, 44, 34, "Y-RES:");
    textedit_draw(c, w->x, w->y, &sr.ty, sr.focus == 1);
    cv_fill(c, w->x + 1, w->y + w->h - 11, w->w - 2, 10, C_WINTITLE);   /* button band */
    button_widget_draw(c, w->x, w->y, &sr.ok);
}
static int sr_mouse(Window *w, int type, int mx, int my, int button)
{
    (void)w;
    if (button_widget_mouse(&sr.ok, type, mx, my, button)) { sr_apply(); return 1; }
    if (type == SDL_MOUSEBUTTONDOWN) { if (textedit_hit(&sr.tx, mx, my)) sr.focus = 0; else if (textedit_hit(&sr.ty, mx, my)) sr.focus = 1; }
    return 1;
}
static int sr_key(Window *w, SDL_Keysym *ks, const char *text)
{
    (void)w;
    if (ks && ks->sym == SDLK_TAB) { sr.focus ^= 1; return 1; }
    int r = textedit_key(sr.focus ? &sr.ty : &sr.tx, ks, text);
    if (r == 2) { sr_apply(); return 1; }
    return r;
}
void dlg_setres_open(void)
{
    Window *w = &sr.win;
    if (!w->w) {
        win_setup(w, "Set resolution", 61, 55, 122, 69, sr_draw, sr_mouse, sr_key);
        sr.tx = (TextEdit){ 48, 17, 42, "", 0, 1, 4 };
        sr.ty = (TextEdit){ 48, 32, 42, "", 0, 1, 4 };
        sr.ok = (Button){ 48, 58, 28, 10, "OKAY", 0, 1 };
    }
    int cw = 320, ch = 240;
    if (host.get_resolution) host.get_resolution(&cw, &ch);
    char b[16];
    snprintf(b, sizeof b, "%d", cw); textedit_set(&sr.tx, b);
    snprintf(b, sizeof b, "%d", ch); textedit_set(&sr.ty, b);
    sr.focus = 0;
    gui_add_window(w);
}

/* ============================ Input device ================================ */
static struct { Window win; ListBox lb; Button redefine; int port; int focus; int last_sel; } in;
static const char *in_text(void *u, int i) { (void)u; return pads_type_name(i); }
static void in_closed(Window *w) { (void)w; pads_test_mode = 0; pads_redefine_cancel(); }

static void in_draw(Window *w, Canvas *c)
{
    window_frame_draw(w, c);
    cv_text_small(c, w->x + 6, w->y + 17, "INPUT TYPE:", C_TEXT);
    if (in.lb.sel != in.last_sel) { in.last_sel = in.lb.sel; pads_set_type(in.port, in.lb.sel); }
    listbox_draw(&in.lb, c, w->x, w->y);
    if (in.focus == 0) cv_rect(c, w->x + in.lb.x, w->y + in.lb.y, in.lb.w, in.lb.h, C_GREEN);
    button_widget_draw(c, w->x, w->y, &in.redefine);
    cv_text_small(c, w->x + 126, w->y + 18, "TEST AREA:", C_TEXT);
    int bx = w->x + 126, by = w->y + 27, bw = 91, bh = 92;
    cv_fill(c, bx, by, bw, bh, C_BLACK);
    cv_rect(c, bx, by, bw, bh, C_WINFRAME);
    uint16_t st = pads_state(in.port);
    const char *prompt = pads_redefine_prompt();
    if (prompt) {
        cv_text_small(c, bx + (bw - font_small_width(prompt)) / 2, by + bh / 2 - 3, prompt, C_HOTKEY);
    } else {
        int dx = 0, dy = 0;
        if (st & 0x0001) dy -= 25;   /* INPUT_UP */
        if (st & 0x0002) dy += 25;   /* INPUT_DOWN */
        if (st & 0x0004) dx -= 25;   /* INPUT_LEFT */
        if (st & 0x0008) dx += 25;   /* INPUT_RIGHT */
        cv_fill(c, bx + bw / 2 - 5 + dx, by + bh / 2 - 5 + dy, 10, 10, C_GREEN);
    }
    static const struct { int x, y; const char *l; uint16_t bit; } btns[7] = {
        { 129, 123, "A", 0x0040 }, { 152, 123, "B", 0x0010 }, { 175, 123, "C", 0x0020 }, { 198, 123, "S", 0x0080 },
        { 129, 147, "X", 0x0400 }, { 152, 147, "Y", 0x0200 }, { 175, 147, "Z", 0x0100 } };
    for (int i = 0; i < 7; i++)
        button_draw(c, w->x + btns[i].x, w->y + btns[i].y, 17, 10, btns[i].l, (st & btns[i].bit) != 0);
}
static int in_mouse(Window *w, int type, int mx, int my, int button)
{
    (void)w;
    int act = 0;
    if (button_widget_mouse(&in.redefine, type, mx, my, button)) { pads_redefine_begin(in.port); return 1; }
    if (type == SDL_MOUSEBUTTONDOWN && mx >= in.lb.x && my >= in.lb.y && mx < in.lb.x + in.lb.w && my < in.lb.y + in.lb.h) in.focus = 0;
    listbox_mouse(&in.lb, type, mx, my, button, &act);
    return 1;
}
static int in_key(Window *w, SDL_Keysym *ks, const char *text)
{
    (void)w; (void)text;
    if (!ks) return 0;
    int act = 0;
    if (listbox_key(&in.lb, ks->sym, &act)) return 1;
    return 0;
}
void dlg_input_open(int port)
{
    Window *w = &in.win;
    if (!w->w) {
        win_setup(w, "Input device 1", 21, 15, 232, 171, in_draw, in_mouse, in_key);
        w->closed = in_closed;
        in.lb.x = 5; in.lb.y = 27; in.lb.w = 112; in.lb.h = 92; in.lb.rowh = 10;
        in.lb.text = in_text; in.lb.icon = NULL; in.lb.count = INDEV_COUNT;
        in.redefine = (Button){ 21, 123, 81, 10, "REDEFINE KEYS...", 0, 1 };
    }
    in.port = port & 1;
    snprintf(w->title, sizeof w->title, "Input device %d", in.port + 1);
    in.lb.sel = in.last_sel = pads_type(in.port);
    in.focus = 0;
    pads_test_mode = 1;
    gui_add_window(w);
}

/* ============================ Genesis Timing ============================== */
static struct { Window win; TextEdit fps; } tm;
static void tm_apply(void)
{
    int v = atoi(tm.fps.buf);
    if (v < 1) v = 0;
    if (v > 600) v = 600;
    if (host.set_vfps) host.set_vfps(v);
    char b[16]; snprintf(b, sizeof b, "%d", host.get_vfps ? host.get_vfps() : v); textedit_set(&tm.fps, b);
}
static void spinner_draw(Canvas *c, int x, int y, int enabled)
{
    uint32_t col = enabled ? C_WHITE : C_GRAY1;
    cv_fill(c, x, y, 8, 9, C_GRAY1);
    cv_bevel(c, x, y, 8, 4, C_GRAY3, C_DKGRAY);
    cv_bevel(c, x, y + 5, 8, 4, C_GRAY3, C_DKGRAY);
    const ResImage *u = res_image("sumark"), *d = res_image("sdmark");
    if (u) cv_blit_mask(c, x + 1, y + 1, u->px, u->w, u->h, col);
    if (d) cv_blit_mask(c, x + 1, y + 5, d->px, d->w, d->h, col);
}
static void ro_field(Canvas *c, Window *w, int y, const char *val)
{
    cv_fill(c, w->x + 93, w->y + y, 32, TEXTEDIT_H, C_BLACK);
    cv_text_small_noshadow(c, w->x + 95, w->y + y + 2, val, C_GRAY2);
    spinner_draw(c, w->x + 125, w->y + y, 0);
}
static void tm_draw(Window *w, Canvas *c)
{
    window_frame_draw(w, c);
    label_r(c, w, 89, 19, "HBLANK INST:"); ro_field(c, w, 17, "42");
    label_r(c, w, 89, 37, "VBLANK INST:"); ro_field(c, w, 35, "2150");
    label_r(c, w, 89, 55, "VIRTUAL FPS:");
    textedit_draw(c, w->x, w->y, &tm.fps, 1);
    spinner_draw(c, w->x + 125, w->y + 53, 1);
}
static int tm_mouse(Window *w, int type, int mx, int my, int button)
{
    (void)w;
    if (type == SDL_MOUSEBUTTONDOWN && button == SDL_BUTTON_LEFT && mx >= 125 && mx < 133 && my >= 53 && my < 62) {
        int v = atoi(tm.fps.buf) + (my < 58 ? 1 : -1);
        char b[16]; snprintf(b, sizeof b, "%d", v); textedit_set(&tm.fps, b);
        tm_apply();
    }
    return 1;
}
static int tm_key(Window *w, SDL_Keysym *ks, const char *text)
{
    (void)w;
    int r = textedit_key(&tm.fps, ks, text);
    if (r == 2) { tm_apply(); return 1; }
    return r;
}
void dlg_timing_open(void)
{
    Window *w = &tm.win;
    if (!w->w) {
        win_setup(w, "Genesis Timing", 21, 15, 142, 80, tm_draw, tm_mouse, tm_key);
        tm.fps = (TextEdit){ 93, 53, 32, "", 0, 1, 3 };
    }
    char b[16]; snprintf(b, sizeof b, "%d", host.get_vfps ? host.get_vfps() : 60); textedit_set(&tm.fps, b);
    gui_add_window(w);
}
