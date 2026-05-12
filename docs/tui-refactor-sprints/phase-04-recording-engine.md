# Phase 4: Recording Engine

**Depends on:** Phase 0, Phase 1

**Status:** Not started

## Objective

Share MIDI packet-to-`MusicSequence` recording logic while preserving the
different CLI and TUI runtime loops.

## Inputs

- `src/command_record.c`
- `src/command_tui.c`
- `src/midi_parser.c`
- Phase 1 error and ownership conventions

## Tasks

- [ ] Compare CLI and TUI recorder state fields.
- [ ] Identify behavior that must remain caller-specific:
  - CLI duration loop
  - CLI status line
  - TUI pause/resume timing
  - TUI status/log updates
  - TUI file naming/session lifecycle
- [ ] Before writing `midi_recorder.h`, preserve the current thread-safety model
  for captured/ignored counters: use `_Atomic` counters from `<stdatomic.h>`
  with relaxed loads/stores for independent counts. CoreMIDI input callbacks run
  on a real-time thread, so do not introduce mutex locking into packet ingestion.
  Document this decision in the header before extraction begins.
- [ ] Design `midi_recorder.h` for sequence construction and packet ingestion.
- [ ] Extract recorder init/dispose behavior.
- [ ] Extract channel event insertion.
- [ ] Extract SysEx/raw event insertion.
- [ ] Extract packet-byte parsing into sequence events.
- [ ] Preserve captured and ignored message counts.
- [ ] Update `command_record.c` to use shared recorder logic.
- [ ] Update `command_tui.c` recording path to use shared recorder logic.
- [ ] Remove duplicated recorder helpers.
- [ ] Add tests for the synchronous parts of the recorder: init, direct event
  insertion, and counter increment. These paths do not require CoreMIDI hardware.
  Mark the async callback path as explicitly untested by automated tests.
- [ ] Update `Makefile`.

## Suggested Files

- `src/midi_recorder.c`
- `src/midi_recorder.h`
- `src/command_record.c`
- `src/command_tui.c`
- `tests/test_midi_recorder.c` — required for synchronous recorder logic
- `Makefile`

## Verification

- [ ] `make clean`
- [ ] `make`
- [ ] `make test`
- [ ] CLI recording creates a valid `.mid` file.
- [ ] TUI recording creates a timestamped `.mid` file.
- [ ] TUI pause/resume does not include paused time in event timing.

## Exit Criteria

- CLI and TUI share sequence event insertion logic.
- TUI-specific session timing remains visible in TUI code.
- Captured/ignored counters still work.
- Save-to-file behavior remains unchanged.

## Risks

- Pause accounting is easy to break. Keep it outside the generic recorder unless
  tests prove a better shared abstraction.
- Recorder callbacks run on a CoreMIDI real-time thread. Keep packet ingestion
  non-blocking; do not replace the existing atomic counters with mutex-protected
  counters without a measured reason.
