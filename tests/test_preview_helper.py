#!/usr/bin/env python3
"""Verify rich text truncation across the helper and terminal reader."""
from pathlib import Path
import os
import subprocess
import tempfile
import zipfile
from terminal_support import Terminal, wait_for

helper = Path(__file__).resolve().parent.parent / "scripts" / "preview_helper.py"
with tempfile.TemporaryDirectory(prefix="termnav-preview-limit-") as tmp:
    root = Path(tmp)
    path = root / "large.zip"
    with zipfile.ZipFile(path, "w") as archive:
        for index in range(150):
            archive.writestr(f"{index:03d}-" + "a" * 500 + ".txt", "")
    result = subprocess.run(["python3", str(helper), "archive", str(path)], capture_output=True, check=True)
    assert len(result.stdout) == 65537, "helper must preserve one byte of truncation evidence"
    terminal = Terminal(tmp, environment={"TERMNAV_IMAGE": "blocks"})
    try:
        terminal.send("\t")
        wait_for(lambda: b"first 64 KiB" in terminal.output, terminal)
        assert b"ARCHIVE / CONTENTS" in terminal.output
    finally:
        terminal.close("\x1bq")
    # Exactly 64 KiB followed by EOF is complete, not truncated.
    fake = root / "fake-helper"; fake.mkdir()
    executable = fake / "python3"
    executable.write_text("#!/bin/sh\nhead -c 65536 /dev/zero | tr '\\000' a\n")
    executable.chmod(0o755)
    terminal = Terminal(tmp, environment={"TERMNAV_IMAGE": "blocks", "PATH": str(fake) + os.pathsep + os.environ["PATH"]})
    try:
        terminal.send("/large.zip\n\t")
        wait_for(lambda: b"ARCHIVE / CONTENTS" in terminal.output, terminal)
        assert b"first 64 KiB" not in terminal.output, "exact-size output incorrectly marked truncated"
    finally:
        terminal.close("\x1bq")
print("PASS: bounded archive output and visible truncation indicator")
