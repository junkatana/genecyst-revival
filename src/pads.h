/* input.h -- controller devices for the two Genesis ports: keyboard sets,
 * game controllers, key redefinition and the test-area state used by the
 * Input device dialog. */
#ifndef GENECYST_PADS_H
#define GENECYST_PADS_H
#include <SDL.h>
#include <stdint.h>

enum { INDEV_NONE, INDEV_KEYBOARD1, INDEV_KEYBOARD2, INDEV_GAMEPAD1, INDEV_GAMEPAD2, INDEV_COUNT };
enum { BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_A, BTN_B, BTN_C, BTN_START, BTN_X, BTN_Y, BTN_Z, BTN_MODE, BTN_COUNT };

void pads_init(void);
void pads_shutdown(void);
void pads_open_controllers(void);         /* (re)scan SDL game controllers */
const char *pads_type_name(int type);
int  pads_type(int port);
void pads_set_type(int port, int type);
/* read devices into emu_pad[]; block_keys = GUI window has keyboard focus */
void pads_poll(int block_keys);
uint16_t pads_state(int port);            /* last polled state (INPUT_* bits) */
extern int pads_test_mode;                 /* set while the Input dialog is open: keys still reach the pads */

/* key redefinition: feed scancodes until it returns 2 */
void pads_redefine_begin(int port);
int  pads_redefine_active(void);
const char *pads_redefine_prompt(void);  /* e.g. "PRESS KEY FOR UP" */
int  pads_redefine_feed(SDL_Scancode sc); /* 1 consumed, 2 finished */
void pads_redefine_cancel(void);
const char *pads_key_name(int port, int button);
/* raw key maps (SDL scancodes) of keyboard set 0/1, for the config file */
void pads_get_keys(int set, int *out12);
void pads_set_keys(int set, const int *in12);
#endif
