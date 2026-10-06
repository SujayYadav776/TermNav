#!/usr/bin/env python3
"""Capture the real dashboard, with optional pyte, for visual review."""
import json
from pathlib import Path
import sys
ROOT = Path(__file__).resolve().parent.parent
sys.path[:0] = [str(ROOT / ".tools" / "python"), str(ROOT / "tests")]
import pyte
from terminal_support import Terminal

class Screen(pyte.Screen):
    last = " "
    def draw(self, data):
        super().draw(data)
        if data:
            self.last = data[-1]
    def repeat_character(self, count=1):
        self.draw(self.last * (count or 1))

class Stream(pyte.Stream):
    csi = dict(pyte.Stream.csi, b="repeat_character")
    events = pyte.Stream.events | {"repeat_character"}

t = Terminal(str(ROOT / "test-playground"), rows=34, cols=144)
try:
    search_mode="--search" in sys.argv
    t.send("SREADME\n" if search_mode else "w\t")
    t.read(.4)
    screen = Screen(144, 34)
    Stream(screen).feed(t.output.decode("utf-8", errors="replace"))
    data = {"rows": 34, "cols": 144, "cells": [[screen.buffer[y][x]._asdict() for x in range(144)] for y in range(34)]}
    path = ROOT / "qa" / ("terminal-search.json" if search_mode else "terminal-home.json")
    path.write_text(json.dumps(data, ensure_ascii=False), encoding="utf-8")
    path.with_suffix(".txt").write_text("\n".join(row.rstrip() for row in screen.display)+"\n", encoding="utf-8")
finally:
    t.close("\x1bq" if search_mode else "q")
