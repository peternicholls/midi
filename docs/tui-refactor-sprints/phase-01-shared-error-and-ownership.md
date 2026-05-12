# Phase 1: Shared Error And Ownership Conventions

**Depends on:** Phase 0

**Status:** Not started

## Objective

Define conventions before extraction so shared modules do not mix CLI printing,
TUI logging, CoreMIDI cleanup, and allocation ownership in inconsistent ways.

## Inputs

- Current error handling in `src/app_support.c`
- Current CLI behavior in `src/command_record.c` and `src/command_play.c`
- Current TUI status/log behavior in `src/command_tui.c`

## Tasks

- [ ] Identify current error styles:
  - direct `fprintf`/`fputs`
  - `log_osstatus_error`
  - TUI `set_status`
  - TUI `log_line`
- [ ] Define the minimum shared error shape needed for reusable modules.
- [ ] Decide how OSStatus values are represented without requiring shared
  modules to print.
- [ ] Decide how allocation and invalid-argument errors are represented.
- [ ] Define naming conventions:
  - `*_init` for caller-owned structs
  - `*_dispose` for releasing internal resources without freeing the struct
  - `*_free` for heap-owned collections
  - `*_open` and `*_close` for CoreMIDI ports/clients
- [ ] Add a short docs note or header comments recording the convention.
- [ ] Update the sprint plan if a better convention emerges during review.

## Suggested Files

- `src/app_support.h`
- `src/app_support.c`
- `src/midi_result.h` — **required deliverable**: defines the shared result type
  used by all new modules. Must exist before Phase 2 extraction begins.
- `docs/error-and-ownership-conventions.md` — optional supplement if the header
  alone is insufficient to record the full decision.

## Verification

- [ ] `src/midi_result.h` exists with the agreed result type.
- [ ] Reviewer can tell who owns every object returned by a proposed API.
- [ ] CLI and TUI callers can adapt the same shared error result to their own
  presentation.
- [ ] `make test` passes if code or tests are changed.

## Exit Criteria

- `src/midi_result.h` exists and defines the result type used by all new shared
  modules. This header is the concrete proof Phase 1 is done.
- Shared modules have an agreed error and ownership style.
- No extracted module needs to call TUI-specific status or log functions.
- No extracted module needs to print directly unless it remains CLI-only.

## Risks

- Over-designing the error type would make a small C project harder to read.
- Under-designing it would preserve the current CLI/TUI coupling.
