# Phase 4: Recording Engine

**Depends on:** Phase 0, Phase 1

**Status:** In progress

## Objective

Share MIDI packet-to-`MusicSequence` recording logic while preserving the
different CLI and TUI runtime loops.

## Inputs

- `src/command_record.c`
- `src/command_tui.c`
- `src/midi_parser.c`
- Phase 1 error and ownership conventions

## Tasks

- [x] Compare CLI and TUI recorder state fields.
- [x] Identify behavior that must remain caller-specific:
  - CLI duration loop
  - CLI status line
  - TUI pause/resume timing
  - TUI status/log updates
  - TUI file naming/session lifecycle
- [x] Before writing `midi_recorder.h`, preserve the current thread-safety model
  for captured/ignored counters: use `_Atomic` counters from `<stdatomic.h>`
  with relaxed loads/stores for independent counts. CoreMIDI input callbacks run
  on a real-time thread, so do not introduce mutex locking into packet ingestion.
  Document this decision in the header before extraction begins.
- [x] Design `midi_recorder.h` for sequence construction and packet ingestion.
- [x] Extract recorder init/dispose behavior.
- [x] Extract channel event insertion.
- [x] Extract SysEx/raw event insertion.
- [x] Extract packet-byte parsing into sequence events.
- [x] Preserve captured and ignored message counts.
- [x] Update `command_record.c` to use shared recorder logic.
- [x] Update `command_tui.c` recording path to use shared recorder logic.
- [x] Remove duplicated recorder helpers.
- [x] Add tests for the synchronous parts of the recorder: init, direct event
  insertion, and counter increment. These paths do not require CoreMIDI hardware.
  Mark the async callback path as explicitly untested by automated tests.
- [x] Update `Makefile`.

## Suggested Files

- `src/midi_recorder.c`
- `src/midi_recorder.h`
- `src/command_record.c`
- `src/command_tui.c`
- `tests/test_midi_recorder.c` — required for synchronous recorder logic
- `Makefile`

## Verification

- [x] `make clean`
- [x] `make`
- [x] `make test`
- [ ] CLI recording creates a valid `.mid` file.
- [x] TUI recording creates a timestamped `.mid` file.
- [ ] TUI pause/resume does not include paused time in event timing.

Evidence:

- Added `src/midi_recorder.c` and `src/midi_recorder.h` to own shared sequence
  construction, packet parsing, and atomic captured/ignored counters.
- `midi_recorder.h` records the Phase 4 thread-safety rule: recorder counters
  stay lock-free with relaxed atomics because CoreMIDI read callbacks can run on
  a real-time thread.
- `command_record.c` and `command_tui.c` now route recording event insertion
  through `midi_recorder_record_packet_bytes`, while each caller still owns its
  own elapsed-time policy and UI/runtime behavior.
- Added `tests/test_midi_recorder.c` covering recorder init, direct channel
  insertion, and supported-vs-ignored packet counting.
- `make clean && make` passed after the extraction.
- `make midi-capture test_midi_recorder && ./test_midi_recorder` passed.
- `make test` passed after the extraction.
- Scripted TUI smoke in a 120x30 terminal started recording, toggled pause and
  resume, stopped, and quit cleanly. It created
  `/tmp/tui-smoke-recordings/20260514000244.mid`, and the terminal returned to a
  normal shell prompt afterward.
- Automated event-driven recording verification is still pending for CLI
  recording and TUI pause/resume timing. This machine exposes a real source and
  destination at index `0` (`KeyLab 88`), while the TUI is fixed to those index
  `0` endpoints. A temporary virtual CoreMIDI source/destination pair was
  appended at index `1`, which let the CLI target it explicitly but could not be
  used by the current TUI. A temporary software loopback probe also confirmed
  that sending to destination `0` does not feed source `0`, so meaningful
  event-timing verification still requires manual hardware input on source `0`
  or a future TUI source-selection capability.

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
