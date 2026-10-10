#include "pads.h"
#include "emu.h"
#include "shared.h"
#include <string.h>
#include <stdio.h>

typedef struct { SDL_Scancode key[BTN_COUNT]; } KeySet;

static KeySet keysets[2] = {
    /* Genecyst defaults: ABC = z x c, Start = v, XYZ = a s d, Mode = f, arrows */
    {{ SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
       SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C, SDL_SCANCODE_V,
       SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_F }},
    {{ SDL_SCANCODE_KP_8, SDL_SCANCODE_KP_2, SDL_SCANCODE_KP_4, SDL_SCANCODE_KP_6,
       SDL_SCANCODE_KP_1, SDL_SCANCODE_KP_3, SDL_SCANCODE_KP_5, SDL_SCANCODE_KP_ENTER,
       SDL_SCANCODE_KP_7, SDL_SCANCODE_KP_9, SDL_SCANCODE_KP_0, SDL_SCANCODE_KP_PERIOD }},
};
static const uint16_t btn_bits[BTN_COUNT] = {
    INPUT_UP, INPUT_DOWN, INPUT_LEFT, INPUT_RIGHT, INPUT_A, INPUT_B, INPUT_C, INPUT_START,
    INPUT_X, INPUT_Y, INPUT_Z, INPUT_MODE };
static const char *btn_names[BTN_COUNT] = { "UP", "DOWN", "LEFT", "RIGHT", "A", "B", "C", "START", "X", "Y", "Z", "MODE" };

static int port_type[2] = { INDEV_KEYBOARD1, INDEV_NONE };
static SDL_GameController *pads[2];
static uint16_t state[2];
int pads_test_mode;

static int redef_port = -1, redef_btn;
static KeySet redef_set;

void pads_init(void) { pads_open_controllers(); }

void pads_shutdown(void)
{
    for (int i = 0; i < 2; i++) if (pads[i]) { SDL_GameControllerClose(pads[i]); pads[i] = NULL; }
}

void pads_open_controllers(void)
{
    int n = 0;
    for (int i = 0; i < 2; i++) if (pads[i]) { SDL_GameControllerClose(pads[i]); pads[i] = NULL; }
    for (int i = 0; i < SDL_NumJoysticks() && n < 2; i++) {
        if (!SDL_IsGameController(i)) continue;
        pads[n] = SDL_GameControllerOpen(i);
        if (!pads[n]) continue;
        emu_message("Joystick %d: %s", n + 1, SDL_GameControllerName(pads[n]));
        /* auto-assign a free port to the new pad */
        int type = INDEV_GAMEPAD1 + n;
        if (port_type[0] != type && port_type[1] != type) {
            if (port_type[1] == INDEV_NONE) port_type[1] = type;
            else if (port_type[0] == INDEV_NONE) port_type[0] = type;
        }
        n++;
    }
}

const char *pads_type_name(int type)
{
    static const char *names[INDEV_COUNT] = { "None", "Keyboard 1", "Keyboard 2", "Joystick 1", "Joystick 2" };
    return (type >= 0 && type < INDEV_COUNT) ? names[type] : "?";
}

int pads_type(int port) { return port_type[port & 1]; }
void pads_set_type(int port, int type)
{
    if (type < 0 || type >= INDEV_COUNT) return;
    port_type[port & 1] = type;
    emu_message("Device %d: %s", (port & 1) + 1, pads_type_name(type));
}

static uint16_t read_keys(const KeySet *ks, const Uint8 *k)
{
    uint16_t v = 0;
    for (int b = 0; b < BTN_COUNT; b++) if (k[ks->key[b]]) v |= btn_bits[b];
    return v;
}

static uint16_t read_pad(SDL_GameController *g)
{
    if (!g) return 0;
    uint16_t v = 0;
#define B(btn) SDL_GameControllerGetButton(g, btn)
    if (B(SDL_CONTROLLER_BUTTON_DPAD_UP)) v |= INPUT_UP;
    if (B(SDL_CONTROLLER_BUTTON_DPAD_DOWN)) v |= INPUT_DOWN;
    if (B(SDL_CONTROLLER_BUTTON_DPAD_LEFT)) v |= INPUT_LEFT;
    if (B(SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) v |= INPUT_RIGHT;
    int ax = SDL_GameControllerGetAxis(g, SDL_CONTROLLER_AXIS_LEFTX), ay = SDL_GameControllerGetAxis(g, SDL_CONTROLLER_AXIS_LEFTY);
    if (ax < -12000) v |= INPUT_LEFT;
    if (ax > 12000) v |= INPUT_RIGHT;
    if (ay < -12000) v |= INPUT_UP;
    if (ay > 12000) v |= INPUT_DOWN;
    if (B(SDL_CONTROLLER_BUTTON_X)) v |= INPUT_A;
    if (B(SDL_CONTROLLER_BUTTON_A)) v |= INPUT_B;
    if (B(SDL_CONTROLLER_BUTTON_B)) v |= INPUT_C;
    if (B(SDL_CONTROLLER_BUTTON_START)) v |= INPUT_START;
    if (B(SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) v |= INPUT_X;
    if (B(SDL_CONTROLLER_BUTTON_Y)) v |= INPUT_Y;
    if (B(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) v |= INPUT_Z;
    if (B(SDL_CONTROLLER_BUTTON_BACK)) v |= INPUT_MODE;
#undef B
    return v;
}

void pads_poll(int block_keys)
{
    const Uint8 *k = SDL_GetKeyboardState(NULL);
    int keys_ok = !block_keys || pads_test_mode;
    if (redef_port >= 0) keys_ok = 0;
    for (int p = 0; p < 2; p++) {
        uint16_t v = 0;
        switch (port_type[p]) {
        case INDEV_KEYBOARD1: if (keys_ok) v = read_keys(&keysets[0], k); break;
        case INDEV_KEYBOARD2: if (keys_ok) v = read_keys(&keysets[1], k); break;
        case INDEV_GAMEPAD1: v = read_pad(pads[0]); break;
        case INDEV_GAMEPAD2: v = read_pad(pads[1]); break;
        default: break;
        }
        state[p] = v;
        emu_pad[p] = v;
    }
}

uint16_t pads_state(int port) { return state[port & 1]; }

/* ---- key redefinition ---------------------------------------------------- */
void pads_redefine_begin(int port)
{
    int t = port_type[port & 1];
    if (t != INDEV_KEYBOARD1 && t != INDEV_KEYBOARD2) { emu_message("Select Keyboard 1 or 2 first"); return; }
    redef_port = port & 1; redef_btn = 0;
    memset(&redef_set, 0, sizeof redef_set);
}

int pads_redefine_active(void) { return redef_port >= 0; }

const char *pads_redefine_prompt(void)
{
    static char buf[40];
    if (redef_port < 0) return NULL;
    snprintf(buf, sizeof buf, "PRESS KEY FOR %s", btn_names[redef_btn]);
    return buf;
}

int pads_redefine_feed(SDL_Scancode sc)
{
    if (redef_port < 0) return 0;
    if (sc == SDL_SCANCODE_ESCAPE) { redef_port = -1; emu_message("Key redefinition cancelled"); return 2; }
    redef_set.key[redef_btn++] = sc;
    if (redef_btn >= BTN_COUNT) {
        int set = port_type[redef_port] == INDEV_KEYBOARD2 ? 1 : 0;
        keysets[set] = redef_set;
        redef_port = -1;
        emu_message("Keys redefined");
        return 2;
    }
    return 1;
}

void pads_redefine_cancel(void) { redef_port = -1; }

void pads_get_keys(int set, int *out12)
{
    for (int b = 0; b < BTN_COUNT; b++) out12[b] = keysets[set & 1].key[b];
}

void pads_set_keys(int set, const int *in12)
{
    for (int b = 0; b < BTN_COUNT; b++) if (in12[b] > 0 && in12[b] < SDL_NUM_SCANCODES) keysets[set & 1].key[b] = (SDL_Scancode)in12[b];
}

const char *pads_key_name(int port, int button)
{
    int t = port_type[port & 1];
    if (t != INDEV_KEYBOARD1 && t != INDEV_KEYBOARD2) return "";
    return SDL_GetScancodeName(keysets[t == INDEV_KEYBOARD2].key[button]);
}
