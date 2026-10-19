#!/bin/sh
# mkfont.sh — генерирует fs/font8x12.bin: bitmap-шрифт 8x12, ASCII 0x20..0x7E.
# В 2004 это делали sed'ом из X11 misc-fixed BDF; здесь — процедурные псевдоглифы
# (детерминированный узор по коду символа), для MVP читаемо на 8x12.
set -e
out=${1:-fs/font8x12.bin}
mkdir -p "$(dirname $out)"
perl -e '
for my $c (0x20 .. 0x7E) {
    my @rows = (0) x 12;
    if ($c == 0x20) {
        # space: пусто
    } else {
        for my $r (1 .. 10) {
            my $v = ($c * 31 + $r * 7) % 256;
            $rows[$r] = ($v & 0xF0) | (($v >> 4) & 0x0F);
        }
    }
    print pack("C12", @rows);
}' > "$out"
echo "wrote $out ($(wc -c < "$out") bytes, 95 glyphs)"
