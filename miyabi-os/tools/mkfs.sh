#!/bin/sh
# mkfs.sh TARGET IMG — собирает IDE-образ kifu: суперблок (блок 0) + файлы.
# Формат: блок0 = строки "имя blk size\n"; дальше данные файлов по 512B блокам.
set -e
target=${1:-ip32}; img=${2:-build/miyabi-$target.img}
mkdir -p build fs
python3 - "$img" <<'PY'
import sys
img=sys.argv[1]
files=[]
# служебные "файлы" образа: конфиги GUI и welcome-рисунок текстом
files.append(("welcome.txt", b"miyabi 0.1 - yoroshiku onegaishimasu\n"))
files.append(("palette",    b"sumi 1a1a1a washi f5f0e6 aka b03428 matsu 2e5d4b\n"))
blk=1
idx=[]
data=[]
for name,content in files:
    idx.append(f"{name} {blk} {len(content)}")
    b=content+b"\0"*(512-len(content)%512) if len(content)%512 else content
    data.append(b); blk+=len(b)//512
super=("MIYABI01\n"+"\n".join(idx)+"\n").encode()
super+=b"\0"*(512-len(super)%512)
with open(img,"wb") as f:
    f.write(super[:512])
    for d in data: f.write(d)
print(f"wrote {img}: {blk} blocks")
PY
