#!/usr/bin/env python3
"""Protocol visual QA with emulated capabilities, rather than a terminal screenshot."""
import json
from pathlib import Path
import re
import sys

ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/".tools"/"python"))
import pyte

sys.path.insert(0,str(ROOT/"tests"))
from terminal_support import Terminal
from image_support import decode_sixel
t=Terminal(str(ROOT/"test-playground"),rows=34,cols=144,environment={"TERMNAV_IMAGE":"auto"},capability_reply=b"\x1b[?62;4;22c\x1b[6;25;10t")
try:
    t.send("/landscape\n\t"); t.read(1)
    images=list(re.finditer(rb'\x1b7\x1b\[(\d+);(\d+)H(\x1bP0;1q.*?\x1b\\)\x1b8',t.output,re.S))
    assert images, "No native image emitted"
    image=images[-1]
    raster=decode_sixel(image[3])
    raster.save(ROOT/"qa"/"native-image-raster.png")
    clean=re.sub(rb'\x1bP0;1q.*?\x1b\\',b"",t.output,flags=re.S)
    screen=pyte.Screen(144,34); stream=pyte.Stream(screen); stream.feed(clean.decode("utf-8","replace"))
    cells=[[screen.buffer[y][x]._asdict() for x in range(144)] for y in range(34)]
    data={"rows":34,"cols":144,"cells":cells,"native_image":{"path":"native-image-raster.png","row":int(image[1])-1,"column":int(image[2])-1}}
    (ROOT/"qa"/"terminal-preview-native.json").write_text(json.dumps(data,ensure_ascii=False),encoding="utf-8")
    (ROOT/"qa"/"terminal-preview-native.txt").write_text("\n".join(screen.display)+"\n",encoding="utf-8")
finally:
    t.close("\x1bq")
print("Captured native Sixel raster and terminal frame (emulated terminal capabilities)")
