# TermNav

**A quiet place for your files.** A real terminal file manager in C, inspired by
Yazi's parent → current → preview flow and built on Linux system calls and
wide-character ncurses. Warm amber accents, blue directories, understated
metadata, generous spacing, and a keyboard-first workflow. No special font needed.

Launch `termnav` without a directory to open the Home dashboard: outlined
quick-access folders, navigation sidebar, file table and details pane, in the
same dark amber-and-blue theme as the browser. Press `w`
to switch between the dashboard and browser, or press `F1` to jump to Home
from the browser, preview, help or a feature panel. Both `make demo` and
`.\termnav.ps1 -Demo` start on Home with the demo files loaded.
On Home, `Tab` cycles folders,
files and drives; arrows choose an item and `Enter` opens it.
The file list also supports Right/`l` to open and Left/`h` or Backspace to go
to the parent directory. The PowerShell launcher connects Home shortcuts to
your Windows user folders, including folders stored in OneDrive.
Drive bars show
live used percentage and available space out of total capacity, including
Windows drives mounted in WSL. Compact two-line drive rows show all four drives
at standard terminal sizes. Amber means 85% full; red means 95% full.
Passing a directory starts directly in the browser.

For the complete command reference and explanations of the code behind each
feature, see [Commands, features, and code guide](docs/COMMANDS_FEATURES_AND_CODE.md).

Press **S** on Home or in the browser to search filenames in the current
directory and its subfolders. Enter a name and press Enter. Results appear as
the background search runs; Enter opens a result, `o` reveals its location,
`S` starts a new query, `x` stops the search and Esc closes it. The existing
`/` filter searches only the current folder. Search honors hidden-file settings,
does not follow directory symlinks or cross into other mounted filesystems,
and limits results to 1,000. To search another drive, open that drive first.

## Install with one command on Linux

On Linux, install the latest version from GitHub with:

```sh
curl -fsSL https://raw.githubusercontent.com/SujayYadav776/TermNav/main/scripts/install.sh | sh
```

The installer builds TermNav from source and installs it into `~/.local`.
When build tools are missing, it offers to install them through `apt`, `apk`,
`dnf`, `pacman`, or `zypper`, using `sudo` when needed. If `~/.local/bin` is not
already on your `PATH`, the installer prints the command to add it.

To choose another install location, pass `TERMNAV_PREFIX` to the shell running
the installer, for example:

```sh
curl -fsSL https://raw.githubusercontent.com/SujayYadav776/TermNav/main/scripts/install.sh | TERMNAV_PREFIX="$HOME/.local" sh
```

A system-wide prefix such as `/usr/local` may require administrator
permissions.

TermNav currently uses Linux-specific filesystem features, so this command
supports native Linux computers and Linux distributions in WSL. Native macOS
and native Windows terminals are not supported. Windows users can install
Ubuntu with `wsl --install -d Ubuntu`, then follow the WSL setup below.

Image preview also needs Python 3 and Pillow. PDF text preview needs
`pdftotext` from Poppler. The file manager itself works without these optional
preview tools.

To remove the default user installation:

```sh
curl -fsSL https://raw.githubusercontent.com/SujayYadav776/TermNav/main/scripts/uninstall.sh | sh
```

Use the same `TERMNAV_PREFIX` setting when removing a custom-prefix
installation. The program files are removed; user trash and configuration data
are kept.

## Run on this Windows computer

A dedicated **TermNav-Dev** Alpine Linux WSL distribution has been set up for this
project. The app is compiled in this folder. Open **Windows Terminal / PowerShell**:

```powershell
cd "C:\Users\ADMIN\OneDrive\Documents\ChatGPT\TermNav"
.\termnav.ps1 -Demo
```

Browse a Windows directory:

```powershell
.\termnav.ps1 -Path "C:\Users\ADMIN\Downloads"
```

If PowerShell's script policy blocks the launcher, run the app directly:

```powershell
wsl -d TermNav-Dev --cd "C:\Users\ADMIN\OneDrive\Documents\ChatGPT\TermNav" --exec ./termnav test-playground
```

Use `-Distribution Ubuntu` with the launcher if you prefer an existing Ubuntu
WSL distribution after installing the dependencies there. The binary built on
Alpine uses musl: rebuild with `make clean && make` when changing distributions.

## Build on Linux / Ubuntu / WSL

Requires Linux, a C11 compiler, make, pthreads, and ncurses with wide-character
support. Python 3 enables rich previews, the demo, and integration tests.
Images additionally use Pillow; PDF text uses Poppler. Archive listings use
Python's standard library. These helpers are optional and already installed in
this computer's TermNav-Dev distribution.

```sh
# Ubuntu / Debian
sudo apt update
sudo apt install build-essential pkg-config libncursesw5-dev python3
# Optional rich previews
sudo apt install python3-pillow poppler-utils

# Alpine
apk add build-base ncurses-dev ncurses-terminfo python3
apk add py3-pillow poppler-utils

make
./termnav                 # current directory
./termnav ~/Downloads     # any directory
make demo                 # create and browse the disposable demo
```

For a new Windows machine, install Ubuntu with `wsl --install -d Ubuntu`, open
Ubuntu, and follow the Linux instructions. WSL must be available; TermNav is a
Linux binary, not a native Windows executable.

Optional system installation:

```sh
sudo make install         # /usr/local/bin/termnav + manual page
make install PREFIX="$HOME/.local"
man termnav
```

## What it does

- Three panes: parent directory, current directory, live preview. At narrower
  widths it switches to two panes, then one. Resize at any time.
- Arrow and vim navigation, paging, directory jumps, hidden-file toggle, and
  name/size/modified sorting. Listings have no fixed 1024-entry limit.
- Text previews with line numbers and lightweight syntax highlighting; directory
  previews; explicit binary, symlink, special-file, and permission-error states.
- Full-screen read-only previews with vertical scrolling, horizontal panning,
  and hexadecimal inspection of binary files. Works in single-pane terminals too.
- Unicode filenames and text, cell-aware clipping, and sanitized control bytes.
- Inline directory creation and rename; atomic refusal to overwrite on rename.
- Case-insensitive fuzzy **subsequence** filtering as you type (`tfm` matches
  `Terminal_File_Manager`). Unicode characters match exactly; case folding is ASCII.
- Multi-file marking, a persistent copy clipboard, and recursive directory copy.
- Persistent trash with batch undo and a recovery browser; restoration never overwrites.
- Queued background operations with progress, speed, elapsed time, pause/resume,
  cancellation, and an operations dashboard with recent results.
- Directory back/forward history that restores selection and filters.
- Background disk usage explorer with allocated-size bars and drill-down navigation.
- Searchable command palette, color image thumbnails, PDF text, and archive listings.
- Refreshes external filesystem changes every two seconds while idle.
- Editor integration, in-app help, ASCII mode, `NO_COLOR`, and simple config.

## Keys

| Key | Action |
| --- | --- |
| `j` / `k`, `↓` / `↑` | Next / previous entry |
| `l`, `→`, `Enter` | Enter a directory, or open a file's full-screen preview |
| `h`, `←`, `Backspace` | Parent directory; reselect the folder you left |
| `gg`, `Home` / `G`, `End` | First / last entry |
| `Ctrl-d`, `PageDown` / `Ctrl-u`, `PageUp` | Page down / up |
| `J` / `K` | Scroll preview down / up |
| `Tab` | Expand the selected entry's preview (including directories) |
| `/` | Live fuzzy filter; `Enter` keeps it, `Esc` clears it |
| `.` | Toggle hidden files |
| `s` | Cycle name → size → modified sorting |
| `R`, `Ctrl-l` | Refresh |
| `Space` | Toggle a mark and move down |
| `v` | Mark / unmark every visible entry |
| `y` | Copy selected or marked paths to the clipboard |
| `p` | Copy clipboard items into the current directory |
| `c` | Copy selected or marked items to an existing directory |
| `a` | Create a directory |
| `r` | Rename the selected entry (starts with its current name) |
| `d` | Trash selected or marked items; type `trash` to confirm |
| `u` / `T` | Undo latest trash batch / browse and restore individual items |
| `X` | Permanently delete selected or marked items; type `delete` to confirm |
| `t` | Operations dashboard; `Space` pauses/resumes, `x` cancels selected job |
| `b` / `f` / `H` | Directory back / forward / history picker |
| `D` | Disk usage; `Enter` drills down, `h` goes up, `o` reveals in browser |
| `:` | Search commands; arrows choose, `Enter` runs, `Esc` closes |
| `o` | Go to an absolute, relative, or `~/` directory |
| `~` | Home directory |
| `e` | Edit file using `$EDITOR`, default `vi` |
| `?` | Help |
| `Esc` | Clear filter and marks, or cancel an active job |
| `q` | Quit; active or queued jobs require typing `q` and Enter |

All prompts: `Enter` accepts, `Esc` cancels, `Ctrl-u` clears, and `Backspace`
removes a Unicode character. Clipboard and batch operations use **all marks in
the current directory**, including marks hidden by a filter. Leaving a directory
clears its marks; clipboard paths remain available.

Copy destinations must already exist. A single copied file retains its name;
use rename after copying if you want a different name. `y`, navigate, `p` is the
quickest way to copy between folders. The editor setting is one executable name
or path, without shell arguments; filenames are passed as a separate argument.

## Full-screen preview

Select a file and press **Enter**, or press **Tab** on any entry to expand its
preview. The viewer is read-only: create, rename, copy, and delete keys are
inactive until you return to the browser. Your filter, selection, and marks
remain in place when you close it.

| Key inside the viewer | Action |
| --- | --- |
| `j` / `k`, `J` / `K`, `↓` / `↑` | Scroll down / up |
| `PageDown`, `Ctrl-d`, `Space` / `PageUp`, `Ctrl-u` | Page down / up |
| `gg`, `Home` / `G`, `End` | First / last preview row |
| `h` / `l`, `←` / `→` | Pan horizontally by four terminal cells |
| `0` | Return to the start of the line |
| `R`, `Ctrl-l` | Refresh the selected preview while keeping its scroll position |
| `Esc`, `Tab`, `q` | Close the viewer and return to the browser |

Code previews recognize C/C++, Python, JavaScript/TypeScript, Rust, Go, JSON,
shell, and Markdown. Highlighting is a lightweight per-line lexer, rather than
a full language parser; it does not track multi-line string/comment state.
Binary previews show sixteen bytes per row, with hexadecimal offsets and a
printable ASCII column. On narrow terminals, use horizontal panning to inspect
the rest of a row. Symlinks show their target without following it; devices,
sockets and FIFOs remain metadata-only.

Content reads remain bounded to the first **64 KiB**, with truncation visibly
labelled. Automatic directory refresh pauses while the full-screen viewer is
open so it cannot disturb your reading position; press `R` for a fresh snapshot.
Horizontal panning resets when you return to the side pane. The command palette
also works in the viewer; executing an action closes the viewer first.

Images (PNG, JPEG, WebP, GIF, BMP) use **native Sixel raster graphics** when the
terminal reports support, with an adaptive image palette and previews sized to
the pane's pixel dimensions. The image stays sharp up to a bounded 1280×960
preview; smaller originals retain their resolution. Other terminals use a finer
Unicode half-block fallback (two vertical pixels per text cell), or plain blocks
in ASCII mode. A 256-color terminal is required for fallback colors. Images keep
their aspect ratio and show the first frame; EXIF orientation and transparency
are handled. Resize or expand with Tab to regenerate at the appropriate size.

Windows Terminal supports Sixel in its 1.22 releases and newer; use Windows
Terminal for native image previews. See Microsoft's
[Sixel announcement](https://devblogs.microsoft.com/commandline/windows-terminal-preview-1-22-release/).
The app queries graphics support and cell
pixel size, rather than assuming support from `TERM`. If a terminal proxy blocks
the capability response, explicitly choose the renderer:

```powershell
.\termnav.ps1 -Demo -ImageRenderer sixel
```

```sh
TERMNAV_IMAGE=sixel ./termnav  # force only on a Sixel-capable terminal
TERMNAV_IMAGE=blocks ./termnav # disable graphics / use the portable fallback
```

Automatic graphics are disabled inside tmux; passthrough requires separate
terminal/multiplexer configuration. Native images clear before menus, resizing
or selection changes. The launcher now rebuilds changed source files on launch.
PDFs show extractable text from the first five pages; scanned pages need OCR
elsewhere. ZIP and tar archives list up to 512 entries without extracting anything.
Compressed tar files are supported; standalone compressed streams show an error.
Rich previews run in a cancellable child process with a five-second timeout,
384 MiB memory limit, three CPU seconds, and a 64 KiB text output cap (8 MiB for
bounded image data). Missing helpers
and malformed files show readable errors. Press `R` to regenerate the preview.

## File-operation behavior

`d` moves files to TermNav's private trash after you type `trash`. `u` restores
the latest remaining batch, even after restarting. `T` lets you restore individual
items with Enter. A conflicting original filename leaves both files intact;
rename the replacement, then retry. Undo applies to trash, rather than arbitrary
copy, rename, or permanent-delete operations. `X` is permanent and requires `delete`.

Trash payloads live in a private `.termnav-trash-<uid>` sibling directory beside
each original file, allowing an atomic rename on the same filesystem. Persistent
records live in `$XDG_DATA_HOME/termnav/trash` or `~/.local/share/termnav/trash`.
This is TermNav's own trash, separate from desktop recycle bins. Keep its records
and payload directories for recovery; if an original parent directory moves,
restore that parent first. Empty payload directories can remain. On filesystems
that cannot enforce private directory permissions, trash reports an error.

The operations dashboard retains 32 recent results per session and queues up to
16 jobs behind one active worker. Pause is cooperative, between filesystem steps;
cancel stops remaining work. Queued jobs can be removed with `x`. Exiting cancels
the active job and discards pending jobs. A batch stops at its first failure.

History retains up to 64 locations per session; visiting a new location after
going back replaces the forward branch. Disk usage scans include hidden files,
use allocated blocks, and never follow symlinks. Apparent size is also shown.
Hard links count once per directory entry, so totals may differ from `du`.

Copies and renames never silently overwrite existing destinations. File copies
write to an exclusive temporary file, sync it, then publish it with an atomic
no-replace hard link. Temporary files are removed on ordinary errors or
cancellation. Regular files preserve permission bits and timestamps; copied
directories preserve permission bits and timestamps after their contents finish.
Setuid/setgid bits, ownership, ACLs, xattrs and hard-link relationships are not
preserved. Symlinks are copied as links; recursive copy/delete never follow them.
Copying a directory into itself or a descendant is rejected, including through
a destination-directory symlink. FIFOs, sockets and devices are never read for
previews, and special files cannot be copied.

Cancellation stops remaining work and **keeps completed changes**. A partially
copied directory may remain; failed batch jobs stop at the first error. Recursive
operations are bounded to 128 levels. Filesystems without Linux no-replace rename
or hard-link support report an error rather than falling back to an unsafe
overwrite. Reads/copies are not filesystem snapshots; avoid modifying the same
tree from another process during an operation. A forced kill or power loss can
leave `.termnav-copy-*` staging files.

## Appearance and configuration

Use a UTF-8 terminal with 256 colors. Windows Terminal works well. Standard
monospace fonts are sufficient; no Nerd Font required. Three panes need 108
columns, two panes need 76, and the minimum usable terminal is 38 × 14.

```sh
./termnav --all --ascii
NO_COLOR=1 ./termnav
./termnav --no-color
```

Copy `config.example` to `~/.config/termnav/config`, or
`$XDG_CONFIG_HOME/termnav/config`:

```ini
show_hidden = false
ascii = false
sort = name
```

CLI options take priority. Preview reads are bounded to 64 KiB. Directory
listings and metadata reads run on the UI thread; very large or slow remote
directories can pause navigation. Plugins, tabs, and a cut/move clipboard are
outside this release. Rich previews and recursive disk scans run in the background.

## Verification

```sh
make check                # filesystem tests + real pseudo-terminal UI tests
make sanitize             # ASan / UBSan build and tests; toolchain support required
valgrind --leak-check=full --error-exitcode=1 ./build/test_fs
make clean && make        # restore normal optimized build after sanitizer tests
```

Tests create their own disposable trees under `/tmp`; they do not operate on
personal files. Coverage includes >1024 entries, metadata, byte-correct copies,
no-clobber behavior, symlink safety, descendant rejection, cancellation, binary
and FIFO previews, background jobs, navigation, Unicode, filtering, create,
rename, copy/paste, batch copy, delete confirmation, editor restoration,
full-screen reading, binary hex inspection, horizontal panning, read-only controls,
terminal resizing, external refresh, signals and terminal restoration. Additional
tests cover persisted trash, restore conflicts, history branching, disk scans,
the command palette, image/PDF previews, archive listing, and corrupt images. CI is
configured for GCC and Clang on Ubuntu with memory and sanitizer checks.

See [the engineering report](docs/REPORT.md) for architecture, syscall choices,
I/O tradeoffs, and the CO5/CO6 mapping; [the demo guide](docs/DEMO.md) for a short
walkthrough. Licensed under MIT.
