# Planning Docs Review

Review of `docs/tui-refactor-plan.md`, `docs/tui-refactor-sprints.md`, and
`docs/tui-refactor-sprints/` for completeness, gaps, issues, and improvements.
Conducted against the current source tree on 2026-05-12.

## Evidence Checked

- `src/command_tui.c` is the main pressure point at 1927 lines and currently
  owns log buffering, file scanning, playback event extraction, MIDI output,
  recording, prompt flow, keyboard handling, and curses rendering.
- `src/command_play.c` and `src/command_tui.c` both define playback event lists,
  sequence iteration, sorting, and raw MIDI send paths.
- `src/command_record.c` and `src/command_tui.c` both convert packet bytes into
  `MusicSequence` events and already use `_Atomic` counters for callback-visible
  captured/ignored counts.
- `src/tui_model.c` and `src/status_line.c` already exist, are small, and have
  tests. They need explicit migration decisions so they are not duplicated or
  accidentally deleted.
- The current `Makefile` has a simple explicit pattern: `APP_SRCS`, `TESTS`,
  one rule per test binary, and the `test` target running each binary.

## Issues Found And Fixed In The Phase Plans

### Existing modules needed explicit ownership

The original plan did not clearly say what happens to `tui_model.c/.h` or
`status_line.c/.h`.

Fix applied: `docs/tui-refactor-plan.md` now records the fate of `tui_model`
helpers and keeps `status_line.*` distinct from the future `tui_log.*` module
until the Phase 10 architecture pass.

### Phase 1 needed a concrete output

Leaving the shared error convention as a vague note would let later phases
invent incompatible result styles.

Fix applied: Phase 1 now requires `src/midi_result.h` before Phase 2 begins.
That keeps shared modules from printing directly or depending on TUI status/log
functions.

### Phase 2 and Phase 5 needed a data contract

Playback extraction and MIDI description are coupled through renderer rows. If
Phase 2 emits only UI-ready text, Phase 5 cannot stay reusable; if Phase 5 owns
sequence loading, the boundaries blur.

Fix applied: Phase 2 now defines the shared event shape as timestamp plus raw
MIDI bytes. Phase 5 consumes those bytes to produce category and description.
Renderer rows carry both without re-parsing.

### Recorder thread-safety was too open

CoreMIDI callbacks should avoid blocking. The current code already uses
`_Atomic` counters in both CLI and TUI recording paths.

Fix applied: Phase 4 now chooses `_Atomic` counters with relaxed independent
loads/stores and explicitly rejects introducing mutex locking into packet
ingestion without measured evidence.

### Test language had escape hatches

Optional-test wording made it too easy to extract the riskiest code with no new
tests.

Fix applied: Phase 2 requires event ordering and duration tests, Phase 4
requires synchronous recorder tests, and Phase 6 requires log/file-list tests.
Phase 0 now prefers generated or in-memory MIDI inputs over opaque checked-in
binary fixtures.

### Phase status was not trackable

Unchecked task lists alone do not communicate whether a phase is not started,
active, blocked, or complete.

Fix applied: the sprint index now defines status values, and every phase file
has a `Status` line. A dependent phase should not start until prerequisite
phases are `Complete` unless the dependency line says otherwise.

### Verification missed failure-mode coverage

Final verification covered happy-path MIDI device usage but not the common
absence of MIDI sources or destinations.

Fix applied: Phase 11 now includes explicit no-device degradation checks for CLI
and TUI paths.

### Phase 9 dependency was incomplete

The directory browser relies on Phase 6 file-list patterns as well as renderer
isolation and the visual redesign.

Fix applied: Phase 9 now depends on Phase 6, Phase 7, and Phase 8.

## Remaining Open Questions

These are intentionally left to their implementation phases because they depend
on the exact API shape discovered while moving code:

- The exact fields in `midi_result.h`; Phase 1 should keep it minimal and avoid
  broad error hierarchies.
- Whether playback tests use in-memory `MusicSequence` construction only, or one
  tiny checked-in `.mid` file to cover file loading.
- The exact renderer input structs for Phase 7; they should be based on the
  Phase 2 event model and Phase 5 description result.
- The specific key binding for entering manual path mode in the Phase 9
  directory browser.

## Priority Before Implementation

1. Complete Phase 0 and record baseline verification evidence.
2. Complete Phase 1 and commit the `midi_result.h` convention.
3. Start Phase 2 only after the event struct contract is written down in
   `midi_sequence.h`.
