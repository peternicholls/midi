# TUI Refactor Sprint Plan

This document breaks `docs/tui-refactor-plan.md` into reviewable delivery
phases. A sprint here means a coherent implementation slice, not a fixed
calendar duration. Each sprint should leave the project buildable, tested, and
usable.

## Delivery Principles

- Keep behavior stable while moving code.
- Prefer shared MIDI/domain modules over command-local duplication.
- Keep source diffs small enough to review.
- Do not add dependencies unless explicitly approved.
- Run `make test` after each sprint.
- For CoreMIDI changes, also perform a macOS manual smoke test with real or
  virtual MIDI endpoints.
- Treat TUI visual work as product work: utility-first, dense, clear, and
  pleasant.
- Track status in the phase file before starting work. A phase is not complete
  until its verification section records evidence, not just checked boxes.
- Prefer generated or source-readable test inputs over opaque binary fixtures.
  Add checked-in `.mid` fixtures only when file loading itself is the behavior
  under test.

## Phase Task Files

- [Phase 0: Baseline And Safety Net](tui-refactor-sprints/phase-00-baseline-and-safety-net.md)
- [Phase 1: Shared Error And Ownership Conventions](tui-refactor-sprints/phase-01-shared-error-and-ownership.md)
- [Phase 2: Playback Event Engine](tui-refactor-sprints/phase-02-playback-event-engine.md)
- [Phase 3: MIDI Output Engine](tui-refactor-sprints/phase-03-midi-output-engine.md)
- [Phase 4: Recording Engine](tui-refactor-sprints/phase-04-recording-engine.md)
- [Phase 5: MIDI Description Model](tui-refactor-sprints/phase-05-midi-description-model.md)
- [Phase 6: TUI Support Modules](tui-refactor-sprints/phase-06-tui-support-modules.md)
- [Phase 7: Renderer Isolation](tui-refactor-sprints/phase-07-renderer-isolation.md)
- [Phase 8: TUI Visual And UX Redesign](tui-refactor-sprints/phase-08-tui-visual-ux-redesign.md)
- [Phase 8 Delivery Plan: TUI UX Redesign](tui-refactor-sprints/phase-08-delivery-plan.md)
- [Phase 8 Remediation Plan: Close Post-Implementation Gaps](tui-refactor-sprints/phase-08-remediation-plan.md)
- [Phase 9: Directory Browser Flow](tui-refactor-sprints/phase-09-directory-browser-flow.md)
- [Phase 10: Command Cleanup And Architecture Pass](tui-refactor-sprints/phase-10-command-cleanup-architecture.md)
- [Phase 11: Final Verification And Release Readiness](tui-refactor-sprints/phase-11-final-verification-release.md)

## Progress Tracking

Each phase file has a `Status` line. Keep it current:

- `Not started`: planning is present, no implementation work has begun.
- `In progress`: implementation has started, and unchecked tasks remain.
- `Blocked`: progress requires a specific decision, dependency, or environment.
- `Complete`: exit criteria and verification evidence are recorded.

Do not start a dependent phase until prerequisite phases are `Complete`, unless
the dependency line explicitly allows parallel work.

## Phase 0: Baseline And Safety Net

Goal: establish current behavior and add tests around low-risk pure logic before
moving implementation.

Deliverables:

- Document the current CLI and TUI behavior that must not regress.
- Add or extend tests for path formatting, clock formatting, MIDI filename
  filtering, and any pure event-description helpers introduced in later sprints.
- Decide the playback-event test input strategy. Default to generated or
  in-memory sequences; add small checked-in `.mid` fixtures only when file-load
  behavior must be covered.
- Capture a short manual smoke-test checklist for `list`, `record`, `play`, and
  `tui`.

Acceptance criteria:

- `make test` passes.
- The smoke-test checklist is specific enough for another developer to run.
- The fixture strategy is documented before Phase 2 starts.
- No architecture moves happen before the protected behavior is clear.

## Phase 1: Shared Error And Ownership Conventions

Goal: prevent each extracted module from inventing its own reporting and cleanup
style.

Deliverables:

- Define a lightweight shared result/error style for reusable modules.
- Decide how shared modules report OSStatus, allocation failures, and invalid
  input without printing directly.
- Establish ownership naming conventions for init/dispose/open/close/free APIs.

Acceptance criteria:

- `src/midi_result.h` exists with the agreed result type; conventions are
  documented in the header or an accompanying docs note.
- CLI callers can print errors.
- TUI callers can convert the same errors into status text and log entries.

## Phase 2: Playback Event Engine

Goal: remove duplicated playback event collection from `command_play.c` and
`command_tui.c`.

Likely files:

- `src/midi_sequence.c`
- `src/midi_sequence.h`
- `src/command_play.c`
- `src/command_tui.c`
- `Makefile`
- `tests/test_midi_sequence.c` — required; event ordering and duration tests

Deliverables:

- Extract playback event list ownership, event appending, sorting, sequence
  loading, sequence length, and event collection.
- Update CLI playback and TUI playback loading to use the same event engine.
- Keep UI-specific status/log formatting outside the shared module.

Acceptance criteria:

- `command_play.c` no longer owns its own playback event list implementation.
- `command_tui.c` no longer duplicates sequence event extraction.
- `make test` passes.
- `./midi-capture play <fixture.mid>` still plays through the selected
  destination in manual testing.

## Phase 3: MIDI Output Engine

Goal: share CoreMIDI output setup and raw MIDI byte sending.

Likely files:

- `src/midi_output.c`
- `src/midi_output.h`
- `src/command_play.c`
- `src/command_tui.c`
- `Makefile`

Deliverables:

- Extract output client/port open and dispose behavior.
- Extract raw MIDI byte send behavior.
- Preserve clear ownership of CoreMIDI clients and ports.

Acceptance criteria:

- CLI and TUI playback use the same send path.
- Output errors are returned to callers rather than printed inside the shared
  module.
- `make test` passes.
- Manual playback smoke test confirms MIDI bytes still reach the destination.

## Phase 4: Recording Engine

Goal: share packet-to-`MusicSequence` recording logic while preserving different
CLI and TUI runtime behavior.

Likely files:

- `src/midi_recorder.c`
- `src/midi_recorder.h`
- `src/command_record.c`
- `src/command_tui.c`
- `Makefile`
- `tests/test_midi_recorder.c` — required; synchronous recorder logic
  (async callback path noted as explicitly not covered)

Deliverables:

- Extract recorder state initialization and disposal.
- Extract channel/SysEx event insertion.
- Extract packet-byte parsing into sequence events.
- Preserve the existing `_Atomic` captured/ignored counter behavior so the
  CoreMIDI callback path stays non-blocking.
- Keep CLI duration loop and TUI pause/resume/session behavior in their command
  layers.

Acceptance criteria:

- `command_record.c` and `command_tui.c` share recorder sequence construction.
- TUI pause/resume timing remains explicit and reviewable.
- `make test` passes.
- Manual recording smoke test creates a valid `.mid` file.

## Phase 5: MIDI Description Model

Goal: add human-readable MIDI descriptions without coupling the renderer to MIDI
parsing rules.

Likely files:

- `src/midi_describe.c`
- `src/midi_describe.h`
- tests for note on/off, control change, program change, pitch bend, channel
  pressure, poly pressure, SysEx, unsupported, and incomplete messages

Deliverables:

- Introduce event categories for color and layout decisions.
- Generate descriptions such as `Note On C4 velocity 96 channel 1`.
- Reuse the model for loaded sequence rows and live stream rows.

Acceptance criteria:

- Description generation is unit tested.
- Note on and note off are distinguishable at the data-model level.
- Renderer-facing rows can carry category, timestamp, bytes, direction, and
  description without re-parsing raw MIDI.

## Phase 6: TUI Support Modules

Goal: split non-rendering TUI support code into focused modules.

Likely files:

- `src/tui_log.c`
- `src/tui_log.h`
- `src/tui_files.c`
- `src/tui_files.h`
- `src/command_tui.c`
- `Makefile`
- focused tests where behavior is pure

Deliverables:

- Extract log ring buffer and snapshot behavior.
- Extract recordings directory scan, sort, and selection preservation.
- Add focused tests for log wraparound and file-list behavior with a temporary
  directory.
- Keep allocation and disposal APIs obvious.

Acceptance criteria:

- `command_tui.c` no longer owns log-buffer internals.
- `command_tui.c` no longer owns file-list scan internals.
- `make test` passes.
- TUI still lists recordings and appends live log rows in manual testing.

## Phase 7: Renderer Isolation

Goal: separate curses rendering from TUI state transitions before the visual
redesign.

Likely files:

- `src/tui_render.c`
- `src/tui_render.h`
- `src/command_tui.c`
- `Makefile`

Deliverables:

- Move layout calculation and drawing functions out of `command_tui.c`.
- Feed the renderer data rows rather than making it own MIDI behavior.
- Add explicit color-pair setup with monochrome fallback.

Acceptance criteria:

- `command_tui.c` calls the renderer instead of drawing panels directly.
- The app remains usable in terminals without color support.
- `make test` passes.
- Manual TUI smoke test confirms no obvious rendering regressions.

## Phase 8: TUI Visual And UX Redesign

Goal: make the TUI clearer, more useful, and more pleasant while preserving
utility density.

Likely files:

- `src/tui_render.c`
- `src/tui_render.h`
- `src/command_tui.c`
- possibly `src/tui_model.c`

Deliverables:

- Side file browser that remains visible.
- Main event table with columns for time, direction, bytes, and description.
- Taller live stream area that uses more vertical space.
- Semantic colors for note on, note off, control/program/pitch messages, SysEx,
  unsupported messages, selected rows, and active transport state.
- Minimum-size behavior for small terminals.

Acceptance criteria:

- Note on and note off are visually distinct.
- The live stream feels like a primary monitoring surface.
- Raw bytes remain visible but no longer dominate the interface.
- Monochrome fallback remains readable.
- Manual TUI smoke test confirms recording, playback, navigation, and live
  monitoring still work.

## Phase 9: Directory Browser Flow

Goal: replace the raw output-directory prompt with a clearer browser-like flow.

Likely files:

- `src/tui_directory_browser.c`
- `src/tui_directory_browser.h`
- `src/command_tui.c`
- `src/tui_render.c`
- `Makefile`

Deliverables:

- Show current directory, parent entry, child directories, and selected
  destination.
- Support arrow navigation, enter to confirm, escape to cancel.
- Keep manual path entry as an advanced escape hatch.
- Validate that the chosen destination exists or can be created.

Acceptance criteria:

- Users can change output directory without guessing path syntax.
- Invalid or inaccessible directories produce clear status text.
- Existing `tui <recordings-dir>` startup behavior still works.
- `make test` passes.

## Phase 10: Command Cleanup And Architecture Pass

Goal: finish the architectural cleanup after shared modules exist.

Likely files:

- `src/command_play.c`
- `src/command_record.c`
- `src/command_tui.c`
- `src/command_list.c`
- shared module headers

Deliverables:

- Thin command files so they mostly parse arguments, call shared behavior, and
  present results.
- Remove dead helper functions and duplicated logic.
- Tighten headers so implementation details stay private.
- Update README/docs if user-facing behavior changed.

Acceptance criteria:

- `command_tui.c` reads as TUI orchestration, not as the MIDI engine.
- CLI commands remain straightforward adapters.
- Public headers expose only cross-module contracts.
- `make test` passes.

## Phase 11: Final Verification And Release Readiness

Goal: prove the refactor is complete enough to ship.

Verification checklist:

- `make clean`
- `make`
- `make test`
- `./midi-capture list`
- `./midi-capture record <path> <short-duration> <source-index>`
- `./midi-capture play <path> <destination-index>`
- `./midi-capture tui <recordings-dir>`
- No-device degradation: with no MIDI source or destination available, CLI and
  TUI paths show readable errors/status and do not crash or hang.
- TUI manual checks:
  - file browser navigation
  - live stream height and scrolling
  - note on/off color distinction
  - description column contents
  - record, pause, resume, stop
  - playback from selected event
  - output directory browser confirm/cancel/manual path
  - small terminal fallback
  - monochrome or no-color terminal fallback

Acceptance criteria:

- No known behavior regressions remain.
- Documentation reflects the new architecture and TUI behavior.
- Remaining risks are listed explicitly before merging.

## Suggested Grouping

If the work needs to be batched into larger milestones:

- Milestone A: Phases 0–4, shared MIDI engine and safety net.
- Milestone B: Phases 5–7, TUI data model and renderer separation.
- Milestone C: Phases 8–9, visible UX improvements.
- Milestone D: Phases 10–11, cleanup, verification, and release readiness.

Milestones are strictly sequential. Each depends on the previous being complete
and passing `make test`. Do not start Milestone B before Milestone A is merged.
Do not start Milestone C before Phase 7 renderer isolation is complete; the
visual redesign in Phase 8 has no stable base without it. Do not start
Milestone D before all previous milestones pass verification.
