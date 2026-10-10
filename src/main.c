/* main.c -- Genecyst revival: SDL2 frontend, menus, input, timing */
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "emu.h"
#include "paths.h"
#include "dialogs.h"
#include "gui/gui.h"
#include "gui/font.h"
#include "filters.h"
#include "pads.h"
#include "cfgfile.h"
#include "shared.h"   /* INPUT_* bits, config, vdp_pal */
#include "vdp_render.h"   /* LAYER_* bits */

/* ------------------------------------------------------------ settings ---- */
static int screen_w = 320, screen_h = 240;   /* virtual DOS screen */
static int scale = 3;
static int vfps_override = 0;   /* Timing dialog: 0 = auto */
static int wave_output = 1;     /* Sound > Wave output: audio device unpaused */
static int show_fm_info = 0;    /* Sound > Show FM info overlay */
static char cfg_path[1200] = "genecyst.cfg";

static int frameskip = 0;        /* 0 = auto */
static int vsync = 1;
static int sound_on = 1;
static int stereo = 1;
static int sample_rate = 44100;
static int region_pref = 0;      /* 0 auto, 1 USA, 2 Europe, 3 Japan */
static int running = 1;
static int cpu_skip = 0;
static int filter_id = FILTER_NONE;   /* View > Filter */
static int out_f = 1;                 /* filter output factor */
static uint32_t *out_px;              /* filtered frame (screen_w*out_f x screen_h*out_f) */

/* game window position inside the virtual screen */
static int game_x = 0, game_y = 8;
static int game_maximized = 0;    /* maximize box: integer-zoom the game to fill a larger virtual screen */

static Canvas canvas;
static uint32_t *screen_px;
static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static SDL_AudioDeviceID audio_dev;

/* ------------------------------------------------------------ commands ---- */
enum {
    CMD_NONE = 0,
    CMD_LOADROM, CMD_FREEROM, CMD_DIRS, CMD_FIXCHECKSUM, CMD_SNAPSHOT, CMD_EXIT,
    CMD_MESSAGES, CMD_PALETTE, CMD_PATTERNS, CMD_SETRES, CMD_SIX1, CMD_SIX2,
    CMD_RESET, CMD_PAUSE, CMD_ADVANCE, CMD_CPUSKIP, CMD_SAVESTATE, CMD_LOADSTATE, CMD_PATCH, CMD_SAVEGST,
    CMD_INPUT1, CMD_INPUT2, CMD_RES0, CMD_RES1, CMD_RES2, CMD_RES3, CMD_RES4, CMD_RES5,
    CMD_FS_AUTO, CMD_FS_0, CMD_FS_1, CMD_FS_2, CMD_FS_3, CMD_VSYNC, CMD_TIMING,
    CMD_DMAFIX, CMD_FORCEDISP, CMD_HINT, CMD_RASTER, CMD_AUTOVER,
    CMD_VER_USA, CMD_VER_JAP, CMD_VER_EUR, CMD_VER_AUTO, CMD_SIXBUTTON,
    CMD_RATE11, CMD_RATE22, CMD_RATE44, CMD_STEREO, CMD_Z80, CMD_Z80FAKE, CMD_FM, CMD_PSG, CMD_DAC,
    CMD_WAVEOUT, CMD_FMINFO, CMD_LOGWAVE, CMD_LOGYM,
    CMD_HIDEGUI, CMD_AUTOHIDE, CMD_MSGPOPUP, CMD_STATEPOPUP, CMD_ABOUT, CMD_CURSOR_SWORD, CMD_CURSOR_ARROW,
    CMD_SCALE1, CMD_SCALE2, CMD_SCALE3, CMD_SCALE4, CMD_FULLSCREEN,
    CMD_FILTER_BASE,   /* + filter id, FILTER_COUNT entries */
    CMD_FILTER_LAST = CMD_FILTER_BASE + FILTER_COUNT - 1,
    CMD_LAYER_BASE,    /* + 0..5: B low, A low, B high, A high, sprites, window */
    CMD_LAYER_LAST = CMD_LAYER_BASE + 5,
    CMD_DUMP_BASE,     /* + DUMP_* */
    CMD_DUMP_LAST = CMD_DUMP_BASE + DUMP_COUNT - 1,
    CMD_RESN_BASE,     /* + index into res_table */
    CMD_RESN_LAST = CMD_RESN_BASE + 8,
};
static const struct { int w, h; } res_table[9] = {
    { 256, 224 }, { 256, 240 }, { 256, 256 }, { 320, 200 }, { 320, 224 }, { 320, 240 }, { 384, 257 }, { 640, 400 }, { 640, 480 } };
static const int layer_bits[6] = { LAYER_B_LOW, LAYER_A_LOW, LAYER_B_HIGH, LAYER_A_HIGH, LAYER_SPRITES, LAYER_WINDOW };

static int chk_pause(void) { return emu.loaded && !emu.running; }
static int chk_cpuskip(void) { return cpu_skip; }
static int chk_vsync(void) { return vsync; }
static int chk_six1(void) { return emu_six_button_port(0); }
static int chk_six2(void) { return emu_six_button_port(1); }
static int chk_stereo(void) { return stereo; }
static int chk_fm(void) { return emu_fm(); }
static int chk_psg(void) { return emu_psg(); }
static int chk_dac(void) { return emu_dac(); }
static int chk_wave(void) { return wave_output; }
static int chk_fminfo(void) { return show_fm_info; }
static int chk_logwav(void) { return emu_wav_logging(); }
static int chk_loggym(void) { return emu_gym_logging(); }
static int en_never(void) { return 0; }
static int chk_msgpopup(void) { return gui_show_msg_popup; }
static int chk_autohide(void) { return gui_auto_hide; }
static int chk_cur_sword(void) { return gui_cursor_style == 0; }
static int chk_cur_arrow(void) { return gui_cursor_style == 1; }
static int chk_statepopup(void) { return gui_show_state_popup; }
static int chk_rate11(void) { return sample_rate == 11025; }
static int chk_rate22(void) { return sample_rate == 22050; }
static int chk_rate44(void) { return sample_rate == 44100; }
static int chk_usa(void) { return region_pref == 1; }
static int chk_eur(void) { return region_pref == 2; }
static int chk_jap(void) { return region_pref == 3; }
static int chk_auto(void) { return region_pref == 0; }
static int chk_fs_auto(void) { return frameskip == 0; }
static int chk_fs0(void) { return frameskip == 1; }
static int chk_fs1(void) { return frameskip == 2; }
static int chk_fs2(void) { return frameskip == 3; }
static int chk_fs3(void) { return frameskip == 4; }
static int chk_scale1(void) { return scale == 1; }
static int chk_scale2(void) { return scale == 2; }
static int chk_scale3(void) { return scale == 3; }
static int chk_scale4(void) { return scale == 4; }
#define CHK_RES(n) static int chk_resn##n(void) { return screen_w == res_table[n].w && screen_h == res_table[n].h; }
CHK_RES(0) CHK_RES(1) CHK_RES(2) CHK_RES(3) CHK_RES(4) CHK_RES(5) CHK_RES(6) CHK_RES(7) CHK_RES(8)
#define CHK_LAYER(n) static int chk_layer##n(void) { return emu_layer(layer_bits[n]); }
CHK_LAYER(0) CHK_LAYER(1) CHK_LAYER(2) CHK_LAYER(3) CHK_LAYER(4) CHK_LAYER(5)
static int en_loaded(void) { return emu.loaded; }
#define CHK_FILTER(n) static int chk_filter##n(void) { return filter_id == n; }
CHK_FILTER(0) CHK_FILTER(1) CHK_FILTER(2) CHK_FILTER(3) CHK_FILTER(4) CHK_FILTER(5) CHK_FILTER(6) CHK_FILTER(7)
static int chk_sound(void) { return sound_on; }

static MenuItem input_sub[] = {
    { "Device 1", NULL, CMD_INPUT1, NULL, NULL, NULL },
    { "Device 2", NULL, CMD_INPUT2, NULL, NULL, NULL },
    { NULL, NULL, -1, NULL, NULL, NULL } };
static MenuItem filter_sub[] = {
    { "None",        NULL, CMD_FILTER_BASE + 0, NULL, chk_filter0, NULL },
    { NULL, NULL, 0, NULL, NULL, NULL },
    { "Scale2x",     NULL, CMD_FILTER_BASE + 1, NULL, chk_filter1, NULL },
    { "2xSaI",       NULL, CMD_FILTER_BASE + 2, NULL, chk_filter2, NULL },
    { "Super 2xSaI", NULL, CMD_FILTER_BASE + 3, NULL, chk_filter3, NULL },
    { "Super Eagle", NULL, CMD_FILTER_BASE + 4, NULL, chk_filter4, NULL },
    { NULL, NULL, 0, NULL, NULL, NULL },
    { "Scanlines",   NULL, CMD_FILTER_BASE + 5, NULL, chk_filter5, NULL },
    { "TV",          NULL, CMD_FILTER_BASE + 6, NULL, chk_filter6, NULL },
    { "CRT",         NULL, CMD_FILTER_BASE + 7, NULL, chk_filter7, NULL },
    { NULL, NULL, -1, NULL, NULL, NULL } };
static MenuItem res_sub[] = {
    { "256x224", NULL,  CMD_RESN_BASE + 0, NULL, chk_resn0, NULL },
    { "256x240", NULL,  CMD_RESN_BASE + 1, NULL, chk_resn1, NULL },
    { "256x256", "F11", CMD_RESN_BASE + 2, NULL, chk_resn2, NULL },
    { "320x200", NULL,  CMD_RESN_BASE + 3, NULL, chk_resn3, NULL },
    { "320x224", NULL,  CMD_RESN_BASE + 4, NULL, chk_resn4, NULL },
    { "320x240", "F12", CMD_RESN_BASE + 5, NULL, chk_resn5, NULL },
    { "384x257", NULL,  CMD_RESN_BASE + 6, NULL, chk_resn6, NULL },
    { "640x400", NULL,  CMD_RESN_BASE + 7, NULL, chk_resn7, NULL },
    { "640x480", NULL,  CMD_RESN_BASE + 8, NULL, chk_resn8, NULL },
    { "Other...", NULL, CMD_SETRES, NULL, NULL, NULL },
    { NULL, NULL, -1, NULL, NULL, NULL } };
static MenuItem window_sub[] = {
    { "1x", NULL, CMD_SCALE1, NULL, chk_scale1, NULL },
    { "2x", NULL, CMD_SCALE2, NULL, chk_scale2, NULL },
    { "3x", NULL, CMD_SCALE3, NULL, chk_scale3, NULL },
    { "4x", NULL, CMD_SCALE4, NULL, chk_scale4, NULL },
    { NULL, NULL, 0, NULL, NULL, NULL },
    { "Fullscreen", "A-RET", CMD_FULLSCREEN, NULL, NULL, NULL },
    { NULL, NULL, -1, NULL, NULL, NULL } };
static MenuItem fs_sub[] = {
    { "Auto", NULL, CMD_FS_AUTO, NULL, chk_fs_auto, NULL },
    { NULL, NULL, 0, NULL, NULL, NULL },
    { "1", NULL, CMD_FS_0, NULL, chk_fs0, NULL },
    { "2", NULL, CMD_FS_1, NULL, chk_fs1, NULL },
    { "3", NULL, CMD_FS_2, NULL, chk_fs2, NULL },
    { "4", NULL, CMD_FS_3, NULL, chk_fs3, NULL },
    { NULL, NULL, -1, NULL, NULL, NULL } };
static MenuItem ver_sub[] = {
    { "USA", NULL, CMD_VER_USA, NULL, chk_usa, NULL },
    { "Japan", NULL, CMD_VER_JAP, NULL, chk_jap, NULL },
    { "Europe", NULL, CMD_VER_EUR, NULL, chk_eur, NULL },
    { NULL, NULL, -1, NULL, NULL, NULL } };
static MenuItem six_sub[] = {
    { "Device 1", NULL, CMD_SIX1, NULL, chk_six1, NULL },
    { "Device 2", NULL, CMD_SIX2, NULL, chk_six2, NULL },
    { NULL, NULL, -1, NULL, NULL, NULL } };
static MenuItem layer_sub[] = {
    { "Scroll B low",  NULL, CMD_LAYER_BASE + 0, NULL, chk_layer0, NULL },
    { "Scroll A low",  NULL, CMD_LAYER_BASE + 1, NULL, chk_layer1, NULL },
    { "Scroll B high", NULL, CMD_LAYER_BASE + 2, NULL, chk_layer2, NULL },
    { "Scroll A high", NULL, CMD_LAYER_BASE + 3, NULL, chk_layer3, NULL },
    { "Sprites",       NULL, CMD_LAYER_BASE + 4, NULL, chk_layer4, NULL },
    { "Window",        NULL, CMD_LAYER_BASE + 5, NULL, chk_layer5, NULL },
    { NULL, NULL, -1, NULL, NULL, NULL } };
static MenuItem dump_sub[] = {
    { "RAM",         NULL, CMD_DUMP_BASE + DUMP_RAM, NULL, NULL, en_loaded },
    { "ROM",         NULL, CMD_DUMP_BASE + DUMP_ROM, NULL, NULL, en_loaded },
    { "VRAM",        NULL, CMD_DUMP_BASE + DUMP_VRAM, NULL, NULL, en_loaded },
    { "VSRAM",       NULL, CMD_DUMP_BASE + DUMP_VSRAM, NULL, NULL, en_loaded },
    { "CRAM",        NULL, CMD_DUMP_BASE + DUMP_CRAM, NULL, NULL, en_loaded },
    { "Z80 RAM",     NULL, CMD_DUMP_BASE + DUMP_Z80RAM, NULL, NULL, en_loaded },
    { "YM2612 regs", NULL, CMD_DUMP_BASE + DUMP_YMREGS, NULL, NULL, en_loaded },
    { "Messages",    NULL, CMD_DUMP_BASE + DUMP_MESSAGES, NULL, NULL, NULL },
    { "VDP base",    NULL, CMD_DUMP_BASE + DUMP_VDPBASE, NULL, NULL, en_loaded },
    { "VDP regs",    NULL, CMD_DUMP_BASE + DUMP_VDPREGS, NULL, NULL, en_loaded },
    { "Sprites",     NULL, CMD_DUMP_BASE + DUMP_SPRITES, NULL, NULL, en_loaded },
    { NULL, NULL, -1, NULL, NULL, NULL } };
static MenuItem rate_sub[] = {
    { "11025", NULL, CMD_RATE11, NULL, chk_rate11, NULL },
    { "22050", NULL, CMD_RATE22, NULL, chk_rate22, NULL },
    { "44100", NULL, CMD_RATE44, NULL, chk_rate44, NULL },
    { NULL, NULL, -1, NULL, NULL, NULL } };

static MenuItem file_items[] = {
    { "Load ROM...", "L", CMD_LOADROM, NULL, NULL, NULL },
    { "Free ROM", NULL, CMD_FREEROM, NULL, NULL, en_loaded },
    { "Directories...", NULL, CMD_DIRS, NULL, NULL, NULL },
    { "Fix ROM checksum", NULL, CMD_FIXCHECKSUM, NULL, NULL, en_loaded },
    { NULL, NULL, 0, NULL, NULL, NULL },
    { "Save snapshot", "F9", CMD_SNAPSHOT, NULL, NULL, en_loaded },
    { NULL, NULL, 0, NULL, NULL, NULL },
    { "Exit", "Q", CMD_EXIT, NULL, NULL, NULL },
    { NULL, NULL, -1, NULL, NULL, NULL } };
static MenuItem view_items[] = {
    { "Messages", "F1", CMD_MESSAGES, NULL, NULL, NULL },
    { "Palette", NULL, CMD_PALETTE, NULL, NULL, en_loaded },
    { "Patterns", NULL, CMD_PATTERNS, NULL, NULL, en_loaded },
    { "Layer", NULL, 0, layer_sub, NULL, NULL },
    { NULL, NULL, 0, NULL, NULL, NULL },
    { "Filter", NULL, 0, filter_sub, NULL, NULL },
    { NULL, NULL, -1, NULL, NULL, NULL } };
static MenuItem cpu_items[] = {
    { "Reset", "R", CMD_RESET, NULL, NULL, en_loaded },
    { "Pause", "P", CMD_PAUSE, NULL, chk_pause, en_loaded },
    { "Advance frame", "A", CMD_ADVANCE, NULL, NULL, en_loaded },
    { "CPU skip", "S", CMD_CPUSKIP, NULL, chk_cpuskip, NULL },
    { NULL, NULL, 0, NULL, NULL, NULL },
    { "Save state", "F5", CMD_SAVESTATE, NULL, NULL, en_loaded },
    { "Restore state", "F7", CMD_LOADSTATE, NULL, NULL, en_loaded },
    { "ROM patch...", "F6", CMD_PATCH, NULL, NULL, en_loaded },
    { "Save GST state", NULL, CMD_SAVEGST, NULL, NULL, en_loaded },
    { "Dump", NULL, 0, dump_sub, NULL, NULL },
    { NULL, NULL, -1, NULL, NULL, NULL } };
static MenuItem settings_items[] = {
    { "Input", NULL, 0, input_sub, NULL, NULL },
    { "Resolution", NULL, 0, res_sub, NULL, NULL },
    { "Frameskip", NULL, 0, fs_sub, NULL, NULL },
    { "Vsync", NULL, CMD_VSYNC, NULL, chk_vsync, NULL },
    { NULL, NULL, 0, NULL, NULL, NULL },
    { "Timing...", NULL, CMD_TIMING, NULL, NULL, NULL },
    { "DMA fix", NULL, CMD_DMAFIX, NULL, NULL, NULL },
    { "Force display", NULL, CMD_FORCEDISP, NULL, NULL, NULL },
    { "H-Interrupt enable", "H", CMD_HINT, NULL, NULL, NULL },
    { "Raster effects", "E", CMD_RASTER, NULL, NULL, NULL },
    { "Auto-Version", NULL, CMD_AUTOVER, NULL, chk_auto, NULL },
    { "Version", NULL, 0, ver_sub, NULL, NULL },
    { "6 Button", NULL, 0, six_sub, NULL, NULL },
    { NULL, NULL, -1, NULL, NULL, NULL } };
static MenuItem sound_items[] = {
    { "Rate", NULL, 0, rate_sub, NULL, NULL },
    { "Stereo", NULL, CMD_STEREO, NULL, chk_stereo, NULL },
    { NULL, NULL, 0, NULL, NULL, NULL },
    { "Z80 enable", "Z", CMD_Z80, NULL, chk_sound, NULL },
    { "Z80 faking", NULL, CMD_Z80FAKE, NULL, NULL, en_never },
    { NULL, NULL, 0, NULL, NULL, NULL },
    { "FM music enable", "M", CMD_FM, NULL, chk_fm, NULL },
    { "PSG sound enable", "G", CMD_PSG, NULL, chk_psg, NULL },
    { "DAC sound enable", "D", CMD_DAC, NULL, chk_dac, NULL },
    { NULL, NULL, 0, NULL, NULL, NULL },
    { "Wave output", "W", CMD_WAVEOUT, NULL, chk_wave, NULL },
    { "Show FM info", "F", CMD_FMINFO, NULL, chk_fminfo, NULL },
    { NULL, NULL, 0, NULL, NULL, NULL },
    { "Log wave", NULL, CMD_LOGWAVE, NULL, chk_logwav, en_loaded },
    { "Log YM2612", NULL, CMD_LOGYM, NULL, chk_loggym, en_loaded },
    { NULL, NULL, -1, NULL, NULL, NULL } };
static MenuItem cursor_sub[] = {
    { "Cross", NULL, CMD_CURSOR_SWORD, NULL, chk_cur_sword, NULL },
    { "Arrow", NULL, CMD_CURSOR_ARROW, NULL, chk_cur_arrow, NULL },
    { NULL, NULL, -1, NULL, NULL, NULL } };
static MenuItem misc_items[] = {
    { "Hide GUI", NULL, CMD_HIDEGUI, NULL, NULL, NULL },
    { "Auto hide GUI", NULL, CMD_AUTOHIDE, NULL, chk_autohide, NULL },
    { "Message popup", NULL, CMD_MSGPOPUP, NULL, chk_msgpopup, NULL },
    { "State popup", NULL, CMD_STATEPOPUP, NULL, chk_statepopup, NULL },
    { NULL, NULL, 0, NULL, NULL, NULL },
    { "Window", NULL, 0, window_sub, NULL, NULL },
    { "Cursor", NULL, 0, cursor_sub, NULL, NULL },
    { NULL, NULL, 0, NULL, NULL, NULL },
    { "About...", NULL, CMD_ABOUT, NULL, NULL, NULL },
    { NULL, NULL, -1, NULL, NULL, NULL } };
static Menu menus[] = {
    { "File", file_items }, { "View", view_items }, { "CPU", cpu_items },
    { "Settings", settings_items }, { "Sound", sound_items }, { "Misc", misc_items } };

/* ---- scripted demo / frame dump (used to record presentation videos without
 * touching the real desktop): -script <file> feeds synthetic events at frame
 * numbers, -framedump <file> writes every frame as 640x480 raw RGB24 -------- */
typedef struct { int frame; char cmd[12]; int a, b; char text[96]; } ScriptLine;
static ScriptLine script[1024];
static int nscript, script_pos, frame_no, exit_after;
static FILE *framedump;

static void script_load(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "script: cannot open %s\n", path); return; }
    char line[256];
    while (fgets(line, sizeof line, f) && nscript < 1024) {
        ScriptLine *l = &script[nscript];
        memset(l, 0, sizeof *l);
        if (line[0] == '#' || line[0] == '\n') continue;
        int n = sscanf(line, "%d %11s %d %d", &l->frame, l->cmd, &l->a, &l->b);
        if (n < 2) continue;
        if (!strcmp(l->cmd, "key") || !strcmp(l->cmd, "altkey") || !strcmp(l->cmd, "text")) {
            char *p = strstr(line, l->cmd) + strlen(l->cmd);
            while (*p == ' ') p++;
            p[strcspn(p, "\r\n")] = 0;
            snprintf(l->text, sizeof l->text, "%s", p);
        }
        nscript++;
    }
    fclose(f);
}

static void script_push_mouse(Uint32 type, int x, int y)
{
    SDL_Event ev; memset(&ev, 0, sizeof ev);
    ev.type = type;
    if (type == SDL_MOUSEMOTION) { ev.motion.x = x * out_f; ev.motion.y = y * out_f; ev.motion.xrel = 1; ev.motion.yrel = 1; }
    else { ev.button.x = x * out_f; ev.button.y = y * out_f; ev.button.button = SDL_BUTTON_LEFT; ev.button.state = type == SDL_MOUSEBUTTONDOWN ? SDL_PRESSED : SDL_RELEASED; }
    SDL_PushEvent(&ev);
}

static void script_push_key(const char *name, int alt)
{
    SDL_Event ev; memset(&ev, 0, sizeof ev);
    SDL_Keycode k = SDL_GetKeyFromName(name);
    if (k == SDLK_UNKNOWN) return;
    ev.type = SDL_KEYDOWN; ev.key.state = SDL_PRESSED;
    ev.key.keysym.sym = k; ev.key.keysym.scancode = SDL_GetScancodeFromKey(k); ev.key.keysym.mod = alt ? KMOD_LALT : 0;
    SDL_PushEvent(&ev);
    ev.type = SDL_KEYUP; ev.key.state = SDL_RELEASED;
    SDL_PushEvent(&ev);
}

static void script_tick(void)
{
    while (script_pos < nscript && script[script_pos].frame <= frame_no) {
        ScriptLine *l = &script[script_pos++];
        if (!strcmp(l->cmd, "move")) script_push_mouse(SDL_MOUSEMOTION, l->a, l->b);
        else if (!strcmp(l->cmd, "click")) { script_push_mouse(SDL_MOUSEMOTION, l->a, l->b); script_push_mouse(SDL_MOUSEBUTTONDOWN, l->a, l->b); script_push_mouse(SDL_MOUSEBUTTONUP, l->a, l->b); }
        else if (!strcmp(l->cmd, "down")) { script_push_mouse(SDL_MOUSEMOTION, l->a, l->b); script_push_mouse(SDL_MOUSEBUTTONDOWN, l->a, l->b); }
        else if (!strcmp(l->cmd, "up")) { script_push_mouse(SDL_MOUSEMOTION, l->a, l->b); script_push_mouse(SDL_MOUSEBUTTONUP, l->a, l->b); }
        else if (!strcmp(l->cmd, "key")) script_push_key(l->text, 0);
        else if (!strcmp(l->cmd, "altkey")) script_push_key(l->text, 1);
        else if (!strcmp(l->cmd, "text")) {
            for (const char *p = l->text; *p; p++) { SDL_Event ev; memset(&ev, 0, sizeof ev); ev.type = SDL_TEXTINPUT; ev.text.text[0] = *p; SDL_PushEvent(&ev); }
        }
        else if (!strcmp(l->cmd, "quit")) running = 0;
    }
}

static void framedump_write(void)
{
    static uint8_t *rgb;
    if (!framedump) return;
    if (!rgb) rgb = malloc(640 * 480 * 3);
    for (int y = 0; y < 480; y++) {
        for (int x = 0; x < 640; x++) {
            uint32_t p;
            if (out_f == 2 && screen_w == 320 && screen_h == 240) p = out_px[y * 640 + x];
            else {
                int sx = x * screen_w / 640, sy = y * screen_h / 480;
                p = screen_px[sy * screen_w + sx];
            }
            uint8_t *d = rgb + (y * 640 + x) * 3;
            d[0] = p >> 16; d[1] = p >> 8; d[2] = p;
        }
    }
    fwrite(rgb, 1, 640 * 480 * 3, framedump);
}

/* --------------------------------------------------------------- video ---- */
static void video_apply(void)
{
    out_f = filter_factor(filter_id);
    int tw = screen_w * out_f, th = screen_h * out_f;
    /* integer scaling only when the window scale is a multiple of the filter
     * factor; otherwise scale linearly so 2x filters still fill a 3x window */
    int integer = (scale % out_f) == 0;
    if (texture) SDL_DestroyTexture(texture);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, tw, th);
    SDL_SetTextureScaleMode(texture, integer ? SDL_ScaleModeNearest : SDL_ScaleModeLinear);
    free(screen_px);
    screen_px = calloc((size_t)screen_w * screen_h, 4);
    free(out_px);
    out_px = out_f > 1 ? calloc((size_t)tw * th, 4) : NULL;
    cv_init(&canvas, screen_px, screen_w, screen_h);
    SDL_RenderSetLogicalSize(renderer, tw, th);
    SDL_RenderSetIntegerScale(renderer, integer ? SDL_TRUE : SDL_FALSE);
    if (!(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP))
        SDL_SetWindowSize(window, screen_w * scale, screen_h * scale);
}

static void set_resolution(int w, int h)
{
    screen_w = w; screen_h = h;
    video_apply();
    gui_message("Resolution set to %dx%d", w, h);
}

static void blit_zoom(Canvas *c, int x, int y, const uint32_t *src, int pitch, int w, int h, int z)
{
    for (int j = 0; j < h * z; j++) {
        int dy = y + j;
        if (dy < 0 || dy >= c->h) continue;
        const uint32_t *s = src + (j / z) * pitch;
        uint32_t *d = c->px + dy * c->w;
        for (int i = 0; i < w * z; i++) { int dx = x + i; if (dx >= 0 && dx < c->w) d[dx] = s[i / z]; }
    }
}

static void draw_fm_info(Canvas *c)
{
    char lines[12][64];
    int n = emu_fm_info(lines, 12);
    for (int i = 0; i < n; i++) cv_text_small(c, 4, MENUBAR_H + 8 + i * 8, lines[i], C_MSGGREEN);
}

static void compose_frame(void)
{
    cv_clear(&canvas, C_BLACK);
    if (emu.loaded) {
        int gw = emu.view_w, gh = emu.view_h;
        int z = 1;
        if (game_maximized) { while ((z + 1) * gw <= screen_w && (z + 1) * gh <= screen_h - MENUBAR_H) z++; }
        game_x = (screen_w - gw * z) / 2; if (game_x < 0) game_x = 0;
        game_y = 0;                       /* original draws the game from the top edge */
        if (z > 1) {
            game_y = (screen_h - gh * z) / 2; if (game_y < MENUBAR_H) game_y = MENUBAR_H;
            blit_zoom(&canvas, game_x, game_y, emu.fb + emu.view_y * EMU_FB_W + emu.view_x, EMU_FB_W, gw, gh, z);
        } else
            cv_blit(&canvas, game_x, game_y, emu.fb + emu.view_y * EMU_FB_W + emu.view_x, EMU_FB_W, gw, gh);
    }
    if (show_fm_info && emu.loaded) draw_fm_info(&canvas);
    gui_draw(&canvas);
}

/* --------------------------------------------------------------- config ---- */
static void runtime_to_cfg(Config *c)
{
    c->res_w = screen_w; c->res_h = screen_h; c->scale = scale; c->filter = filter_id;
    c->fullscreen = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
    c->sound = sound_on; c->rate = sample_rate; c->stereo = stereo; c->region = region_pref;
    c->autohide = gui_auto_hide; c->cursor = gui_cursor_style; c->msg_popup = gui_show_msg_popup; c->state_popup = gui_show_state_popup;
    for (int p = 0; p < 2; p++) { c->six_button[p] = emu_six_button_port(p); c->device[p] = pads_type(p); pads_get_keys(p, c->keys[p]); }
    snprintf(c->romdir, sizeof c->romdir, "%s", paths.romdir);
    snprintf(c->savedir, sizeof c->savedir, "%s", paths.savedir);
    snprintf(c->pcxdir, sizeof c->pcxdir, "%s", paths.pcxdir);
    snprintf(c->logdir, sizeof c->logdir, "%s", paths.logdir);
    snprintf(c->patdir, sizeof c->patdir, "%s", paths.patdir);
}

static void toggle_maximized(void)
{
    game_maximized = !game_maximized;
    gui_message(game_maximized ? "Game window maximized" : "Game window restored");
}

static void present(void)
{
    if (out_f > 1) {
        filter_apply(filter_id, screen_px, screen_w, screen_h, out_px);
        SDL_UpdateTexture(texture, NULL, out_px, screen_w * out_f * 4);
    } else SDL_UpdateTexture(texture, NULL, screen_px, screen_w * 4);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

/* --------------------------------------------------------------- audio ---- */
static void audio_open(void)
{
    if (audio_dev) { SDL_CloseAudioDevice(audio_dev); audio_dev = 0; }
    SDL_AudioSpec want = {0}, have;
    want.freq = sample_rate; want.format = AUDIO_S16SYS; want.channels = 2; want.samples = 1024;
    audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!audio_dev) { gui_message("SDL audio open failed: %s", SDL_GetError()); return; }
    SDL_PauseAudioDevice(audio_dev, 0);
}

static void audio_push(void)
{
    if (!audio_dev || !sound_on || !emu.audio_samples) return;
    /* keep latency bounded: drop the frame if more than ~100ms queued */
    if (SDL_GetQueuedAudioSize(audio_dev) > (Uint32)(sample_rate / 10 * 4)) return;
    if (!stereo) {
        for (int i = 0; i < emu.audio_samples; i++) {
            int m = (emu.audio[2 * i] + emu.audio[2 * i + 1]) / 2;
            emu.audio[2 * i] = emu.audio[2 * i + 1] = (int16_t)m;
        }
    }
    SDL_QueueAudio(audio_dev, emu.audio, (Uint32)emu.audio_samples * 4);
}

/* --------------------------------------------------------------- input ---- */
static void poll_input(void)
{
    int gui_has_focus = gui_visible() && gui_any_window_focused();
    pads_poll(gui_has_focus);
}

static int cur_vfps(void) { return vfps_override ? vfps_override : (emu.pal ? 50 : 60); }
static void set_vfps(int fps) { vfps_override = fps; gui_message("Virtual FPS: %d%s", cur_vfps(), fps ? "" : " (auto)"); }
static void get_res(int *w, int *h) { *w = screen_w; *h = screen_h; }

/* ------------------------------------------------------------ commands ---- */
static void on_rom_picked(const char *path)
{
    if (emu_load_rom(path)) {
        char title[300];
        snprintf(title, sizeof title, "genecyst - %s", emu.romname);
        SDL_SetWindowTitle(window, title);
    }
}

static void do_command(int id)
{
    switch (id) {
    case CMD_LOADROM: dlg_loadrom_open(); break;
    case CMD_FREEROM: emu_free_rom(); SDL_SetWindowTitle(window, "genecyst"); gui_message("ROM freed"); break;
    case CMD_DIRS: dlg_directories_open(); break;
    case CMD_FIXCHECKSUM: gui_message("Checksum: core already patches the header"); break;
    case CMD_SNAPSHOT: { char p[1200]; if (paths_next_snapshot(p, sizeof p)) emu_save_pcx(p); } break;
    case CMD_EXIT: running = 0; break;
    case CMD_MESSAGES: dlg_messages_open(); break;
    case CMD_PALETTE: dlg_palette_open(); break;
    case CMD_PATTERNS: dlg_patterns_open(); break;
    case CMD_SETRES: dlg_setres_open(); break;
    case CMD_RESET: emu_reset(); break;
    case CMD_PAUSE: if (emu.loaded) { emu.running = !emu.running; gui_message(emu.running ? "CPU emulation resumed" : "CPU emulation paused"); } break;
    case CMD_ADVANCE: if (emu.loaded) { emu.running = 1; emu_frame(); emu.running = 0; } break;
    case CMD_CPUSKIP: cpu_skip = !cpu_skip; gui_message("CPU skipping %s", cpu_skip ? "on" : "off"); break;
    case CMD_SAVESTATE: emu_save_state(emu.state_slot); break;
    case CMD_LOADSTATE: emu_load_state(emu.state_slot); break;
    case CMD_PATCH: dlg_patcher_open(); break;
    case CMD_INPUT1: dlg_input_open(0); break;
    case CMD_INPUT2: dlg_input_open(1); break;
    case CMD_SCALE1: scale = 1; video_apply(); break;
    case CMD_SCALE2: scale = 2; video_apply(); break;
    case CMD_SCALE3: scale = 3; video_apply(); break;
    case CMD_SCALE4: scale = 4; video_apply(); break;
    case CMD_FULLSCREEN: {
        int fs = SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP;
        SDL_SetWindowFullscreen(window, fs ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
        if (fs) SDL_SetWindowSize(window, screen_w * scale, screen_h * scale);
        break; }
    case CMD_FS_AUTO: frameskip = 0; gui_message("Frameskip: auto"); break;
    case CMD_FS_0: case CMD_FS_1: case CMD_FS_2: case CMD_FS_3: frameskip = id - CMD_FS_0 + 1; gui_message("Frameskip: %d", frameskip - 1); break;
    case CMD_VSYNC: vsync = !vsync; gui_message("Vsync %s", vsync ? "on" : "off"); break;
    case CMD_TIMING: dlg_timing_open(); break;
    case CMD_DMAFIX: case CMD_FORCEDISP: case CMD_HINT: case CMD_RASTER:
        gui_message("Always accurate in this build"); break;
    case CMD_AUTOVER: case CMD_VER_AUTO: region_pref = 0; config.region_detect = 0; gui_message("Hardware Version: Auto"); break;
    case CMD_VER_USA: region_pref = 1; config.region_detect = 1; gui_message("Hardware Version: USA"); break;
    case CMD_VER_EUR: region_pref = 2; config.region_detect = 2; gui_message("Hardware Version: Europe"); break;
    case CMD_VER_JAP: region_pref = 3; config.region_detect = 3; gui_message("Hardware Version: Japan"); break;
    case CMD_SIX1: emu_set_six_button_port(0, !emu_six_button_port(0)); break;
    case CMD_SIX2: emu_set_six_button_port(1, !emu_six_button_port(1)); break;
    case CMD_RATE11: sample_rate = 11025; goto rate;
    case CMD_RATE22: sample_rate = 22050; goto rate;
    case CMD_RATE44: sample_rate = 44100;
    rate: audio_open(); if (emu.loaded) audio_set_rate(sample_rate, 0); gui_message("Soundrate set: %dhz", sample_rate); break;
    case CMD_STEREO: stereo = !stereo; gui_message("Stereo %s", stereo ? "on" : "OFF"); break;
    case CMD_Z80: sound_on = !sound_on; gui_message("Sound %s", sound_on ? "enabled" : "disabled"); break;
    case CMD_FM: emu_set_fm(!emu_fm()); break;
    case CMD_PSG: emu_set_psg(!emu_psg()); break;
    case CMD_DAC: emu_set_dac(!emu_dac()); break;
    case CMD_WAVEOUT: wave_output = !wave_output; if (audio_dev) SDL_PauseAudioDevice(audio_dev, !wave_output); gui_message("Wave output %s", wave_output ? "on" : "off"); break;
    case CMD_FMINFO: show_fm_info = !show_fm_info; break;
    case CMD_LOGWAVE: emu_wav_toggle(); break;
    case CMD_LOGYM: emu_gym_toggle(); break;
    case CMD_SAVEGST: emu_save_state_gst(emu.state_slot); break;
    case CMD_Z80FAKE: break;
    case CMD_HIDEGUI: gui_set_visible(0); break;
    case CMD_CURSOR_SWORD: gui_cursor_style = 0; break;
    case CMD_CURSOR_ARROW: gui_cursor_style = 1; break;
    case CMD_AUTOHIDE: gui_auto_hide = !gui_auto_hide; gui_message("Auto hide GUI %s", gui_auto_hide ? "on (3 s)" : "off"); break;
    case CMD_MSGPOPUP: gui_show_msg_popup = !gui_show_msg_popup; break;
    case CMD_STATEPOPUP: gui_show_state_popup = !gui_show_state_popup; break;
    case CMD_ABOUT: dlg_about_open(); break;
    default:
        if (id >= CMD_FILTER_BASE && id <= CMD_FILTER_LAST) {
            filter_id = id - CMD_FILTER_BASE;
            video_apply();
            gui_message("Filter: %s", filter_name(filter_id));
        } else if (id >= CMD_LAYER_BASE && id <= CMD_LAYER_LAST) {
            int bit = layer_bits[id - CMD_LAYER_BASE];
            emu_set_layer(bit, !emu_layer(bit));
            gui_message("%s %s", layer_sub[id - CMD_LAYER_BASE].label, emu_layer(bit) ? "on" : "off");
        } else if (id >= CMD_DUMP_BASE && id <= CMD_DUMP_LAST) {
            emu_dump(id - CMD_DUMP_BASE);
        } else if (id >= CMD_RESN_BASE && id <= CMD_RESN_LAST) {
            set_resolution(res_table[id - CMD_RESN_BASE].w, res_table[id - CMD_RESN_BASE].h);
        }
        break;
    }
}

static void emu_msg_sink(const char *m) { gui_message("%s", m); }

/* ---------------------------------------------------------------- main ---- */
static void usage(const char *argv0)
{
    printf("genecyst (revival)\nusage: %s [options] [rom]\n"
           "  -run <rom>        load & run\n  -res <w> <h>      virtual screen size\n"
           "  -scale <n>        window scale\n  -romdir <dir>     startup dir for ROMs\n"
           "  -savedir <dir>    .GS?/.GSV dir\n  -pcxdir <dir>     snapshot dir\n"
           "  -logdir <dir>     .gym/.wav dir\n  -patdir <dir>     .pat dir\n"
           "  -nosound          disable audio\n  -hidegui          start with GUI hidden\n"
           "  -filter <name>    none|scale2x|2xsai|super2xsai|supereagle|scanlines|tv|crt\n"
           "  -noautohide       keep the GUI visible (default: hides after 3 s idle)\n"
           "  -cfg <file>       settings file (default genecyst.cfg, written on exit)\n"
           "  -version <USA|Japan|Europe>\n", argv0);
}

int main(int argc, char **argv)
{
    const char *rom = NULL;
    int hidegui = 0;
    paths_init();
    Config cfg;
    cfg_defaults(&cfg);
    for (int i = 1; i < argc - 1; i++) if (!strcmp(argv[i], "-cfg")) snprintf(cfg_path, sizeof cfg_path, "%s", argv[i + 1]);
    int have_cfg = cfg_load(&cfg, cfg_path);
    if (have_cfg) {
        screen_w = cfg.res_w; screen_h = cfg.res_h; scale = cfg.scale;
        filter_id = cfg.filter >= 0 && cfg.filter < FILTER_COUNT ? cfg.filter : 0;
        sound_on = cfg.sound; sample_rate = cfg.rate; stereo = cfg.stereo; region_pref = cfg.region;
        gui_auto_hide = cfg.autohide; gui_cursor_style = cfg.cursor; gui_show_msg_popup = cfg.msg_popup; gui_show_state_popup = cfg.state_popup;
        if (cfg.romdir[0]) snprintf(paths.romdir, sizeof paths.romdir, "%s", cfg.romdir);
        snprintf(paths.savedir, sizeof paths.savedir, "%s", cfg.savedir);
        snprintf(paths.pcxdir, sizeof paths.pcxdir, "%s", cfg.pcxdir);
        snprintf(paths.logdir, sizeof paths.logdir, "%s", cfg.logdir);
        snprintf(paths.patdir, sizeof paths.patdir, "%s", cfg.patdir);
    }
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-run") || !strcmp(argv[i], "-load")) { if (i + 1 < argc) rom = argv[++i]; }
        else if (!strcmp(argv[i], "-res") && i + 2 < argc) { screen_w = atoi(argv[i + 1]); screen_h = atoi(argv[i + 2]); i += 2; }
        else if (!strcmp(argv[i], "-scale") && i + 1 < argc) scale = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-romdir") && i + 1 < argc) snprintf(paths.romdir, sizeof paths.romdir, "%s", argv[++i]);
        else if (!strcmp(argv[i], "-savedir") && i + 1 < argc) snprintf(paths.savedir, sizeof paths.savedir, "%s", argv[++i]);
        else if (!strcmp(argv[i], "-pcxdir") && i + 1 < argc) snprintf(paths.pcxdir, sizeof paths.pcxdir, "%s", argv[++i]);
        else if (!strcmp(argv[i], "-logdir") && i + 1 < argc) snprintf(paths.logdir, sizeof paths.logdir, "%s", argv[++i]);
        else if (!strcmp(argv[i], "-patdir") && i + 1 < argc) snprintf(paths.patdir, sizeof paths.patdir, "%s", argv[++i]);
        else if (!strcmp(argv[i], "-nosound")) sound_on = 0;
        else if (!strcmp(argv[i], "-filter") && i + 1 < argc) { int f = filter_by_key(argv[++i]); if (f >= 0) filter_id = f; else fprintf(stderr, "unknown filter %s\n", argv[i]); }
        else if (!strcmp(argv[i], "-hidegui")) hidegui = 1;
        else if (!strcmp(argv[i], "-noautohide")) gui_auto_hide = 0;
        else if (!strcmp(argv[i], "-cfg") && i + 1 < argc) i++;
        else if (!strcmp(argv[i], "-script") && i + 1 < argc) script_load(argv[++i]);
        else if (!strcmp(argv[i], "-framedump") && i + 1 < argc) framedump = fopen(argv[++i], "wb");
        else if (!strcmp(argv[i], "-exitafter") && i + 1 < argc) exit_after = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-version") && i + 1 < argc) { i++; region_pref = !strcasecmp(argv[i], "USA") ? 1 : !strcasecmp(argv[i], "Europe") ? 2 : !strcasecmp(argv[i], "Japan") ? 3 : 0; }
        else if (!strcmp(argv[i], "-?") || !strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
        else if (argv[i][0] != '-') rom = argv[i];
    }
    if (scale < 1) scale = 1;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER) < 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    window = SDL_CreateWindow("genecyst", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              screen_w * scale, screen_h * scale, SDL_WINDOW_RESIZABLE);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | (vsync ? SDL_RENDERER_PRESENTVSYNC : 0));
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, 0);
    SDL_ShowCursor(SDL_DISABLE);
    video_apply();

    gui_init(menus, 6, do_command);
    gui_set_maxbox_handler(toggle_maximized);
    static const DialogHost host = { on_rom_picked, set_resolution, get_res, set_vfps, cur_vfps };
    dlg_init(&host);
    emu_set_message_handler(emu_msg_sink);
    emu_init();
    config.region_detect = region_pref;
    audio_open();
    pads_init();
    if (have_cfg) {
        for (int p = 0; p < 2; p++) {
            pads_set_type(p, cfg.device[p]);
            if (cfg.keys[p][0] >= 0) pads_set_keys(p, cfg.keys[p]);
            emu_set_six_button_port(p, cfg.six_button[p]);
        }
        if (cfg.fullscreen) SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }
    SDL_StartTextInput();
    gui_message("genecyst revival - Esc toggles the GUI");
    if (hidegui) gui_set_visible(0);
    if (rom) on_rom_picked(rom);

    Uint64 perf_freq = SDL_GetPerformanceFrequency();
    Uint64 last = SDL_GetPerformanceCounter();
    double accum = 0;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            static int last_mx, last_my;   /* last pointer position in virtual screen pixels */
            int mx = last_mx, my = last_my;
            if (e.type == SDL_MOUSEMOTION) { mx = e.motion.x / out_f; my = e.motion.y / out_f; }
            else if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) { mx = e.button.x / out_f; my = e.button.y / out_f; }
            /* SDL maps motion/button coordinates into the logical space (the
             * filtered frame); wheel events carry no usable position, so they
             * use the last known pointer position */
            last_mx = mx; last_my = my;
            if (e.type == SDL_QUIT) running = 0;
            else if (e.type == SDL_CONTROLLERDEVICEADDED) pads_open_controllers();
            else if (e.type == SDL_DROPFILE) { on_rom_picked(e.drop.file); SDL_free(e.drop.file); }
            else if (e.type == SDL_KEYDOWN && !e.key.repeat) {
                SDL_Keycode k = e.key.keysym.sym;
                if (pads_redefine_active()) { pads_redefine_feed(e.key.keysym.scancode); continue; }
                if (k == SDLK_RETURN && (e.key.keysym.mod & KMOD_ALT)) { do_command(CMD_FULLSCREEN); continue; }
                if (k == SDLK_q && (e.key.keysym.mod & KMOD_ALT)) { running = 0; continue; }
                /* a focused dialog gets the keys first (text edits need digits and letters) */
                if (gui_visible() && gui_window_count() > 0 && gui_event(&e, mx, my)) continue;
                /* state slot keys 0-9 (top row) like the original */
                if (k >= SDLK_0 && k <= SDLK_9 && !(e.key.keysym.mod & (KMOD_CTRL | KMOD_ALT))) {
                    emu.state_slot = k - SDLK_0;
                    if (gui_show_state_popup) gui_message("State slot %d", emu.state_slot);
                } else gui_event(&e, mx, my);
            } else if (e.type == SDL_KEYDOWN && e.key.repeat) {
                if (gui_visible() && gui_window_count() > 0) gui_event(&e, mx, my);   /* key repeat inside dialogs */
            } else gui_event(&e, mx, my);
        }

        poll_input();
        Uint64 now = SDL_GetPerformanceCounter();
        double dt = (double)(now - last) / (double)perf_freq;
        last = now;
        if (dt > 0.25) dt = 0.25;
        double frame_t = vfps_override ? 1.0 / vfps_override : (emu.pal ? 1.0 / 50.0 : 1.0 / 59.92);
        accum += dt;
        int frames = 0;
        if (framedump || nscript) {
            /* deterministic mode: exactly one emulated frame per loop iteration */
            script_tick();
            emu_frame();
            audio_push();
            gui_tick();
            accum = 0;
            frame_no++;
            if (exit_after && frame_no >= exit_after) running = 0;
        } else {
            while (accum >= frame_t && frames < 4) {
                emu_frame();
                audio_push();
                gui_tick();
                accum -= frame_t;
                frames++;
            }
            if (!emu.loaded || !emu.running) { if (frames == 0) gui_tick(); accum = 0; }
        }
        compose_frame();
        present();
        framedump_write();
        if (!vsync) SDL_Delay(1);
    }

    if (framedump) fclose(framedump);
    runtime_to_cfg(&cfg);
    cfg_save(&cfg, cfg_path);
    emu_shutdown();
    pads_shutdown();
    if (audio_dev) SDL_CloseAudioDevice(audio_dev);
    free(out_px);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
