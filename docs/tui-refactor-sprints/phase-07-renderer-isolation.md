# Phase 7: Renderer Isolation

**Depends on:** Phase 5, Phase 6

**Status:** Not started

## Objective

Separate curses drawing from TUI state transitions before making visual design
changes.

## Inputs

- `src/command_tui.c`
- Phase 5 MIDI description model
- Phase 6 TUI support modules

## Tasks

- [ ] Identify all curses calls currently in `command_tui.c`.
- [ ] Define renderer input structs for:
  - app header/status
  - file browser rows
  - sequence/event rows
  - live stream rows
  - footer/status line
- [ ] Move layout calculation into `tui_render.c`.
- [ ] Move panel drawing into `tui_render.c`.
- [ ] Move color-pair setup into renderer code.
- [ ] Add monochrome fallback paths for terminals without color support.
- [ ] Keep keyboard handling and state transitions in `command_tui.c`.
- [ ] Update `command_tui.c` to call renderer APIs.
- [ ] Update `Makefile`.

## Suggested Files

- `src/tui_render.c`
- `src/tui_render.h`
- `src/command_tui.c`
- `Makefile`

## Verification

- [ ] `make clean`
- [ ] `make`
- [ ] `make test`
- [ ] Launch TUI in a normal terminal.
- [ ] Launch TUI with a small terminal and confirm minimum-size behavior.
- [ ] Launch TUI in no-color or limited-color mode and record the terminal
  setting used for the check.

## Exit Criteria

- `command_tui.c` does not directly draw panels.
- Renderer consumes prepared display data rather than re-parsing MIDI.
- The current UI still works before the redesign phase starts.

## Risks

- Renderer isolation can accidentally become a redesign. Keep visual changes
  minimal in this phase.
- Curses state is global. Keep renderer APIs simple and avoid hiding too much
  lifecycle behavior.
