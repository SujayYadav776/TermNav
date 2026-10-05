# Release verification — 2026-10-04

Image preview update verified on **2026-10-05**:

- Native Sixel output is negotiated using emulated terminal capability replies in
  PTY tests. An independent decoder compared a 640x360 fine-edge raster with its
  original: mean channel error below 2/255, without 96x64 downsampling.
- Pane-sized raster output, unsupported-terminal half-block fallback, resizing,
  menu/selection cleanup and idle caching passed. All filesystem and terminal
  regression suites passed after the change.
- Warnings-as-errors build passed. The Sixel encoder passed Valgrind with zero
  errors and all allocations freed. Large 1280x960 indexed image preparation
  completed within the helper's existing limits.
- The updated PowerShell launcher opened the rebuilt app and exited cleanly.
  Its optional `-ImageRenderer sixel` override is for Sixel-capable terminals.
- `terminal-preview-native.png` reconstructs the emitted image and terminal cells
  under emulated capabilities; it is protocol-level visual QA, not a screenshot
  of a graphical terminal. Actual rendering depends on the user's terminal.

Verified locally in the project-specific Alpine Linux 3.23 WSL2 distribution.
Final optimized binary: `termnav`, version 1.1.0, compiled with GCC 15.2 and
wide-character ncurses. Launches as the regular Linux user `termnav`.

| Check | Result |
| --- | --- |
| Optimized C11 build, `-Wall -Wextra -Wpedantic -Wformat=2 -Wshadow -Werror` | Passed |
| Filesystem, preview and background-worker suite | 4,954 assertions passed, including a 1,200-entry directory and indexed text previews |
| Real pseudo-terminal integration suite | Passed: navigation, Unicode, sanitization, full-screen text/directory previews, binary hex view, pan/scroll, read-only controls, filter preservation, editor, mkdir, rename, copy, batch, deletion, resize, refresh, signals, terminal restoration |
| PowerShell launcher, `.\termnav.ps1 -Demo` | Launched the compiled ncurses binary in a Windows PTY; `q` restored the terminal and exited with status 0 |
| Valgrind filesystem/worker suite | Zero errors; all heap blocks freed; only three inherited standard descriptors open at exit |
| New feature suites | Passed: persistent trash and restart undo, collision protection, symlink recovery, history branching, disk cancellation, palette, queue ordering/removal, pause/resume, image/PDF/archive previews and corrupt-image fallback |
| Valgrind feature suite | Zero errors and zero definite/indirect leaks; 216 bytes of runtime/environment allocations reachable at exit |
| Valgrind live UI | Image preview and all explorer panels exercised in a PTY; zero errors and zero definite/indirect/possible leaks; ncurses/runtime caches remain reachable |
| Staged installation | Installed binary, helper and manual under a disposable build prefix; installed image preview located its helper and passed |
| UndefinedBehaviorSanitizer | Full filesystem, feature and pseudo-terminal suites passed with fail-on-error instrumentation |
| AddressSanitizer on local Alpine toolchain | Unverified: GCC's libasan failed to link (`__sanitizer::struct_sock_fprog_sz` undefined) |
| Visual review | Wide, compact, help, JSON, binary hex, image, archive, disk usage, history and palette captures; actual terminal cells decoded with pyte plus xterm REP and scroll-sequence support, rendered with Pillow |
| GitHub Actions | GCC/Clang, Valgrind, ASan/UBSan jobs configured; not run remotely |

`terminal-wide.png`, `terminal-compact.png`, `terminal-help.png`,
`terminal-preview-json.png` and `terminal-preview-hex.png` are local
renders of the actual program's captured terminal cells. Their JSON/text sources
are included. PNGs and build/tool outputs are ignored by Git.

The build environment's Alpine rootfs was downloaded from the official Alpine
CDN and its SHA-256 matched the published checksum. No WSL distribution existed
before setup. The dedicated distribution is registered as `TermNav-Dev`, stores
its disk under `.tools/wsl`, and enables Linux permission metadata on Windows
mounts. `.tools` is ignored by Git and is not a source dependency.
