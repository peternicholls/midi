# Phase 2: Playback Event Engine

**Depends on:** Phase 0, Phase 1

**Status:** Complete

## Objective

Remove duplicated playback event collection from `command_play.c` and
`command_tui.c` by extracting a shared sequence/playback event module.

## Inputs

- `src/command_play.c`
- `src/command_tui.c`
- `src/midi_parser.c`
- Phase 1 error and ownership conventions

## Tasks

- [x] Map duplicate playback code in CLI and TUI:
  - playback event struct/list
  - append/realloc/free behavior
  - event sorting
  - sequence loading
  - track iteration
  - beat-to-second conversion
  - total duration calculation
- [x] Before writing `midi_sequence.h`, agree the event row struct shape with
  the Phase 5 MIDI description model design. The contract: `midi_sequence`
  events carry timestamps and raw MIDI bytes; `midi_describe` (Phase 5) reads
  those bytes and produces a category and text description; the renderer rows
  in Phases 7–8 carry both. Define the struct in this phase so Phase 5 can
  extend it without a breaking change.
- [x] Design `midi_sequence.h` with a narrow public API.
- [x] Move playback event list ownership into `midi_sequence.c`.
- [x] Move sequence loading and event collection into `midi_sequence.c`.
- [x] Keep caller presentation separate:
  - CLI prints errors
  - TUI maps errors to status/log text
- [x] Update `command_play.c` to use the shared module.
- [x] Update `command_tui.c` playback loading to use the shared module.
- [x] Remove duplicated playback helpers after both callers compile.
- [x] Add tests for event ordering and duration calculation. Use fixtures from
  `tests/fixtures/` or generate a minimal in-memory sequence; do not skip this
  step. At minimum: assert two events sort correctly and total duration matches
  expected beats-to-seconds conversion.
- [x] Update `Makefile` for new sources and tests.

## Suggested Files

- `src/midi_sequence.c`
- `src/midi_sequence.h`
- `src/command_play.c`
- `src/command_tui.c`
- `tests/test_midi_sequence.c` — required
- `Makefile`

## Verification

- [x] `make clean`
- [x] `make`
- [x] `make test`
- [x] `./midi-capture play <fixture.mid> <destination-index>`
- [x] TUI can load a selected `.mid` file and show its events.

Evidence:

- Added `src/midi_sequence.c` and `src/midi_sequence.h`.
- `MidiSequenceEvent` carries seconds plus raw MIDI bytes; Phase 5 can add
  semantic descriptions by reading those bytes without changing playback
  collection.
- Added `tests/test_midi_sequence.c` with generated in-memory sequences for
  event sorting, note-on/note-off expansion, raw event byte ownership, and
  total duration coverage.
- `command_play.c` and `command_tui.c` now call
  `midi_sequence_load_events_from_file` instead of owning playback event
  extraction.
- `make clean && make && make test` passed after the extraction.
- `./midi-capture play recordings/20260512040548.mid 0` completed against
  destination `[0] KeyLab 88`.
- Scripted TUI smoke at 120x30 loaded `20260512040548.mid`, showed the Sequence
  panel with event count, connected `MON source[0] KeyLab 88`, and quit cleanly.

## Exit Criteria

- `command_play.c` no longer owns playback event collection internals.
- `command_tui.c` no longer duplicates playback event extraction.
- Event ordering remains stable.
- Playback output behavior is unchanged.

## Risks

- AudioToolbox event iteration has multiple event types. Preserve all currently
  supported types before deleting old helpers.
- Memory ownership regressions are likely during this phase. Review free/dispose
  paths carefully.
