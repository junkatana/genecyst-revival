# TODO (fidelity first, then improvements)

## Done (2026-10-09)
- [x] Second (small, upper-case) GUI font extracted; used by submenus, hotkeys, labels, log
- [x] Messages window (F1) with scroll bar, green log / grey newest line
- [x] Palette and Patterns viewers (CRAM grid, VRAM tiles with addresses and scroll bar)
- [x] Layer toggles (Scroll A/B low/high, sprites, window) via core LUT mask patch
- [x] CPU > Dump submenu (RAM, ROM, VRAM, VSRAM, CRAM, Z80 RAM, YM2612, messages, VDP base/regs, sprites)
- [x] About dialog text and layout
- [x] Directories dialog (Save/PCX/Log/Patch) with text edits
- [x] Set resolution dialog ("Other...") and the full resolution list with F11/F12
- [x] Input device dialog: input type list, test area, REDEFINE KEYS
- [x] Genesis Timing dialog (virtual FPS is live; H/V-blank values are informational)
- [x] ROM patcher dialog (F6): Game Genie + raw codes, Add/Remove/Toggle/Save, `.pat` files
- [x] Maximize box: integer zoom of the game inside larger virtual screens
- [x] Video filters: Scale2x, 2xSaI, Super 2xSaI, Super Eagle, Scanlines, TV, CRT
- [x] Auto-hide of the GUI after 3 s idle (revival feature requested by Arthur; mouse/Esc bring it back)
- [x] Exact window palette (frame/title/body), Genecyst 3D boxes, three-tone blood drips
- [x] Bitmaps from the EXE resource container (icons, check, arrows, close box, arrow cursor)
- [x] Golden inverted-cross cursor (hand-drawn from an early-version screenshot), Misc > Cursor

- [x] Sound menu: FM/PSG/DAC mutes, Wave output, Show FM info overlay, Log wave (.wav), Log YM2612 (.gym)
- [x] Genecyst/Gens GST save states: automatic import on restore, CPU > Save GST state
- [x] genecyst.cfg (resolution, filter, sound, directories, devices, key maps)
- [x] Windows build via GitHub Actions (MSYS2/MinGW-w64), toolchain file, drive letters in Load ROM

## GUI parity
- [ ] Use font.fnt / sfont.fnt from the EXE instead of the screenshot-extracted fonts (format not decoded yet)
- [ ] bc1..bc4.r2 (21x33 animated blob with runtime palette 208..216): purpose unknown (NESticle-style blood cursor?)
- [ ] Window title bar / close box pixel match (compare reference/original/screenshots/explore/)
- [ ] Message popup placement/colour vs original; State popup
- [ ] Movable game window inside the screen (original lets you drag it)
- [ ] Input dialog: joystick calibration / button remap pages of the original
- [ ] Maximize box: exact behaviour of the original is still unknown (does nothing at 320x240)

## Formats
- [ ] 8-bit PCX like the original (currently 24-bit)
- [ ] `.1` split ROM loading

## Emulation options
- [ ] Z80 faking (the original skipped the Z80 and emulated the sound driver by hand; not meaningful with this core, item is disabled)
- [ ] Frameskip, CPU skip semantics
- [ ] Rom checksum fixer
- [ ] Timing dialog: real H-blank/V-blank instruction counts need core support

## Platform
- [ ] Windows: application icon, test on real hardware/joysticks
- [ ] Joystick remapping UI
