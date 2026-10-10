#include "gui.h"
#include "font.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ---------------------------------------------------------------- state ---- */
static Menu *g_menus;
static int g_nmenus;
static void (*g_command)(int id);
static void (*g_maxbox)(void);
static int g_visible = 1;
static int g_open_menu = -1;        /* index of open top-level menu */
static int g_hover_item = -1;       /* hovered item index in the open menu */
static MenuItem *g_open_sub;        /* open submenu items */
static int g_sub_x, g_sub_y, g_sub_hover = -1;
static int g_mouse_x, g_mouse_y;
static int g_mouse_down;
static Window *g_windows[GUI_MAX_WINDOWS];
static int g_nwindows;
static Window *g_drag;              /* window being dragged */
static int g_drag_dx, g_drag_dy;
int gui_show_msg_popup = 1;
int gui_show_state_popup = 1;
int gui_auto_hide = 1;
int gui_cursor_style = 0;

const ResImage *res_image(const char *name)
{
    for (int i = 0; i < res_image_count; i++) if (!strcmp(res_images[i].name, name)) return &res_images[i];
    return NULL;
}
static void res_draw(Canvas *c, const char *name, int x, int y)
{
    const ResImage *r = res_image(name);
    if (r) cv_blit_indexed(c, x, y, r->px, r->w, r->h, res_palette);
}
static int g_idle_ticks;            /* ticks since the last mouse/keyboard interaction with the GUI */

#define MSG_LINES 64
static char g_msgs[MSG_LINES][256];
static int g_msg_count;
static int g_popup_timer;           /* frames remaining for the message popup */
static int g_drip_tick;             /* blood drip animation clock */
static int g_canvas_w, g_canvas_h;  /* last drawn canvas size (hit tests, submenu placement) */

/* ------------------------------------------------------------ geometry ---- */
static int menu_title_x(int i)
{
    int x = 0;
    for (int k = 0; k < i; k++) x += font_text_width(g_menus[k].title) + 15;   /* measured: 16px gap between titles */
    return x;
}
static int menu_title_w(int i) { return font_text_width(g_menus[i].title) + 15; }

static int menu_item_count(MenuItem *items)
{
    int n = 0;
    while (!(items[n].label == NULL && items[n].id == -1)) n++;
    return n;
}

static int menu_width_f(MenuItem *items, int small)
{
    int w = 0, hk = 0, has_sub = 0;
    int n = menu_item_count(items);
    for (int i = 0; i < n; i++) {
        if (!items[i].label) continue;
        int tw = small ? font_small_width(items[i].label) : font_text_width(items[i].label);
        if (tw > w) w = tw;
        if (items[i].hotkey) { int hw = font_small_width(items[i].hotkey); if (hw > hk) hk = hw; }
        if (items[i].sub) has_sub = 1;
    }
    return 8 + w + (hk ? 8 + hk : 0) + (has_sub ? 10 : 0) + 8;
}
static int menu_width(MenuItem *items) { return menu_width_f(items, 0); }

static int menu_height(MenuItem *items) { return menu_item_count(items) * MENU_LINE; }

/* ------------------------------------------------------------- drawing ---- */
static void draw_drips(Canvas *c, int x, int y, int w)
{
    /* tile the 248px drip strip horizontally, clipped to w */
    int fr = (g_drip_tick / DRIP_FRAME_TICKS) % drip_frames;
    cv_clip(c, x, y, w, drip_h);
    for (int tx = 0; tx < w; tx += drip_w) {
        for (int j = 0; j < drip_h; j++) {
            for (int i = 0; i < drip_w; i++) {
                if (drip_bright[fr][j][i >> 3] & (1 << (i & 7))) cv_pixel(c, x + tx + i, y + j, C_RED);
                else if (drip_dark[fr][j][i >> 3] & (1 << (i & 7))) cv_pixel(c, x + tx + i, y + j, C_DKRED);
                else if (drip_light[fr][j][i >> 3] & (1 << (i & 7))) cv_pixel(c, x + tx + i, y + j, C_LTRED);
            }
        }
    }
    cv_noclip(c);
}

static void draw_menubar(Canvas *c)
{
    int total = menu_title_x(g_nmenus);
    cv_fill(c, 0, 0, total, MENUBAR_H, C_RED);
    draw_drips(c, 0, MENUBAR_H, total);   /* same width as the bar, no notch */
    for (int i = 0; i < g_nmenus; i++) {
        int x = menu_title_x(i);
        if (i == g_open_menu) cv_fill(c, x, 0, menu_title_w(i), MENUBAR_H, C_GREEN);
        cv_text(c, x + 8, 1, g_menus[i].title, C_WHITE);
    }
    /* maximize box at top-right, like the original (only when it fits): 8x9,
     * light right edge, dark left/bottom, black top, ring inside */
    int bx = c->w - 10;
    if (bx < total + 2) return;
    cv_fill(c, bx, 1, 8, 9, C_BTN);
    cv_hline(c, bx, 1, 8, C_BLACK);
    cv_vline(c, bx, 2, 8, C_BTNDARK);
    cv_vline(c, bx + 7, 2, 7, C_BTNLIGHT);
    cv_hline(c, bx, 9, 7, C_BTNDARK);
    res_draw(c, "wm", bx + 1, 3);
}

static void draw_menu_items(Canvas *c, MenuItem *items, int x, int y, int hover, int small)
{
    int w = menu_width_f(items, small), h = menu_height(items);
    int n = menu_item_count(items);
    cv_fill(c, x, y, w, h, C_RED);
    draw_drips(c, x, y + h, w);
    for (int i = 0; i < n; i++) {
        int iy = y + i * MENU_LINE;
        MenuItem *it = &items[i];
        if (!it->label) {
            cv_hline(c, x + 2, iy + 4, w - 4, C_DKRED);
            cv_hline(c, x + 2, iy + 5, w - 4, C_SHADOW);
            continue;
        }
        int enabled = it->enabled ? it->enabled() : 1;
        uint32_t col = enabled ? C_TEXT : C_GRAY1;
        if (i == hover && enabled) {
            cv_fill(c, x, iy, w, MENU_LINE, C_GREEN);
        }
        if (it->checked && it->checked()) res_draw(c, "mcheck", x + 1, iy + 1);
        if (small) cv_text_small(c, x + 8, iy + 2, it->label, col);
        else cv_text(c, x + 8, iy + 1, it->label, col);
        if (it->hotkey) {
            int hw = font_small_width(it->hotkey);
            cv_text_small(c, x + w - 8 - hw, iy + 2, it->hotkey, enabled ? C_HOTKEY : C_GRAY1);
        }
        if (it->sub) res_draw(c, "menur", x + w - 7, iy + 2);
    }
}

/* Genecyst 3D box: light top & right edge, dark left & bottom edge */
static void gc_box(Canvas *c, int x, int y, int w, int h, int pressed)
{
    uint32_t light = pressed ? C_BTNDARK : C_BTNLIGHT, dark = pressed ? C_BTNLIGHT : C_BTNDARK;
    cv_fill(c, x, y, w, h, C_BTN);
    cv_hline(c, x + 1, y, w - 1, light);
    cv_vline(c, x + w - 1, y, h - 1, light);
    cv_vline(c, x, y, h, dark);
    cv_hline(c, x, y + h - 1, w - 1, dark);
}

void button_draw(Canvas *c, int x, int y, int w, int h, const char *label, int pressed)
{
    gc_box(c, x, y, w, h, pressed);
    int tw = font_small_width(label);
    cv_text_small_noshadow(c, x + (w - tw) / 2 + pressed, y + (h - 5) / 2 + pressed, label, C_WHITE);
}

void button_widget_draw(Canvas *c, int ox, int oy, Button *b)
{
    button_draw(c, ox + b->x, oy + b->y, b->w, b->h, b->label, b->pressed);
}

int button_widget_mouse(Button *b, int type, int mx, int my, int button)
{
    if (button != SDL_BUTTON_LEFT && type != SDL_MOUSEMOTION) return 0;
    int in = button_hit(b->x, b->y, b->w, b->h, mx, my);
    if (type == SDL_MOUSEBUTTONDOWN && in) { b->pressed = 1; return 0; }
    if (type == SDL_MOUSEMOTION && b->pressed) { return 0; }
    if (type == SDL_MOUSEBUTTONUP && b->pressed) { b->pressed = 0; return in; }
    return 0;
}

/* ------------------------------------------------------------ scrollbar ---- */
void scrollbar_clamp(ScrollBar *sb)
{
    if (sb->top > sb->count - sb->rows) sb->top = sb->count - sb->rows;
    if (sb->top < 0) sb->top = 0;
}

void scrollbar_draw(Canvas *c, int ox, int oy, ScrollBar *sb, int focused)
{
    int sx = ox + sb->x, sy = oy + sb->y, sw = sb->w, sh = sb->h;
    cv_fill(c, sx, sy, sw, sh, C_TRACK);
    gc_box(c, sx, sy, sw, 9, 0);
    gc_box(c, sx, sy + sh - 9, sw, 9, 0);
    res_draw(c, "umark", sx + (sw - 9) / 2, sy + 1);
    res_draw(c, "dmark", sx + (sw - 9) / 2, sy + sh - 8);
    int track = sh - 18;
    if (sb->count > sb->rows && track > 4) {
        int th = track * sb->rows / sb->count; if (th < 6) th = 6;
        int ty = sy + 9 + (track - th) * sb->top / (sb->count - sb->rows);
        gc_box(c, sx, ty, sw, th, 0);
    }
    if (focused) cv_rect(c, sx - 1, sy - 1, sw + 2, sh + 2, C_GREEN);
}

int scrollbar_mouse(ScrollBar *sb, int type, int mx, int my, int button)
{
    int inside = mx >= sb->x && my >= sb->y && mx < sb->x + sb->w && my < sb->y + sb->h;
    if (type == SDL_MOUSEBUTTONUP) { int was = sb->drag; sb->drag = 0; return was || inside; }
    if (type == SDL_MOUSEBUTTONDOWN && button == SDL_BUTTON_LEFT && inside) {
        if (my < sb->y + 9) { sb->top--; scrollbar_clamp(sb); return 1; }
        if (my >= sb->y + sb->h - 9) { sb->top++; scrollbar_clamp(sb); return 1; }
        sb->drag = 1;
    }
    if (sb->drag && (type == SDL_MOUSEMOTION || type == SDL_MOUSEBUTTONDOWN)) {
        int track = sb->h - 18;
        if (sb->count > sb->rows && track > 0) {
            int th = track * sb->rows / sb->count; if (th < 6) th = 6;
            int pos = my - sb->y - 9 - th / 2;
            int span = track - th > 0 ? track - th : 1;
            sb->top = pos * (sb->count - sb->rows) / span;
            scrollbar_clamp(sb);
        }
        return 1;
    }
    return inside;
}

/* ------------------------------------------------------------ text edit ---- */
void textedit_set(TextEdit *te, const char *text)
{
    snprintf(te->buf, sizeof te->buf, "%s", text ? text : "");
    te->cursor = (int)strlen(te->buf);
}

void textedit_draw(Canvas *c, int ox, int oy, TextEdit *te, int focused)
{
    int x = ox + te->x, y = oy + te->y, w = te->w, h = TEXTEDIT_H;
    cv_fill(c, x, y, w, h, C_BLACK);
    if (focused) cv_rect(c, x, y, w, h, C_GREEN);
    /* scroll so the caret is visible */
    char shown[TEXTEDIT_MAX + 2];
    snprintf(shown, sizeof shown, "%s", te->buf);
    int start = 0;
    int maxw = w - 6;
    while (1) {
        char tmp[TEXTEDIT_MAX + 2];
        int n = te->cursor - start; if (n < 0) n = 0;
        memcpy(tmp, shown + start, (size_t)n); tmp[n] = 0;
        if (font_small_width(tmp) + 4 <= maxw || start >= te->cursor) break;
        start++;
    }
    cv_clip(c, x + 1, y + 1, w - 2, h - 2);
    int tx = x + 2;
    tx += cv_text_small_noshadow(c, tx, y + 2, shown + start, C_WHITE);
    if (focused && (gui_frame_counter() & 16)) {
        char tmp[TEXTEDIT_MAX + 2];
        int n = te->cursor - start; if (n < 0) n = 0;
        memcpy(tmp, shown + start, (size_t)n); tmp[n] = 0;
        int cx = x + 2 + font_small_width(tmp);
        cv_hline(c, cx, y + 7, 4, C_WHITE);
    }
    (void)tx;
    cv_noclip(c);
}

int textedit_hit(TextEdit *te, int mx, int my)
{
    return mx >= te->x && my >= te->y && mx < te->x + te->w && my < te->y + TEXTEDIT_H;
}

int textedit_key(TextEdit *te, SDL_Keysym *ks, const char *text)
{
    int len = (int)strlen(te->buf);
    int maxlen = te->maxlen ? te->maxlen : TEXTEDIT_MAX;
    if (text) {
        for (const char *p = text; *p; p++) {
            unsigned char ch = (unsigned char)*p;
            if (ch < 32 || ch > 126) continue;
            if (te->numeric && !(ch >= '0' && ch <= '9')) continue;
            if (len >= maxlen) break;
            memmove(te->buf + te->cursor + 1, te->buf + te->cursor, (size_t)(len - te->cursor + 1));
            te->buf[te->cursor++] = (char)ch; len++;
        }
        return 1;
    }
    if (!ks) return 0;
    switch (ks->sym) {
    case SDLK_LEFT: if (te->cursor > 0) te->cursor--; return 1;
    case SDLK_RIGHT: if (te->cursor < len) te->cursor++; return 1;
    case SDLK_HOME: te->cursor = 0; return 1;
    case SDLK_END: te->cursor = len; return 1;
    case SDLK_BACKSPACE:
        if (te->cursor > 0) { memmove(te->buf + te->cursor - 1, te->buf + te->cursor, (size_t)(len - te->cursor + 1)); te->cursor--; }
        return 1;
    case SDLK_DELETE:
        if (te->cursor < len) memmove(te->buf + te->cursor, te->buf + te->cursor + 1, (size_t)(len - te->cursor));
        return 1;
    case SDLK_RETURN: case SDLK_KP_ENTER: return 2;
    default: break;
    }
    return 0;
}

int button_hit(int x, int y, int w, int h, int mx, int my)
{
    return mx >= x && my >= y && mx < x + w && my < y + h;
}

void window_frame_draw(Window *w, Canvas *c)
{
    cv_fill(c, w->x, w->y, w->w, w->h, C_WINBODY);
    cv_fill(c, w->x + 1, w->y + 1, w->w - 2, 12, C_WINTITLE);     /* title bar: 12 rows under the frame */
    cv_rect(c, w->x, w->y, w->w, w->h, C_WINFRAME);
    cv_text(c, w->x + 5, w->y + 3, w->title, C_WHITE);
    if (w->closeable) {
        int bx = w->x + w->w - 11, by = w->y + 2;
        gc_box(c, bx, by, 9, 9, 0);
        res_draw(c, "wx", bx + 1, by + 1);
    }
}

/* ------------------------------------------------------------ list box ---- */
static int lb_rows(ListBox *lb) { return (lb->h - 4) / lb->rowh; }

void listbox_ensure_visible(ListBox *lb)
{
    int rows = lb_rows(lb);
    if (lb->sel < lb->top) lb->top = lb->sel;
    if (lb->sel >= lb->top + rows) lb->top = lb->sel - rows + 1;
    if (lb->top > lb->count - rows) lb->top = lb->count - rows;
    if (lb->top < 0) lb->top = 0;
}


void listbox_draw(ListBox *lb, Canvas *c, int ox, int oy)
{
    int x = ox + lb->x, y = oy + lb->y, w = lb->w, h = lb->h;
    int sbw = 10;
    cv_fill(c, x, y, w, h, C_BLACK);
    cv_rect(c, x, y, w, h, C_BTN);
    int rows = lb_rows(lb);
    cv_clip(c, x + 2, y + 2, w - 4 - sbw, h - 4);
    for (int r = 0; r < rows; r++) {
        int i = lb->top + r;
        if (i >= lb->count) break;
        int ry = y + 2 + r * lb->rowh;
        if (i == lb->sel) cv_fill(c, x + 2, ry, w - 4 - sbw, lb->rowh, C_GREEN);
        int tx = x + 4;
        int ic = lb->icon ? lb->icon(lb->user, i) : 0;
        if (ic == 1) res_draw(c, "nes", tx - 1, ry + 1);
        else if (ic == 2) res_draw(c, "dir", tx - 1, ry + 1);
        else if (ic == 3) res_draw(c, "drive", tx - 1, ry + 1);
        if (ic) tx += 12;
        cv_text(c, tx, ry + 1, lb->text(lb->user, i), i == lb->sel ? C_WHITE : C_TEXT);
    }
    cv_noclip(c);
    /* scrollbar */
    ScrollBar sb = { x + w - 2 - sbw, y + 2, sbw, h - 4, lb->count, rows, lb->top, 0 };
    scrollbar_draw(c, 0, 0, &sb, 0);
}

int listbox_mouse(ListBox *lb, int type, int mx, int my, int button, int *activated)
{
    int sbw = 10;
    int rows = lb_rows(lb);
    if (mx < lb->x || my < lb->y || mx >= lb->x + lb->w || my >= lb->y + lb->h) {
        if (type == SDL_MOUSEBUTTONUP) lb->drag_thumb = 0;
        if (lb->drag_thumb && type == SDL_MOUSEMOTION) goto drag;
        return 0;
    }
    if (type == SDL_MOUSEWHEEL) {
        lb->top -= button * 3; /* button carries wheel y */
        if (lb->top > lb->count - rows) lb->top = lb->count - rows;
        if (lb->top < 0) lb->top = 0;
        return 1;
    }
    int in_sb = mx >= lb->x + lb->w - 2 - sbw;
    if (type == SDL_MOUSEBUTTONDOWN && button == SDL_BUTTON_LEFT) {
        if (in_sb) {
            int sy = lb->y + 2, sh = lb->h - 4;
            if (my < sy + 9) { if (lb->top > 0) lb->top--; }
            else if (my >= sy + sh - 9) { if (lb->top < lb->count - rows) lb->top++; }
            else lb->drag_thumb = 1;
            if (lb->drag_thumb) goto drag;
            return 1;
        }
        int r = (my - lb->y - 2) / lb->rowh;
        int i = lb->top + r;
        if (i >= 0 && i < lb->count) {
            static Uint32 last_click; static int last_i = -1;
            Uint32 now = SDL_GetTicks();
            if (i == lb->sel && last_i == i && now - last_click < 400) { if (activated) *activated = 1; last_click = 0; }
            else { lb->sel = i; last_click = now; }
            last_i = i;
        }
        return 1;
    }
    if (type == SDL_MOUSEBUTTONUP) { lb->drag_thumb = 0; return 1; }
    if (type == SDL_MOUSEMOTION && lb->drag_thumb) {
drag:;
        int sy = lb->y + 2, sh = lb->h - 4, track = sh - 18;
        if (lb->count > rows && track > 0) {
            int th = track * rows / lb->count; if (th < 6) th = 6;
            int pos = my - sy - 9 - th / 2;
            int top = pos * (lb->count - rows) / (track - th > 0 ? track - th : 1);
            if (top < 0) top = 0;
            if (top > lb->count - rows) top = lb->count - rows;
            lb->top = top;
        }
        return 1;
    }
    return 1;
}

int listbox_key(ListBox *lb, SDL_Keycode k, int *activated)
{
    int rows = lb_rows(lb);
    switch (k) {
    case SDLK_UP: if (lb->sel > 0) lb->sel--; break;
    case SDLK_DOWN: if (lb->sel < lb->count - 1) lb->sel++; break;
    case SDLK_PAGEUP: lb->sel -= rows; if (lb->sel < 0) lb->sel = 0; break;
    case SDLK_PAGEDOWN: lb->sel += rows; if (lb->sel >= lb->count) lb->sel = lb->count - 1; break;
    case SDLK_HOME: lb->sel = 0; break;
    case SDLK_END: lb->sel = lb->count - 1; break;
    case SDLK_RETURN: case SDLK_KP_ENTER: if (activated) *activated = 1; return 1;
    default: return 0;
    }
    listbox_ensure_visible(lb);
    return 1;
}

/* ------------------------------------------------------------ messages ---- */
void gui_message(const char *fmt, ...)
{
    char msg[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    if (g_msg_count < MSG_LINES) {
        snprintf(g_msgs[g_msg_count++], 256, "%s", msg);
    } else {
        memmove(g_msgs[0], g_msgs[1], sizeof(g_msgs) - 256);
        snprintf(g_msgs[MSG_LINES - 1], 256, "%s", msg);
    }
    g_popup_timer = 150;
}
const char *gui_message_line(int i) { return (i >= 0 && i < g_msg_count) ? g_msgs[g_msg_count - 1 - i] : ""; }
int gui_message_count(void) { return g_msg_count; }
void gui_tick(void)
{
    if (g_popup_timer > 0) g_popup_timer--;
    g_drip_tick++;
    /* auto-hide: after a few idle seconds the menu bar gets out of the way of
     * the game; it comes back on mouse movement or Esc. Open menus and
     * dialogs keep it visible. */
    if (g_visible && gui_auto_hide && g_open_menu < 0 && g_nwindows == 0) {
        if (++g_idle_ticks >= GUI_AUTO_HIDE_TICKS) { g_visible = 0; g_idle_ticks = 0; }
    } else g_idle_ticks = 0;
}
int gui_frame_counter(void) { return g_drip_tick; }
int gui_window_count(void) { return g_nwindows; }

static void draw_popup(Canvas *c)
{
    if (!g_popup_timer || !g_msg_count || !gui_show_msg_popup) return;
    const char *m = g_msgs[g_msg_count - 1];
    int w = font_text_width(m) + 8;
    int y = c->h - 12;
    cv_fill(c, 0, y, w, 11, C_RED);
    cv_rect(c, 0, y, w, 11, C_DKRED);
    cv_text(c, 4, y + 2, m, C_WHITE);
}

/* ------------------------------------------------------------- windows ---- */
int gui_add_window(Window *w)
{
    for (int i = 0; i < g_nwindows; i++) if (g_windows[i] == w) { w->visible = 1; gui_raise_window(w); return 1; }
    if (g_nwindows >= GUI_MAX_WINDOWS) return 0;
    g_windows[g_nwindows++] = w;
    w->visible = 1;
    return 1;
}

void gui_close_window(Window *w)
{
    for (int i = 0; i < g_nwindows; i++) if (g_windows[i] == w) {
        memmove(&g_windows[i], &g_windows[i + 1], (size_t)(g_nwindows - i - 1) * sizeof(Window *));
        g_nwindows--;
        w->visible = 0;
        if (w->closed) w->closed(w);
        return;
    }
}

void gui_raise_window(Window *w)
{
    for (int i = 0; i < g_nwindows; i++) if (g_windows[i] == w) {
        memmove(&g_windows[i], &g_windows[i + 1], (size_t)(g_nwindows - i - 1) * sizeof(Window *));
        g_windows[g_nwindows - 1] = w;
        return;
    }
}

int gui_any_window_focused(void) { return g_nwindows > 0; }

/* ---------------------------------------------------------------- init ---- */
void gui_init(Menu *menus, int nmenus, void (*command)(int id))
{
    g_menus = menus; g_nmenus = nmenus; g_command = command;
}
void gui_set_maxbox_handler(void (*fn)(void)) { g_maxbox = fn; }
void gui_set_visible(int on) { g_visible = on; if (!on) { g_open_menu = -1; g_open_sub = NULL; } }
int gui_visible(void) { return g_visible; }

void gui_draw(Canvas *c)
{
    g_canvas_w = c->w; g_canvas_h = c->h;
    if (!g_visible) { draw_popup(c); return; }
    draw_menubar(c);
    for (int i = 0; i < g_nwindows; i++) if (g_windows[i]->visible) g_windows[i]->draw(g_windows[i], c);
    if (g_open_menu >= 0) {
        MenuItem *items = g_menus[g_open_menu].items;
        draw_menu_items(c, items, menu_title_x(g_open_menu), MENUBAR_H, g_hover_item, 0);
        if (g_open_sub) draw_menu_items(c, g_open_sub, g_sub_x, g_sub_y, g_sub_hover, 1);
    }
    draw_popup(c);
    /* mouse cursor: golden dagger (early builds) or the arrow from cursor.r2 */
    if (gui_cursor_style == 0) cv_blit_indexed(c, g_mouse_x, g_mouse_y, cursor_sword.px, cursor_sword.w, cursor_sword.h, res_palette);
    else res_draw(c, "cursor", g_mouse_x, g_mouse_y);
}

/* -------------------------------------------------------------- events ---- */
static int hit_menu_title(int mx, int my)
{
    if (my >= MENUBAR_H) return -1;
    for (int i = 0; i < g_nmenus; i++) {
        int x = menu_title_x(i);
        if (mx >= x && mx < x + menu_title_w(i)) return i;
    }
    return -1;
}

static int hit_menu_item_f(MenuItem *items, int x, int y, int mx, int my, int small)
{
    int w = menu_width_f(items, small), h = menu_height(items);
    if (mx < x || my < y || mx >= x + w || my >= y + h) return -1;
    return (my - y) / MENU_LINE;
}
static int hit_menu_item(MenuItem *items, int x, int y, int mx, int my) { return hit_menu_item_f(items, x, y, mx, my, 0); }

static void activate_item(MenuItem *it)
{
    if (!it || !it->label) return;
    if (it->enabled && !it->enabled()) return;
    if (it->sub) return;
    g_open_menu = -1; g_open_sub = NULL;
    if (g_command && it->id) g_command(it->id);
}

static int find_hotkey(MenuItem *items, SDL_Keysym *ks)
{
    char name[16];
    int n = menu_item_count(items);
    const char *kn = SDL_GetKeyName(ks->sym);
    snprintf(name, sizeof name, "%s", kn);
    for (int i = 0; i < n; i++) {
        if (items[i].sub && find_hotkey(items[i].sub, ks)) return 1;
        if (!items[i].label || !items[i].hotkey) continue;
        if (SDL_strcasecmp(items[i].hotkey, name) == 0) {
            /* single letters need Alt (original: alt-<key>); F-keys work bare */
            int is_f = name[0] == 'F' && name[1] >= '0' && name[1] <= '9';
            if (is_f || (ks->mod & KMOD_ALT)) { activate_item(&items[i]); return 1; }
        }
    }
    return 0;
}

/* open the submenu of item hi of the open top-level menu; it goes to the right
 * of the parent, or to the left when it would run off the screen */
static void open_submenu(int hi, int hover)
{
    MenuItem *items = g_menus[g_open_menu].items;
    int px = menu_title_x(g_open_menu), pw = menu_width(items);
    g_open_sub = items[hi].sub;
    int sw = menu_width_f(g_open_sub, 1), sh = menu_height(g_open_sub);
    g_sub_x = px + pw - 2;
    if (g_canvas_w && g_sub_x + sw > g_canvas_w) g_sub_x = px - sw + 2;
    if (g_sub_x < 0) g_sub_x = 0;
    g_sub_y = MENUBAR_H + hi * MENU_LINE;
    if (g_canvas_h && g_sub_y + sh + drip_h > g_canvas_h) g_sub_y = g_canvas_h - sh - drip_h;
    if (g_sub_y < MENUBAR_H) g_sub_y = MENUBAR_H;
    g_sub_hover = hover;
}

int gui_event(const SDL_Event *e, int mx, int my)
{
    if (e->type == SDL_MOUSEMOTION) {
        g_mouse_x = mx; g_mouse_y = my;
        g_idle_ticks = 0;
        if (!g_visible && gui_auto_hide && (e->motion.xrel || e->motion.yrel)) { g_visible = 1; return 1; }
    }
    if (e->type == SDL_MOUSEBUTTONDOWN || e->type == SDL_MOUSEWHEEL || e->type == SDL_KEYDOWN) g_idle_ticks = 0;

    /* keyboard */
    if (e->type == SDL_KEYDOWN) {
        SDL_Keysym ks = e->key.keysym;
        if (ks.sym == SDLK_ESCAPE) { gui_set_visible(!g_visible); g_idle_ticks = 0; return 1; }
        if (!g_visible) {
            /* bare F-keys still work with the GUI hidden */
            for (int i = 0; i < g_nmenus; i++) if (find_hotkey(g_menus[i].items, &ks)) return 1;
            return 0;
        }
        if (g_open_menu >= 0) {
            MenuItem *items = g_open_sub ? g_open_sub : g_menus[g_open_menu].items;
            int *hover = g_open_sub ? &g_sub_hover : &g_hover_item;
            int n = menu_item_count(items);
            switch (ks.sym) {
            case SDLK_DOWN: do { *hover = (*hover + 1) % n; } while (!items[*hover].label); return 1;
            case SDLK_UP: do { *hover = (*hover + n - 1) % n; } while (!items[*hover].label); return 1;
            case SDLK_LEFT: if (g_open_sub) { g_open_sub = NULL; } else { g_open_menu = (g_open_menu + g_nmenus - 1) % g_nmenus; g_hover_item = 0; } return 1;
            case SDLK_RIGHT:
                if (!g_open_sub && g_hover_item >= 0 && g_menus[g_open_menu].items[g_hover_item].sub) {
                    open_submenu(g_hover_item, 0);
                } else { g_open_sub = NULL; g_open_menu = (g_open_menu + 1) % g_nmenus; g_hover_item = 0; }
                return 1;
            case SDLK_RETURN: case SDLK_KP_ENTER:
                if (*hover >= 0) {
                    MenuItem *it = &items[*hover];
                    if (it->sub) open_submenu(g_hover_item, 0);
                    else activate_item(it);
                }
                return 1;
            default: break;
            }
        }
        /* focused window gets keys first */
        if (g_nwindows) {
            Window *w = g_windows[g_nwindows - 1];
            if (w->key && w->key(w, &ks, NULL)) return 1;
        }
        for (int i = 0; i < g_nmenus; i++) if (find_hotkey(g_menus[i].items, &ks)) return 1;
        return 0;
    }
    if (e->type == SDL_TEXTINPUT) {
        if (g_visible && g_nwindows) {
            Window *w = g_windows[g_nwindows - 1];
            if (w->key && w->key(w, NULL, e->text.text)) return 1;
        }
        return 0;
    }
    if (!g_visible) return 0;

    /* mouse */
    if (e->type == SDL_MOUSEMOTION || e->type == SDL_MOUSEBUTTONDOWN || e->type == SDL_MOUSEBUTTONUP || e->type == SDL_MOUSEWHEEL) {
        int button = e->type == SDL_MOUSEWHEEL ? e->wheel.y : e->button.button;
        if (e->type == SDL_MOUSEBUTTONDOWN) g_mouse_down = 1;
        if (e->type == SDL_MOUSEBUTTONUP) g_mouse_down = 0;

        /* open menu takes everything */
        if (g_open_menu >= 0) {
            MenuItem *items = g_menus[g_open_menu].items;
            int mxl = menu_title_x(g_open_menu);
            int t = hit_menu_title(mx, my);
            int hi = hit_menu_item(items, mxl, MENUBAR_H, mx, my);
            int si = g_open_sub ? hit_menu_item_f(g_open_sub, g_sub_x, g_sub_y, mx, my, 1) : -1;
            if (e->type == SDL_MOUSEMOTION) {
                if (t >= 0 && t != g_open_menu) { g_open_menu = t; g_hover_item = -1; g_open_sub = NULL; }
                else if (si >= 0) g_sub_hover = si;
                else if (hi >= 0) {
                    g_hover_item = hi;
                    if (items[hi].sub) open_submenu(hi, -1);
                    else g_open_sub = NULL;
                }
                return 1;
            }
            if (e->type == SDL_MOUSEBUTTONDOWN || e->type == SDL_MOUSEBUTTONUP) {
                if (si >= 0) { if (e->type == SDL_MOUSEBUTTONUP) activate_item(&g_open_sub[si]); return 1; }
                if (hi >= 0) { if (e->type == SDL_MOUSEBUTTONUP) activate_item(&items[hi]); return 1; }
                if (t >= 0) { if (e->type == SDL_MOUSEBUTTONDOWN) { if (t == g_open_menu) { g_open_menu = -1; g_open_sub = NULL; } else { g_open_menu = t; g_hover_item = -1; g_open_sub = NULL; } } return 1; }
                if (e->type == SDL_MOUSEBUTTONDOWN) { g_open_menu = -1; g_open_sub = NULL; }
                return 1;
            }
            return 1;
        }
        /* menu bar */
        if (e->type == SDL_MOUSEBUTTONDOWN && button == SDL_BUTTON_LEFT) {
            int t = hit_menu_title(mx, my);
            if (t >= 0) { g_open_menu = t; g_hover_item = -1; g_open_sub = NULL; return 1; }
            if (g_maxbox && my < MENUBAR_H && mx >= g_canvas_w - 10 && mx < g_canvas_w - 2 && mx >= menu_title_x(g_nmenus) + 2) { g_maxbox(); return 1; }
        }
        /* window drag */
        if (g_drag) {
            if (e->type == SDL_MOUSEMOTION) { g_drag->x = mx - g_drag_dx; g_drag->y = my - g_drag_dy; return 1; }
            if (e->type == SDL_MOUSEBUTTONUP) { g_drag = NULL; return 1; }
        }
        /* windows, topmost first */
        for (int i = g_nwindows - 1; i >= 0; i--) {
            Window *w = g_windows[i];
            if (!w->visible) continue;
            int inside = mx >= w->x && my >= w->y && mx < w->x + w->w && my < w->y + w->h;
            if (!inside && !(e->type == SDL_MOUSEMOTION || e->type == SDL_MOUSEBUTTONUP)) continue;
            if (!inside && (e->type == SDL_MOUSEMOTION || e->type == SDL_MOUSEBUTTONUP)) {
                if (w->mouse) w->mouse(w, e->type, mx - w->x, my - w->y, button);
                continue;
            }
            if (e->type == SDL_MOUSEBUTTONDOWN) gui_raise_window(w);
            int lx = mx - w->x, ly = my - w->y;
            if (e->type == SDL_MOUSEBUTTONDOWN && button == SDL_BUTTON_LEFT && ly < 13) {
                if (w->closeable && lx >= w->w - 11 && lx < w->w - 2) { gui_close_window(w); return 1; }
                g_drag = w; g_drag_dx = lx; g_drag_dy = ly;
                return 1;
            }
            if (w->mouse) w->mouse(w, e->type, lx, ly, button);
            return 1;
        }
        return e->type != SDL_MOUSEMOTION && my < MENUBAR_H;
    }
    return 0;
}
