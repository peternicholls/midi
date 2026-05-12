# Phase 0: Baseline And Safety Net

**Depends on:** nothing — this phase must complete before any other phase begins.

**Status:** Not started

## Objective

Protect current behavior before moving code. This phase should create enough
evidence that later extractions are refactors, not accidental rewrites.

## Inputs

- `docs/tui-refactor-plan.md`
- `docs/tui-refactor-sprints.md`
- Existing tests under `tests/`
- Current command behavior in `src/command_*.c`
- Current TUI behavior in `src/command_tui.c`

## Tasks

- [ ] Inventory current command behavior:
  - `./midi-capture list`
  - `./midi-capture record <path> [seconds] [source-index]`
  - `./midi-capture play <path> [destination-index]`
  - `./midi-capture tui [recordings-dir]`
- [ ] Create a manual smoke-test checklist for hardware-dependent behavior.
- [ ] Extend tests for existing pure helpers:
  - MIDI filename filtering
  - recording filename formatting
  - path joining
  - clock formatting
- [ ] Use generated or in-memory sequences for playback-event tests by default.
  Add checked-in `.mid` fixtures only if file loading itself must be tested.
- [ ] Add one simple playback-event ordering input and one input with multiple
  MIDI event kinds, using the fixture strategy above.
- [ ] Run baseline build and tests before any extraction starts.
- [ ] Document the expected Makefile pattern for new modules and tests:
  each new module's `.c` file adds to `APP_SRCS`; each new test binary
  adds to `TESTS`, gets its own build rule pairing the test source with the
  module under test, and gets a run line in the `test` target. Record this
  in a Makefile comment or a short docs note so all phases follow the same
  pattern.

## Suggested Files

- `docs/manual-smoke-test.md`
- `tests/test_tui_model.c`
- future fixture files under `tests/fixtures/` if used
- `Makefile` if new tests are added

## Verification

- [ ] `make clean`
- [ ] `make`
- [ ] `make test`
- [ ] Manual smoke-test checklist can be followed by someone other than the
  author.

## Exit Criteria

- Current behavior is documented.
- Pure helper behavior is protected by tests.
- Known untested hardware paths are explicitly listed.
- No architecture move is started until this phase is complete.

## Risks

- CoreMIDI behavior cannot be fully automated without hardware or virtual MIDI
  setup.
- Fixture `.mid` files can become opaque. Prefer tiny, documented fixtures.
