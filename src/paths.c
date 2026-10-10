#include "paths.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

Paths paths;

void paths_init(void)
{
    memset(&paths, 0, sizeof paths);
    if (!getcwd(paths.romdir, sizeof paths.romdir))
        strcpy(paths.romdir, ".");
}

void paths_dirname(const char *path, char *out, size_t outsz)
{
    const char *s = strrchr(path, '/');
#ifdef _WIN32
    const char *b = strrchr(path, '\\');
    if (!s || (b && b > s)) s = b;
#endif
    if (!s) { out[0] = 0; return; }
    size_t n = (size_t)(s - path);
    if (n >= outsz) n = outsz - 1;
    memcpy(out, path, n);
    out[n] = 0;
}

void paths_rom_basename(const char *rompath, char *out, size_t outsz)
{
    const char *s = strrchr(rompath, '/');
#ifdef _WIN32
    const char *b = strrchr(rompath, '\\');
    if (!s || (b && b > s)) s = b;
#endif
    s = s ? s + 1 : rompath;
    snprintf(out, outsz, "%s", s);
    char *dot = strrchr(out, '.');
    if (dot && dot != out) *dot = 0;
}

void paths_sidecar(const char *rompath, const char *dir, const char *ext, char *out, size_t outsz)
{
    char base[512], d[1024];
    paths_rom_basename(rompath, base, sizeof base);
    if (dir && dir[0]) snprintf(d, sizeof d, "%s", dir);
    else paths_dirname(rompath, d, sizeof d);
    if (d[0]) snprintf(out, outsz, "%s/%s.%s", d, base, ext);
    else snprintf(out, outsz, "%s.%s", base, ext);
}

int paths_next_snapshot(char *out, size_t outsz)
{
    for (int i = 0; i < 1000; i++) {
        struct stat st;
        if (paths.pcxdir[0]) snprintf(out, outsz, "%s/snap%03d.pcx", paths.pcxdir, i);
        else snprintf(out, outsz, "snap%03d.pcx", i);
        if (stat(out, &st) != 0) return 1;
    }
    return 0;
}
