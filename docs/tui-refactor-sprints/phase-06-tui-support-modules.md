# Phase 6: TUI Support Modules

**Depends on:** Phase 0, Phase 1, Phase 5

**Status:** Complete

## Objective

Move non-rendering TUI support logic out of `command_tui.c` so the command file
can become a coordinator instead of a container for every helper.

## Inputs

- `src/command_tui.c`
- Existing `src/tui_model.c`
- Phase 1 ownership conventions

## Tasks

- [x] Extract the TUI log ring buffer:
  - struct ownership
  - mutex init/dispose
  - append
  - snapshot
- [x] Add tests for log capacity and wraparound.
- [x] Extract recordings file list scanning:
  - directory open/read
  - `.mid` filtering
  - path joining
  - sort order
  - selected-file preservation
  - free/dispose behavior
- [x] Migrate `tui_is_midi_filename` and `tui_join_path` from `tui_model.c`
  into `tui_files.c`; update all callers and migrate their tests.
- [x] Keep `tui_format_recording_name`, `tui_format_clock_time`, and
  `tui_format_midi_bytes` in `tui_model.c` during this phase. Revisit them only
  in Phase 8 or Phase 10 if all remaining callers move to renderer-specific row
  formatting.
- [x] Add tests for file-list behavior using a temporary directory.
- [x] Keep UI status text outside these modules unless it is returned as an
  error.
- [x] Update `command_tui.c` to use the new APIs.
- [x] Remove duplicated structs/helpers from `command_tui.c`.
- [x] Update `Makefile`.

## Suggested Files

- `src/tui_log.c`
- `src/tui_log.h`
- `src/tui_files.c`
- `src/tui_files.h`
- `src/tui_model.c` (for migrated function moves)
- `src/command_tui.c`
- `tests/test_tui_log.c` — required
- `tests/test_tui_files.c` — required
- `Makefile`

## Verification

- [x] `make clean`
- [x] `make`
- [x] `make test`
- [x] TUI still lists recordings.
- [x] TUI still appends and displays live stream rows.

Evidence:

- Added `src/tui_log.c` and `src/tui_log.h` to own the TUI log ring buffer,
  mutex lifecycle, append formatting, and snapshot behavior.
- Added `src/tui_files.c` and `src/tui_files.h` to own MIDI filename filtering,
  path joining, recordings directory scanning, descending sort order, selected
  file preservation, and file-list disposal.
- `command_tui.c` now calls `tui_log_*` and `tui_file_*` APIs instead of owning
  log-buffer or recordings-scan internals.
- `tui_model.c` now keeps only display formatters for recording names, clock
  times, and raw MIDI bytes.
- Added `tests/test_tui_log.c` for append, snapshot capacity, and wraparound.
- Added `tests/test_tui_files.c` for filename matching, path joining, temporary
  directory scanning, sort order, selection preservation, missing-directory
  behavior, and disposal reset.
- `make clean && make && make test` passed without warnings.
- A scripted TUI launch against `recordings` returned cleanly, and the captured
  alternate-screen output contained `20260512040548.mid` plus the live-stream
  monitor row (`MON source[0]`).

## Exit Criteria

- `command_tui.c` no longer owns log-buffer internals.
- `command_tui.c` no longer owns file-list scan internals.
- TUI support APIs have clear init/dispose/free rules.

## Risks

- Temporary-directory tests can become platform-sensitive. Keep them simple.
- Thread safety can regress if mutex ownership moves without careful review.
