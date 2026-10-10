# Local patches to the vendored Genesis Plus GX core

- `core/vdp_render.c`, `core/vdp_render.h`: `genecyst_layer_mask` /
  `genecyst_set_layer_mask()` — per-layer display toggles (Scroll A/B low and
  high priority, sprites, window) for the View > Layer debug menu. Implemented by
  masking the inputs of the priority look-up tables and by blanking the window
  plane columns. Search for `GENECYST` to find the hunks.
- `core/vdp_ctrl.c`, `core/vdp_ctrl.h`: `genecyst_vdp_reg_w()` wrapper around the
  static `vdp_reg_w()` so the frontend can import Genecyst/Gens GST save states.
- `core/sound/psg.c`, `core/sound/psg.h`: `genecyst_psg_hook` function pointer
  called on every PSG write (GYM logging, PSG register shadow for the FM info overlay).
- `core/sound/ym2612.c`, `core/sound/ym2612.h`: `genecyst_dac_mute` flag to silence
  the channel 6 DAC (Sound > DAC sound enable).
