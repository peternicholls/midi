# Phase 3: MIDI Output Engine

**Depends on:** Phase 0, Phase 1, Phase 2

**Status:** Not started

## Objective

Share CoreMIDI output setup and raw MIDI byte sending between CLI playback and
TUI playback.

## Inputs

- `src/command_play.c`
- `src/command_tui.c`
- Phase 1 error and ownership conventions
- Phase 2 shared playback event module

## Tasks

- [ ] Compare CLI and TUI output-port creation and disposal.
- [ ] Compare raw MIDI packet construction and `MIDISend` usage.
- [ ] Design `midi_output.h` around explicit client/port ownership.
- [ ] Extract output open/close behavior into `midi_output.c`.
- [ ] Extract raw byte send behavior into `midi_output.c`.
- [ ] Keep endpoint selection in caller code unless a shared endpoint module is
  clearly useful.
- [ ] Update `command_play.c` to use shared MIDI output.
- [ ] Update `command_tui.c` to use shared MIDI output.
- [ ] Remove duplicated output helpers.
- [ ] Update `Makefile`.

## Suggested Files

- `src/midi_output.c`
- `src/midi_output.h`
- `src/command_play.c`
- `src/command_tui.c`
- `Makefile`

## Verification

- [ ] `make clean`
- [ ] `make`
- [ ] `make test`
- [ ] CLI playback sends MIDI to a selected destination.
- [ ] TUI playback sends MIDI to destination `0` as before.
- [ ] Disconnecting or losing a destination still produces a readable error.

## Exit Criteria

- CLI and TUI playback use the same raw MIDI send path.
- CoreMIDI output client and port lifetimes are obvious from the API.
- Output errors are returned to callers without CLI/TUI presentation leakage.

## Risks

- Packet buffer sizing needs to preserve current SysEx/raw message behavior.
- CoreMIDI APIs include deprecated entry points. Keep existing warning handling
  intentional and local.
