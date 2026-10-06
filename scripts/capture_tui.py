#!/usr/bin/env python3
"""Capture actual terminal cells for visual QA. Optional dependency: pyte==0.8.2."""
import json
from pathlib import Path
import runpy
import sys

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / ".tools" / "python"))
sys.path.insert(0, str(ROOT / "tests"))
import pyte
# Handle xterm repeat and scroll sequences absent from pyte 0.8.2.
class CaptureScreen(pyte.Screen):
    last_character = " "
    def draw(self, data):
        super().draw(data)
        if data:
            self.last_character = data[-1]
    def repeat_character(self, count=1):
        self.draw(self.last_character * (count or 1))
    def scroll_up(self, count=1):
        x,y=self.cursor.x,self.cursor.y
        self.cursor.y=self.margins.bottom if self.margins else self.lines-1
        for _ in range(count or 1):
            self.index()
        self.cursor.x,self.cursor.y=x,y
    def scroll_down(self, count=1):
        x,y=self.cursor.x,self.cursor.y
        self.cursor.y=self.margins.top if self.margins else 0
        for _ in range(count or 1):
            self.reverse_index()
        self.cursor.x,self.cursor.y=x,y
class CaptureStream(pyte.Stream):
    csi = dict(pyte.Stream.csi, b="repeat_character",S="scroll_up",T="scroll_down")
    events = pyte.Stream.events | {"repeat_character","scroll_up","scroll_down"}
from terminal_support import Terminal
runpy.run_path(str(ROOT / "scripts" / "create_demo.py"))
qa = ROOT / "qa"
qa.mkdir(exist_ok=True)

def capture(t, name, rows, cols):
    screen = CaptureScreen(cols, rows)
    stream = CaptureStream(screen)
    stream.feed(t.output.decode("utf-8", errors="replace"))
    cells = [[screen.buffer[y][x]._asdict() for x in range(cols)] for y in range(rows)]
    (qa / (name + ".json")).write_text(json.dumps({"rows": rows, "cols": cols, "cells": cells}, ensure_ascii=False), encoding="utf-8")
    (qa / (name + ".txt")).write_text("\n".join(screen.display) + "\n", encoding="utf-8")

t = Terminal(str(ROOT / "test-playground"), rows=34, cols=144)
try:
    t.send("jjjj")
    capture(t, "terminal-wide", 34, 144)
    t.send("?")
    capture(t, "terminal-help", 34, 144)
    t.send("\x1b")
    t.resize(26, 90)
    capture(t, "terminal-compact", 26, 90)
    t.resize(34, 144)
    t.send("/palette\n\t")
    capture(t, "terminal-preview-json", 34, 144)
    t.send("\x1b")
    t.send("\x1b/sample\n\t")
    capture(t, "terminal-preview-hex", 34, 144)
    t.send("\x1b")
    t.send("\x1b/landscape\n\t")
    t.read(0.7)
    capture(t, "terminal-preview-image", 34, 144)
    t.send("\x1b\x1b/project.zip\n\t")
    t.read(0.7)
    capture(t, "terminal-preview-archive", 34, 144)
    t.send("\x1b")
    t.send("\x1b:disk usage\n")
    t.read(0.6)
    capture(t, "terminal-disk-usage", 34, 144)
    t.send("\x1b:history\n")
    capture(t, "terminal-history", 34, 144)
    t.send("\x1b:preview")
    capture(t, "terminal-palette", 34, 144)
    t.send("\x1b")
finally:
    t.close()
print("Captured live terminal screens in qa/")
