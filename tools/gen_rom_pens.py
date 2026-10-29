#!/usr/bin/env python3
"""Generate src/data/rom_pens.h from the Bomb Jack sprite ROMs.

The clone's atlas (assets/sprites.png) is a colour rip, which cannot tell
some ROM pens apart. Palette-14 sprites (Jack while the freeze lasts, the
Power ball) are recoloured per pen, so this finds the ROM sprite behind each
of Jack's atlas cells (atlas::JACK) and writes its pen map.

usage: tools/gen_rom_pens.py ROMDIR      (ROMDIR holds 14_j07b.bin, 15_l07b.bin,
                                          16_m07b.bin of the MAME "bombjack" set)
Run from the repository root. Needs Pillow.
"""
import re
import sys
from PIL import Image

ROOT = '.'


def load_planes(romdir):
    return [open(f'{romdir}/{f}', 'rb').read() for f in ('14_j07b.bin', '15_l07b.bin', '16_m07b.bin')]


def mask(planes, code):
    """16x16 pen map of sprite `code` (3 bitplanes, four 8x8 quarters)."""
    m = [[0] * 16 for _ in range(16)]
    base = code * 32
    for y in range(16):
        for x in range(16):
            off = base + (y % 8) + (16 if y >= 8 else 0) + (8 if x >= 8 else 0)
            bit = 7 - (x % 8)
            m[y][x] = sum(((planes[k][off] >> bit) & 1) << k for k in range(3))
    return m


def orient(m, o):
    """Rotate a pen map o%4 quarter turns, then mirror it if o >= 4 (the
    cabinet's monitor is rotated, so the atlas holds the sprites turned)."""
    r = [row[:] for row in m]
    for _ in range(o % 4):
        r = [list(x) for x in zip(*r[::-1])]
    if o >= 4:
        r = [row[::-1] for row in r]
    return r


def jack_cells():
    src = open(f'{ROOT}/src/render/sprite_atlas.h').read()
    blk = src[src.index('inline constexpr AtlasRect JACK[]'):]
    blk = blk[:blk.index('};')]
    return [(int(a), int(b)) for a, b in re.findall(r'\{\s*(\d+),\s*(\d+), SIZE_16PX', blk)]


def main():
    planes = load_planes(sys.argv[1])
    px = Image.open(f'{ROOT}/assets/sprites.png').convert('RGBA').load()

    # Each Jack cell: the sprite and orientation whose shape matches exactly.
    out = []
    for (x0, y0) in jack_cells():
        best = (0,)
        for c in range(0x100):
            for o in (1, 5):
                m = orient(mask(planes, c), o)
                i = u = 0
                for y in range(16):
                    for x in range(16):
                        a = m[y][x] > 0
                        b = px[x0 + x, y0 + y][3] > 0
                        i += a and b
                        u += a or b
                if u and i / u > best[0]:
                    best = (i / u, c, o, m)
        assert best[0] == 1.0, (x0, y0, best[:3])
        out.append(best[1:])

    def rows(m, n):
        return '    "' + '"\n    "'.join(''.join('%x' % m[y][x] for x in range(16)) for y in range(n))

    lines = [
        '// Generated from the original Bomb Jack gfx ROMs (MAME "bombjack", Tehkan 1984).',
        '//',
        '// ROM pen (0-7) of every pixel of the sprites the arcade draws with palette 14',
        '// (one hex digit per pixel): Jack\'s living frames, in atlas::JACK order and',
        '// JACK_FH rows of SIZE_16PX, while he holds the Power ball; and the Power ball',
        '// (sprite 0x3D, 16x16 cell atlas::POWER_BALL). Palette 14 rotates, so these are',
        '// recoloured per pen at draw time. The rip shows Jack\'s pens 2 and 4 in the',
        '// same white, so the atlas alone cannot tell them apart.',
        '#pragma once', '', 'namespace rompens {', '',
        'inline constexpr const char* kJack[] = {',
    ]
    for code, o, m in out:
        lines.append('    // ROM sprite 0x%02X%s' % (code, ' (mirrored)' if o == 5 else ''))
        lines.append(rows(m, 15) + '",')
    lines += ['};', '', '// ROM sprite 0x3D.', 'inline constexpr const char* kPowerBall =']
    lines.append(rows(orient(mask(planes, 0x3D), 1), 16) + '";')
    lines += ['', '}  // namespace rompens', '']
    open(f'{ROOT}/src/data/rom_pens.h', 'w').write('\n'.join(lines))


if __name__ == '__main__':
    main()
