# Phase 0: Baseline And Safety Net

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
- [ ] Decide whether to add checked-in `.mid` fixtures or generate fixtures in
  tests.
- [ ] If fixtures are practical, add one simple fixture for playback-event
  ordering and one with multiple MIDI event kinds.
- [ ] Run baseline build and tests before any extraction starts.

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
- Pure helper behavior is protected by tests where practical.
- Known untested hardware paths are explicitly listed.
- No architecture move is started until this phase is complete.

## Risks

- CoreMIDI behavior cannot be fully automated without hardware or virtual MIDI
  setup.
- Fixture `.mid` files can become opaque. Prefer tiny, documented fixtures.
