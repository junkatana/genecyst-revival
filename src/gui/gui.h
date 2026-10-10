/* gui.h -- Genecyst-style in-screen GUI: menu bar with blood drips, dropdown
 * menus, windows, list boxes, buttons, message popups. Everything is drawn into
 * a Canvas in native screen pixels; the frontend scales it up. */
#ifndef GENECYST_GUI_H
#define GENECYST_GUI_H
#include "canvas.h"
#include "res_data.h"
#include <SDL.h>

#define MENUBAR_H 10
#define MENU_LINE 10
#define GUI_MAX_WINDOWS 8

typedef struct MenuItem {
    const char *label;      /* NULL = separator */
    const char *hotkey;     /* right column text, e.g. "F5", may be NULL */
    int id;                 /* command id (CMD_*), 0 for submenu parents */
    struct MenuItem *sub;   /* submenu items (terminated by label==NULL && id==-1) */
    int (*checked)(void);   /* NULL = no checkbox; else returns 1 when checked */
    int (*enabled)(void);   /* NULL = always enabled */
} MenuItem;

typedef struct {
    const char *title;
    MenuItem *items;
} Menu;

/* ---- windows --------------------------------------------------------------- */
typedef struct Window Window;
struct Window {
    char title[64];
    int x, y, w, h;
    int visible;
    int closeable;
    void (*draw)(Window *, Canvas *);
    /* return 1 if consumed. mx,my are window-relative */
    int (*mouse)(Window *, int type, int mx, int my, int button);
    int (*key)(Window *, SDL_Keysym *, const char *text);
    void (*closed)(Window *);
    void *user;
};

/* ---- list box -------------------------------------------------------------- */
typedef struct {
    int x, y, w, h;          /* window-relative */
    int count;
    int top, sel;
    int rowh;
    const char *(*text)(void *user, int i);
    int (*icon)(void *user, int i);  /* 0 none, 1 file, 2 folder, 3 drive */
    void *user;
    int drag_thumb;
} ListBox;

void listbox_draw(ListBox *lb, Canvas *c, int ox, int oy);
int  listbox_mouse(ListBox *lb, int type, int mx, int my, int button, int *activated);
int  listbox_key(ListBox *lb, SDL_Keycode k, int *activated);
void listbox_ensure_visible(ListBox *lb);

/* ---- buttons --------------------------------------------------------------- */
void button_draw(Canvas *c, int x, int y, int w, int h, const char *label, int pressed);
int  button_hit(int x, int y, int w, int h, int mx, int my);
void window_frame_draw(Window *w, Canvas *c);

/* push button with press/release tracking (coords window-relative) */
typedef struct { int x, y, w, h; const char *label; int pressed; int enabled; } Button;
void button_widget_draw(Canvas *c, int ox, int oy, Button *b);
/* returns 1 when the button was clicked (released inside) */
int  button_widget_mouse(Button *b, int type, int mx, int my, int button);

/* ---- vertical scroll bar ---------------------------------------------------- */
typedef struct { int x, y, w, h; int count, rows, top; int drag; } ScrollBar;
void scrollbar_draw(Canvas *c, int ox, int oy, ScrollBar *sb, int focused);
int  scrollbar_mouse(ScrollBar *sb, int type, int mx, int my, int button);  /* 1 = consumed */
void scrollbar_clamp(ScrollBar *sb);

/* ---- text edit --------------------------------------------------------------- */
#define TEXTEDIT_MAX 255
typedef struct { int x, y, w; char buf[TEXTEDIT_MAX + 1]; int cursor; int numeric; int maxlen; } TextEdit;
#define TEXTEDIT_H 9
void textedit_set(TextEdit *te, const char *text);
void textedit_draw(Canvas *c, int ox, int oy, TextEdit *te, int focused);
/* returns 0 = not handled, 1 = handled, 2 = Enter pressed */
int  textedit_key(TextEdit *te, SDL_Keysym *ks, const char *text);
int  textedit_hit(TextEdit *te, int mx, int my);

/* ---- GUI singleton --------------------------------------------------------- */
void gui_init(Menu *menus, int nmenus, void (*command)(int id));
void gui_set_maxbox_handler(void (*fn)(void));   /* square box at the top-right of the menu bar */
void gui_set_visible(int on);
int  gui_visible(void);
void gui_draw(Canvas *c);
/* feed SDL events; mouse coords must already be in canvas space. Returns 1 if consumed. */
int  gui_event(const SDL_Event *e, int mx, int my);
void gui_message(const char *fmt, ...);       /* message log + popup (printf style) */
const char *gui_message_line(int i);          /* i=0 most recent */
int  gui_message_count(void);
int  gui_add_window(Window *w);
void gui_close_window(Window *w);
void gui_raise_window(Window *w);
int  gui_any_window_focused(void);
void gui_tick(void);                          /* per-frame timers (popup fade) */
int  gui_frame_counter(void);                 /* for caret blinking */
int  gui_window_count(void);
extern int gui_show_msg_popup;
extern int gui_show_state_popup;
extern int gui_auto_hide;
extern int gui_cursor_style;                  /* 0 = golden dagger (early Genecyst), 1 = arrow (cursor.r2) */
extern const ResImage cursor_sword;                     /* hide the GUI after GUI_AUTO_HIDE_TICKS without interaction */
#define GUI_AUTO_HIDE_TICKS 180               /* 3 s at 60 Hz */

/* bitmap resources */
extern const int drip_w, drip_h, drip_frames;
extern const uint8_t drip_bright[][24][31];
extern const uint8_t drip_dark[][24][31];
extern const uint8_t drip_light[][24][31];   /* light highlight pixels (#ED5355) */
#define DRIP_FRAME_TICKS 20   /* ~1/3 s per state at 60 Hz, measured on the original */
#endif
