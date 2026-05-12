# Phase 11: Final Verification And Release Readiness

## Objective

Prove the refactor is complete, documented, and ready to merge or release.

## Inputs

- Completed phases 0-10
- Updated source tree
- Updated docs
- Available MIDI source and destination, real or virtual

## Tasks

- [ ] Run full clean build and tests.
- [ ] Run CLI manual smoke tests.
- [ ] Run TUI manual smoke tests.
- [ ] Verify small-terminal and no-color behavior.
- [ ] Inspect headers for accidental public implementation details.
- [ ] Inspect `command_tui.c` length and responsibilities.
- [ ] Inspect `command_play.c` and `command_record.c` for duplicated shared
  engine logic.
- [ ] Update `README.md` for changed TUI layout and key bindings.
- [ ] Update architecture docs if module names or boundaries changed during
  implementation.
- [ ] Record remaining risks and explicit non-goals.
- [ ] Prepare a final summary suitable for a PR or commit message.

## Verification Commands

- [ ] `make clean`
- [ ] `make`
- [ ] `make test`
- [ ] `./midi-capture list`
- [ ] `./midi-capture record <path> <short-duration> <source-index>`
- [ ] `./midi-capture play <path> <destination-index>`
- [ ] `./midi-capture tui <recordings-dir>`

## TUI Manual Checks

- [ ] File browser navigation works.
- [ ] Live stream height and scrolling work.
- [ ] Note on/off color distinction is visible.
- [ ] Description column is populated.
- [ ] Record, pause, resume, and stop work.
- [ ] Playback from selected event works.
- [ ] Output directory browser confirm/cancel/manual path work.
- [ ] Small terminal fallback is readable.
- [ ] Monochrome or no-color fallback is readable.

## Exit Criteria

- No known behavior regressions remain.
- Tests and manual verification evidence are recorded.
- Documentation matches the implemented architecture and TUI behavior.
- Remaining risks are explicit and acceptable.

## Risks

- Manual verification can miss timing bugs. Prefer short repeated tests for
  record/play/pause flows.
- Hardware availability can delay final signoff. A virtual MIDI setup should be
  acceptable for smoke testing when physical hardware is unavailable.
