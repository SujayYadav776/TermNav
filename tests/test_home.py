#!/usr/bin/env python3
"""Exercise dashboard startup, real capacity, navigation and confirmation prompts."""
from pathlib import Path
import tempfile
from terminal_support import Terminal, wait_for

t = Terminal(None, rows=34, cols=144)
try:
    wait_for(lambda: b"THIS PC / Drives" in t.output, t)
    assert b"QUICK ACCESS" in t.output and b"THIS PC / Drives" in t.output
    assert b"free of" in t.output and b"% used" in t.output
    for drive in ("/mnt/c", "/mnt/d", "/mnt/e"):
        if Path(drive).is_mount():
            assert drive.encode() in t.output, f"Mounted drive {drive} was hidden"
    t.send("\t\t\n")  # Open root drive.
    assert b"FILES" in t.output
    t.send("\x1bOP")  # xterm F1 from the browser.
    assert b"Your workspace" in t.output
    for keys in ("?", "H", "t"):
        t.send(keys)
        result = t.send("\x1bOP")
        if keys != "?":  # Help only covers part of Home; curses may keep its header.
            assert b"Your workspace" in result, f"F1 did not return from {keys}"
    t.resize(14, 38)
    assert b"HOME / Tab" in t.output
    t.resize(40, 144)
    t.send("w:Home dashboard\n")
    assert t.proc.poll() is None
finally:
    t.close()

with tempfile.TemporaryDirectory(prefix="termnav-home-") as tmp:
    root = Path(tmp)
    (root / "sample.txt").write_text("preview from home\n")
    t = Terminal(tmp, rows=40, cols=144, extra=("--home", "--ascii", "--no-color"))
    try:
        wait_for(lambda: b"free of" in t.output, t)
        assert b"free of" in t.output and b"#" in t.output
        t.send("aCreated\n")
        wait_for(lambda: (root / "Created").is_dir(), t)
        t.send("/sample\n\n")
        assert b"preview from home" in t.output
        result = t.send("\x1bOP")  # Home directly from full-screen preview.
        assert b"Your workspace" in result
        t.send("dtrash\n")
        wait_for(lambda: not (root / "sample.txt").exists(), t)
        t.send("u")
        wait_for(lambda: (root / "sample.txt").exists(), t)
    finally:
        t.close()
print("PASS: home startup, drive capacity, compact layout, palette, folders, previews and trash undo")
