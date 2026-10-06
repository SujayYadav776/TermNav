# A three-minute walkthrough

Run `make demo` on Linux or `.\termnav.ps1 -Demo` from PowerShell. Both start
on the Home dashboard; press `w` to open the browser or `F1` to return to Home.
The fixture
generator creates missing files only and never overwrites existing work.

1. Start wide (140 columns if available). Show parent, files, and directory
   preview. Explain that every entry is on the real filesystem.
2. Use `j/k`, `l`, and `h` to enter and leave `projects/aurora`. Highlight `main.c`
   to show text preview with line numbers. `J/K` scrolls longer text. Press Enter
   to expand the preview; use j/k to scroll and h/l to pan, then Esc to return.
3. Return to the demo root; filter `/welcome` and press Enter. Show the welcome
   text, then Esc to clear the filter. Filter `/bin` to show the binary placeholder.
   Press Tab to inspect hexadecimal bytes, then Esc to close the viewer.
4. Press `.` to reveal the hidden note. Press `s` to cycle sorting.
5. Filter `/notes`, enter it, and highlight `日本語.txt` to show Unicode support.
6. Return to root. Press `a`, type `demo-output`, Enter. Press `r`, Ctrl-u, type
   `delivery`, Enter. Explain the no-overwrite behavior.
7. Filter `/welcome`, Enter, `y`. Esc, filter `/delivery`, Enter, `l`, then `p`.
   The copied file appears while the worker job completes. Its bytes match the
   original. Show a second terminal's `ls -l` if useful.
8. On the copy, press `d`, type `trash`, Enter. Press `T` to see its recovery
   record, Esc to return, then `u` to restore it. `X` with `delete` is permanent.
9. Open `t` for operation results. Use `b/f/H` for directory history. Press `D`
   to explore disk use; Enter drills down, h goes up, Esc closes. Press `:` and
   type `preview` to see matching commands. Select `landscape.png` and `project.zip`
   for rich previews when the optional Python helpers are installed.
10. Shrink the terminal through two-pane and single-pane modes, then enlarge it.
   Press `?` to show the keyboard guide, Esc to dismiss, `q` to exit cleanly.

For background progress, put a disposable large file in the demo and copy it
into `inbox`. Navigate during the copy, then Esc to cancel. Completed files remain;
a partially copied directory can remain. Use `t`, Space to pause/resume, and x
to cancel an active or queued job. Stay in the disposable demo for this walkthrough.
