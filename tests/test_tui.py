#!/usr/bin/env python3
"""Exercise the real ncurses program through a Linux pseudo-terminal."""
import os
from pathlib import Path
import signal
import subprocess
import tempfile

from terminal_support import BIN, Terminal, wait_for

for arguments in (("/tmp", "/tmp"), ("/tmp", "--", "/tmp")):
    result = subprocess.run([BIN, *arguments], capture_output=True)
    assert result.returncode == 2 and b"Usage:" in result.stdout

with tempfile.TemporaryDirectory(prefix="termnav-tui-") as tmp:
    root = Path(tmp)
    (root / "folder").mkdir()
    (root / "folder" / "nested.txt").write_text("NESTED CONTENT\n")
    (root / "alpha.txt").write_text("PREVIEW ALPHA\n")
    (root / "binary.bin").write_bytes(b"\x00\x01\x02")
    (root / ".hidden").write_text("secret")
    (root / "escape-\x1b[31m.txt").write_text("safe")
    (root / "日本語.txt").write_text("こんにちは\n")
    t = Terminal(tmp)
    try:
        assert b"TERM / NAV" in t.output and b"PREVIEW" in t.output
        assert b"escape-?[31m.txt" in t.output, "control byte in filename was not sanitized"
        t.send("l")
        assert b"nested.txt" in t.output
        t.send("h")
        t.send("/alpha\n")
        assert b"PREVIEW ALPHA" in t.output
        t.send("e")
        assert t.proc.poll() is None
        t.send("r\x15renamed.txt\n")
        wait_for(lambda: (root / "renamed.txt").exists(), t)
        assert not (root / "alpha.txt").exists()
        t.send("aoutput\n")
        wait_for(lambda: (root / "output").is_dir(), t)
        t.send("\x1b")
        t.send("/renamed\ny")
        t.send("\x1b")
        t.send("/output\nlp")
        wait_for(lambda: (root / "output" / "renamed.txt").exists(), t)
        assert (root / "output" / "renamed.txt").read_text() == "PREVIEW ALPHA\n"
        t.send("dno\n")
        assert (root / "output" / "renamed.txt").exists()
        t.send("Xdelete\n")
        wait_for(lambda: not (root / "output" / "renamed.txt").exists(), t)
        t.send("h")
        t.send("/binary\n")
        assert b"Binary file" in t.output
        t.send("\x1b.")
        assert b".hidden" in t.output
        t.send("/日本\n")
        assert "こんにちは".encode() in t.output
        t.send("\x1b?")
        assert b"Find your way around" in t.output
        t.send("\x1b")
        for rows, cols in [(24, 90), (20, 55), (8, 20), (32, 140)]:
            t.resize(rows, cols)
            assert t.proc.poll() is None
        t.send("Ggg")
        t.send("sR")
        t.send("o/nonexistent-termnav\n")
        assert b"Cannot open directory" in t.output
        # Two marked files copied in a batch to a destination prompt.
        t.send("/txt\nv")
        t.send("coutput\n")
        wait_for(lambda: (root / "output" / "renamed.txt").exists() and (root / "output" / "日本語.txt").exists(), t)
    finally:
        t.close()
    for extra in [("--no-color",), ("--ascii",)]:
        t = Terminal(tmp, extra=extra)
        t.close()
    t = Terminal(tmp)
    os.kill(t.proc.pid, signal.SIGTERM)
    t.close(keys="")
    # External changes refresh without a keypress.
    t = Terminal(tmp)
    (root / "external.txt").write_text("external")
    t.read(2.5)
    assert b"external.txt" in t.output
    t.close()

    # Full-screen previews preserve the browser selection and cannot mutate files.
    (root / "long-preview.txt").write_text("a" * 180 + "RIGHT_MARKER\n" + "".join(f"ROW{i:03d}\n" for i in range(100)))
    (root / "raw-preview.bin").write_bytes(bytes(range(64)))
    (root / "code-preview.json").write_text('{\n  "enabled": true,\n  "name": "TermNav",\n  "count": 42\n}\n')
    t = Terminal(tmp)
    try:
        t.send("/long-preview\n")
        opened = t.send("\n")
        assert b"READ-ONLY PREVIEW" in opened
        assert b"RIGHT_MARKER" not in opened, "long lines should initially be clipped"
        panned = t.send("l" * 20)
        assert b"RIGHT_MARKER" in panned
        t.send("0G")
        assert b"ROW099" in t.output
        t.send("gg")
        # Directory polling must not reset the scroll while reading.
        t.send("G")
        unchanged = t.read(2.5)
        assert b"ROW000" not in unchanged, "viewer scroll was reset by idle refresh"
        t.send("R")
        t.send("Xdelete\naoops\n")
        assert (root / "long-preview.txt").exists() and not (root / "oops").exists()
        for rows, cols in [(20, 55), (14, 38), (8, 20), (32, 140)]:
            t.resize(rows, cols)
            assert t.proc.poll() is None
        t.send("\x1b")
        # Filter remains active after closing the viewer; y copies the same file.
        t.send("y")
        assert b"Copied 1 path" in t.output
        t.send("\x1b/raw-preview\n\t")
        assert b"BINARY / HEX" in t.output and b"00000000" in t.output and b"00 01 02" in t.output
        t.send("j")
        assert b"00000010" in t.output
        t.send("q")
        assert t.proc.poll() is None, "q in a preview should return to the browser"
        t.send("\x1b/code-preview\n\t")
        assert b"JSON" in t.output and b"TermNav" in t.output
        t.send("\t")
        t.send("\x1b/folder\n\t")
        assert b"DIRECTORY" in t.output and b"nested.txt" in t.output
        t.send("\x1b")
        t.send("\x1b/日本\n\t")
        assert "こんにちは".encode() in t.output
        t.send("\x1b")
    finally:
        t.close(keys="\x1bq")

result = subprocess.run([BIN, "--version"], capture_output=True, text=True)
assert result.returncode == 0 and "1.1.0" in result.stdout
result = subprocess.run([BIN], capture_output=True, text=True)
assert result.returncode == 2 and "interactive terminal" in result.stderr
result = subprocess.run([BIN, "--bad-option"], capture_output=True, text=True)
assert result.returncode == 2
print("PASS: PTY navigation, Unicode, previews, full-screen viewer, hex inspection, pan/scroll, read-only controls, editor, filtering, create, rename, copy, batch, delete, resize, external refresh, signals and terminal restoration")
