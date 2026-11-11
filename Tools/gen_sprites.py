#!/usr/bin/env python3
"""Генератор Source/Sprites.h. Кандзи растеризуются системным шрифтом (Unifont)
только как генератор массивов, сам шрифт в плагин не вшивается."""
from PIL import Image, ImageDraw, ImageFont
import math, sys

def kanji(ch, path):
    f = ImageFont.truetype(path, 16)
    im = Image.new('1', (16, 16), 0)
    d = ImageDraw.Draw(im)
    d.fontmode = '1'
    d.text((0, 0), ch, font=f, fill=1)
    return [''.join('#' if im.getpixel((x, y)) else '.' for x in range(16)) for y in range(16)]

def circles(w, h, cs):
    rows = []
    for y in range(h):
        r = ''
        for x in range(w):
            on = any((x + .5 - cx) ** 2 + (y + .5 - cy) ** 2 <= rad * rad for cx, cy, rad in cs)
            r += '#' if on else '.'
        rows.append(r)
    return rows

def moon():
    rows = []
    for y in range(13):
        r = ''
        for x in range(13):
            a = (x + .5 - 6.5) ** 2 + (y + .5 - 6.5) ** 2 <= 6.4 ** 2
            b = (x + .5 - 9.6) ** 2 + (y + .5 - 5.2) ** 2 <= 5.7 ** 2
            r += '#' if a and not b else '.'
        rows.append(r)
    return rows

def flatten_bottom(rows, keep):
    return rows[:keep] + ['.' * len(rows[0])] * (len(rows) - keep)

def cloud(w, h, cs):
    rows = circles(w, h, cs)
    # плоское дно
    rows = [r if y < h - 1 else '#' * (w - 2) and ('.' + '#' * (w - 2) + '.') for y, r in enumerate(rows)]
    return rows

HEART = ['.##.##.', '#######', '#######', '.#####.', '..###..', '...#...']
SPARK = ['..#..', '..#..', '#####', '..#..', '..#..']
BOLT = ['....###', '...###.', '..###..', '.######', '....##.', '...##..', '..##...', '.##....', '##.....']
CLOUD_S = cloud(16, 7, [(4, 5, 2.6), (8, 3.4, 3.4), (12, 5, 2.8)])
CLOUD_M = cloud(26, 10, [(6, 7, 3.5), (12, 5, 5), (19, 6, 4.2), (23, 8, 2.6)])
CLOUD_L = cloud(40, 14, [(8, 10, 4.5), (15, 7, 6.5), (24, 8, 6), (32, 10, 4.5), (36, 11, 3)])

sprites = [('HEART', HEART), ('SPARKLE', SPARK), ('MOON', moon()), ('BOLT', BOLT),
           ('CLOUD_S', CLOUD_S), ('CLOUD_M', CLOUD_M), ('CLOUD_L', CLOUD_L)]
path = sys.argv[1] if len(sys.argv) > 1 else '/usr/share/fonts/opentype/unifont/unifont.otf'
sprites += [('KANJI_HAI', kanji('灰', path)), ('KANJI_AI', kanji('愛', path))]

out = ['// Сгенерировано Tools/gen_sprites.py. Не править руками.',
       '#pragma once', '', 'namespace sprites', '{', '']
for name, rows in sprites:
    out.append(f'struct {name}_T {{ static constexpr int w = {len(rows[0])}, h = {len(rows)}; }};')
    out.append(f'inline constexpr const char* {name}[] = {{')
    for r in rows:
        out.append(f'    "{r}",')
    out.append('};')
    out.append('')
out.append('} // namespace sprites')
open('Source/Sprites.h', 'w').write('\n'.join(out) + '\n')
for name, rows in sprites:
    if name in ('MOON', 'KANJI_HAI', 'KANJI_AI', 'CLOUD_M', 'BOLT'):
        print(name); print('\n'.join(rows)); print()
