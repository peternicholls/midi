# Repository Handoff

## Start Here

If you are a new agent or a fresh context, read these in order before making
Phase 8 TUI changes:

1. `docs/tui-refactor-sprints/phase-08-contract-driven-implementation-slices.md`
2. `docs/tui-refactor-sprints/phase-08-supporting-docs/README.md`
3. The specific supporting docs named by the current slice

Do not start Phase 8 work from older planning docs by default. The slice plan
is the active working document.

## Current Focus

The repository is in a C-first Phase 8 TUI parity effort.

The goal is to make the terminal UI mirror the approved V3 mockup intent using
the existing C boundary:

`command_tui.c` -> `TuiRenderState` -> `tui_render.c`

Do not switch the implementation direction to Go or Bubble Tea unless the user
explicitly reopens that decision.

## What Is Required

- Keep Phase 8 implementation contract-driven.
- Work from one slice at a time.
- Tie every implementation change to the frozen screen contracts and parity
  checklist.
- Keep the intention of the current slice narrow; do not absorb adjacent polish
  or unrelated screen work.
- Preserve the C-owned state and render boundary rather than pushing behavior
  into renderer guesswork.

## Where Things Are

### Active Phase 8 working document

- `docs/tui-refactor-sprints/phase-08-contract-driven-implementation-slices.md`

### Essential Phase 8 supporting references

- `docs/tui-refactor-sprints/phase-08-supporting-docs/README.md`
- `docs/tui-refactor-sprints/phase-08-supporting-docs/phase-08-screen-contracts.md`
- `docs/tui-refactor-sprints/phase-08-supporting-docs/phase-08-parity-checklist.md`
- `docs/tui-refactor-sprints/phase-08-supporting-docs/tui-c-interop-language.md`
- `docs/tui-refactor-sprints/phase-08-supporting-docs/phase-08-detailed-design-plan.md`
- `docs/tui-refactor-sprints/phase-08-supporting-docs/phase-08-mockups-v3/`

### Phase 8 historical or supporting context

These live under `docs/tui-refactor-sprints/phase-08-supporting-docs/` and are
useful when the active slice needs rationale or prior investigation:

- `phase-08-mockup-parity-investigation.md`
- `phase-08-remediation-plan.md`
- `phase-08-gap-analysis.md`
- `phase-08-delivery-plan.md`
- `phase-08-tui-visual-ux-redesign.md`
- `phase-08-review-claude-sonnet.md`
- `tui-go-vs-c-interop-evaluation.md`

### Main implementation files

- `src/command_tui.c`
- `src/tui_render.c`
- `src/tui_render.h`
- `src/tui_model.c`
- `src/tui_model.h`

### Validation surfaces

- `make`
- `make test`
- `docs/manual-smoke-test.md`

Note: the project currently has no dedicated fixed-size renderer snapshot
harness, so visual parity still relies on contract review plus manual checks.

## Repo Facts

- The built CLI binary name is `midi`.
- User-facing command examples and CI should use `./midi`.
- `docs/manual-smoke-test.md` covers interactive behavior, not exact visual
  parity.

## Working Rule For New Contexts

Before editing Phase 8 TUI behavior:

1. identify the current slice from the slice plan
2. read only the supporting docs required by that slice
3. state the slice intention before editing
4. validate narrowly before widening scope

If a proposed change cannot name a slice, a contract clause, and a checklist
target, it is probably the wrong change.