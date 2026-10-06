#!/usr/bin/env python3
"""End-to-end keyboard checks for the six feature panels and rich previews."""
from pathlib import Path
import os
import subprocess
import tempfile
import zipfile

from terminal_support import Terminal, wait_for

with tempfile.TemporaryDirectory(prefix="termnav-new-features-") as tmp:
    root = Path(tmp)
    os.environ["XDG_DATA_HOME"] = str(root / "private-data")
    (root / "folder").mkdir()
    (root / "folder" / "nested.txt").write_text("NESTED HISTORY MARKER\n")
    (root / "recover.txt").write_text("recover me")
    t = Terminal(tmp)
    try:
        t.send("/recover\ndtrash\n")
        wait_for(lambda: not (root / "recover.txt").exists(), t)
        t.send("t")
        assert b"OPERATIONS DASHBOARD" in t.output and b"COMPLETE" in t.output
        t.send("\x1bT")
        assert b"TRASH / RECOVER" in t.output and b"recover.txt" in t.output
        t.send("\x1b")
    finally:
        t.close()
    t = Terminal(tmp)
    try:
        t.send("u")
        wait_for(lambda: (root / "recover.txt").exists(), t)
        assert (root / "recover.txt").read_text() == "recover me"
        t.send("\x1b/folder\nl")
        assert b"NESTED HISTORY MARKER" in t.output
        t.send("b")
        t.send("f")
        t.send("H")
        assert b"DIRECTORY HISTORY" in t.output
        t.send("\x1bbD")
        t.read(0.5)
        assert b"DISK USAGE EXPLORER" in t.output and b"allocated" in t.output
        for rows, cols in [(14,38),(20,76),(32,140)]:
            t.resize(rows,cols)
        t.send("\x1b:operations\n")
        assert b"COMMAND PALETTE" in t.output and b"OPERATIONS DASHBOARD" in t.output
        t.send("\x1b:zzzzzz")
        assert b"No matching actions" in t.output
        t.send("\x1b")
    finally:
        t.close()

    # Pause an actual directory copy, enqueue another, cancel that queued item,
    # then resume and verify a subsequent queued job starts automatically.
    tree=root / "queue-source"; tree.mkdir()
    for index in range(200):
        (tree / f"file-{index:04}").write_text(f"payload {index}")
    destinations=[root / name for name in ("destination-one","destination-cancel","destination-two")]
    for destination in destinations:
        destination.mkdir()
    t=Terminal(tmp)
    try:
        t.send(f"/queue-source\nc{destinations[0]}\nt ")
        assert b"PAUSED" in t.output, "active directory copy did not pause"
        t.send(f"\x1bc{destinations[1]}\nt")
        assert b"QUEUED" in t.output
        t.send("jx")
        assert not (destinations[1] / "queue-source").exists()
        t.send(f"\x1bc{destinations[2]}\ntk ")
        wait_for(lambda: (destinations[2] / "queue-source" / "file-0199").exists(),t,seconds=60)
        t.read(0.3)
        assert b"COMPLETE" in t.output
        assert (destinations[0] / "queue-source" / "file-0199").read_text()=="payload 199"
        assert not (destinations[1] / "queue-source").exists()
        t.send("\x1b")
    finally:
        t.close(keys="\x1bqq\n")

    with zipfile.ZipFile(root / "sample.zip", "w") as archive:
        archive.writestr("../../never-extracted.txt", "archive payload")
        archive.writestr("folder/readme.txt", "hello")
    t = Terminal(tmp)
    try:
        t.send("/sample.zip\n\t")
        t.read(0.8)
        assert b"ARCHIVE / CONTENTS" in t.output and b"never-extracted.txt" in t.output
        assert not (root.parent / "never-extracted.txt").exists()
        t.send("\x1b")
        (root / "broken.png").write_bytes(b"not an image")
        t.send("R\x1b/broken.png\n\t")
        wait_for(lambda: b"unavailable" in t.output,t)
        t.send("\x1b")
    finally:
        t.close(keys="\x1bq")

    try:
        from PIL import Image
    except ImportError:
        print("SKIP: image/PDF fixture generation needs optional Pillow")
    else:
        image = Image.new("RGB", (100,60),(220,80,30))
        image.save(root / "image.png")
        # A minimal text PDF without a third-party PDF generation dependency.
        content=b"BT /F1 16 Tf 50 120 Td (PDF RICH MARKER) Tj ET"
        objects=[b"<< /Type /Catalog /Pages 2 0 R >>",b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>",b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 200] /Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>",b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",b"<< /Length "+str(len(content)).encode()+b" >>\nstream\n"+content+b"\nendstream"]
        pdf=b"%PDF-1.4\n"; offsets=[0]
        for index,obj in enumerate(objects,1):
            offsets.append(len(pdf)); pdf+=str(index).encode()+b" 0 obj\n"+obj+b"\nendobj\n"
        start=len(pdf); pdf+=b"xref\n0 6\n0000000000 65535 f \n"+b"".join(f"{offset:010} 00000 n \n".encode() for offset in offsets[1:])+b"trailer\n<< /Size 6 /Root 1 0 R >>\nstartxref\n"+str(start).encode()+b"\n%%EOF\n"
        (root / "text.pdf").write_bytes(pdf)
        t=Terminal(tmp)
        try:
            t.send("/image.png\n\t"); t.read(0.8)
            assert b"100 x 60 pixels" in t.output and b"IMAGE" in t.output
            t.send("\x1b\x1b/text.pdf\n\t"); t.read(0.8)
            assert b"PDF / FIRST 5 PAGES" in t.output and b"PDF RICH MARKER" in t.output
            t.send("\x1b")
        finally:
            t.close()
print("PASS: trash/undo across restart, operations, history, disk explorer, palette, archive, malformed file, image and PDF previews")
