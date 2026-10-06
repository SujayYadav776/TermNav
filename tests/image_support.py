"""Independent Sixel decoding helpers for tests and captures."""
import re
from PIL import Image

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
