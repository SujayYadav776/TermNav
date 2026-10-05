#!/usr/bin/env python3
"""Verify image detail, Sixel raster output, capability negotiation and cleanup."""
import os
from pathlib import Path
import re
import subprocess
import tempfile

namespace={"__file__":str(Path(__file__).with_name("test_tui.py"))}
exec(Path(namespace["__file__"]).read_text().split('with tempfile.TemporaryDirectory(prefix="termnav-tui-")')[0],namespace)
Terminal=namespace["Terminal"]
try:
    from PIL import Image
except ImportError:
    print("SKIP: image quality checks require optional Pillow")
    raise SystemExit(0)

def decode_sixel(payload):
    """Independent decoder for the raster commands emitted by our helper."""
    match=re.search(rb'"1;1;(\d+);(\d+)',payload)
    assert match and payload.startswith(b"\x1bP0;1q") and payload.endswith(b"\x1b\\")
    width,height=map(int,match.groups())
    image=Image.new("RGB",(width,height)); palette={}; color=x=y=0
    stream=payload[match.end():-2]; index=0
    while index<len(stream):
        value=stream[index]
        if value==35:
            match=re.match(rb"#(\d+)(?:;2;(\d+);(\d+);(\d+))?",stream[index:]); assert match
            color=int(match[1]); index+=match.end()
            if match[2] is not None: palette[color]=tuple(round(int(c)*255/100) for c in match.groups()[1:])
            continue
        count=1
        if value==33:
            match=re.match(rb"!(\d+)",stream[index:]); assert match
            count=int(match[1]); index+=match.end(); value=stream[index]
        if 63<=value<=126:
            assert color in palette
            for column in range(x,x+count):
                assert column<width
                for bit in range(6):
                    if value-63 & (1<<bit):
                        assert y+bit<height
                        image.putpixel((column,y+bit),palette[color])
            x+=count
        elif value==36: x=0
        elif value==45: x=0; y+=6
        else: raise AssertionError(f"unexpected sixel byte {value}")
        index+=1
    return image

def raster_from(output):
    return re.findall(rb'\x1bP0;1q.*?\x1b\\',output,re.S)

with tempfile.TemporaryDirectory(prefix="termnav-images-") as tmp:
    root=Path(tmp)
    # Fine alternating edges that the old 96x64 thumbnail destroyed.
    image=Image.new("RGB",(640,360))
    pixels=image.load()
    for y in range(360):
        for x in range(640): pixels[x,y]=(245,220,30) if (x//2+y//2)%2 else (15,40,180)
    image.save(root / "detail.png")
    (root / "other.txt").write_text("TEXT AFTER IMAGE\n")
    helper=str(Path(__file__).resolve().parent.parent / "scripts" / "preview_helper.py")
    result=subprocess.run(["python3",helper,"image",str(root/"detail.png"),"1280","960","sixel"],capture_output=True,check=True)
    header,payload=result.stdout.split(b"\n",1)
    assert header==b"TNINDEX 640 360 640 360"
    assert len(payload)==768+640*360
    decoded=Image.frombytes("P",(640,360),payload[768:]); decoded.putpalette(payload[:768]); decoded=decoded.convert("RGB")
    assert decoded.size==(640,360)
    error=sum(abs(a-b) for source,target in zip(image.getdata(),decoded.getdata()) for a,b in zip(source,target))/(640*360*3)
    assert error<2, f"Native raster lost fine image detail: {error}"
    result=subprocess.run(["python3",helper,"image",str(root/"detail.png"),"300","200","blocks"],capture_output=True,check=True)
    header,payload=result.stdout.split(b"\n",1)
    dimensions=list(map(int,header.split()[1:])); assert dimensions[:2]==[300,169]
    assert len(payload)==300*169*3, "image output is still capped at 96x64"

    t=Terminal(tmp,rows=40,cols=180,environment={"TERMNAV_IMAGE":"auto"},capability_reply=b"\x1b[?62;4;22c\x1b[6;16;8t")
    try:
        assert t.capability_answered
        t.send("/detail\n\t"); t.read(2)
        rasters=raster_from(t.output); assert rasters and b"SIXEL" in t.output, t.output[-1200:]
        assert decode_sixel(rasters[-1]).width>96
        native=decode_sixel(rasters[-1]); assert native.size==image.size
        error=sum(abs(a-b) for source,target in zip(image.getdata(),native.getdata()) for a,b in zip(source,target))/(640*360*3)
        assert error<2, "Encoded raster lost fine source edges"
        count=len(rasters); assert not raster_from(t.read(0.5)), "unchanged images retransmit every UI tick"
        opened=t.send(":"); assert b"\x1b[2J" in opened, "native raster was not cleared before overlay"
        assert not raster_from(opened)
        t.send("\x1b"); t.read(0.3); assert len(raster_from(t.output))>count
        t.resize(24,90); t.read(1)
        smaller=decode_sixel(raster_from(t.output)[-1]); assert smaller.width<=84*8 and smaller.height<=10*16
        cleared=t.send("\x1b\x1b/other\n\t"); t.read(0.3)
        assert b"\x1b[2J" in cleared and b"TEXT AFTER IMAGE" in t.output
        assert not raster_from(cleared), "native image survived changing selection"
        t.send("\x1b")
    finally:
        t.close("\x1bq")

    t=Terminal(tmp,environment={"TERMNAV_IMAGE":"auto"},capability_reply=b"\x1b[?62;22c\x1b[6;16;8t")
    try:
        t.send("/detail\n\t"); t.read(0.6)
        assert not raster_from(t.output)
        assert "▀".encode() in t.output and b"half-block fallback" in t.output
        t.send("\x1b")
    finally:
        t.close("\x1bq")
print("PASS: native image detail, adaptive Sixel colors, higher-resolution fallback, capability detection, resize, overlay/selection cleanup and idle caching")
