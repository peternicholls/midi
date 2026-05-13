# Phase 5: MIDI Description Model

**Depends on:** Phase 0, Phase 1, Phase 2 (event row struct shape)

**Status:** Complete

## Objective

Add a shared MIDI description model so the TUI can show English descriptions and
semantic colors without embedding MIDI interpretation in rendering code.

## Inputs

- `src/midi_parser.c`
- `src/midi_parser.h`
- Phase 2 playback event model
- Future TUI renderer row needs

## Tasks

- [x] Define event categories needed by the UI:
  - note on
  - note off
  - control change
  - program change
  - pitch bend
  - channel pressure
  - poly pressure
  - SysEx
  - unsupported
  - incomplete
- [x] Design a compact description result struct with category and text.
- [x] Implement note-name formatting with octave labels.
- [x] Treat note-on velocity `0` as note off if that matches MIDI semantics used
  by the project.
- [x] Describe common channel messages in plain English.
- [x] Describe SysEx and unsupported messages without over-parsing.
- [x] Add unit tests for all supported categories.
- [x] Update `Makefile` for new tests.

## Suggested Files

- `src/midi_describe.c`
- `src/midi_describe.h`
- `tests/test_midi_describe.c`
- `Makefile`

## Verification

- [x] `make clean`
- [x] `make`
- [x] `make test`
- [x] Unit tests prove note on and note off produce different categories and
  descriptions.

Evidence:

- Added `src/midi_describe.c` and `src/midi_describe.h` with a renderer-neutral
  `MidiDescription` result: semantic category plus compact display text.
- The description model covers note on, note off, control change, program
  change, pitch bend, channel pressure, poly pressure, SysEx, unsupported, and
  incomplete messages.
- Note names use scientific pitch octave labels where MIDI note `60` is `C4`;
  note-on messages with velocity `0` are described and categorized as note off.
- `command_tui.c` now uses the shared description model for monitor/playback log
  text and sequence rows, while `tui_format_midi_bytes` remains responsible only
  for raw byte formatting.
- Sequence rows use `MidiDescriptionCategory` to select semantic curses colors
  without embedding MIDI interpretation in the renderer.
- Added `tests/test_midi_describe.c` covering all Phase 5 categories and note
  naming.
- `make clean && make && make test` passed.

## Exit Criteria

- MIDI descriptions are available without curses or TUI dependencies.
- Renderer-facing rows can use category and description directly.
- Raw byte formatting remains separate from semantic description.

## Risks

- Overly verbose descriptions can make the TUI harder to scan. Keep wording
  short and consistent.
- MIDI note octave naming conventions vary. Pick one convention and test it.
