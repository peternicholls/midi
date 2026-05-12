# Phase 5: MIDI Description Model

**Depends on:** Phase 0, Phase 1, Phase 2 (event row struct shape)

**Status:** Not started

## Objective

Add a shared MIDI description model so the TUI can show English descriptions and
semantic colors without embedding MIDI interpretation in rendering code.

## Inputs

- `src/midi_parser.c`
- `src/midi_parser.h`
- Phase 2 playback event model
- Future TUI renderer row needs

## Tasks

- [ ] Define event categories needed by the UI:
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
- [ ] Design a compact description result struct with category and text.
- [ ] Implement note-name formatting with octave labels.
- [ ] Treat note-on velocity `0` as note off if that matches MIDI semantics used
  by the project.
- [ ] Describe common channel messages in plain English.
- [ ] Describe SysEx and unsupported messages without over-parsing.
- [ ] Add unit tests for all supported categories.
- [ ] Update `Makefile` for new tests.

## Suggested Files

- `src/midi_describe.c`
- `src/midi_describe.h`
- `tests/test_midi_describe.c`
- `Makefile`

## Verification

- [ ] `make clean`
- [ ] `make`
- [ ] `make test`
- [ ] Unit tests prove note on and note off produce different categories and
  descriptions.

## Exit Criteria

- MIDI descriptions are available without curses or TUI dependencies.
- Renderer-facing rows can use category and description directly.
- Raw byte formatting remains separate from semantic description.

## Risks

- Overly verbose descriptions can make the TUI harder to scan. Keep wording
  short and consistent.
- MIDI note octave naming conventions vary. Pick one convention and test it.
