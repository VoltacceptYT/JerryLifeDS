"""Hand-drawn small pixel font ("fontsm") with real lowercase letters.

Replaces the old 8 px rasterisation of JerrifiedPixels.ttf, which only has capitals and turns to
mush at small sizes. Cell: 8 rows (rows 0-5 = capitals/ascenders, rows 2-5 = x-height, rows 6-7 =
descenders), glyphs up to 5 px wide with proportional advance.

Format: a line "@ <char> [first_row]" starts a glyph, followed by art lines ('#' = ink).
Glyphs default to starting at row 0; punctuation/lowercase that sits lower gives its first row.
"""

HEIGHT = 8
SPACE_ADV = 3

ART = r"""
@ A
.###.
#...#
#...#
#####
#...#
#...#
@ B
####.
#...#
####.
#...#
#...#
####.
@ C
.###.
#...#
#....
#....
#...#
.###.
@ D
####.
#...#
#...#
#...#
#...#
####.
@ E
#####
#....
####.
#....
#....
#####
@ F
#####
#....
####.
#....
#....
#....
@ G
.###.
#...#
#....
#.###
#...#
.###.
@ H
#...#
#...#
#####
#...#
#...#
#...#
@ I
###
.#.
.#.
.#.
.#.
###
@ J
..###
...#.
...#.
...#.
#..#.
.##..
@ K
#...#
#..#.
###..
#..#.
#...#
#...#
@ L
#....
#....
#....
#....
#....
#####
@ M
#...#
##.##
#.#.#
#...#
#...#
#...#
@ N
#...#
##..#
#.#.#
#..##
#...#
#...#
@ O
.###.
#...#
#...#
#...#
#...#
.###.
@ P
####.
#...#
#...#
####.
#....
#....
@ Q
.###.
#...#
#...#
#.#.#
#..#.
.##.#
@ R
####.
#...#
#...#
####.
#..#.
#...#
@ S
.####
#....
.###.
....#
....#
####.
@ T
#####
..#..
..#..
..#..
..#..
..#..
@ U
#...#
#...#
#...#
#...#
#...#
.###.
@ V
#...#
#...#
#...#
#...#
.#.#.
..#..
@ W
#...#
#...#
#...#
#.#.#
##.##
#...#
@ X
#...#
.#.#.
..#..
..#..
.#.#.
#...#
@ Y
#...#
.#.#.
..#..
..#..
..#..
..#..
@ Z
#####
....#
...#.
..#..
.#...
#####
@ a 2
.##.
...#
.###
#.##
@ b
#...
#...
###.
#..#
#..#
###.
@ c 2
.###
#...
#...
.###
@ d
...#
...#
.###
#..#
#..#
.###
@ e 2
.##.
#..#
####
.###
@ f
.##
#..
###
#..
#..
#..
@ g 2
.###
#..#
#..#
.###
...#
.##.
@ h
#...
#...
###.
#..#
#..#
#..#
@ i
#
.
#
#
#
#
@ j
..#
...
..#
..#
..#
..#
..#
.#.
@ k
#...
#...
#..#
#.#.
###.
#..#
@ l
##.
.#.
.#.
.#.
.#.
.##
@ m 2
##.#.
#.#.#
#.#.#
#.#.#
@ n 2
###.
#..#
#..#
#..#
@ o 2
.##.
#..#
#..#
.##.
@ p 2
###.
#..#
#..#
###.
#...
#...
@ q 2
.###
#..#
#..#
.###
...#
...#
@ r 2
#.##
##..
#...
#...
@ s 2
.###
#...
...#
###.
@ t
...
.#.
###
.#.
.#.
.##
@ u 2
#..#
#..#
#..#
.###
@ v 2
#...#
#...#
.#.#.
..#..
@ w 2
#...#
#.#.#
#.#.#
.#.#.
@ x 2
#..#
.##.
.##.
#..#
@ y 2
#..#
#..#
#..#
.###
...#
.##.
@ z 2
####
..#.
.#..
####
@ 0
.###.
#..##
#.#.#
##..#
#...#
.###.
@ 1
..#.
.##.
..#.
..#.
..#.
.###
@ 2
.###.
#...#
...#.
..#..
.#...
#####
@ 3
####.
....#
.###.
....#
....#
####.
@ 4
...#.
..##.
.#.#.
#..#.
#####
...#.
@ 5
#####
#....
####.
....#
#...#
.###.
@ 6
.###.
#....
####.
#...#
#...#
.###.
@ 7
#####
....#
...#.
..#..
..#..
..#..
@ 8
.###.
#...#
.###.
#...#
#...#
.###.
@ 9
.###.
#...#
#...#
.####
....#
.###.
@ !
#
#
#
#
.
#
@ "
#.#
#.#
@ #
.#.#.
#####
.#.#.
#####
.#.#.
@ $
..#..
.####
#.#..
.###.
..#.#
####.
..#..
@ %
##..#
##.#.
..#..
.#.##
#..##
@ &
.##..
#..#.
.##..
#.#.#
#..#.
.##.#
@ '
#
#
@ (
.#
#.
#.
#.
#.
.#
@ )
#.
.#
.#
.#
.#
#.
@ *
#.#.#
.###.
#.#.#
@ + 2
.#.
###
.#.
@ , 5
.#
#.
@ - 3
###
@ . 5
#
@ /
..#
..#
.#.
.#.
#..
#..
@ : 2
#
.
#
@ ; 2
.#
..
.#
#.
@ < 1
..#
.#.
#..
.#.
..#
@ = 2
###
...
###
@ > 1
#..
.#.
..#
.#.
#..
@ ?
.###.
#...#
...#.
..#..
.....
..#..
@ @
.###.
#...#
#.###
#.#.#
#.###
.###.
@ [
##
#.
#.
#.
#.
##
@ \
#..
#..
.#.
.#.
..#
..#
@ ]
##
.#
.#
.#
.#
##
@ ^
.#.
#.#
@ _ 6
#####
@ `
#.
.#
@ {
.##
.#.
#..
.#.
.#.
.##
@ |
#
#
#
#
#
#
@ }
##.
.#.
..#
.#.
.#.
##.
@ ~ 2
.##.#
#.##.
"""


def parse():
    glyphs = {}
    cur = None
    for line in ART.strip("\n").split("\n"):
        if line.startswith("@ "):
            parts = line[2:].split(" ")
            ch = parts[0] if parts[0] else " "
            top = int(parts[1]) if len(parts) > 1 and parts[1] else 0
            cur = (ch, top, [])
            glyphs[ch] = cur
        else:
            cur[2].append(line)
    return glyphs


def font_data():
    """Returns (height, rows, adv) in the same layout as font_data() in convert_assets.py."""
    g = parse()
    rows, adv = [], []
    for c in range(32, 127):
        ch = chr(c)
        if ch == " ":
            rows.append([0] * HEIGHT)
            adv.append(SPACE_ADV)
            continue
        _, top, art = g[ch]
        assert top + len(art) <= HEIGHT, ch
        w = max(len(a) for a in art)
        r = [0] * HEIGHT
        for i, line in enumerate(art):
            v = 0
            for x, p in enumerate(line):
                if p == "#":
                    v |= 1 << x
            r[top + i] = v
        rows.append(r)
        adv.append(w + 1)
    return HEIGHT, rows, adv


if __name__ == "__main__":  # preview sheet: python3 tools/smallfont.py out.png
    import sys
    from PIL import Image
    h, rows, adv = font_data()
    text = ["ABCDEFGHIJKLMNOPQRSTUVWXYZ", "abcdefghijklmnopqrstuvwxyz", "0123456789 !\"#$%&'()*+,-./",
            ":;<=>?@[\\]^_`{|}~", "Jeremy ate a Chicken Sandwich. He loves it!", "Hungry, bored & lonely: 12 coins (x2)"]
    S = 4
    im = Image.new("RGB", (330 * S, (h + 2) * len(text) * S), (40, 40, 40))
    for li, t in enumerate(text):
        x = 2
        for ch in t:
            gi = ord(ch) - 32
            for ry in range(h):
                for b in range(16):
                    if rows[gi][ry] >> b & 1:
                        for dy in range(S):
                            for dx in range(S):
                                im.putpixel(((x + b) * S + dx, (li * (h + 2) + ry + 1) * S + dy), (255, 255, 255))
            x += adv[gi]
    im.save(sys.argv[1])
