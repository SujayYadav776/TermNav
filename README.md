# TermNav

**A quiet place for your files.** A keyboard-first terminal file manager in C,
built on Linux system calls and wide-character ncurses, with a
parent → current → preview flow, live background operations, and a recoverable
trash. Warm amber accents, blue directories, no special font required.

```sh
curl -fsSL https://raw.githubusercontent.com/SujayYadav776/TermNav/main/scripts/install.sh | sh
```

Builds from source, installs into `~/.local`, and offers to install missing
build tools. Open a new terminal and run `termnav`. Requires Linux or a Linux
distribution under WSL; native macOS and native Windows are not supported.

Launch without arguments to open the **Home** dashboard: quick-access folders,
a file table, a details pane, and live drive capacity bars. Pass a directory to
start straight in the browser.

---

## What you get

- **Three panes** — parent directory, current directory, live preview. Collapses
  to two, then one, as the terminal narrows. Resize at any time.
- **Navigation that keeps its place** — vim and arrow keys, paging, sorting by
  name/size/modified, hidden-file toggle, and back/forward history that restores
  your selection and filter. Listings have no fixed entry limit.
- **Two search scopes** — a `/` fuzzy subsequence filter over the current folder
  (`tfm` matches `Terminal_File_Manager`), and `S` for a recursive background
  filename search whose results stream in as they are found.
- **Read-only previews** — text with line numbers and lightweight highlighting,
  hexadecimal binary inspection with horizontal panning, colour images, PDF text,
  and archive listings. Nothing is ever modified by looking at it.
- **Recoverable deletion** — `d` moves items to a private trash; `u` restores the
  last batch even after a restart, and `T` browses individual items.
- **Visible background work** — an operations dashboard with progress, speed,
  elapsed time, pause/resume, cancellation, and recent results.
- **Storage inspection** — a background disk usage explorer with allocated-size
  bars and drill-down, plus drive capacity rows that work for Windows drives
  mounted in WSL.
- **Colour images where terminals support them** — native Sixel raster output
  sized to the pane, with a Unicode half-block fallback elsewhere. Windows
  Terminal 1.22 and newer renders the native path.
- A searchable command palette, inline create and rename, multi-file marking, a
  persistent copy clipboard, editor integration, in-app help, `NO_COLOR`, ASCII
  mode, and a simple config file.

## Keys

The essentials. The complete reference, with the code behind each feature, is in
[docs/COMMANDS_FEATURES_AND_CODE.md](docs/COMMANDS_FEATURES_AND_CODE.md).

| Key | Action |
| --- | --- |
| `j` `k` / `l` `h` | Move, enter directory, go to parent |
| `Enter` / `Tab` | Open a file's full-screen preview / expand the preview |
| `/` | Live fuzzy filter (`Esc` clears) |
| `S` | Recursive filename search; `x` stops, `o` reveals a result |
| `Space` / `v` | Mark one / mark everything visible |
| `y` `p` `c` | Copy to clipboard, paste here, copy to a directory |
| `a` / `r` | Create a directory / rename |
| `d` / `u` / `T` / `X` | Trash (type `trash`) / undo batch / restore browser / delete permanently (type `delete`) |
| `t` | Operations dashboard; `Space` pauses, `x` cancels |
| `b` `f` `H` | Back, forward, history picker |
| `D` | Disk usage explorer |
| `s` `.` `R` | Cycle sort, toggle hidden, refresh |
| `:` | Command palette |
| `e` / `?` / `q` | Edit in `$EDITOR`, help, quit |

Clipboard and batch operations use **all marks in the current directory**,
including ones a filter has hidden. Leaving a directory clears its marks.

Inside the full-screen viewer the same movement keys scroll, `h` `l` pan
horizontally by four cells, `0` returns to the start of a line, and `R`
regenerates the preview while keeping your position. The viewer is read-only:
create, rename, copy and delete keys are inactive until you return with `Esc`,
`Tab` or `q`, and your filter, selection and marks are still there when you do.

## Behaviour worth knowing before you trust it

- Copies and renames **never silently overwrite**. A file is written to an
  exclusive temporary file, synced, then published with an atomic no-replace
  hard link. Filesystems without no-replace support report an error rather than
  falling back to an unsafe overwrite.
- Trash payloads live in a private `.termnav-trash-<uid>` directory beside the
  original, so restore is an atomic rename on the same filesystem. This is
  TermNav's own trash, not the desktop recycle bin.
- **Cancellation keeps completed work.** A partially copied directory can remain;
  a batch stops at its first failure. Undo covers trash only, not copy, rename,
  or permanent delete.
- Recursive operations are bounded to 128 levels; searches stop at 1,000 results
  and 255-byte queries; preview reads stop at 64 KiB; PDF text covers the first
  five pages and archives 512 entries. Rich previews run in a cancellable child
  with a five-second timeout and a memory cap.
- Metadata fidelity is partial by design: permission bits and timestamps are
  preserved, ownership, ACLs, xattrs, setuid and hard-link relationships are not.
  Symlinks are copied as links and never followed by recursive operations.
- Directory listings and metadata reads run on the UI thread, so a very large or
  slow remote directory can pause navigation. Plugins, tabs and a cut/move
  clipboard are outside this release.

## Measured search behaviour

Recursive no-match filename search, on generated trees of empty files, query
`qzxv`, median of three runs, one Windows 11 host with WSL2. Times are
end-to-end interface responses, from submitting the query until the app reports
that nothing matched.

| Files | TermNav, native WSL storage | TermNav, NTFS through `/mnt/c` | Mounted-path penalty |
| ---: | ---: | ---: | ---: |
| 1,000 | 0.084 s | 0.470 s | 5.6x |
| 10,000 | 0.083 s | 3.836 s | 46.0x |
| 100,000 | 0.166 s | 37.250 s | 224x |

The same query against Windows File Explorer, on one shared NTFS volume:

| Files | File Explorer | TermNav | Result |
| ---: | ---: | ---: | ---: |
| 1,000 | 1.80 s | 1.14 s | TermNav 1.58x faster |
| 10,000 | 2.49 s | 8.48 s | Explorer 3.41x faster |
| 100,000 | 7.76 s | 76.29 s | Explorer 9.83x faster |

**How to read this.** The storage path dominates. The gap between the two
TermNav columns is the WSL mount layer, not the search code, and it widens with
the tree because recursive search performs directory reads and metadata checks
at every level. On the shared volume Explorer scales better at the larger sizes;
TermNav's advantage appears only on native WSL storage. Windows Search was
running but held no indexed entries for these folders, cache state was not reset
between runs, values below 0.1 s sit near the redraw floor of the interface, and
three runs on one machine bind the result to this environment. These are
measurements of one configuration, not a general ranking of the two tools.

## Install

One command, as above. To choose a prefix:

```sh
curl -fsSL https://raw.githubusercontent.com/SujayYadav776/TermNav/main/scripts/install.sh | TERMNAV_PREFIX="$HOME/.local" sh
```

To remove a user installation (keeps trash and configuration):

```sh
curl -fsSL https://raw.githubusercontent.com/SujayYadav776/TermNav/main/scripts/uninstall.sh | sh
```

### Build from source

Requires a C11 compiler, make, pthreads and ncurses with wide-character support.
Python 3 enables rich previews, the demo and the integration tests; Pillow is
used for images and Poppler for PDF text.

```sh
# Ubuntu / Debian
sudo apt install build-essential pkg-config libncursesw5-dev python3
sudo apt install python3-pillow poppler-utils        # optional rich previews

# Alpine
apk add build-base ncurses-dev ncurses-terminfo python3
apk add py3-pillow poppler-utils

make && ./termnav            # current directory
./termnav ~/Downloads        # any directory
make demo                    # create and browse a disposable demo tree

make install PREFIX="$HOME/.local"   # or: sudo make install
```

### From Windows

TermNav is a Linux binary. On a new Windows machine, install WSL first:

```powershell
wsl --install -d Ubuntu        # restart if prompted, then open Ubuntu
```

In Ubuntu, run the one-line installer from the top of this page. Then, from the
project folder in PowerShell, `termnav.ps1` launches the build inside WSL and
maps your Windows user folders, including locations redirected to OneDrive.

```powershell
.\termnav.ps1 -Demo
.\termnav.ps1 -Path "$env:USERPROFILE\Downloads"
.\scripts\install-powershell.ps1        # register `termnav` for future shells
```

This is a convenience wrapper around the Linux binary, not native Windows
support. To force or disable the image renderer:

```sh
TERMNAV_IMAGE=sixel ./termnav     # only on a Sixel-capable terminal
TERMNAV_IMAGE=blocks ./termnav    # portable character-cell fallback
```

## Terminal and configuration

Use a UTF-8 terminal with 256 colours. Three panes want 108 columns, two panes
76, and the minimum usable size is 38 × 14. Standard monospace fonts are enough.

```sh
./termnav --ascii        ./termnav --no-color        NO_COLOR=1 ./termnav
```

Copy `config.example` to `~/.config/termnav/config`:

```ini
show_hidden = false
ascii = false
sort = name
```

Command-line options take priority. External filesystem changes are picked up
every two seconds while idle, and paused while the full-screen viewer is open.

## Verification

```sh
make check       # filesystem tests plus real pseudo-terminal UI tests
make sanitize    # ASan / UBSan build and tests
```

Tests build their own disposable trees under `/tmp` and never touch personal
files. Coverage includes listings beyond 1,024 entries, byte-correct copies,
no-clobber behaviour, symlink safety, descendant rejection, cancellation, binary
and FIFO previews, background jobs, navigation, Unicode, filtering, create,
rename, paste, batch copy, delete confirmation, editor restoration, full-screen
reading, horizontal panning, resizing, signals and terminal restoration, plus
persisted trash, restore conflicts, history branching, disk scans, the command
palette, image and PDF previews, archive listing and corrupt input. CI runs GCC
and Clang on Ubuntu with memory and sanitizer checks.

## Documentation

| Document | Contents |
| --- | --- |
| [Commands, features and code](docs/COMMANDS_FEATURES_AND_CODE.md) | Every key, every prompt, and the code behind each feature |
| [Codebase walkthrough](docs/CODEBASE_WALKTHROUGH.md) | Module map and how the event loop, workers and previews fit together |
| [Engineering report](docs/REPORT.md) | Architecture, syscall choices, I/O tradeoffs |
| [Demo guide](docs/DEMO.md) | A short guided walkthrough |
| [Manual page](docs/termnav.1) | `man termnav` after `make install` |

## License

MIT. See [LICENSE](LICENSE).
