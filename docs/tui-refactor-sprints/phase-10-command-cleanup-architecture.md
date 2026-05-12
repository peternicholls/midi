# Phase 10: Command Cleanup And Architecture Pass

## Objective

Finish the architectural cleanup after shared modules exist by making command
files thin adapters.

## Inputs

- Shared modules from Phases 2-9
- `src/command_play.c`
- `src/command_record.c`
- `src/command_tui.c`
- `src/command_list.c`
- Public headers

## Tasks

- [ ] Review each `command_*.c` file for remaining command-local domain logic.
- [ ] Remove dead helpers made obsolete by shared modules.
- [ ] Move any remaining reusable MIDI logic into shared modules if the move is
  local and testable.
- [ ] Keep argument parsing and presentation in command files.
- [ ] Tighten public headers:
  - remove private structs
  - remove unused declarations
  - document ownership for public APIs
- [ ] Revisit `app_support.*` and keep only truly shared app/platform helpers
  there.
- [ ] Update README and docs if architecture or user-facing behavior changed.
- [ ] Run formatting manually by matching the existing C style.

## Suggested Files

- `src/command_play.c`
- `src/command_record.c`
- `src/command_tui.c`
- `src/command_list.c`
- `src/app_support.c`
- `src/app_support.h`
- shared module headers
- `README.md`

## Verification

- [ ] `make clean`
- [ ] `make`
- [ ] `make test`
- [ ] CLI smoke test for `list`, `record`, and `play`.
- [ ] TUI smoke test for browse, record, playback, and live stream.

## Exit Criteria

- Command files read as adapters and coordinators.
- Shared modules own reusable behavior.
- Public headers are narrow and intentional.
- No obvious duplication remains between CLI and TUI paths.

## Risks

- This phase can turn into unrelated cleanup. Only change code touched by the
  refactor or needed to clarify the new boundaries.
- Removing helpers too aggressively can hide useful error context. Preserve
  readable caller-side error messages.
