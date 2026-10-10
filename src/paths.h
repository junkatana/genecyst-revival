/* paths.h -- ROM/save/snapshot directory handling, Genecyst style (<romname>.GS?, .GSV, snapNNN.pcx) */
#ifndef GENECYST_PATHS_H
#define GENECYST_PATHS_H

#include <stddef.h>

typedef struct {
    char romdir[1024];   /* startup dir for the Load ROM dialog */
    char savedir[1024];  /* .GS? and .GSV files; empty = next to the ROM */
    char pcxdir[1024];   /* snapshots; empty = current dir */
    char logdir[1024];   /* .wav / .gym logs */
    char patdir[1024];   /* .pat files */
} Paths;

extern Paths paths;

void paths_init(void);
/* basename of the rom without extension, e.g. "/x/Sonic.bin" -> "Sonic" */
void paths_rom_basename(const char *rompath, char *out, size_t outsz);
/* builds "<dir>/<base>.<ext>"; dir falls back to the rom's directory when empty */
void paths_sidecar(const char *rompath, const char *dir, const char *ext, char *out, size_t outsz);
/* next free "<pcxdir>/snapNNN.pcx" */
int paths_next_snapshot(char *out, size_t outsz);
/* directory part of a path ("" when none); trailing slash removed */
void paths_dirname(const char *path, char *out, size_t outsz);

#endif
