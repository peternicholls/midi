# Phase 8: TUI Visual And UX Redesign

## Objective

Make the TUI clearer, more useful, and more pleasant while preserving utility
density.

## Inputs

- `src/tui_render.c`
- `src/tui_render.h`
- Phase 5 MIDI description categories
- Phase 7 renderer isolation

## Tasks

- [ ] Define target layout at common terminal sizes:
  - minimum supported size
  - comfortable default size
  - wide terminal behavior
- [ ] Place the file browser as a persistent side column.
- [ ] Make the live stream a taller primary monitoring area.
- [ ] Add tabular columns for event rows:
  - time
  - direction
  - bytes
  - description
- [ ] Add semantic color pairs:
  - note on
  - note off
  - control/program/pitch messages
  - SysEx
  - unsupported/incomplete
  - selected row
  - active recording/playback
- [ ] Keep raw bytes visible but visually secondary.
- [ ] Improve active transport status readability.
- [ ] Ensure text clipping does not corrupt columns.
- [ ] Preserve keyboard flows from the current TUI.
- [ ] Manually test with representative live MIDI input.

## Suggested Files

- `src/tui_render.c`
- `src/tui_render.h`
- `src/command_tui.c`
- possible `src/tui_model.c`

## Verification

- [ ] `make clean`
- [ ] `make`
- [ ] `make test`
- [ ] Manual TUI check at minimum terminal size.
- [ ] Manual TUI check at a comfortable desktop terminal size.
- [ ] Note on and note off are visually distinct.
- [ ] Monochrome fallback is still readable.
- [ ] Recording, playback, navigation, and live monitoring still work.

## Exit Criteria

- The TUI has a side file browser.
- The live stream uses meaningful vertical space.
- Event rows include a human-readable description.
- Color communicates event type and transport state without reducing readability.

## Risks

- Visual polish can sprawl. Keep the work focused on scannability and utility.
- Terminals vary in color and glyph support. Prefer plain ASCII and conservative
  color usage.
