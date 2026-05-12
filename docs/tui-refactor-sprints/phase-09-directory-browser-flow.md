# Phase 9: Directory Browser Flow

**Depends on:** Phase 6, Phase 7, Phase 8

**Status:** Not started

## Objective

Replace the raw output-directory prompt with a browser-like flow that helps the
user choose a valid recordings directory.

## Inputs

- `src/command_tui.c`
- `src/tui_render.c`
- Phase 6 file-list patterns
- Phase 7 renderer isolation
- Phase 8 target layout constraints

## Tasks

- [ ] Define directory browser state:
  - current directory
  - parent entry
  - child directories
  - selected row
  - selected destination
  - manual path input mode
- [ ] Implement directory scanning for child directories.
- [ ] Add navigation:
  - arrows move selection
  - enter opens or confirms
  - escape cancels
  - a key enters manual path mode
- [ ] Show validation feedback for missing, non-directory, or inaccessible paths.
- [ ] Preserve ability to create a recordings directory when appropriate.
- [ ] Integrate browser state into the TUI event loop.
- [ ] Render the browser as an in-TUI mode or overlay without losing context.
- [ ] Apply the selected directory by rescanning recordings and reloading the
  selected file.
- [ ] Update docs/README if key bindings or behavior change.

## Suggested Files

- `src/tui_directory_browser.c`
- `src/tui_directory_browser.h`
- `src/tui_render.c`
- `src/tui_render.h`
- `src/command_tui.c`
- `Makefile`

## Verification

- [ ] `make clean`
- [ ] `make`
- [ ] `make test`
- [ ] Open directory browser from TUI.
- [ ] Navigate into a child directory.
- [ ] Navigate to parent directory.
- [ ] Confirm a valid directory.
- [ ] Cancel without changing directory.
- [ ] Enter a manual path.
- [ ] Try an invalid path and confirm readable status feedback.

## Exit Criteria

- Users no longer need to guess what to type for output directory changes.
- Existing `./midi-capture tui <recordings-dir>` startup behavior still works.
- Directory changes trigger file list refresh and selected-file reload.

## Risks

- Directory browsing can expand into full file-manager behavior. Keep scope to
  choosing the recordings destination.
- Permission errors must be handled cleanly, especially under protected macOS
  directories.
