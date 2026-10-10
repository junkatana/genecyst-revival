#include "emu.h"
#include "paths.h"
#include "shared.h"
#include "vdp_render.h"
#include "gui/gui.h"
#include <stdarg.h>
#include <strings.h>
#include <ctype.h>
#include "m68k.h"
#include "z80.h"
#include "ym2612.h"
#include "psg.h"

Emu emu;
uint16_t emu_pad[2];
static void emu_hook_sound(void);
static void emu_apply_sound_options(void);
static void emu_sound_frame_end(void);
int log_error = 0;
static emu_msg_fn msg_handler;
static int16_t audio_buf[2048 * 2];

void emu_set_message_handler(emu_msg_fn fn) { msg_handler = fn; }

void emu_message(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(emu.last_msg, sizeof emu.last_msg, fmt, ap);
    va_end(ap);
    if (msg_handler) msg_handler(emu.last_msg);
    else puts(emu.last_msg);
}

/* called by the core once per frame to refresh controller state */
int genecyst_input_update(void)
{
    input.pad[0] = emu_pad[0];
    input.pad[4] = emu_pad[1];   /* port 2 = device index 4 */
    return 1;
}

int emu_init(void)
{
    memset(&emu, 0, sizeof emu);
    emu.fb = calloc(EMU_FB_W * EMU_FB_H, sizeof(uint32_t));
    emu.audio = audio_buf;
    if (!emu.fb) return 0;

    error_init();
    set_config_defaults();
    config.hq_fm = 1;
    config.filter = 0;
    config.overscan = 0;
    system_bios = 0;
    memset(boot_rom, 0xFF, sizeof(boot_rom));

    memset(&bitmap, 0, sizeof(t_bitmap));
    bitmap.width = EMU_FB_W;
    bitmap.height = EMU_FB_H;
    bitmap.pitch = EMU_FB_W * 4;
    bitmap.data = (uint8 *)emu.fb;
    bitmap.viewport.changed = 3;
    return 1;
}

static void emu_update_viewport(void)
{
    emu.view_x = bitmap.viewport.x;
    emu.view_y = bitmap.viewport.y;
    emu.view_w = bitmap.viewport.w;
    emu.view_h = bitmap.viewport.h;
    emu.pal = vdp_pal;
}

int emu_load_rom(const char *path)
{
    if (emu.loaded) emu_free_rom();
    if (!load_rom((char *)path)) {
        emu_message("Unable to open %s", path);
        return 0;
    }
    snprintf(emu.rompath, sizeof emu.rompath, "%s", path);
    paths_rom_basename(path, emu.romname, sizeof emu.romname);

    audio_init(EMU_SAMPLE_RATE, 0);
    system_init();
    emu_load_sram();
    system_reset();
    emu_update_viewport();
    emu.loaded = 1;
    emu.running = 1;
    {
        const char *dot = strrchr(path, '.');
        emu_message("%dK read (%s)", cart.romsize / 1024, (dot && !strcasecmp(dot, ".smd")) ? "interleaved" : "binary");
    }
    emu_hook_sound();
    emu_apply_sound_options();
    emu_patch_load();
    return 1;
}

void emu_free_rom(void)
{
    if (!emu.loaded) return;
    if (emu_gym_logging()) emu_gym_toggle();
    if (emu_wav_logging()) emu_wav_toggle();
    emu_patch_clear();
    emu_save_sram();
    audio_shutdown();
    emu.loaded = 0;
    emu.running = 0;
    memset(emu.fb, 0, EMU_FB_W * EMU_FB_H * 4);
}

void emu_reset(void)
{
    if (!emu.loaded) return;
    system_reset();
    emu_message("68000 reset");
}

static void apply_ram_patches(void)
{
    for (int i = 0; i < emu_npatches; i++) {
        Patch *p = &emu_patches[i];
        if (p->on && p->addr >= 0xFF0000) { WRITE_WORD(work_ram, p->addr & 0xFFFE, p->val); }
    }
}

void emu_frame(void)
{
    emu.audio_samples = 0;
    if (!emu.loaded || !emu.running) return;
    apply_ram_patches();
    if (system_hw == SYSTEM_MCD) system_frame_scd(0);
    else if ((system_hw & SYSTEM_PBC) == SYSTEM_MD) system_frame_gen(0);
    else system_frame_sms(0);
    if (bitmap.viewport.changed & 1) {
        bitmap.viewport.changed &= ~1;
        emu_update_viewport();
    }
    emu.audio_samples = audio_update(audio_buf);
    emu_sound_frame_end();
}

void emu_shutdown(void)
{
    emu_free_rom();
    error_shutdown();
    free(emu.fb);
    emu.fb = NULL;
}

/* ---- sidecar files -------------------------------------------------------- */

static void state_path(int slot, char *out, size_t n)
{
    char ext[8];
    snprintf(ext, sizeof ext, "gs%d", slot);
    paths_sidecar(emu.rompath, paths.savedir, ext, out, n);
}

/* Save state container: "GST\0" magic kept for familiarity, followed by a
 * Genecyst/GPGX tag and the raw core state. Original Genecyst GST layout is
 * documented and will be supported for import later. */
#define STATE_TAG "GENECYST-GPGX-1"

int emu_save_state(int slot)
{
    if (!emu.loaded) return 0;
    char path[1200];
    state_path(slot, path, sizeof path);
    unsigned char *buf = malloc(STATE_SIZE);
    if (!buf) return 0;
    int len = state_save(buf);
    FILE *f = fopen(path, "wb");
    if (!f) { free(buf); emu_message("Unable to create %s", path); return 0; }
    fwrite("GST\0", 1, 4, f);
    fwrite(STATE_TAG, 1, 16, f);
    fwrite(buf, 1, len, f);
    fclose(f);
    free(buf);
    emu_message("State saved to %s", path);
    return 1;
}

int emu_load_state(int slot)
{
    if (!emu.loaded) return 0;
    char path[1200];
    state_path(slot, path, sizeof path);
    if (emu_is_gst_file(path)) return emu_import_gst(path);
    FILE *f = fopen(path, "rb");
    if (!f) { emu_message("Unable to open %s", path); return 0; }
    char hdr[20];
    if (fread(hdr, 1, 20, f) != 20 || memcmp(hdr, "GST\0", 4) || memcmp(hdr + 4, STATE_TAG, 15)) {
        fclose(f);
        emu_message("Invalid state file");
        return 0;
    }
    unsigned char *buf = malloc(STATE_SIZE);
    if (!buf) { fclose(f); return 0; }
    size_t n = fread(buf, 1, STATE_SIZE, f);
    fclose(f);
    int ok = n > 0 && state_load(buf);
    free(buf);
    if (ok) {
        emu_update_viewport();
        emu_message("State restored from %s", path);
    } else emu_message("Invalid state file");
    return ok;
}

int emu_save_sram(void)
{
    if (!emu.loaded || !sram.on) return 0;
    char path[1200];
    paths_sidecar(emu.rompath, paths.savedir, "gsv", path, sizeof path);
    FILE *f = fopen(path, "wb");
    if (!f) { emu_message("Unable to save to %s", path); return 0; }
    fwrite(sram.sram, 1, 0x10000, f);
    fclose(f);
    emu_message("SRAM written to %s", path);
    return 1;
}

int emu_load_sram(void)
{
    if (!sram.on) return 0;
    char path[1200];
    paths_sidecar(emu.rompath, paths.savedir, "gsv", path, sizeof path);
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    size_t n = fread(sram.sram, 1, 0x10000, f);
    fclose(f);
    if (n) emu_message("SRAM read from %s", path);
    return n > 0;
}

int emu_set_six_button(int on)
{
    for (int i = 0; i < MAX_INPUTS; i++)
        config.input[i].padtype = on ? (DEVICE_PAD2B | DEVICE_PAD3B | DEVICE_PAD6B) : (DEVICE_PAD2B | DEVICE_PAD3B);
    if (emu.loaded) { io_init(); emu_message("6 button pads %s (reset needed)", on ? "enabled" : "disabled"); }
    return on;
}

int emu_set_six_button_port(int port, int on)
{
    int idx = port ? 4 : 0;
    config.input[idx].padtype = on ? (DEVICE_PAD2B | DEVICE_PAD3B | DEVICE_PAD6B) : (DEVICE_PAD2B | DEVICE_PAD3B);
    if (emu.loaded) { io_init(); emu_message("Device %d: %d button pad", port + 1, on ? 6 : 3); }
    return on;
}

int emu_six_button_port(int port)
{
    return (config.input[port ? 4 : 0].padtype & DEVICE_PAD6B) != 0;
}

/* ---- debug viewers --------------------------------------------------- */
uint32_t emu_cram_color(int index)
{
    /* the core keeps CRAM packed as 9-bit BBBGGGRRR (see palette_init) */
    uint16_t v = *(uint16_t *)&cram[(index & 63) * 2];
    uint32_t r = (v & 7) * 36, g = ((v >> 3) & 7) * 36, b = ((v >> 6) & 7) * 36;
    return (r << 16) | (g << 8) | b;
}

void emu_draw_tiles(uint32_t *dst, int pitch_px, int first_tile, int cols, int rows, int pal)
{
    uint32_t colors[16];
    for (int i = 0; i < 16; i++) colors[i] = emu_cram_color((pal & 3) * 16 + i);
    for (int ty = 0; ty < rows; ty++) {
        for (int tx = 0; tx < cols; tx++) {
            int tile = first_tile + ty * cols + tx;
            if (tile < 0 || tile >= 0x800) continue;
            for (int y = 0; y < 8; y++) {
                uint32_t bp = *(uint32_t *)&vram[(tile << 5) | (y << 2)];
                uint32_t *o = dst + (ty * 8 + y) * pitch_px + tx * 8;
                for (int x = 0; x < 8; x++) {
                    uint32_t c = bp & 0x0F;
                    bp >>= 4;
#ifdef LSB_FIRST
                    o[x ^ 3] = colors[c];
#else
                    o[x ^ 7] = colors[c];
#endif
                }
            }
        }
    }
}

void emu_set_layer(int bit, int on)
{
    unsigned m = genecyst_layer_mask;
    if (on) m |= bit; else m &= ~bit;
    genecyst_set_layer_mask(m);
}

int emu_layer(int bit) { return (genecyst_layer_mask & bit) != 0; }

/* ---- dumps ----------------------------------------------------------- */
static FILE *dump_open(const char *ext, char *path, size_t n)
{
    paths_sidecar(emu.rompath, paths.savedir, ext, path, n);
    FILE *f = fopen(path, "wb");
    if (!f) emu_message("Unable to create %s", path);
    return f;
}

int emu_dump(int what)
{
    if (!emu.loaded && what != DUMP_MESSAGES) { emu_message("No ROM loaded"); return 0; }
    char path[1200];
    FILE *f;
    switch (what) {
    case DUMP_RAM:
        if (!(f = dump_open("ram", path, sizeof path))) return 0;
        for (int i = 0; i < 0x10000; i += 2) { fputc(work_ram[i ^ 1], f); fputc(work_ram[i], f); }
        fclose(f); emu_message("RAM dumped"); return 1;
    case DUMP_ROM:
        if (!(f = dump_open("rom", path, sizeof path))) return 0;
        for (uint32_t i = 0; i < cart.romsize; i += 2) { fputc(cart.rom[i ^ 1], f); fputc(cart.rom[i], f); }
        fclose(f); emu_message("ROM dumped"); return 1;
    case DUMP_VRAM:
        if (!(f = dump_open("vram", path, sizeof path))) return 0;
        fwrite(vram, 1, 0x10000, f); fclose(f); emu_message("VRAM dumped"); return 1;
    case DUMP_VSRAM:
        if (!(f = dump_open("vsram", path, sizeof path))) return 0;
        fwrite(vsram, 1, 0x80, f); fclose(f); emu_message("VSRAM dumped"); return 1;
    case DUMP_CRAM:
        if (!(f = dump_open("cram", path, sizeof path))) return 0;
        fwrite(cram, 1, 0x80, f); fclose(f); emu_message("CRAM dumped"); return 1;
    case DUMP_Z80RAM:
        if (!(f = dump_open("z80", path, sizeof path))) return 0;
        fwrite(zram, 1, 0x2000, f); fclose(f); emu_message("Z80 RAM dumped"); return 1;
    case DUMP_YMREGS: {
        if (!(f = dump_open("ym", path, sizeof path))) return 0;
        unsigned char *buf = calloc(1, 0x10000);
        int n = buf ? YM2612SaveContext(buf) : 0;
        if (n > 0) fwrite(buf, 1, (size_t)n, f);
        free(buf); fclose(f); emu_message("YM2612 regs dumped"); return 1; }
    case DUMP_MESSAGES: {
        if (emu.loaded) paths_sidecar(emu.rompath, paths.savedir, "msg", path, sizeof path);
        else snprintf(path, sizeof path, "genecyst.msg");
        f = fopen(path, "w");
        if (!f) { emu_message("Unable to create %s", path); return 0; }
        for (int i = gui_message_count() - 1; i >= 0; i--) fprintf(f, "%s\n", gui_message_line(i));
        fclose(f); emu_message("Messages dumped to %s", path); return 1; }
    case DUMP_VDPBASE:
        if (!(f = dump_open("vdp", path, sizeof path))) return 0;
        fprintf(f, "Scroll A name table: %04X\nScroll B name table: %04X\nWindow name table:   %04X\n"
                   "Sprite attr table:   %04X\nH-scroll table:      %04X\n", ntab, ntbb, ntwb, satb, hscb);
        fclose(f); emu_message("VDP base dumped to %s", path); return 1;
    case DUMP_VDPREGS:
        if (!(f = dump_open("vdpregs", path, sizeof path))) return 0;
        for (int i = 0; i < 24; i++) fprintf(f, "reg %02d: %02X\n", i, reg[i]);
        fprintf(f, "status: %04X\n", status);
        fclose(f); emu_message("VDP regs dumped to %s", path); return 1;
    case DUMP_SPRITES: {
        if (!(f = dump_open("spr", path, sizeof path))) return 0;
        fprintf(f, "  #   X    Y  size link attr\n");
        int link = 0;
        for (int n = 0; n < 80; n++) {
            uint16_t *e = (uint16_t *)&vram[satb + link * 8];
            int y = e[0] & 0x3FF, size = (e[1] >> 8) & 0x0F, nl = e[1] & 0x7F, attr = e[2], x = e[3] & 0x1FF;
            fprintf(f, "%3d %4d %4d  %dx%d  %3d %04X\n", link, x, y, (size >> 2) + 1, (size & 3) + 1, nl, attr);
            if (!nl || nl >= 80) break;
            link = nl;
        }
        fclose(f); emu_message("Sprites dumped to %s", path); return 1; }
    default: return 0;
    }
}

/* ---- ROM patches ----------------------------------------------------- */
Patch emu_patches[MAX_PATCHES];
int emu_npatches;

static const char gg_chars[] = "ABCDEFGHJKLMNPRSTVWXYZ0123456789";

int emu_patch_decode(const char *code, uint32_t *addr, uint16_t *val)
{
    char c[32];
    int n = 0;
    for (const char *p = code; *p && n < 31; p++) if (!isspace((unsigned char)*p)) c[n++] = (char)toupper((unsigned char)*p);
    c[n] = 0;
    const char *colon = strchr(c, ':');
    if (colon) {
        char *end;
        unsigned long a = strtoul(c, &end, 16);
        if (end != colon) return 0;
        unsigned long v = strtoul(colon + 1, &end, 16);
        if (*end || a > 0xFFFFFF || v > 0xFFFF) return 0;
        *addr = (uint32_t)a; *val = (uint16_t)v;
        return 1;
    }
    /* Game Genie XXXX-XXXX */
    char g[9]; int k = 0;
    for (int i = 0; c[i] && k < 8; i++) if (c[i] != '-') g[k++] = c[i];
    if (k != 8) return 0;
    uint32_t a = 0; uint32_t d = 0;
    for (int i = 0; i < 8; i++) {
        const char *p = strchr(gg_chars, g[i]);
        if (!p) return 0;
        uint32_t x = (uint32_t)(p - gg_chars);
        switch (i) {
        case 0: d |= x << 3; break;
        case 1: d |= x >> 2; a |= (x & 3) << 14; break;
        case 2: a |= x << 9; break;
        case 3: a |= (x & 0xF) << 20 | (x >> 4) << 8; break;
        case 4: d |= (x & 1) << 12; a |= (x >> 1) << 16; break;
        case 5: d |= (x & 1) << 15 | (x >> 1) << 8; break;
        case 6: d |= (x >> 3) << 13; a |= (x & 7) << 5; break;
        case 7: a |= x; break;
        }
    }
    *addr = a; *val = (uint16_t)d;
    return 1;
}

static void patch_apply(Patch *p, int on)
{
    if (p->addr >= 0xFF0000) { p->on = on; return; }          /* RAM: applied per frame */
    if (p->addr + 1 >= cart.romsize) { p->on = 0; return; }
    if (on && !p->on) { p->orig = READ_WORD(cart.rom, p->addr & 0xFFFFFE); WRITE_WORD(cart.rom, p->addr & 0xFFFFFE, p->val); }
    else if (!on && p->on) { WRITE_WORD(cart.rom, p->addr & 0xFFFFFE, p->orig); }
    p->on = on;
}

int emu_patch_add(const char *code, const char *name)
{
    if (emu_npatches >= MAX_PATCHES) return -1;
    uint32_t a; uint16_t v;
    if (!emu_patch_decode(code, &a, &v)) { emu_message("Invalid patch code"); return -1; }
    Patch *p = &emu_patches[emu_npatches];
    memset(p, 0, sizeof *p);
    snprintf(p->code, sizeof p->code, "%.15s", code);
    snprintf(p->name, sizeof p->name, "%.39s", name ? name : "");
    p->addr = a; p->val = v;
    patch_apply(p, 1);
    return emu_npatches++;
}

void emu_patch_remove(int i)
{
    if (i < 0 || i >= emu_npatches) return;
    patch_apply(&emu_patches[i], 0);
    memmove(&emu_patches[i], &emu_patches[i + 1], (size_t)(emu_npatches - i - 1) * sizeof(Patch));
    emu_npatches--;
}

void emu_patch_toggle(int i)
{
    if (i < 0 || i >= emu_npatches) return;
    patch_apply(&emu_patches[i], !emu_patches[i].on);
}

void emu_patch_clear(void)
{
    for (int i = 0; i < emu_npatches; i++) patch_apply(&emu_patches[i], 0);
    emu_npatches = 0;
}

static void pat_path(char *out, size_t n)
{
    paths_sidecar(emu.rompath, paths.patdir, "pat", out, n);
}

int emu_patch_save(void)
{
    if (!emu.loaded) return 0;
    char path[1200];
    pat_path(path, sizeof path);
    FILE *f = fopen(path, "w");
    if (!f) { emu_message("Unable to create %s", path); return 0; }
    for (int i = 0; i < emu_npatches; i++)
        fprintf(f, "%06X:%04X    %s\n", emu_patches[i].addr, emu_patches[i].val, emu_patches[i].name);
    fclose(f);
    emu_message("Patches saved to %s", path);
    return 1;
}

int emu_patch_load(void)
{
    char path[1200], line[256];
    pat_path(path, sizeof path);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int n = 0;
    while (fgets(line, sizeof line, f) && emu_npatches < MAX_PATCHES) {
        char *code = line;
        while (isspace((unsigned char)*code)) code++;
        if (!*code || *code == '#' || *code == ';') continue;
        char *name = code;
        while (*name && !isspace((unsigned char)*name)) name++;
        if (*name) { *name++ = 0; while (isspace((unsigned char)*name)) name++; }
        name[strcspn(name, "\r\n")] = 0;
        uint32_t a; uint16_t v;
        if (!emu_patch_decode(code, &a, &v)) continue;
        Patch *p = &emu_patches[emu_npatches++];
        memset(p, 0, sizeof *p);
        snprintf(p->code, sizeof p->code, "%.15s", code);
        snprintf(p->name, sizeof p->name, "%.39s", name);
        p->addr = a; p->val = v; p->on = 0;   /* loaded disabled, like a cheat list */
        n++;
    }
    fclose(f);
    if (n) emu_message("%d patches read from %s", n, path);
    return n;
}

/* 8-bit PCX writer of the current viewport (palette quantised to RGB332 is
 * lossy, so we write 24-bit PCX with 3 planes like most tools accept). */
int emu_save_pcx(const char *path)
{
    if (!emu.loaded) return 0;
    int w = emu.view_w, h = emu.view_h;
    FILE *f = fopen(path, "wb");
    if (!f) { emu_message("Unable to create %s", path); return 0; }
    unsigned char hdr[128] = {0};
    hdr[0] = 10; hdr[1] = 5; hdr[2] = 1; hdr[3] = 8;
    hdr[8] = (w - 1) & 255; hdr[9] = (w - 1) >> 8; hdr[10] = (h - 1) & 255; hdr[11] = (h - 1) >> 8;
    hdr[12] = 72; hdr[14] = 72; hdr[65] = 3; hdr[66] = w & 255; hdr[67] = w >> 8; hdr[68] = 1;
    fwrite(hdr, 1, 128, f);
    for (int y = 0; y < h; y++) {
        const uint32_t *row = emu.fb + (emu.view_y + y) * EMU_FB_W + emu.view_x;
        for (int plane = 0; plane < 3; plane++) {
            int shift = 16 - plane * 8;
            int x = 0;
            while (x < w) {
                unsigned char v = (row[x] >> shift) & 255;
                int run = 1;
                while (x + run < w && run < 63 && ((row[x + run] >> shift) & 255) == v) run++;
                if (run > 1 || (v & 0xC0) == 0xC0) fputc(0xC0 | run, f);
                fputc(v, f);
                x += run;
            }
        }
    }
    fclose(f);
    emu_message("Snapshot saved to %s", path);
    return 1;
}

/* ======================= Sound menu: mutes, logging, FM info ============== */
static int fm_on = 1, psg_on = 1, dac_on = 1, psg_dirty;
static uint8_t fm_regs[2][256];
static int fm_latch[2];
static uint8_t fm_keyon[6];
static uint8_t psg_regs[8];      /* 0,2,4 tone (10 bits), 6 noise, 1,3,5,7 volumes */
static int psg_latch_ch, psg_latch_vol;
static void (*orig_fm_write)(unsigned int, unsigned int, unsigned int);
static FILE *gym_file, *wav_file;
static uint32_t wav_bytes;

static void hook_fm_write(unsigned int cycles, unsigned int a, unsigned int d)
{
    int port = (a >> 1) & 1;
    if (!(a & 1)) fm_latch[port] = d & 0xFF;
    else {
        int r = fm_latch[port];
        fm_regs[port][r] = (uint8_t)d;
        if (port == 0 && r == 0x28) { int ch = (d & 3) + ((d & 4) ? 3 : 0); if ((d & 3) != 3) fm_keyon[ch] = (d >> 4) & 0xF; }
        if (gym_file) { fputc(1 + port, gym_file); fputc(r, gym_file); fputc(d & 0xFF, gym_file); }
    }
    if (orig_fm_write) orig_fm_write(cycles, a, d);
}

static void hook_psg(unsigned int d)
{
    if (d & 0x80) {
        psg_latch_ch = (d >> 5) & 3; psg_latch_vol = (d >> 4) & 1;
        int idx = psg_latch_ch * 2 + psg_latch_vol;
        if (psg_latch_vol || psg_latch_ch == 3) psg_regs[idx] = d & 0x0F;
        else psg_regs[idx] = (psg_regs[idx] & 0x3F0) | (d & 0x0F);
    } else if (!psg_latch_vol && psg_latch_ch < 3) {
        int idx = psg_latch_ch * 2;
        psg_regs[idx] = (psg_regs[idx] & 0x0F) | ((d & 0x3F) << 4);
    }
    if (gym_file) { fputc(3, gym_file); fputc(d & 0xFF, gym_file); }
}

static void emu_hook_sound(void)
{
    if (fm_write != hook_fm_write) { orig_fm_write = fm_write; fm_write = hook_fm_write; }
    genecyst_psg_hook = hook_psg;
    memset(fm_regs, 0, sizeof fm_regs); memset(fm_keyon, 0, sizeof fm_keyon); memset(psg_regs, 0, sizeof psg_regs);
}

static void emu_apply_sound_options(void)
{
    config.fm_preamp = fm_on ? 100 : 0;
    genecyst_dac_mute = !dac_on;
    psg_dirty = 1;
}

static void emu_sound_frame_end(void)
{
    if (psg_dirty) { psg_config(0, psg_on ? config.psg_preamp : 0, 0xff); psg_dirty = 0; }
    if (gym_file) fputc(0, gym_file);
    if (wav_file && emu.audio_samples > 0) {
        fwrite(emu.audio, 4, (size_t)emu.audio_samples, wav_file);
        wav_bytes += (uint32_t)emu.audio_samples * 4;
    }
}

void emu_set_fm(int on)  { fm_on = on; emu_apply_sound_options(); emu_message("FM music %s", on ? "enabled" : "disabled"); }
void emu_set_psg(int on) { psg_on = on; emu_apply_sound_options(); emu_message("PSG sound %s", on ? "enabled" : "disabled"); }
void emu_set_dac(int on) { dac_on = on; emu_apply_sound_options(); emu_message("DAC sound %s", on ? "enabled" : "disabled"); }
int emu_fm(void) { return fm_on; }
int emu_psg(void) { return psg_on; }
int emu_dac(void) { return dac_on; }

int emu_gym_logging(void) { return gym_file != NULL; }
int emu_gym_toggle(void)
{
    if (gym_file) { fclose(gym_file); gym_file = NULL; emu_message("GYM log closed"); return 0; }
    if (!emu.loaded) { emu_message("No ROM loaded"); return 0; }
    char path[1200];
    paths_sidecar(emu.rompath, paths.logdir, "gym", path, sizeof path);
    gym_file = fopen(path, "wb");
    if (!gym_file) { emu_message("Unable to create %s", path); return 0; }
    emu_message("Logging YM2612/PSG to %s", path);
    return 1;
}

static void wav_write_header(FILE *f, uint32_t data_bytes, int rate)
{
    uint8_t h[44] = "RIFF----WAVEfmt ";
    uint32_t chunk = 36 + data_bytes, fmtlen = 16, byterate = (uint32_t)rate * 4;
    uint16_t pcm = 1, ch = 2, align = 4, bits = 16;
    memcpy(h + 4, &chunk, 4); memcpy(h + 16, &fmtlen, 4); memcpy(h + 20, &pcm, 2); memcpy(h + 22, &ch, 2);
    memcpy(h + 24, &rate, 4); memcpy(h + 28, &byterate, 4); memcpy(h + 32, &align, 2); memcpy(h + 34, &bits, 2);
    memcpy(h + 36, "data", 4); memcpy(h + 40, &data_bytes, 4);
    fseek(f, 0, SEEK_SET); fwrite(h, 1, 44, f);
}

int emu_wav_logging(void) { return wav_file != NULL; }
int emu_wav_toggle(void)
{
    if (wav_file) { wav_write_header(wav_file, wav_bytes, snd.sample_rate); fclose(wav_file); wav_file = NULL; emu_message("WAV log closed"); return 0; }
    if (!emu.loaded) { emu_message("No ROM loaded"); return 0; }
    char path[1200];
    paths_sidecar(emu.rompath, paths.logdir, "wav", path, sizeof path);
    wav_file = fopen(path, "wb");
    if (!wav_file) { emu_message("Unable to create %s", path); return 0; }
    wav_bytes = 0;
    wav_write_header(wav_file, 0, snd.sample_rate);
    emu_message("Logging wave to %s", path);
    return 1;
}

int emu_fm_info(char lines[][64], int max)
{
    int n = 0;
    if (!emu.loaded || max < 1) return 0;
    for (int ch = 0; ch < 6 && n < max; ch++) {
        int port = ch / 3, c = ch % 3;
        int fnum = ((fm_regs[port][0xA4 + c] & 7) << 8) | fm_regs[port][0xA0 + c];
        int block = (fm_regs[port][0xA4 + c] >> 3) & 7;
        int alg = fm_regs[port][0xB0 + c] & 7, fb = (fm_regs[port][0xB0 + c] >> 3) & 7;
        double hz = fnum * 53267.0 * (1 << block) / 2097152.0;
        int dac = (ch == 5) && (fm_regs[0][0x2B] & 0x80);
        snprintf(lines[n++], 64, "FM%d %s %5.0fHZ ALG%d FB%d KEY %X", ch + 1, dac ? "DAC" : "   ", hz, alg, fb, fm_keyon[ch]);
    }
    for (int c = 0; c < 3 && n < max; c++) {
        int tone = psg_regs[c * 2], vol = psg_regs[c * 2 + 1];
        double hz = tone ? 3579545.0 / (32.0 * tone) : 0;
        snprintf(lines[n++], 64, "PSG%d %5.0fHZ VOL %2d", c + 1, hz, 15 - vol);
    }
    if (n < max) snprintf(lines[n++], 64, "NOISE MODE %X VOL %2d  LFO %02X TIMERS %02X", psg_regs[6], 15 - psg_regs[7], fm_regs[0][0x22], fm_regs[0][0x27]);
    return n;
}

/* ======================= GST save states (Genecyst / Gens) ================= */
/* Layout measured from a state written by the original (see
 * reference/original/demo-original.gs0): "GST\0 00 00 E0 40"; 68K D0-D7 @0x80,
 * A0-A7 @0xA0, PC @0xC8, SR @0xD0 (16 bit), USP @0xD2, SSP @0xD6; VDP regs
 * @0xFA (24); CRAM @0x112 (64 LE words, ----BBB-GGG-RRR-); VSRAM @0x192 (80);
 * YM2612 regs @0x1E4 (2 x 256); Z80 AF BC DE HL IX IY PC SP AF' BC' DE' HL'
 * @0x404 (32-bit each), I @0x434, IFF1 @0x435, IFF2 @0x436, IM @0x437,
 * Z80 state @0x438, Z80 bank @0x43C; Z80 RAM @0x474 (8K); 68K RAM @0x2478
 * (64K, big-endian); VRAM @0x12478 (64K); SRAM @0x22478 (64K, Genecyst only).
 * Gens files stop at 0x22478. */
#define GST_SIZE_GENS      0x22478
#define GST_SIZE_GENECYST  0x32478

static uint32_t rd32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static void wr32(uint8_t *p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
static void wr16(uint8_t *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }

int emu_is_gst_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    uint8_t h[8] = {0};
    size_t n = fread(h, 1, 8, f);
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    return n == 8 && !memcmp(h, "GST", 3) && h[3] == 0 && h[4] == 0 && (size == GST_SIZE_GENS || size == GST_SIZE_GENECYST);
}

static void swap_copy(uint8_t *dst, const uint8_t *src, size_t n)
{
    for (size_t i = 0; i + 1 < n; i += 2) { dst[i] = src[i + 1]; dst[i + 1] = src[i]; }
}

int emu_import_gst(const char *path)
{
    if (!emu.loaded) return 0;
    FILE *f = fopen(path, "rb");
    if (!f) { emu_message("Unable to open %s", path); return 0; }
    uint8_t *g = calloc(1, GST_SIZE_GENECYST);
    size_t n = g ? fread(g, 1, GST_SIZE_GENECYST, f) : 0;
    fclose(f);
    if (n < GST_SIZE_GENS) { free(g); emu_message("Invalid GST file"); return 0; }

    /* memories (GPGX keeps 16-bit words little-endian on this host) */
    swap_copy(work_ram, g + 0x2478, 0x10000);
    swap_copy(vram, g + 0x12478, 0x10000);
    memcpy(zram, g + 0x474, 0x2000);
    memcpy(vsram, g + 0x192, 0x50);
    if (n >= GST_SIZE_GENECYST && sram.on) memcpy(sram.sram, g + 0x22478, 0x10000);

    /* VDP registers, then CRAM (packed BBBGGGRRR + pixel tables) */
    for (int i = 0; i < 24; i++) genecyst_vdp_reg_w(i, g[0xFA + i]);
    for (int i = 0; i < 64; i++) {
        uint16_t w = rd16(g + 0x112 + i * 2);
        uint16_t packed = ((w >> 1) & 7) | (((w >> 5) & 7) << 3) | (((w >> 9) & 7) << 6);
        *(uint16_t *)&cram[i * 2] = packed;
        color_update_m5(i, packed);
    }
    color_update_m5(0x00, *(uint16_t *)&cram[(reg[7] & 0x3F) * 2]);
    for (int i = 0; i < 0x800; i++) { bg_name_dirty[i] = 0xFF; bg_name_list[i] = i; }
    bg_list_index = 0x800;

    /* 68000 */
    m68k_set_reg(M68K_REG_SR, rd16(g + 0xD0));
    for (int i = 0; i < 8; i++) { m68k.dar[i] = rd32(g + 0x80 + i * 4); m68k.dar[8 + i] = rd32(g + 0xA0 + i * 4); }
    m68k_set_reg(M68K_REG_PC, rd32(g + 0xC8));

    /* Z80 */
    Z80.af.w.l = rd32(g + 0x404); Z80.bc.w.l = rd32(g + 0x408); Z80.de.w.l = rd32(g + 0x40C); Z80.hl.w.l = rd32(g + 0x410);
    Z80.ix.w.l = rd32(g + 0x414); Z80.iy.w.l = rd32(g + 0x418); Z80.pc.w.l = rd32(g + 0x41C); Z80.sp.w.l = rd32(g + 0x420);
    Z80.af2.w.l = rd32(g + 0x424); Z80.bc2.w.l = rd32(g + 0x428); Z80.de2.w.l = rd32(g + 0x42C); Z80.hl2.w.l = rd32(g + 0x430);
    Z80.i = g[0x434]; Z80.iff1 = g[0x435] & 1; Z80.iff2 = g[0x436] & 1; Z80.im = g[0x437] & 3;
    zstate = g[0x438] & 3;
    zbank = rd32(g + 0x43C) & 0xFF8000;

    /* YM2612: replay the registers (except key-on and DAC data) */
    for (int port = 0; port < 2; port++)
        for (int r = 0x22; r < 0xB8; r++) {
            if (port == 0 && (r == 0x28 || r == 0x2A)) continue;
            if (r > 0x2B && r < 0x30) continue;
            fm_write(0, port * 2, r);
            fm_write(0, port * 2 + 1, g[0x1E4 + port * 256 + r]);
        }
    free(g);
    bitmap.viewport.changed |= 1;
    emu_update_viewport();
    emu_message("GST state restored from %s", path);
    return 1;
}

int emu_export_gst(const char *path)
{
    if (!emu.loaded) return 0;
    uint8_t *g = calloc(1, GST_SIZE_GENECYST);
    if (!g) return 0;
    memcpy(g, "GST\0\0\0\xE0\x40", 8);
    for (int i = 0; i < 8; i++) { wr32(g + 0x80 + i * 4, m68k.dar[i]); wr32(g + 0xA0 + i * 4, m68k.dar[8 + i]); }
    wr32(g + 0xC8, m68k_get_reg(M68K_REG_PC));
    wr16(g + 0xD0, (uint16_t)m68k_get_reg(M68K_REG_SR));
    wr32(g + 0xD2, m68k_get_reg(M68K_REG_USP));
    wr32(g + 0xD6, m68k_get_reg(M68K_REG_ISP));
    for (int i = 0; i < 24; i++) g[0xFA + i] = reg[i];
    for (int i = 0; i < 64; i++) {
        uint16_t p = *(uint16_t *)&cram[i * 2];
        wr16(g + 0x112 + i * 2, (uint16_t)(((p & 7) << 1) | (((p >> 3) & 7) << 5) | (((p >> 6) & 7) << 9)));
    }
    memcpy(g + 0x192, vsram, 0x50);
    memcpy(g + 0x1E4, fm_regs[0], 256); memcpy(g + 0x2E4, fm_regs[1], 256);
    wr32(g + 0x404, Z80.af.w.l); wr32(g + 0x408, Z80.bc.w.l); wr32(g + 0x40C, Z80.de.w.l); wr32(g + 0x410, Z80.hl.w.l);
    wr32(g + 0x414, Z80.ix.w.l); wr32(g + 0x418, Z80.iy.w.l); wr32(g + 0x41C, Z80.pc.w.l); wr32(g + 0x420, Z80.sp.w.l);
    wr32(g + 0x424, Z80.af2.w.l); wr32(g + 0x428, Z80.bc2.w.l); wr32(g + 0x42C, Z80.de2.w.l); wr32(g + 0x430, Z80.hl2.w.l);
    g[0x434] = Z80.i; g[0x435] = Z80.iff1; g[0x436] = Z80.iff2; g[0x437] = Z80.im;
    g[0x438] = zstate & 3;
    wr32(g + 0x43C, zbank);
    memcpy(g + 0x474, zram, 0x2000);
    swap_copy(g + 0x2478, work_ram, 0x10000);
    swap_copy(g + 0x12478, vram, 0x10000);
    if (sram.on) memcpy(g + 0x22478, sram.sram, 0x10000);
    FILE *f = fopen(path, "wb");
    if (!f) { free(g); emu_message("Unable to create %s", path); return 0; }
    fwrite(g, 1, GST_SIZE_GENECYST, f);
    fclose(f);
    free(g);
    emu_message("GST state saved to %s", path);
    return 1;
}

int emu_save_state_gst(int slot)
{
    if (!emu.loaded) return 0;
    char path[1200];
    state_path(slot, path, sizeof path);
    return emu_export_gst(path);
}
