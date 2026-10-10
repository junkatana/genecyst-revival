/* osd.h -- platform glue expected by the Genesis Plus GX core (included via shared.h) */
#ifndef _OSD_H_
#define _OSD_H_

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>

#define MAX_INPUTS 8

#include "config.h"
#include "error.h"
#include "unzip.h"
#include "fileio.h"

extern int log_error;
extern int genecyst_input_update(void);
#define osd_input_update genecyst_input_update

/* Optional firmware/lock-on ROMs, looked up next to the executable's working dir */
#define GG_ROM      "./ggenie.bin"
#define AR_ROM      "./areplay.bin"
#define SK_ROM      "./sk.bin"
#define SK_UPMEM    "./sk2chip.bin"
#define CD_BIOS_US  "./bios_CD_U.bin"
#define CD_BIOS_EU  "./bios_CD_E.bin"
#define CD_BIOS_JP  "./bios_CD_J.bin"
#define MD_BIOS     "./bios_MD.bin"
#define MS_BIOS_US  "./bios_U.sms"
#define MS_BIOS_EU  "./bios_E.sms"
#define MS_BIOS_JP  "./bios_J.sms"
#define GG_BIOS     "./bios.gg"

#endif /* _OSD_H_ */
