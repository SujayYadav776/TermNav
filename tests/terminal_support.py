#!/usr/bin/env python3
"""Shared pseudo-terminal driver for integration tests and visual captures."""
import errno
import fcntl
import os
from pathlib import Path
import pty
import select
import signal
import struct
import subprocess
import termios
import time

BIN = str(Path(__file__).resolve().parent.parent / "termnav")

class Terminal:
    def __init__(self, directory, rows=32, cols=140, extra=(), environment=None, capability_reply=None):
        self.master, self.slave = pty.openpty()
        self.resize(rows, cols, notify=False)
        self.before = termios.tcgetattr(self.slave)
        env = dict(os.environ, TERM="xterm-256color", LANG="C.UTF-8", EDITOR="true")
        env.pop("NO_COLOR", None)
        if environment:
            env.update(environment)
        self.capability_reply=capability_reply
        self.capability_answered=False
        self.proc = subprocess.Popen([BIN, *extra, *([] if directory is None else [directory])], stdin=self.slave, stdout=self.slave, stderr=self.slave, env=env, start_new_session=True)
        self.output = b""
        self.read(0.35)
    def resize(self, rows, cols, notify=True):
        fcntl.ioctl(self.slave, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))
        if notify:
            os.kill(self.proc.pid, signal.SIGWINCH)
            self.read(0.15)
    def read(self, duration=0.18):
        end = time.monotonic() + duration
        data = b""
        while time.monotonic() < end:
            if select.select([self.master], [], [], max(0, end - time.monotonic()))[0]:
                try:
                    chunk = os.read(self.master, 65536)
                    if not chunk:
                        break
                    data += chunk
                    if self.capability_reply and not self.capability_answered and b"\x1b[c" in data:
                        os.write(self.master,self.capability_reply)
                        self.capability_answered=True
                except OSError as exc:
                    if exc.errno == errno.EIO:
                        break
                    raise
        self.output += data
        return data
    def send(self, keys):
        os.write(self.master, keys.encode() if isinstance(keys, str) else keys)
        return self.read()
    def close(self, keys="q"):
        if self.proc.poll() is None:
            self.send(keys)
        try:
            self.proc.wait(timeout=5)
            self.read(0.05)
            assert self.proc.returncode == 0, self.output[-4000:]
            assert termios.tcgetattr(self.slave) == self.before, "terminal settings were not restored"
        finally:
            if self.proc.poll() is None:
                self.proc.kill()
                self.proc.wait(timeout=5)
            os.close(self.master)
            os.close(self.slave)

def wait_for(predicate, terminal, seconds=5):
    deadline = time.monotonic() + seconds
    while not predicate():
        assert terminal.proc.poll() is None, terminal.output[-4000:]
        assert time.monotonic() < deadline, "filesystem change did not complete"
        terminal.read(0.1)
