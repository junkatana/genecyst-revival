/* emu.h -- thin wrapper around the Genesis Plus GX core */
#ifndef GENECYST_EMU_H
#define GENECYST_EMU_H

#include <stdint.h>

#define EMU_SAMPLE_RATE 44100
#define EMU_FB_W 720
#define EMU_FB_H 576

typedef struct {
    int loaded;             /* a ROM is loaded */
    int running;            /* CPU emulation running (not paused) */
    char rompath[1024];
    char romname[256];      /* basename without extension */
    int state_slot;         /* 0..9 */
    uint32_t *fb;           /* EMU_FB_W x EMU_FB_H XRGB8888 */
    int view_x, view_y, view_w, view_h;  /* current viewport inside fb */
    int pal;
    int16_t *audio;         /* interleaved stereo for the last frame */
    int audio_samples;      /* stereo frames produced by the last emu_frame() */
    char last_msg[256];
} Emu;

extern Emu emu;

/* message sink, set by the frontend (GUI message log / popup) */
typedef void (*emu_msg_fn)(const char *msg);
void emu_set_message_handler(emu_msg_fn fn);
void emu_message(const char *fmt, ...);

int  emu_init(void);
int  emu_load_rom(const char *path);
void emu_free_rom(void);
void emu_reset(void);
void emu_frame(void);           /* run one frame, update fb viewport + audio */
void emu_shutdown(void);

int  emu_save_state(int slot);
int  emu_load_state(int slot);
int  emu_save_sram(void);
int  emu_load_sram(void);
int  emu_save_pcx(const char *path);

/* input: 12 bits per pad (Genesis Plus GX INPUT_* mask), set by the frontend each frame */
extern uint16_t emu_pad[2];
int  emu_set_six_button(int on);
int  emu_set_six_button_port(int port, int on);   /* port 0/1 */
int  emu_six_button_port(int port);

/* ---- debug viewers ---------------------------------------------------- */
uint32_t emu_cram_color(int index);               /* CRAM entry 0..63 -> XRGB */
/* decode 8x8 4bpp tiles from VRAM: tiles [first, first+cols*rows) laid out
 * cols x rows into dst (pitch in pixels) with palette line pal (0..3) */
void emu_draw_tiles(uint32_t *dst, int pitch_px, int first_tile, int cols, int rows, int pal);

/* View > Layer toggles (LAYER_* bits from the core's vdp_render.h) */
void emu_set_layer(int bit, int on);
int  emu_layer(int bit);

/* CPU > Dump */
enum { DUMP_RAM, DUMP_ROM, DUMP_VRAM, DUMP_VSRAM, DUMP_CRAM, DUMP_Z80RAM, DUMP_YMREGS,
       DUMP_MESSAGES, DUMP_VDPBASE, DUMP_VDPREGS, DUMP_SPRITES, DUMP_COUNT };
int  emu_dump(int what);

/* ---- ROM patches (Game Genie / raw AAAAAA:VVVV), CPU > ROM patch... ---- */
typedef struct { char code[16]; char name[40]; uint32_t addr; uint16_t val, orig; int on; } Patch;
#define MAX_PATCHES 64
extern Patch emu_patches[MAX_PATCHES];
extern int emu_npatches;
int  emu_patch_decode(const char *code, uint32_t *addr, uint16_t *val);
int  emu_patch_add(const char *code, const char *name);   /* index or -1 */
void emu_patch_remove(int i);
void emu_patch_toggle(int i);
int  emu_patch_save(void);
int  emu_patch_load(void);
void emu_patch_clear(void);

/* ---- Sound menu --------------------------------------------------------- */
void emu_set_fm(int on);   int emu_fm(void);     /* FM music enable */
void emu_set_psg(int on);  int emu_psg(void);    /* PSG sound enable */
void emu_set_dac(int on);  int emu_dac(void);    /* DAC sound enable */
int  emu_gym_toggle(void); int emu_gym_logging(void);   /* Log YM2612 -> <rom>.gym */
int  emu_wav_toggle(void); int emu_wav_logging(void);   /* Log wave   -> <rom>.wav */
/* "Show FM info": fills up to max lines of text describing the FM/PSG state */
int  emu_fm_info(char lines[][64], int max);

/* ---- Genecyst / Gens GST save states ------------------------------------ */
int  emu_is_gst_file(const char *path);
int  emu_import_gst(const char *path);
int  emu_export_gst(const char *path);
int  emu_save_state_gst(int slot);     /* writes the slot in GST format */

#endif
