# Phase 3: MIDI Output Engine

**Depends on:** Phase 0, Phase 1, Phase 2

**Status:** Complete

## Objective

Share CoreMIDI output setup and raw MIDI byte sending between CLI playback and
TUI playback.

## Inputs

- `src/command_play.c`
- `src/command_tui.c`
- Phase 1 error and ownership conventions
- Phase 2 shared playback event module

## Tasks

- [x] Compare CLI and TUI output-port creation and disposal.
- [x] Compare raw MIDI packet construction and `MIDISend` usage.
- [x] Design `midi_output.h` around explicit client/port ownership.
- [x] Extract output open/close behavior into `midi_output.c`.
- [x] Extract raw byte send behavior into `midi_output.c`.
- [x] Keep endpoint selection in caller code unless a shared endpoint module is
  clearly useful.
- [x] Update `command_play.c` to use shared MIDI output.
- [x] Update `command_tui.c` to use shared MIDI output.
- [x] Remove duplicated output helpers.
- [x] Update `Makefile`.

## Suggested Files

- `src/midi_output.c`
- `src/midi_output.h`
- `src/command_play.c`
- `src/command_tui.c`
- `Makefile`

## Verification

- [x] `make clean`
- [x] `make`
- [x] `make test`
- [x] CLI playback sends MIDI to a selected destination.
- [x] TUI playback sends MIDI to destination `0` as before.
- [x] Disconnecting or losing a destination still produces a readable error.

Evidence:

- Added `src/midi_output.c` and `src/midi_output.h`.
- `MidiOutput` owns the CoreMIDI client and output port; callers select
  endpoints and translate `MidiResult` into their own presentation.
- `command_play.c` and `command_tui.c` now share `midi_output_open`,
  `midi_output_close`, and `midi_output_send`.
- `make clean && make && make test` passed.
- `./midi play recordings/20260512040548.mid 0` completed against
  destination `[0] KeyLab 88`.
- TUI smoke showed destination `[0] KeyLab 88` and still loaded the selected
  recording.
- Destination loss remains caller-readable: `playback_tick` still checks
  `get_destination_by_index` before sending and reports `Destination [0] is not
  available`; send failures are mapped through `set_midi_result_error`.

## Exit Criteria

- CLI and TUI playback use the same raw MIDI send path.
- CoreMIDI output client and port lifetimes are obvious from the API.
- Output errors are returned to callers without CLI/TUI presentation leakage.

## Risks

- Packet buffer sizing needs to preserve current SysEx/raw message behavior.
- CoreMIDI APIs include deprecated entry points. Keep existing warning handling
  intentional and local.
