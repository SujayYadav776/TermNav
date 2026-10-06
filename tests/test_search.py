#!/usr/bin/env python3
"""Search real nested files through the terminal UI and open/reveal results."""
from pathlib import Path
import tempfile
from terminal_support import Terminal, wait_for
with tempfile.TemporaryDirectory(prefix="termnav-search-ui-") as tmp:
    root=Path(tmp)
    (root / "nested").mkdir()
    (root / "nested" / "Report.txt").write_text("FOUND IN A SUBFOLDER\n")
    (root / ".hidden").mkdir()
    (root / ".hidden" / "secret.txt").write_text("HIDDEN RESULT\n")
    t=Terminal(tmp,extra=("--home",))
    try:
        t.send("Sreport\n")
        wait_for(lambda: b"nested/Report.txt" in t.output,t)
        t.send("\n")
        wait_for(lambda: b"FOUND IN A SUBFOLDER" in t.output,t)
        t.send("\x1b")
        t.send("Sreport\n")
        result=t.send("o")
        assert b"FILES" in result and b"Report.txt" in t.output
        t.send("h:Search files recursively\nReport\n")
        wait_for(lambda: b"SEARCH RESULTS" in t.output,t)
        t.send("Snomatch\n")
        wait_for(lambda: b"0 matches" in t.output,t)
        t.send("\x1b.Ssecret\n")
        wait_for(lambda: b".hidden/secret.txt" in t.output,t)
        t.send("xR")
        t.send("\x1bOP")
        assert t.proc.poll() is None
    finally:
        t.close()
print("PASS: recursive search from Home and palette, result preview, reveal, new query, hidden files and Home shortcut")
