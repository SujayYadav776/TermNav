#!/usr/bin/env python3
"""Render captured terminal cells to PNG for review (optional Pillow)."""
import json
from pathlib import Path
import sys
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parent.parent
font_path = sys.argv[1] if len(sys.argv) > 1 else "C:/Windows/Fonts/consola.ttf"
font = ImageFont.truetype(font_path, 18)
bold_path = str(Path(font_path).with_name("consolab.ttf"))
bold = ImageFont.truetype(bold_path, 18) if Path(bold_path).exists() else font
cell_w = round(font.getlength("M"))
cell_h = 25
colors = {"default": "#d0d0d0", "black": "#000000", "red": "#cd0000", "green": "#00cd00", "brown": "#cdcd00", "blue": "#0000ee", "magenta": "#cd00cd", "cyan": "#00cdcd", "white": "#e5e5e5"}
def color(value, background=False):
    if value == "default":
        return "#1c1c1c" if background else "#d0d0d0"
    return colors.get(value, "#" + value)

for path in (root / "qa").glob("terminal-*.json"):
    data = json.loads(path.read_text(encoding="utf-8"))
    pad, chrome = 24, 48
    img = Image.new("RGB", (data["cols"] * cell_w + pad * 2, data["rows"] * cell_h + pad * 2 + chrome), "#1c1c1c")
    draw = ImageDraw.Draw(img)
    draw.rectangle((0, 0, img.width, chrome), fill="#242424")
    draw.text((pad, 12), "TermNav  /  Linux terminal", font=font, fill="#d7af87")
    for y, row in enumerate(data["cells"]):
        for x, cell in enumerate(row):
            fg, bg = color(cell["fg"]), color(cell["bg"], True)
            if cell["reverse"]:
                fg, bg = bg, fg
            px, py = pad + x * cell_w, pad + chrome + y * cell_h
            draw.rectangle((px, py, px + cell_w - 1, py + cell_h - 1), fill=bg)
            if cell["data"]:
                draw.text((px, py), cell["data"], font=bold if cell["bold"] else font, fill=fg)
    if "native_image" in data:
        native=data["native_image"]
        raster=Image.open(path.parent/native["path"]).convert("RGB")
        img.paste(raster,(pad+native["column"]*cell_w,pad+chrome+native["row"]*cell_h))
    img.save(path.with_suffix(".png"))
    print(path.with_suffix(".png"))
