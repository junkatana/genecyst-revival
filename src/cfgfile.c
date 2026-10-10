#include "cfgfile.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

void cfg_defaults(Config *c)
{
    memset(c, 0, sizeof *c);
    c->res_w = 320; c->res_h = 240; c->scale = 3;
    c->sound = 1; c->rate = 44100; c->stereo = 1;
    c->autohide = 1; c->msg_popup = 1; c->state_popup = 1;
    c->device[0] = 1; c->device[1] = 0;
    for (int p = 0; p < 2; p++) for (int b = 0; b < 12; b++) c->keys[p][b] = -1;   /* -1 = keep built-in default */
}

static void trim(char *s)
{
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
    char *b = s;
    while (*b && isspace((unsigned char)*b)) b++;
    if (b != s) memmove(s, b, strlen(b) + 1);
}

static void parse_keys(const char *v, int *keys)
{
    char buf[256];
    snprintf(buf, sizeof buf, "%s", v);
    int i = 0;
    for (char *t = strtok(buf, ", "); t && i < 12; t = strtok(NULL, ", ")) keys[i++] = atoi(t);
}

int cfg_load(Config *c, const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[1400];
    while (fgets(line, sizeof line, f)) {
        char *eq = strchr(line, '=');
        if (!eq || line[0] == '#' || line[0] == ';') continue;
        *eq = 0;
        char *k = line, *v = eq + 1;
        trim(k); trim(v);
#define I(name, field) if (!strcmp(k, name)) { c->field = atoi(v); continue; }
#define S(name, field) if (!strcmp(k, name)) { snprintf(c->field, sizeof c->field, "%s", v); continue; }
        I("res_w", res_w) I("res_h", res_h) I("scale", scale) I("fullscreen", fullscreen) I("filter", filter)
        I("sound", sound) I("rate", rate) I("stereo", stereo) I("version", region)
        I("autohide", autohide) I("cursor", cursor) I("msg_popup", msg_popup) I("state_popup", state_popup)
        I("sixbutton1", six_button[0]) I("sixbutton2", six_button[1]) I("device1", device[0]) I("device2", device[1])
        S("romdir", romdir) S("savedir", savedir) S("pcxdir", pcxdir) S("logdir", logdir) S("patdir", patdir)
#undef I
#undef S
        if (!strcmp(k, "keys1")) parse_keys(v, c->keys[0]);
        else if (!strcmp(k, "keys2")) parse_keys(v, c->keys[1]);
    }
    fclose(f);
    return 1;
}

int cfg_save(const Config *c, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    fprintf(f, "# genecyst.cfg -- written on exit; command-line options override these values\n");
    fprintf(f, "res_w = %d\nres_h = %d\nscale = %d\nfullscreen = %d\nfilter = %d\n", c->res_w, c->res_h, c->scale, c->fullscreen, c->filter);
    fprintf(f, "sound = %d\nrate = %d\nstereo = %d\nversion = %d\n", c->sound, c->rate, c->stereo, c->region);
    fprintf(f, "autohide = %d\ncursor = %d\nmsg_popup = %d\nstate_popup = %d\n", c->autohide, c->cursor, c->msg_popup, c->state_popup);
    fprintf(f, "sixbutton1 = %d\nsixbutton2 = %d\ndevice1 = %d\ndevice2 = %d\n", c->six_button[0], c->six_button[1], c->device[0], c->device[1]);
    fprintf(f, "romdir = %s\nsavedir = %s\npcxdir = %s\nlogdir = %s\npatdir = %s\n", c->romdir, c->savedir, c->pcxdir, c->logdir, c->patdir);
    for (int p = 0; p < 2; p++) {
        fprintf(f, "keys%d =", p + 1);
        for (int b = 0; b < 12; b++) fprintf(f, "%s%d", b ? "," : " ", c->keys[p][b]);
        fprintf(f, "\n");
    }
    fclose(f);
    return 1;
}
