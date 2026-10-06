#!/usr/bin/env python3
"""Bounded read-only previews. No shell commands, extraction, or file execution."""
import os
import resource
import subprocess
import sys
import tarfile
import zipfile

resource.setrlimit(resource.RLIMIT_AS, (384 * 1024**2, 384 * 1024**2))
resource.setrlimit(resource.RLIMIT_CPU, (3, 3))
resource.setrlimit(resource.RLIMIT_FSIZE, (0, 0))

def main(mode, path, width="1280", height="960", renderer="blocks"):
    if mode == "image":
        try:
            from PIL import Image, ImageOps
        except ImportError:
            raise RuntimeError("Image preview needs Pillow: install python3-pillow (py3-pillow on Alpine)")
        Image.MAX_IMAGE_PIXELS = 20_000_000
        with Image.open(path) as image:
            image = ImageOps.exif_transpose(image)
            original = image.size
            bounds=(max(1,min(int(width),1280)),max(1,min(int(height),960)))
            image.thumbnail(bounds, Image.Resampling.LANCZOS)
            rgba = image.convert("RGBA")
            background = Image.new("RGBA", image.size, (28, 28, 28, 255))
            background.alpha_composite(rgba)
            rgb = background.convert("RGB")
            if renderer=="sixel":
                indexed=rgb.quantize(colors=256,method=Image.Quantize.MEDIANCUT)
                palette=bytes(indexed.getpalette())
                palette=palette[:768].ljust(768,b"\0")
                sys.stdout.buffer.write(f"TNINDEX {rgb.width} {rgb.height} {original[0]} {original[1]}\n".encode()+palette+indexed.tobytes())
            else:
                sys.stdout.buffer.write(f"TNIMG {rgb.width} {rgb.height} {original[0]} {original[1]}\n".encode() + rgb.tobytes())
    elif mode == "pdf":
        # The source remains open in this process; Poppler opens that same inode.
        source = f"/proc/{os.getpid()}/fd/3" if path == "/proc/self/fd/3" else path
        try:
            result = subprocess.run(["pdftotext", "-f", "1", "-l", "5", "-layout", "-enc", "UTF-8", source, "-"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=4)
        except FileNotFoundError:
            raise RuntimeError("PDF preview needs pdftotext: install poppler-utils")
        if result.returncode:
            raise RuntimeError("Cannot preview this PDF (damaged, encrypted, or unsupported)")
        # One lookahead byte lets the reader identify a truncated 64 KiB preview.
        sys.stdout.buffer.write(result.stdout[:65537] or b"No extractable text. This may be a scanned PDF.\n")
    elif mode == "archive":
        lines = ["ARCHIVE CONTENTS (listing only; nothing is extracted)", ""]
        if zipfile.is_zipfile(path):
            with zipfile.ZipFile(path) as archive:
                for index, item in enumerate(archive.infolist()):
                    if index == 512:
                        lines.append("... first 512 entries shown")
                        break
                    lines.append(f"{item.file_size:>12,}  {item.filename}")
        else:
            with tarfile.open(path, mode="r|*") as archive:
                for index, item in enumerate(archive):
                    if index == 512:
                        lines.append("... first 512 entries shown")
                        break
                    lines.append(f"{item.size:>12,}  {item.name}{'/' if item.isdir() else ''}{' -> ' + item.linkname if item.issym() or item.islnk() else ''}")
        sys.stdout.buffer.write(("\n".join(lines) + "\n").encode("utf-8", "replace")[:65537])
    else:
        raise RuntimeError("Unsupported preview")

if __name__ == "__main__":
    try:
        main(*sys.argv[1:])
    except Exception as error:
        print(f"Preview unavailable: {error}")
        sys.exit(1)
