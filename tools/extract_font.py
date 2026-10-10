#!/usr/bin/env python3
"""Extract the Genecyst GUI bitmap font from native 320x240 screenshots.

Regions: (image, x0, x1, y0, y1, [line texts top->bottom]). Text rows are
detected as bands of rows containing text-coloured pixels; glyphs are split on
blank columns and matched left-to-right against the known text. Adjacent glyphs
that touch (no blank column) are separated iteratively using glyph widths
learned from unambiguous occurrences. Output: C table, 9 rows per glyph."""
import sys, os
from collections import Counter
from PIL import Image

CHECK = '\x01'
GLYPH_H = 9
MAXW = 8

def is_text(p):
    r, g, b = p[:3]
    return 195 <= r <= 248 and 205 <= g <= 252 and b >= 230   # #D4DCF6 / #F2FBFF, not cursor white

def bands(px, x0, x1, y0, y1):
    rows = [y for y in range(y0, y1) if any(is_text(px[x, y]) for x in range(x0, x1))]
    out, cur = [], []
    for y in rows:
        if cur and (y - cur[-1] > 2 or y - cur[0] >= 10):
            out.append(cur); cur = []
        cur.append(y)
    if cur: out.append(cur)
    return [(b[0], b[-1]) for b in out]

def col_profile(px, x0, x1, ytop):
    return [any(is_text(px[x, y]) for y in range(ytop, ytop + GLYPH_H)) for x in range(x0, x1)]

def segments(cols, x0):
    segs, start = [], None
    for i, c in enumerate(cols + [False]):
        if c and start is None: start = i
        if not c and start is not None:
            segs.append((x0 + start, x0 + i)); start = None
    return segs

def bits(px, xa, xb, ytop):
    rows = []
    for y in range(ytop, ytop + GLYPH_H):
        v = 0
        for i, x in enumerate(range(xa, xb)):
            if is_text(px[x, y]): v |= 1 << i
        rows.append(v)
    return tuple(rows)

class Line:
    def __init__(self, img, px, x0, x1, ytop, text):
        self.img, self.px, self.ytop, self.text = img, px, ytop, text
        self.chars = [c for c in text if c != ' ']
        self.segs = segments(col_profile(px, x0, x1, ytop), x0)

def resolve(lines, votes, widths):
    """One pass: returns number of newly resolved glyph occurrences."""
    progress = 0
    for ln in lines:
        ci = 0
        pending = list(ln.segs)
        while pending and ci < len(ln.chars):
            xa, xb = pending.pop(0)
            W = xb - xa
            ch = ln.chars[ci]
            if ch in widths:
                w = widths[ch]
                if W == w:
                    votes[ch][bits(ln.px, xa, xb, ln.ytop)] += 1; ci += 1; progress += 1
                elif W > w:
                    votes[ch][bits(ln.px, xa, xa + w, ln.ytop)] += 1; ci += 1; progress += 1
                    pending.insert(0, (xa + w, xb))        # rest of the merged run
                else:
                    ci += 1                                 # inconsistent, skip char
            else:
                nxt = ln.chars[ci + 1] if ci + 1 < len(ln.chars) else None
                if W <= MAXW and (nxt is None or nxt in widths or W <= 4 or len(pending) >= len(ln.chars) - ci - 1):
                    # treat as a single glyph when the remaining seg count can still cover remaining chars
                    votes[ch][bits(ln.px, xa, xb, ln.ytop)] += 1; ci += 1; progress += 1
                elif nxt in widths and 1 <= W - widths[nxt] <= MAXW:
                    w = W - widths[nxt]
                    votes[ch][bits(ln.px, xa, xa + w, ln.ytop)] += 1; ci += 1; progress += 1
                    pending.insert(0, (xa + w, xb))
                else:
                    ci += 1  # give up on this char for now
    return progress

def is_menu(p):
    r, g, b = p[:3]
    return (r > 120 and g < 40 and b < 40) or is_text(p) or (r < 40 and g < 40 and b < 40)

def auto_bounds(px, x0, x1, y0, y1):
    # shrink to the menu-coloured area starting at (x0+1, y0+1); tolerate 2px lines
    def scan(get, a, b):
        e, miss = a, 0
        while e < b:
            if is_menu(get(e)) or max(get(e)[:3]) > 245: miss = 0
            else:
                miss += 1
                if miss > 2: return e - miss + 1
            e += 1
        return b
    return scan(lambda x: px[x, y0 + 1], x0, x1), scan(lambda y: px[x0 + 1, y], y0, y1)

def main(regions, out_path):
    lines = []
    for img, x0, x1, y0, y1, texts in regions:
        im = Image.open(img).convert('RGB'); px = im.load()
        bs = bands(px, x0, x1, y0, y1)
        if len(bs) != len(texts):
            print(f'{os.path.basename(img)}: {len(bs)} bands vs {len(texts)} lines -> {bs}', file=sys.stderr)
        for (ya, yb), text in zip(bs, texts):
            lines.append(Line(os.path.basename(img), px, x0, x1, ya, text))
    # pass 1: lines whose segment count matches the char count are unambiguous
    votes = _VoteDict()
    widths = {}
    for ln in lines:
        if len(ln.segs) == len(ln.chars):
            for ch, (xa, xb) in zip(ln.chars, ln.segs):
                votes[ch][bits(ln.px, xa, xb, ln.ytop)] += 1
    def settle():
        for ch, c in votes.items():
            b, _ = c.most_common(1)[0]
            widths[ch] = max(len(bin(r)) - 2 for r in b) if any(b) else 1
    settle()
    # iterative passes over everything
    for _ in range(8):
        for ch in list(votes): votes[ch] = Counter()
        resolve(lines, votes, widths)
        settle()
    glyphs = {}
    for ch, c in votes.items():
        if not c: continue
        b, n = c.most_common(1)[0]
        if len(c) > 1:
            print(f'{ch!r}: {len(c)} variants, kept one with {n} votes of {sum(c.values())}', file=sys.stderr)
        w = max((len(bin(r)) - 2 for r in b if r), default=1)
        glyphs[ch] = (w, b)
    with open(out_path, 'w') as f:
        f.write('/* Generated by tools/extract_font.py from reference screenshots. Do not edit. */\n#include "font.h"\n\n')
        f.write('const FontGlyph font_genecyst_glyphs[] = {\n')
        for ch in sorted(glyphs):
            w, rows = glyphs[ch]
            f.write('  { %3d, %d, {%s} }, /* %s */\n' % (ord(ch), w, ','.join('0x%02x' % r for r in rows), 'CHECK' if ch == CHECK else repr(ch)))
        f.write('};\nconst int font_genecyst_count = sizeof(font_genecyst_glyphs)/sizeof(font_genecyst_glyphs[0]);\n')
    print('chars:', ''.join(c if c != CHECK else '✓' for c in sorted(glyphs)))
    print('missing:', ''.join(c for c in 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.,:;!?-_/\\()[]#%&*+=<>\'"' if c not in glyphs))

class _VoteDict(dict):
    def __getitem__(self, k):
        if k not in self: dict.__setitem__(self, k, Counter())
        return dict.__getitem__(self, k)

if __name__ == '__main__':
    S = sys.argv[1]
    R = lambda n: os.path.join(S, n)
    regions = [
        (R('native_20_clean.png'), 0, 246, 0, 10, ['File View CPU Settings Sound Misc']),
        (R('native_31_file.png'), 0, 112, 10, 90, ['Load ROM... L', 'Free ROM', 'Directories...', 'Fix ROM checksum', 'Save snapshot F9', 'Exit Q']),
        (R('native_32_view.png'), 36, 104, 10, 50, ['Messages F1', 'Palette', 'Patterns', 'Layer']),
        (R('native_33_cpu.png'), 72, 136, 10, 100, ['Reset R', 'Pause P', 'Advance frame A', 'CPU skip S', 'Save state F5', 'Restore state F7', 'ROM patch... F6', 'Dump']),
        (R('native_34_settings.png'), 108, 230, 10, 132, ['Input', 'Resolution', 'Frameskip', 'Vsync', 'Timing...', 'DMA fix', 'Force display', 'H-Interrupt enable H', 'Raster effects E', 'Auto-Version', 'Version', '6 Button']),
        (R('native_35_sound.png'), 160, 236, 10, 160, ['Rate', 'Stereo', 'Z80 enable Z', 'Z80 faking', 'FM music enable M', 'PSG sound enable G', 'DAC sound enable D', 'Wave output W', 'Show FM info F', 'Log wave', 'Log YM2612']),
        (R('native_36_misc.png'), 212, 300, 10, 58, ['Hide GUI', 'Message popup', 'State popup', 'About...']),
        # Load ROM dialog (native_20_clean): title, path, list entries, button
        (R('native_20_clean.png'), 26, 90, 17, 28, ['Load ROM']),
        (R('native_20_clean.png'), 26, 60, 29, 40, ['C:\\']),
        (R('native_20_clean.png'), 40, 180, 41, 52, ['DEMO.BIN']),
        (R('native_20_clean.png'), 40, 180, 71, 190, ['[-A-]', '[-B-]', '[-C-]', '[-D-]', '[-E-]', '[-F-]', '[-G-]', '[-H-]', '[-I-]', '[-J-]', '[-K-]', '[-L-]']),
        (R('native_20_clean.png'), 98, 130, 193, 203, ['LOAD']),
    ]
    main(regions, sys.argv[2])
