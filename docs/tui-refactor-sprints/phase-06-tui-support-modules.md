# Phase 6: TUI Support Modules

**Depends on:** Phase 0, Phase 1, Phase 5

**Status:** Not started

## Objective

Move non-rendering TUI support logic out of `command_tui.c` so the command file
can become a coordinator instead of a container for every helper.

## Inputs

- `src/command_tui.c`
- Existing `src/tui_model.c`
- Phase 1 ownership conventions

## Tasks

- [ ] Extract the TUI log ring buffer:
  - struct ownership
  - mutex init/dispose
  - append
  - snapshot
- [ ] Add tests for log capacity and wraparound.
- [ ] Extract recordings file list scanning:
  - directory open/read
  - `.mid` filtering
  - path joining
  - sort order
  - selected-file preservation
  - free/dispose behavior
- [ ] Migrate `tui_is_midi_filename` and `tui_join_path` from `tui_model.c`
  into `tui_files.c`; update all callers and migrate their tests.
- [ ] Keep `tui_format_recording_name`, `tui_format_clock_time`, and
  `tui_format_midi_bytes` in `tui_model.c` during this phase. Revisit them only
  in Phase 8 or Phase 10 if all remaining callers move to renderer-specific row
  formatting.
- [ ] Add tests for file-list behavior using a temporary directory.
- [ ] Keep UI status text outside these modules unless it is returned as an
  error.
- [ ] Update `command_tui.c` to use the new APIs.
- [ ] Remove duplicated structs/helpers from `command_tui.c`.
- [ ] Update `Makefile`.

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

- [ ] `make clean`
- [ ] `make`
- [ ] `make test`
- [ ] TUI still lists recordings.
- [ ] TUI still appends and displays live stream rows.

## Exit Criteria

- `command_tui.c` no longer owns log-buffer internals.
- `command_tui.c` no longer owns file-list scan internals.
- TUI support APIs have clear init/dispose/free rules.

## Risks

- Temporary-directory tests can become platform-sensitive. Keep them simple.
- Thread safety can regress if mutex ownership moves without careful review.
