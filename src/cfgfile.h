/* cfgfile.h -- genecyst.cfg: simple "key = value" settings file, written on
 * exit and read at start-up (command-line options override it). */
#ifndef GENECYST_CFGFILE_H
#define GENECYST_CFGFILE_H

typedef struct {
    int res_w, res_h, scale, fullscreen, filter;
    int sound, rate, stereo, region;          /* region: 0 auto, 1 USA, 2 Europe, 3 Japan */
    int autohide, cursor, msg_popup, state_popup;
    int six_button[2], device[2];
    int keys[2][12];                           /* SDL scancodes, BTN_* order */
    char romdir[1024], savedir[1024], pcxdir[1024], logdir[1024], patdir[1024];
} Config;

void cfg_defaults(Config *c);
int  cfg_load(Config *c, const char *path);   /* 1 when the file existed */
int  cfg_save(const Config *c, const char *path);
#endif
