# TermNav — engineering report

TermNav implements the supplied PRD as a Linux C application using ncursesw and
POSIX threads. The parent/current/preview arrangement keeps directory hierarchy,
selection, and content visible together. All operations affect the real OS
filesystem. There is no simulated filesystem or browser UI.

## Architecture

| Module | Responsibility |
| --- | --- |
| `main.c`, `app.h` | Application state, reload, filtering, navigation, event loop, config, signals |
| `fs_ops.c/.h` | Dynamic listing, metadata, sorting, path handling, copy, rename, mkdir, deletion |
| `ui.c` | Ncurses windows, color roles, layouts, Unicode-safe rendering, prompts, help |
| `input.c` | Keyboard actions, input modes, marks, copy clipboard, job dispatch |
| `preview.c/.h` | Bounded reads, binary detection, directory and symlink previews |
| `async_ops.c/.h` | One background job, atomic progress/cancellation, ownership and joining |
| `editor.c` | Shell-free editor spawn and process reaping |

Each listing owns dynamically allocated entries and names. The displayed-index
array is separate from the listing, so filtering does not erase metadata or
marks. Reload builds a replacement listing before swapping it into state: an
unreadable directory does not discard the last useful view. Selection persists
by name; marks persist by name and inode during refreshes in the same directory.

The UI uses independent curses windows with deferred `wnoutrefresh` / `doupdate`
updates. A UTF-8 decoder computes terminal cell widths before printing; invalid
or control characters become `?`. This prevents names or preview bytes from
injecting terminal escape commands. Compact layouts respond to ncurses resize
events; extremely small terminals show a recoverable resize message.

## Filesystem APIs and CO5 / CO6

`opendir` / `readdir` enumerate actual directory entries. `fstatat` with
`AT_SYMLINK_NOFOLLOW` retrieves metadata without replacing a symlink's identity
with its target. A second target stat enables navigation into directory links,
while previews identify the link explicitly. Size, mode, timestamps, and inode
come from `struct stat`; `d_type` is not treated as authoritative.

`mkdirat` creates directories. Linux `renameat2(RENAME_NOREPLACE)` renames entries
atomically without replacement; the syscall wrapper works on glibc and musl.
Recursive operations use directory descriptors and `openat`, `fstatat`,
`readlinkat`, `symlinkat`, `mkdirat`, `linkat`, and `unlinkat`. `O_NOFOLLOW` prevents
symlink traversal in recursive mutation paths. Deletion handles children before
`unlinkat(..., AT_REMOVEDIR)` removes their parent. Root and invalid names are
rejected; recursive depth is bounded. Errors propagate to the UI and jobs stop
at the first error rather than claiming unconditional success.

These choices demonstrate filesystem hierarchy, inodes, permission bits,
descriptor lifetime, buffered I/O, error propagation, and real Linux process
and thread APIs. The course mapping is CO5 for filesystem organization and I/O
reliability, and CO6 for OS-based programming with open-source Linux tools.

## I/O decisions

Previews read at most 64 KiB. They do not scan a whole large file on every cursor
movement, and the selected path is cached between moves. Null bytes or a high
control-character ratio produce a binary placeholder. UTF-8 high bytes remain
valid text. Special files are never read, avoiding blocked FIFO/device previews.

Text previews now index line starts once after loading. Scrolling uses this
index instead of rescanning all preceding bytes. A full-screen read-only viewer
shares the cached preview with the side pane, supports horizontal panning in
terminal cells, and inspects binary content as sixteen-byte hex rows. A small
per-line lexer colors keywords, strings, numbers and comments; it is not a full
parser. Preview controls cannot dispatch filesystem mutations. Browser marks,
filter and selection persist when the viewer closes, and idle refresh pauses
while reading. Explicit refresh retains the vertical offset, clamped to available
rows. All content remains bounded to 64 KiB.

Copies use a 128 KiB heap buffer with retry handling for interrupted reads and
partial writes. The destination is an exclusively created staging file. Data,
permissions, and timestamps are written and synced before a hard link publishes
the final name atomically without replacement. The staging name is then removed
and the parent directory synced. This avoids exposing partially written regular
files as a successful destination and never truncates an existing file.

`sendfile` could avoid copying payload through userspace and reduce CPU costs for
large regular files. The buffered implementation is easier to cancel after each
chunk and behaves consistently across more mounted filesystems. Disk scheduling
remains the kernel's responsibility: the app submits sequential reads/writes
rather than implementing a user-space disk scheduler.

Directory copies become visible incrementally and are not transactions. Partial
directory trees and completed entries remain after cancellation or errors. This
behavior is documented, and the UI reports partial-completion errors.

## Concurrency

Only the UI thread touches curses, listings, marks, and cursor state. A worker
owns a snapshot of selected absolute paths and its destination. C11 atomics
publish byte/item counters, cancellation, and completion. The UI reads the error
and failed-item fields only after completion and `pthread_join`. Job-owned memory
is freed by the collector after joining. An explicit 2 MiB worker stack avoids
small musl stack defaults; file buffers live on the heap.

The app permits navigation and queuing operations while a job runs but blocks
rename, mkdir, or editor until it completes. The queue holds up to 16 pending jobs;
the dashboard retains 32 session results. Atomic pause/resume and cancellation
are cooperative. Quitting with active or queued jobs requires a prompt.
Signal exit cancels and joins the worker before
restoring the terminal. Cancellation cannot interrupt an OS call blocked by a
slow or disconnected filesystem.

## Verification and limits

The filesystem suite creates disposable fixtures and checks real bytes and
metadata. The integration suite runs the compiled binary in a pseudo-terminal,
sends real keys, checks filesystem effects, resizes the terminal, and verifies
the original terminal settings are restored. A warnings-as-errors build is part
of the release checks. Ubuntu CI covers GCC and Clang and includes Valgrind and
ASan/UBSan targets; CI has to run on the hosting repository to confirm those jobs.

This release intentionally omits tabs, plugins, general undo,
ownership/ACL/xattr preservation, and a move clipboard. Unicode text is supported;
fuzzy case folding is ASCII. Enumeration and preview metadata remain synchronous
and can be slow on network mounts or very large directories. Concurrent external
mutations are not snapshot-isolated. Signal handlers restore through the normal
event loop; SIGKILL and power loss cannot be handled. For these reasons, the
included checks establish tested behavior, not a guarantee for every filesystem
or adversarial environment.

## Feature expansion in 1.1.0

Trash uses atomic same-filesystem no-replace rename into private sibling bins,
with fsynced records in the user's data directory written before moving payloads.
A registry lock serializes multiple instances. Restore verifies records, preserves
symlinks, and refuses existing destinations. Batch undo persists across restarts.
It is a private TermNav recovery store rather than a desktop trash implementation.

Directory history stores 64 session snapshots, including selection, scroll and
filter; branching truncates forward entries. The command palette dispatches
native actions through the same input handlers, without a shell.

Disk scans run in their own cancellable worker, publish items under a mutex, and
sort by allocated blocks on completion. Recursion never follows symlinks and is
depth bounded. Hard links count per entry; scans are not snapshot isolated.

Rich preview workers use argv-based process spawning, a process group for
cancellation, and an open regular-file descriptor to keep the selected inode.
The Python helper limits address space and CPU. Parent-side reads cap output at
64 KiB and enforce a five-second timeout. Pillow supplies bounded RGB thumbnails;
curses renders quantized color blocks. Poppler extracts the first five PDF pages;
Python lists ZIP and tar contents without extracting. Missing dependencies and
damaged files remain readable error states. Only the UI thread touches curses.

The image renderer now negotiates Sixel support with primary device attributes
and queries terminal cell size. Pillow produces an adaptive 256-color Sixel
raster with Lanczos downsampling, bounded to 1280x960 and 8 MiB output. Native
graphics are emitted after curses updates, cached while idle, and cleared before
overlay, selection or geometry changes. The portable fallback uses paired
foreground/background colors and Unicode half blocks, replacing the coarse
96x64 source and two-character full-cell pixels. Tests independently decode the
Sixel stream and compare a fine-edge fixture to the source, then exercise
capability negotiation and graphics cleanup through a pseudo-terminal.
