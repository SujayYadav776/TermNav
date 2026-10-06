# Local code review and fixes

Reviewed the C application, Python preview / verification helpers, Windows launcher, build targets, and test coverage. Changes remain local and uncommitted. No push was performed.

The invoked Ponytail review skill focuses on unnecessary complexity. The user's request also authorized implementation and correctness fixes, so both passes were performed. This report describes concrete findings and checks; it is not a guarantee that every possible defect has been eliminated.

## Simplification findings applied

1. `src/preview.c`:L104: reuse: duplicated newline counting and offset construction. Use `preview_index()` for both basic and rich text previews.
2. `src/async_ops.h`:L9: delete: `running` and `deleting` fields had writers but no readers. Use the existing `started` state and `JobKind`.
3. `src/fs_ops.c`:L33: reuse: repeated path-array destruction in input, queue, shutdown, and worker code. Use `fs_paths_free()`.
4. `tests/terminal_support.py`:L1: reuse: tests and capture scripts sliced source files and executed the extracted text to access helpers. Import shared terminal and image helper modules directly.

net: -6 lines possible in the C simplification blocks (applied). This excludes correctness guards, regression tests, moved Python helpers, and documentation.

## Correctness fixes applied

- Background completion defers directory reload while a prompt is open. A trash confirmation cannot silently switch to the next file when an earlier operation removes the selected entry.
- Rename and mkdir recheck active-job state when the prompt is submitted, covering jobs that start after the prompt was opened.
- Trash rejects moving its metadata registry or an ancestor directory, which would make its recovery records unreachable. The registry path is canonicalized so the check also works through symlinked data directories.
- A failed exclusive metadata creation no longer removes a record that this call did not create.
- Trash / restore workers propagate cancellation from their final progress callback, rather than reporting success after cancellation was requested.
- Worker shutdown resets the owned path count after freeing it. Preview reindexing frees its previous offsets before replacing them.
- CLI parsing rejects multiple starting directories, including a second directory after `--`.
- Rich preview caching compares the device as well as the inode.
- Malformed image helper output remains an image error instead of being interpreted as archive text after a successful helper exit.
- PDF and archive helpers and the C reader retain one lookahead byte beyond the display limit. Oversized output is marked truncated; output of exactly 64 KiB followed by EOF is correctly marked complete.
- Panel formatting explicitly bounds path text. The terminal test driver waits for killed child processes during cleanup.

## Regression coverage

- `tests/test_app.c` exercises the actual application state, input routing, queue and workers without starting curses. It reproduces a file disappearing while a trash confirmation is open and verifies that the neighboring file survives. It also checks rename submission while a job is active.
- Filesystem tests verify that repeated text indexing preserves line contents.
- Feature tests verify registry protection and worker shutdown ownership cleanup.
- Terminal tests verify ambiguous CLI arguments are rejected.
- Image terminal tests inject malformed successful helper output and verify it is not displayed as archive text.
- A helper / terminal regression verifies the lookahead byte and truncation indicator for oversized output, and no false indicator for exact-size output.
- Existing terminal suites cover navigation, filtering, editing, filesystem operations, history, dashboard, trash restart recovery, disk usage, previews, graphics negotiation, resizing, and terminal restoration.

The Makefile includes the new application test in `make test` and `make check`, with header dependency tracking for its renamed-main object. Shared Python modules are imported without running the test suites as import side effects.

## Verification

| Check | Result |
| --- | --- |
| Baseline full suite | Passed before changes. |
| Strict optimized build and integration suites | Passed; warnings treated as errors. |
| Updated C suites | Passed, including 4,956 filesystem / preview / worker assertions and the application-state regressions. |
| Valgrind: filesystem, features, encoder, application state | Zero errors and zero definite, indirect, or possible leaks; feature / application runs retain small runtime environment allocations. |
| Full UndefinedBehaviorSanitizer run | Passed with fail-on-error instrumentation, including native / fallback image terminal tests. |
| Final preview-reader refinement | Oversized and exact-size output checks passed; focused image and boundary checks repeated with the reader instrumented. |
| GCC static analysis | Reviewed. One queue-ownership warning remains: allocated paths escape into `App.queue` and are freed by worker / queue cleanup. Application-state Valgrind coverage verifies the exercised ownership path; this is not a claim of a warning-free analyzer run. |
| AddressSanitizer | Local toolchain limitation recorded in `qa/VERIFICATION.md`; not claimed as passed. |

Image quality still depends on the terminal graphics protocol and the source image. The character-cell fallback remains inherently coarse; this review does not add zoom or a different graphics protocol.
