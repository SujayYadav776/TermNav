# TermNav: commands, features, and code guide

This guide describes the current implementation of TermNav, a C11 terminal file manager built with ncursesw and POSIX threads. The application runs on Linux; the Windows launcher runs it through WSL. Native macOS and native Windows are not supported. Commands below describe implemented behavior, not proposed features.

## Contents

- [Starting the application](#starting-the-application)
- [Browser commands](#browser-commands)
- [Prompts and confirmation](#prompts-and-confirmation)
- [Full-screen preview commands](#full-screen-preview-commands)
- [Feature panel commands](#feature-panel-commands)
- [Command palette](#command-palette)
- [Recursive filename search](#recursive-filename-search)
- [Features and their code](#features-and-their-code)
- [Source map](#source-map)
- [Configuration and stored data](#configuration-and-stored-data)
- [Build and verification commands](#build-and-verification-commands)

## Starting the application

### Linux / WSL

```sh
make
./termnav
./termnav ~/Downloads
./termnav --all --ascii ~/Downloads
./termnav --help
./termnav --version
```

Syntax: `termnav [options] [directory]`. The default directory is `.`.

Without a directory argument, TermNav opens the Home dashboard over the current
directory. With a directory argument it starts in the browser. The PowerShell
launcher follows the same rule; use `-Path` to start directly in a directory.

### Home dashboard

| Key | Behavior |
| --- | --- |
| `w` | Toggle Home and browser. Available in the command palette. |
| `F1` | Jump to Home from the browser, preview, help or feature panels. Does not interrupt text-entry or confirmation prompts. |
| `Tab` | Cycle quick-access folders, file table and drives. |
| Up/Down, `j k` | Select an item in the focused area. Left/Right also select quick-access cards. |
| Right, `l` | Open a selected file-list entry or drive. |
| Left, `h` | Open the parent directory from the file list. |
| Backspace | Open the current directory's parent from any Home area. |
| `g`, `G` | First / last item in the focused area. |
| `Enter` | Open a quick folder or drive in the browser; files open rich preview. |
| `Esc` | Return to the browser. |
| `/`, `a`, `r`, `d`, `u`, `p`, `:` | Search, create, rename, trash, undo, paste and commands work from Home. |

The dashboard uses the browser's dark background, amber highlights and blue
folders. Folder cards have tabbed outlines and an amber selection indicator.
It adapts the sidebar, folder cards, file table and details pane to terminal
cells. Narrow terminals hide the sidebars; short
terminals show a compact selection and capacity bar. Home, Downloads, Documents
and Pictures shortcuts use `$HOME` on Linux. The PowerShell launcher maps them
to your Windows profile and user folders, including a OneDrive fallback when
the standard folder is missing. Unavailable folders report their path and error.
`TERMNAV_USER_HOME`, `TERMNAV_DOWNLOADS`, `TERMNAV_DOCUMENTS` and
`TERMNAV_PICTURES` can override these shortcuts with Linux/WSL paths.

`src/ui.c:home_dashboard` draws the dashboard and capacity bars.
`src/input.c:home_input` handles focus and navigation, and reuses existing file
operations and confirmation prompts. `src/drives.c:drives_read` discovers local
mounts with `getmntent` and reads capacity with `statvfs`, including WSL Windows
mounts. Pseudo filesystems, loop devices and duplicate local sources are omitted.
At most 16 drives are listed. Compact two-line rows show all four drives at
standard terminal sizes; shorter terminals scroll as a drive is selected.
`src/main.c` refreshes capacities every five seconds while Home is open; `R`
requests an immediate refresh. Used space is total minus space available to the
current user, so reserved filesystem blocks count as occupied. Bars turn amber
at 85% and red at 95%; monochrome mode uses `#` and `-` characters.

| Option | Behavior |
| --- | --- |
| `-a`, `--all` | Include hidden entries. |
| `--ascii` | Use ASCII interface characters and the compatible image fallback. |
| `--no-color` | Disable interface colors. |
| `--home` | Start on the Home dashboard, including when a directory is specified. |
| `-h`, `--help` | Print command-line usage and exit. |
| `-v`, `--version` | Print the application version and exit. |
| `--` | End option parsing, allowing a directory beginning with `-`. |

Normal operation requires interactive terminal input and output. Help and version work without an interactive terminal. Unknown options and multiple starting directories return exit status 2.

### Windows launcher

Run these examples from the project folder in PowerShell:

```powershell
.\termnav.ps1
.\termnav.ps1 -Path 'C:\Users\ADMIN\Downloads'
.\termnav.ps1 -Demo
.\termnav.ps1 -All -Ascii
.\termnav.ps1 -ImageRenderer blocks
.\termnav.ps1 -ImageRenderer sixel
```

| Parameter | Behavior |
| --- | --- |
| `-Path` | Starting directory; defaults to `.`. Windows drive paths are converted to WSL paths. |
| `-Distribution` | WSL distribution; defaults to `TermNav-Dev`. |
| `-All` | Show hidden files. |
| `-Ascii` | Enable ASCII mode. |
| `-NoColor` | Disable colors. |
| `-Demo` | Generate the demo fixtures and open Home with `test-playground` loaded. |
| `-ImageRenderer auto` | Probe the terminal for native Sixel support; default. |
| `-ImageRenderer sixel` | Force Sixel output; use with a terminal that supports it. |
| `-ImageRenderer blocks` | Use terminal character cells for images. |

`termnav.ps1` builds the application before launching it. Forcing Sixel cannot add graphics support to a terminal that lacks it.

### Install on Linux from GitHub

```sh
curl -fsSL https://raw.githubusercontent.com/SujayYadav776/TermNav/main/scripts/install.sh | sh
```

The installer builds the latest `main` branch and installs to `~/.local` by default. If build tools are missing, it installs them through `apt`, `apk`, `dnf`, `pacman`, or `zypper`, requesting root / sudo access when required. Set `TERMNAV_PREFIX` on the installer shell to change the absolute install path. Native Linux and WSL are supported; the program currently depends on Linux system calls and cannot run natively on macOS or Windows.

To remove the user installation:

```sh
curl -fsSL https://raw.githubusercontent.com/SujayYadav776/TermNav/main/scripts/uninstall.sh | sh
```

## Browser commands

These bindings apply to the normal directory browser. Keys are case-sensitive: `d`, `D`, and `Ctrl-d` perform different actions.

| Key | Command / feature |
| --- | --- |
| `j`, Down | Select the next visible entry. |
| `k`, Up | Select the previous visible entry. |
| `l`, Right, Enter | Enter a directory; open a regular file in the full-screen viewer. |
| `h`, Left, Backspace | Go to the parent directory and select the directory you just left. |
| `gg`, Home | Select the first entry. |
| `G`, End | Select the last entry. |
| `Ctrl-d`, Page Down | Move down one page. |
| `Ctrl-u`, Page Up | Move up one page. |
| Tab | Open the full-screen preview of the selected entry, including directories. |
| `J`, `K` | Scroll the side preview down / up. |
| `/` | Start a live fuzzy filename filter. |
| `.` | Toggle hidden entries. |
| `s` | Cycle sorting: name → size → modified time. Directories remain first. |
| `R`, `Ctrl-l` | Reload the directory and regenerate the selected rich preview. |
| Space | Toggle the selected entry's mark, then move down. |
| `v` | Toggle marks on all currently visible entries. |
| `y` | Copy marked entry paths, or the selected path, to TermNav's internal clipboard. |
| `p` | Queue a copy of clipboard entries into the current directory. |
| `c` | Prompt for an existing destination directory and queue a copy. |
| `a` | Prompt for a new directory name. |
| `r` | Prompt to rename the selected entry; the old name is prefilled. |
| `d` | Move marked entries, or the selected entry, to private trash after confirmation. |
| `X` | Permanently delete marked entries, or the selected entry, after confirmation. |
| `u` | Queue restoration of the latest remaining trash batch. |
| `T` | Open the trash browser. |
| `t` | Open the operations dashboard. |
| `D` | Open the disk usage explorer for the current directory. |
| `H` | Open directory history. |
| `b`, `f` | Navigate backward / forward through directory history. |
| `o` | Prompt for a directory path. Absolute, relative, and `~/` paths are supported. |
| `~` | Go to the home directory. |
| `e` | Open the selected file in the external editor. |
| `:` | Open the command palette. |
| `?` | Show keyboard help. |
| Esc | Cancel an active operation; otherwise clear the filter and marks. |
| `q` | Quit. With active or queued operations, confirm by typing `q` and pressing Enter. |

Selection-based copy and deletion use all marked entries when marks exist. Filtering does not erase marks on entries outside the visible results. The clipboard belongs to TermNav, not the operating system clipboard, and lasts only for the current session.

Creating directories, renaming, and opening the editor are blocked while a background file operation is active. Copy, trash, delete, and restore requests can be queued.

## Prompts and confirmation

| Key / input | Behavior |
| --- | --- |
| Enter | Submit the prompt. |
| Esc | Cancel the prompt. In the filter prompt, this clears the filter. |
| Backspace | Remove one UTF-8 character. |
| `Ctrl-u` | Clear the prompt text. |
| Printable characters | Append text within the input buffer limit. |
| Literal `trash` | Confirm moving entries to trash. |
| Literal `delete` | Confirm permanent deletion. |
| Literal `q` | Confirm quitting and cancelling / dropping operations. |

An incorrect trash or delete confirmation cancels the action. Enter in the filter prompt keeps the active filter. Help closes with `?`, Esc, `q`, or Enter.

## Full-screen preview commands

| Key | Behavior |
| --- | --- |
| Esc, `q`, Tab | Return to the browser. Here, `q` does not quit TermNav. |
| `j`, `J`, Down / `k`, `K`, Up | Scroll vertically down / up. |
| `Ctrl-d`, Space, Page Down | Scroll forward one page. |
| `Ctrl-u`, Page Up | Scroll backward one page. |
| `gg`, Home / `G`, End | Go to the beginning / final row. |
| `h`, Left / `l`, Right | Pan text or hex output left / right by four columns. |
| `0` | Reset horizontal scrolling. |
| `R`, `Ctrl-l` | Refresh the preview, retaining a valid scroll position. |
| `:` | Open the command palette. Running a command leaves the viewer. |

The viewer retains browser selection, filter, and marks. Images fit the available area; image zoom and image panning are not implemented.

## Feature panel commands

All four panels share `j` / Down, `k` / Up, `g` for the first row, and `G` for the last row. Esc or `q` closes a panel. Its opening key (`t`, `T`, `H`, or `D`) also closes it. `:` opens the command palette. Panel navigation does not currently implement Page Up / Down or Home / End.

| Panel | Additional commands |
| --- | --- |
| Operations dashboard (`t`) | Space pauses / resumes the selected active operation. `x` or `c` cancels the selected active operation or removes the selected queued operation. Completed rows cannot be changed. |
| Trash browser (`T`) | Enter queues restoration of the selected record. `u` restores the latest remaining batch. `R` reloads trash records. |
| Directory history (`H`) | Enter navigates to the selected history entry and closes the panel. The current position has a `*` marker. |
| Disk usage explorer (`D`) | Enter or `l` enters a selected directory. `h`, Left, or Backspace goes to the parent. `R` rescans. `o` reveals the selected item in the browser. Closing the panel cancels its scan. |

## Command palette

Press `:` to search commands by name and description. Type to filter, use Up / Down to select, Enter to run, Esc to close, Backspace to remove a character, and `Ctrl-u` to clear the search.

The palette is a fixed registry of application actions, not a shell. Search uses a case-insensitive subsequence match and retains registry order; it does not rank results by relevance.

All 23 registered commands are listed here:

| Palette command | Shortcut |
| --- | --- |
| Preview selected entry | Tab |
| Operations dashboard | `t` |
| Disk usage explorer | `D` |
| Directory history | `H` |
| History back | `b` |
| History forward | `f` |
| Move selection to trash | `d` |
| Undo last trash operation | `u` |
| Browse trash | `T` |
| Permanently delete selection | `X` |
| Copy to directory | `c` |
| Paste clipboard here | `p` |
| Copy paths to clipboard | `y` |
| Create directory | `a` |
| Rename selected entry | `r` |
| Go to directory | `o` |
| Go home | `~` |
| Filter files | `/` |
| Toggle hidden files | `.` |
| Cycle sort order | `s` |
| Refresh directory | `R` |
| Keyboard help | `?` |
| Quit TermNav | `q` |

Navigation, marking, and the external editor also have keyboard bindings, but are not separate palette entries.

## Recursive filename search

Press `S` from Home or the browser, type a filename query and press Enter.
This searches the current directory and its subfolders using the same fuzzy
filename matching as `/`, with case-insensitive matching for ASCII letters.
It searches filenames, not file contents. `/` remains the current-folder filter.

| Key in results | Action |
| --- | --- |
| Up/Down, `j k` | Select a result. |
| `g`, `G` | First / last result. |
| Enter, Right, `l` | Open a directory or preview a file. |
| `o` | Open the containing directory and select the file. Directories open directly. |
| `S` | Enter another search query. |
| `R` | Repeat the query in the original search directory. |
| `x` | Stop scanning and keep results already found. |
| Esc, `q` | Close results and stop the worker. |
| `F1` | Stop searching and return to Home. |

The command palette includes **Search files recursively**. Toggle hidden files
with `.` before searching to include hidden files and directories. Searches
stay on the starting filesystem; open another drive to search it. Symlinked
directories are listed when matched but never traversed. Results are capped
at 1,000 and traversal depth at 128; skipped entries and the result limit are
visible in the search status. Search input is limited to 255 bytes.

`src/search.c` owns the background worker, bounded results and cancellation.
It uses `openat`, `fstatat`, `fdopendir` and `readdir`, checks each directory
without following symlinks, and protects published results with a mutex.
`src/input.c` handles the prompt and result actions; `src/ui.c` renders the
results panel. `src/main.c` joins the worker and frees results on shutdown.

## Features and their code

The explanations below identify the major functions and data blocks. Text flow examples are explanatory pseudocode. C excerpts are portions of the implementation and require their surrounding source to compile.

### 1. Application state and event loop

**Main files:** `src/app.h`, `src/main.c`, `src/input.c`.

`App` holds directory listings, visible entry indexes, cursor positions, filter and prompt buffers, clipboard paths, preview state, operation queue, history, trash records, disk scan state, and ncurses windows. `InputMode` and `Panel` distinguish the active interaction context.

`main()` loads settings, parses arguments, initializes workers and the terminal, and runs this cycle:

```text
collect finished rich preview → advance operation queue
→ periodically reload an idle browser → render interface
→ read a key → route it to the active input handler
```

The terminal input timeout is 80 ms, allowing progress and completed previews to appear without a keypress. Idle directory refresh occurs approximately every two seconds. SIGINT, SIGTERM, and SIGHUP request orderly shutdown: workers are cancelled and joined and terminal settings are restored.

This excerpt from `main.c` connects completed background work to keyboard handling:

```c
if (rich_collect(&a.rich,&a.preview)) ++a.image_revision;
app_operations_tick(&a);
```

After the idle-refresh check, the loop renders and reads input:

```c
ui_render(&a); wint_t key; int r = get_wch(&key);
if (r != ERR) app_input(&a, key, r == KEY_CODE_YES);
```

`image_revision` tells rendering that new image content is available. `KEY_CODE_YES` distinguishes special keys such as arrows from ordinary characters.

`app_input()` routes keys to the palette, feature panel, viewer, help, prompt, or browser in that order. This routing is why `q`, Space, `c`, and Enter have different meanings in different screens.

### 2. Directory navigation, listing, filter, and marks

**Main files:** `src/main.c`, `src/fs_ops.c`, `src/input.c`.

`fs_list()` resolves a directory to an absolute path, reads entries, collects metadata, and grows its entry array dynamically. `Entry` stores the name, stat information, directory / symlink flags, and a mark flag. There is no fixed 1,024-entry limit.

`load_current()` replaces the listing, rebuilds visible indexes, reloads the parent pane, and updates the selected preview. On a same-directory refresh it preserves marks when both name and inode still match. `app_selected()` maps the visible cursor back to the underlying entry.

```text
Listing.entries = all loaded entries, including their marks
visible[]       = indexes of entries matching the current filter
selected entry  = Listing.entries[visible[cursor]]
```

`app_filter()` calls the filename matcher and clamps the cursor. The matcher walks UTF-8 characters and accepts a query when its characters appear in order; it is not a regular expression or typo-correcting search. ASCII case is folded; full Unicode case folding is not implemented.

`app_navigate()` saves the previous location state, clears the new view's filter and scroll, loads the destination, and pushes directory history. Failed navigation restores the previous view state. Directory symlinks can be followed during browsing, while preview and file operations handle symlinks separately.

### 3. Responsive interface and text rendering

**Main file:** `src/ui.c`.

`ui_init()` configures ncurses input and colors. `ui_layout()` chooses three panes at 108 or more columns, two panes at 76–107 columns, and a single current-directory pane below that. The minimum supported terminal size is 38 columns by 14 rows. Full-screen previews and feature panels use the wider viewing area.

`ui_render()` draws the frame, status area, panes or active panel, and overlays, then flushes ncurses output. The status area shows metadata, selection / marks, clipboard state, and operation progress. The operations UI shows completed bytes / items, elapsed-time average speed, queued work, recent results, and errors.

Text rendering decodes UTF-8 and uses display-cell widths, so clipping accounts for wide characters. Unsafe control characters are replaced. Code highlighting uses a small line-based lexer for keywords, strings, comments, and numbers; Markdown headings receive styling. This is not a complete language parser or Markdown renderer.

### 4. Copy, paste, directory creation, rename, and permanent deletion

**Main files:** `src/fs_ops.c`, `src/async_ops.c`, `src/input.c`, `src/features.c`.

The input layer gathers absolute source paths and validates prompts. `fs_mkdir()` validates a single name and creates it relative to an opened parent directory. `fs_rename()` uses Linux `renameat2` with no-replace semantics to avoid overwriting a destination.

The recursive copy block dispatches by entry type:

```text
symlink → copy link text without following its target
directory → create destination, recurse, then finish metadata
regular file → create exclusive temporary file, copy chunks,
               sync data and metadata, publish without overwriting
```

Regular files are copied in 128 KiB chunks. Partial writes and interrupted system calls are handled, progress is reported, and timestamps and ordinary permission bits are retained. Destination validation prevents copying a directory into itself or a descendant, including through a destination symlink path.

Recursive permanent deletion removes children before their directory and does not follow symlinks. Traversal has a depth limit of 128. Cancellation is cooperative between filesystem steps. Successful earlier entries can remain after a later error or cancellation; these operations are not a transactional batch rollback.

Copying does not preserve ownership, ACLs, extended attributes, hard-link relationships, or setuid / setgid bits. Name validation rejects empty names, `.` / `..`, and names containing `/`.

### 5. Background operations and dashboard

**Main files:** `src/features.c`, `src/async_ops.c`, `src/app.h`.

`app_enqueue()` owns the accepted request's source paths and adds it to the FIFO queue. The application supports one active file operation, up to 16 pending requests, and 32 recent completed results. Operation kinds are copy, delete, trash, and restore.

`app_operations_tick()` collects a finished worker, records its result, reloads affected views, and starts the next pending request. Directory reload waits until an open prompt closes so its target remains stable. `job_start_kind()` creates the worker; `job_collect()` joins it and frees its paths through the shared `fs_paths_free()` helper. Worker input is a path snapshot, so it does not depend on a listing that the user may navigate away from.

```text
queued request → worker planning → running / paused
               → completed, failed, or cancelled → recent result
```

Atomic fields carry bytes, item counts, planning state, pause, cancellation, and completion signals. The pause loop checks cancellation while waiting. A blocked operating-system call is not instantly interruptible. Quitting cancels the active job, discards pending work, and joins the worker before cleanup.

For example, the dashboard pause command in `input.c` changes an atomic flag rather than manipulating the worker directly:

```c
if (a->job.started && !a->panel_cursor && key == ' ')
    atomic_store(&a->job.paused, !atomic_load(&a->job.paused));
```

The condition restricts this action to row zero when an active job exists. The worker observes the flag at its cooperative checkpoints.

The dashboard and result history are session-only. They are not a persistent audit log.

### 6. Trash and batch undo

**Main files:** `src/trash.c`, `src/features.c`.

`trash_put()` stores the payload inside a private `.termnav-trash-UID` directory beside the original entry. Keeping the payload on its original filesystem allows an atomic move. The metadata registry is stored under the user's TermNav data directory.

Each record contains a `TNTRASH1` identifier, batch ID, timestamp, original path, and stored path. The code writes and syncs the record before moving the payload with no-replace semantics. Failure cleans up the provisional record. Private directories, ownership checks, no-follow file opens, bounded record parsing, and a registry lock protect record access across application instances.

`trash_list()` loads valid records whose payloads still exist and orders them newest first. Restore validates the record and moves its payload to the original path without overwriting anything. An occupied destination keeps both the trash record and payload available for a later retry.

`app_undo()` selects the latest remaining batch and queues restoration of its records. This is undo for trash operations, not general undo for copy, rename, or permanent deletion. The trash is persistent across sessions, but is separate from Windows Recycle Bin and the desktop Freedesktop trash. There is no empty-trash command. A missing original parent must be restored or recreated first; empty private payload directories may remain.

### 7. Directory history

**Main files:** `src/history.c`, `src/main.c`, `src/features.c`.

History keeps up to 64 locations in memory. Each entry retains the path and browsing state, including the filter and selected entry. Back / forward movement and the history picker use `app_history_jump()`.

The replay flag prevents a history jump from adding another history entry. The target state is copied before navigation, then its filter, selection, cursor, and scroll are restored. History lasts for the current session and is not written to disk.

### 8. Disk usage explorer

**Main files:** `src/usage.c`, `src/features.c`, `src/ui.c`.

`usage_start()` resolves the directory, stops an old scan, and starts a new worker. `scan_worker()` lists root entries, measures each recursively, publishes progress under a mutex, and sorts completed entries by allocated space. `usage_stop()` requests cancellation, joins the worker, and frees scan results.

`measure_at()` collects two sizes: apparent regular-file bytes (`st_size`) and allocated bytes (`st_blocks × 512`). The panel ranks entries and draws proportional allocated-space bars. It includes hidden entries, does not follow symlink targets, counts traversal errors, and applies the same 128-level depth bound.

The scan has its own worker and can run independently of a file operation. Hard links are counted per entry rather than deduplicated, so totals can differ from `du`. Totals describe entries under the scanned root and exclude the root directory's own blocks. A scan is not an atomic snapshot of a changing filesystem.

### 9. Text, directory, symlink, binary, and special-file previews

**Main files:** `src/preview.c`, `src/main.c`, `src/ui.c`.

`preview_load()` classifies the selected entry and builds a `Preview`. A directory gets an entry listing; a symlink displays its target; devices, FIFOs, and sockets receive metadata rather than being read as ordinary files.

Regular-file preview reads at most 64 KiB using no-follow, nonblocking opens and verifies the opened file type. NUL bytes or a high control-character ratio select binary mode; otherwise the preview is indexed as text. `preview_line()` retrieves a line from stored offsets, and `preview_rows()` exposes the number of scrollable rows.

The full-screen binary viewer displays offsets, 16 hexadecimal bytes per row, and printable text. The side pane directs the user to the full-screen hex view. Large text / binary previews are bounded and can be truncated. Syntax labels cover common C / C++, Python, JSON, Markdown, JavaScript / TypeScript, Rust, Go, and shell files.

### 10. Rich image, PDF, and archive previews

**Main files:** `src/rich_preview.c`, `scripts/preview_helper.py`, `src/main.c`.

`rich_mode()` selects a helper mode for regular files by extension:

| Type | Supported preview |
| --- | --- |
| PNG, JPG / JPEG, WEBP, GIF, BMP | Image preview; the first frame is used for animated images. |
| PDF | Text extracted from the first five pages using `pdftotext`. |
| ZIP, TAR, TGZ, GZ, BZ2, XZ | Archive contents listing, limited to 512 entries. Compressed formats are interpreted as compressed tar archives. |

The helper needs Python 3; image decoding needs Pillow and PDF extraction needs `pdftotext`. Archive previews list contents without extracting files. Standalone compressed files that are not tar archives may not produce an archive listing. Scanned PDFs have no OCR support.

The rich worker opens and verifies the selected file, passes an open descriptor to a Python subprocess, and reads a bounded response. The cache considers path, file identity / metadata, requested dimensions, and renderer. Changing selection or refreshing can cancel an old worker.

```text
selected entry → validate open file → choose helper mode
→ subprocess decodes / extracts / lists → validate response
→ collect Preview on the main thread → redraw
```

The worker enforces a five-second wall-time limit and bounded output (64 KiB for text, 8 MiB for image data). Cancellation terminates the helper process group and waits for cleanup. The Python helper also applies memory, CPU, and file-output limits. Pillow handles orientation, high-quality thumbnail resampling, alpha compositing, and large-image checks.

### 11. Native images and character-cell fallback

**Main files:** `src/image_terminal.c`, `src/sixel.c`, `src/rich_preview.c`, `src/ui.c`.

`image_terminal_probe()` asks the terminal for capabilities and cell dimensions, waiting up to 250 ms. It preserves pending key input. Unknown cell dimensions fall back to 8 × 16 pixels. Automatic detection is disabled inside tmux unless Sixel is forced; ASCII or no-color mode skips the graphics probe.

For native output, the helper produces a 256-color indexed image. `sixel_encode()` emits color definitions and six-row pixel bands, compressing repeated runs. The UI writes the resulting Sixel payload after ncurses has drawn the interface. It tracks image revisions and layout changes to avoid repeatedly sending unchanged images and clears old graphics before redraws or overlays.

For compatible fallback output, the helper produces RGB samples. The UI approximates colors with terminal color pairs and uses the upper-half-block character to show two vertical samples per cell. ASCII mode has lower visual detail.

**Image-quality limits:** the block renderer is inherently pixelated because its resolution is tied to text cells. Native Sixel uses actual pixel graphics, but is capped at 1,280 × 960 pixels and quantized to 256 colors. The terminal must support Sixel. Kitty and iTerm image protocols, animated playback, zoom, and image panning are not implemented. Protocol tests do not establish the appearance in every user's terminal.

### 12. Command registry and external editor

**Main files:** `src/commands.c`, `src/features.c`, `src/input.c`, `src/editor.c`.

The `commands[]` registry holds each palette action's name, description, and shortcut. Palette filtering searches that metadata; execution dispatches into the existing application actions, keeping palette and keyboard behavior aligned.

One actual registry entry in `commands.c` is:

```c
{"Operations dashboard", "Queue, progress, pause, cancel and completed jobs", 't'},
```

The execution block in `input.c` reads the matched command's key, closes the palette's previous screen, and calls `app_input()` with that key. Therefore the dashboard command shares the same implementation as pressing `t` in the browser.

The editor path suspends ncurses, starts `$EDITOR` (default `vi`) with the selected path as a separate argument, waits, and restores the terminal interface. It does not run a shell command. `$EDITOR` must name a single executable; embedded options or whitespace-separated arguments are not supported.

## Source map

Paths below are relative to the project root.

| File / block | Responsibility |
| --- | --- |
| `src/app.h` | Shared application state, input modes, panels, operation records, declarations. |
| `src/main.c` | Startup, settings, main loop, listing lifecycle, navigation, preview coordination. |
| `src/input.c` | Context-aware keyboard handling, prompts, browser and viewer actions. |
| `src/ui.c` | Layout, colors, text / hex / image rendering, panels and overlays. |
| `src/fs_ops.c`, `.h` | Listing, matching, sorting, copy, mkdir, rename, deletion, formatting. |
| `src/async_ops.c`, `.h` | File-operation workers, progress, pause / cancel, planning and cleanup. |
| `src/features.c` | Queue management, dashboard, history / trash / usage panel actions, palette. |
| `src/history.c`, `.h` | Bounded directory history and replay state. |
| `src/trash.c`, `.h` | Private persistent trash records, payload moves, listing and restore. |
| `src/usage.c`, `.h` | Cancellable recursive disk measurement and background scan results. |
| `src/commands.c`, `.h` | Palette command registry and matching. |
| `src/preview.c`, `.h` | Basic preview classification, bounded data loading and line indexing. |
| `src/rich_preview.c`, `.h` | Rich-preview subprocess, caching, response validation and lifecycle. |
| `src/image_terminal.c`, `.h` | Terminal graphics detection and cell metrics. |
| `src/sixel.c`, `.h` | Bounded indexed-image to Sixel encoding. |
| `src/editor.c` | External editor launch and terminal suspension / restoration. |
| `scripts/preview_helper.py` | Pillow image conversion, PDF text extraction, archive listings. |
| `termnav.ps1` | Windows / WSL path conversion, build and launch options. |
| `scripts/install.sh`, `uninstall.sh` | One-command Linux installation from GitHub and removal from the selected prefix. |
| `Makefile` | Compilation, tests, installation, demo and cleanup. |
| `tests/` | Filesystem / feature / encoder tests and interactive terminal integration tests. |
| `tests/test_app.c` | Application-state regression tests for prompts and background completion. |
| `tests/terminal_support.py`, `image_support.py` | Shared terminal driver and independent graphics decoder for tests and captures. |
| `scripts/create_demo.py` | Disposable demo fixture generation. |
| `scripts/capture_tui.py`, `render_tui.py` | Capture and render terminal sessions for visual inspection. |
| `scripts/capture_image_native.py` | Capture and reconstruct native image protocol output for inspection. |
| `.github/workflows/ci.yml` | Automated build and verification configuration. |
| `docs/termnav.1` | Installed manual page. |
| `qa/VERIFICATION.md` | Recorded verification evidence and toolchain limitations. |

## Configuration and stored data

Settings are read from `$XDG_CONFIG_HOME/termnav/config`, or `~/.config/termnav/config` when that variable is not set. See `config.example`:

```ini
show_hidden = false
ascii = false
sort = name
```

`sort` accepts `name`, `size`, or `modified`. Runtime toggles are not written back to this file. Command-line options are applied after configuration.

| Environment variable | Purpose |
| --- | --- |
| `HOME` | Home navigation and default config / data paths. |
| `XDG_CONFIG_HOME` | Alternate configuration location. |
| `XDG_DATA_HOME` | Alternate trash registry base; an absolute path is required for this override. |
| `EDITOR` | Single editor executable; defaults to `vi`. |
| `TERM` | Terminal capabilities used by ncurses. |
| `LANG` / locale variables | Character encoding and Unicode display behavior. |
| `NO_COLOR` | Its presence disables colors. |
| `TERMNAV_IMAGE` | `auto` (default), `sixel`, or `blocks`. |
| `TMUX` | Automatic native-image detection guard. |

The persistent trash registry defaults to `~/.local/share/termnav/trash`. Payloads remain in their original parent directories' private trash folders. Clipboard, marks, directory history, operation queue, and completed-operation history are session-only.

## Build and verification commands

These commands run inside Linux / WSL from the project root. Core compilation requires a C compiler, make, pkg-config, ncursesw development files, and pthread support. Rich previews additionally need the helper dependencies described above; terminal capture tools may require pyte and Pillow.

| Command | Purpose |
| --- | --- |
| `make` / `make all` | Compile the application. Header dependency files allow incremental rebuilding. |
| `make test` | Run C filesystem, feature, Sixel encoder, and application-state tests. |
| `make integration` | Run browser, feature-panel, and image-preview terminal integration tests. |
| `make check` | Run unit and integration tests. |
| `make sanitize` | Clean, rebuild, and check with AddressSanitizer and UndefinedBehaviorSanitizer. Requires compatible compiler runtimes. |
| `make demo` | Generate fixtures and launch their directory. |
| `make install` | Install binary, rich-preview helper, and manual; default prefix `/usr/local`. |
| `make install PREFIX="$HOME/.local"` | Install under the current user's local prefix. |
| `make install DESTDIR=/tmp/termnav-package` | Stage installation under another root. |
| `make uninstall` | Remove installed binary, helper, and manual using the selected prefix / staging root. |
| `make clean` | Remove generated `build` files and the application binary. |
| `make CC=clang` | Select an alternate compiler. |
| `man termnav` | Read the manual after installation and manual-path setup. |

The test suites exercise behavior such as filesystem safety, cancellation, trash restoration conflicts, directory history, palette actions, and graphics encoding / terminal interactions. See `qa/VERIFICATION.md` for actual recorded results. The local ASan build has a runtime-linking limitation; the existence of `make sanitize` or a CI configuration is not evidence that those checks passed on every environment.
