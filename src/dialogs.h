#ifndef GENECYST_DIALOGS_H
#define GENECYST_DIALOGS_H

/* callbacks into the frontend */
typedef struct {
    void (*rom_picked)(const char *path);
    void (*set_resolution)(int w, int h);
    void (*get_resolution)(int *w, int *h);
    void (*set_vfps)(int fps);          /* 0 = auto (50/60 by region) */
    int  (*get_vfps)(void);
} DialogHost;

void dlg_init(const DialogHost *host);
void dlg_loadrom_open(void);
void dlg_about_open(void);
void dlg_messages_open(void);
void dlg_directories_open(void);
void dlg_palette_open(void);
void dlg_patterns_open(void);
void dlg_patcher_open(void);
void dlg_setres_open(void);
void dlg_input_open(int port);
void dlg_timing_open(void);
#endif
