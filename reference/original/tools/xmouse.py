#!/usr/bin/env python3
"""xmouse.py WX WY SCALE  [move] nx ny [nx ny ...]
Click (or just move, with a 'move' token before a pair) at native coords inside
the DOSBox window whose client origin is WX,WY and integer scale SCALE."""
import sys, time
from Xlib import display, X
from Xlib.ext import xtest
d = display.Display()
wx, wy, sc = int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3])
args = sys.argv[4:]
i = 0
while i < len(args):
    click = True
    if args[i] == 'move':
        click = False; i += 1
    x = wx + int(args[i]) * sc + sc // 2; y = wy + int(args[i+1]) * sc + sc // 2; i += 2
    xtest.fake_input(d, X.MotionNotify, x=x, y=y); d.sync(); time.sleep(0.15)
    if click:
        xtest.fake_input(d, X.ButtonPress, 1); d.sync(); time.sleep(0.08)
        xtest.fake_input(d, X.ButtonRelease, 1); d.sync()
    time.sleep(0.5)
