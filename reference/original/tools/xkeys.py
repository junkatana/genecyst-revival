#!/usr/bin/env python3
"""Send key chords to the focused X11 window via XTest. Usage: xkeys.py 'alt+f' 'Down' 'Return' ... (pauses 0.4s between)"""
import sys, time
from Xlib import display, X, XK
from Xlib.ext import xtest
d = display.Display()
MODS = {'alt': 'Alt_L', 'ctrl': 'Control_L', 'shift': 'Shift_L'}
def kc(name):
    ks = XK.string_to_keysym(name)
    if ks == 0: raise SystemExit(f'unknown key {name}')
    return d.keysym_to_keycode(ks)
for chord in sys.argv[1:]:
    if chord.startswith('sleep:'):
        time.sleep(float(chord[6:])); continue
    parts = chord.split('+')
    keys = [MODS.get(p.lower(), p) for p in parts]
    codes = [kc(k) for k in keys]
    for c in codes: xtest.fake_input(d, X.KeyPress, c); d.sync(); time.sleep(0.05)
    for c in reversed(codes): xtest.fake_input(d, X.KeyRelease, c); d.sync(); time.sleep(0.05)
    time.sleep(0.4)
