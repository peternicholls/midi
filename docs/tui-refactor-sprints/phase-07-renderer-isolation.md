# Phase 7: Renderer Isolation

**Depends on:** Phase 5, Phase 6

**Status:** Complete

## Objective

Separate curses drawing from TUI state transitions before making visual design
changes.

## Inputs

- `src/command_tui.c`
- Phase 5 MIDI description model
- Phase 6 TUI support modules

## Tasks

- [x] Identify all curses calls currently in `command_tui.c`.
- [x] Define renderer input structs for:
  - app header/status
  - file browser rows
  - sequence/event rows
  - live stream rows
  - footer/status line
- [x] Move layout calculation into `tui_render.c`.
- [x] Move panel drawing into `tui_render.c`.
- [x] Move color-pair setup into renderer code.
- [x] Add monochrome fallback paths for terminals without color support.
- [x] Keep keyboard handling and state transitions in `command_tui.c`.
- [x] Update `command_tui.c` to call renderer APIs.
- [x] Update `Makefile`.

## Suggested Files

- `src/tui_render.c`
- `src/tui_render.h`
- `src/command_tui.c`
- `Makefile`

## Verification

- [x] `make clean`
- [x] `make`
- [x] `make test`
- [x] Launch TUI in a normal terminal.
- [x] Launch TUI with a small terminal and confirm minimum-size behavior.
- [x] Launch TUI in no-color or limited-color mode and record the terminal
  setting used for the check.

Evidence:

- Added `src/tui_render.c` and `src/tui_render.h` to own layout calculation,
  panel drawing, color-pair setup, and monochrome fallback behavior.
- `command_tui.c` now prepares a `TuiRenderState`, including endpoint labels,
  activity flags, file-list reference, log snapshot, footer text, and a sequence
  row provider. It still owns curses lifecycle, keyboard handling, prompt input,
  and state transitions.
- Sequence rows are prepared through a coordinator-side provider that formats
  clock text, raw MIDI bytes, and MIDI descriptions before handing display rows
  to the renderer.
- `make clean && make && make test` passed without warnings.
- Scripted normal TUI capture with `TERM=xterm` showed
  `20260512040548.mid` and the live-stream monitor row (`MON source[0]`).
- Scripted small-terminal capture with `stty rows 10 cols 40` showed the
  minimum-size message: `Resize terminal to at least 90x20 for TUI mode.`
- Scripted limited-color/no-color fallback check with `TERM=vt100` launched and
  showed `20260512040548.mid`.

## Exit Criteria

- `command_tui.c` does not directly draw panels.
- Renderer consumes prepared display data rather than re-parsing MIDI.
- The current UI still works before the redesign phase starts.

## Risks

- Renderer isolation can accidentally become a redesign. Keep visual changes
  minimal in this phase.
- Curses state is global. Keep renderer APIs simple and avoid hiding too much
  lifecycle behavior.
