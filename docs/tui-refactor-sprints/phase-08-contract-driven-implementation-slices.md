# Phase 8 Contract-Driven Implementation Slices

**Date:** 2026-05-20

**Status:** Planning

**Primary purpose:** break the Phase 8 implementation into narrow C-first
execution slices that are explicitly tied to the frozen screen contracts and the
parity checklist.

## Working Set

This file stays at the top level of `docs/tui-refactor-sprints/` as the active
Phase 8 working document.

Use `phase-08-imperative-implementation-tasks.md` as the execution companion
when a slice needs explicit imperative tasks rather than planning language.

All supporting Phase 8 material now lives in:

- `phase-08-supporting-docs/`
- `phase-08-supporting-docs/README.md`

Essential sprint inputs:

- `phase-08-supporting-docs/phase-08-screen-contracts.md`
- `phase-08-supporting-docs/phase-08-parity-checklist.md`
- `phase-08-supporting-docs/tui-c-interop-language.md`
- `phase-08-supporting-docs/phase-08-detailed-design-plan.md`
- `phase-08-supporting-docs/phase-08-mockups-v3/`
- `phase-08-imperative-implementation-tasks.md`

## How To Use This Plan

Each slice in this document must be implemented with one dominant intention.

That means:

- one primary design problem per slice
- one main screen family or one shared primitive area per slice
- explicit contract references
- explicit parity-checklist references
- explicit files to touch
- explicit stop conditions so the slice does not grow sideways

If a change cannot be justified by the slice intention, it belongs in a later
slice.

## Slice Execution Rule

For every slice, the implementation loop is:

1. restate the slice intention
2. identify the exact contract clauses being implemented
3. identify the exact checklist items that will be used to review it
4. make the smallest coherent code changes for that slice only
5. run the narrowest validation available
6. stop when the slice intention is satisfied, even if adjacent polish is still
   tempting

## Shared Review Vocabulary

Unless stated otherwise, basename references in this document resolve inside
`phase-08-supporting-docs/`.

Use these document anchors in every implementation note or review summary:

- `phase-08-screen-contracts.md`
- `phase-08-parity-checklist.md`
- `tui-c-interop-language.md`

When referencing this plan, cite slices by ID: `S0`, `S1`, `S2`, and so on.

## Slice Map

### S0: Shared Primitive And Evidence Foundation

**Primary intention:** make the renderer capable of expressing the frozen screen
contracts without improvising spacing, clipping, or token behavior later.

Why this slice exists:

- all later slices depend on stable terminal primitives
- this slice prevents repeated renderer churn across the screen passes

Primary contract targets:

- `phase-08-screen-contracts.md`:
  - Shared Structural Contract
  - Global Width Priorities
  - Rail Contract
  - Keybar Contract
  - File Pane Contract
- `tui-c-interop-language.md`:
  - Rendering Primitives
  - The Key Interop Rule

Primary checklist targets:

- `phase-08-parity-checklist.md`:
  - Global Parity Checklist
  - Compact Stress Checklist
  - Wide-Terminal Sanity Checklist

Likely code files:

- `src/tui_render.c`
- `src/tui_render.h`
- possibly `src/tui_model.c`

Allowed work:

- cell clipping helpers
- token spacing rules
- rail token alignment behavior
- keybar token rendering behavior
- file-pane path-row support
- mini-bar rendering helper design

Do not do yet:

- Live Player scope ownership
- Sequence-specific semantic content changes
- Diagnostic-specific row logic changes
- Settings content wording changes

Validation focus:

- build succeeds
- existing tests pass
- shared regions render intentionally at `90x24` and `120x36`

Stop condition:

- the renderer has stable shared primitives and region rules that later slices
  can consume without redefining them

### S1: Explicit Live Player Scope Ownership

**Primary intention:** remove the biggest interaction-spec gap by making Live
Player scope a real app-owned concept instead of renderer inference.

Why this slice exists:

- the mockup implies a real `c channel` interaction
- the current implementation gap is semantic, not cosmetic

Primary contract targets:

- `phase-08-screen-contracts.md`:
  - Live Player @90x24
  - Live Player @120x36
  - ownership rule: `channel-scope` must be explicit app state
- `tui-c-interop-language.md`:
  - Canonical TUI Nouns And Their C Owners
  - Canonical TUI Verbs And Their C Owners
  - Recommended State Additions: Explicit Live Player channel scope

Primary checklist targets:

- `phase-08-parity-checklist.md`:
  - Live Player @90x24
    - rail uses explicit channel scope
    - keybar includes `c channel` and interaction is real
    - controls row is driven by explicit app-owned scope state
  - Interaction Reality Checklist

Likely code files:

- `src/command_tui.c`
- `src/tui_render.h`
- `src/tui_render.c`

Allowed work:

- add explicit Live Player scope state
- add `c` key handling for scope cycling
- expose scope state in render snapshot
- make controls row render from explicit scope

Do not do yet:

- full Live Player width rebalance
- bar/pill polish beyond what scope state requires

Validation focus:

- build succeeds
- existing tests pass
- Live Player keybar no longer lies about `c channel`

Stop condition:

- `channel-scope` is owned by app state and visible in render state

### S2: Sequence Contract Pass

**Primary intention:** make Sequence mode match the frozen contract exactly,
especially around column content and hierarchy.

Why this slice exists:

- Sequence is the easiest place to drift because table density dominates the
  screen
- the extra non-approved `description` column changes the whole balance

Primary contract targets:

- `phase-08-screen-contracts.md`:
  - Sequence @90x24
  - Sequence @120x36
  - File Pane Contract
  - Rail Contract
- `tui-c-interop-language.md`:
  - Sequence contract

Primary checklist targets:

- `phase-08-parity-checklist.md`:
  - Sequence @90x24
  - Sequence @120x36

Likely code files:

- `src/tui_render.c`
- `src/tui_render.h`
- `src/command_tui.c`

Allowed work:

- remove or preserve only approved Sequence columns
- lock `#/` format to `current/total`
- align Sequence title row and event count presentation
- restore file-pane path row if not already present
- rebalance `event`, `target`, `value`, and `raw` widths to the contract

Do not do yet:

- Live Player behavior
- Diagnostic row behavior
- Settings overlay behavior

Validation focus:

- build succeeds
- tests pass
- Sequence parity checklist can be reviewed cleanly at `90x24` and `120x36`

Stop condition:

- Sequence matches the frozen screen contract without relying on future slices

### S3: Live Player Visual Contract Pass

**Primary intention:** make Live Player feel like the approved task surface now
that scope ownership is real.

Why this slice exists:

- after S1, the semantics are correct, but the screen still needs contract-level
  layout alignment

Primary contract targets:

- `phase-08-screen-contracts.md`:
  - Live Player @90x24
  - Live Player @120x36
  - Rail Contract
  - Keybar Contract
- `tui-c-interop-language.md`:
  - Live Player contract

Primary checklist targets:

- `phase-08-parity-checklist.md`:
  - Live Player @90x24
  - Live Player @120x36

Likely code files:

- `src/tui_render.c`
- `src/tui_render.h`
- `src/command_tui.c`

Allowed work:

- controls row spacing and widths
- note row width rebalance
- file-pane info-row behavior in live mode
- footer summary wording for live context
- bar rendering for velocity/pressure/bend-mod if primitive is already available

Do not do yet:

- Live Diagnostic-specific row logic
- Settings overlay adjustments beyond shared primitive reuse

Validation focus:

- build succeeds
- tests pass
- Live Player checklist is reviewable independently

Stop condition:

- Live Player matches the frozen contract at both target sizes and no longer
  depends on later visual reinterpretation

### S4: Live Diagnostic Contract Pass

**Primary intention:** align the debug/inspection surface with the V3 stream
table contract while keeping raw bytes and descriptions readable together.

Why this slice exists:

- Diagnostic has the densest content mix and needs its own width and clipping
  decisions

Primary contract targets:

- `phase-08-screen-contracts.md`:
  - Live Diagnostic @90x24
  - Live Diagnostic @120x36
- `tui-c-interop-language.md`:
  - Live Diagnostic contract

Primary checklist targets:

- `phase-08-parity-checklist.md`:
  - Live Diagnostic @90x24
  - Live Diagnostic @120x36

Likely code files:

- `src/tui_render.c`
- `src/tui_render.h`
- possibly `src/tui_log.c`
- possibly `src/tui_log.h`

Allowed work:

- rebalance `time`, `dir`, `ch`, `event`, `target`, `value`, `raw`,
  `description`
- decide compact clipping behavior for `description`
- improve skipped/non-MIDI row readability

Do not do yet:

- Settings overlay content changes
- shared polish not needed by Diagnostic

Validation focus:

- build succeeds
- tests pass
- Diagnostic rows read coherently at both sizes

Stop condition:

- Diagnostic matches the contract and preserves both raw bytes and semantic
  explanation

### S5: Settings And Shared Polish Pass

**Primary intention:** finish the remaining Phase 8 parity details that are
shared across screens or isolated to the overlay.

Why this slice exists:

- by this point the three primary screens should already be contract-correct
- the remaining work should be polish and discoverability, not structural churn

Primary contract targets:

- `phase-08-screen-contracts.md`:
  - Settings Overlay @90x24
  - Settings Overlay @120x36
  - Rail Contract
  - Keybar Contract
  - File Pane Contract

Primary checklist targets:

- `phase-08-parity-checklist.md`:
  - Settings @90x24
  - Settings @120x36
  - Global Parity Checklist

Likely code files:

- `src/tui_render.c`
- `src/tui_render.h`
- `src/command_tui.c`

Allowed work:

- notes-column readability
- overlay spacing and hierarchy
- final shared rail/keybar/pill polish
- any remaining approved file-pane polish

Do not do:

- reopening the screen contracts
- adding Phase 9 directory-browser columns

Validation focus:

- build succeeds
- tests pass
- settings checklist passes at both target sizes

Stop condition:

- Settings and shared visual language are no longer the reason parity is failing

### S6: Verification Harness And Signoff

**Primary intention:** make parity provable and freeze the result.

Why this slice exists:

- without this slice, parity claims will drift again

Primary contract targets:

- all frozen screen contracts

Primary checklist targets:

- all parity checklist sections

Likely code files:

- `Makefile`
- possible new tests or harness files
- documentation files capturing evidence

Allowed work:

- renderer snapshot harness if feasible
- fixed-size capture workflow
- evidence recording and final checklist completion

Do not do:

- new UI redesign work masked as verification

Validation focus:

- executable validation path exists
- parity evidence is captured per screen and size

Stop condition:

- parity is demonstrated rather than asserted

## Slice Dependencies

Use this dependency order:

```text
S0 -> S1 -> S2 -> S3 -> S4 -> S5 -> S6
```

Dependency notes:

- S2 can start only after S0 stabilizes shared primitives.
- S3 depends on S1 because Live Player contract correctness requires explicit
  scope ownership.
- S5 should not absorb unfinished structural work from S2-S4.

## Intention Guardrails

These rules apply during implementation.

### Guardrail 1

Do not mix semantic ownership changes with broad visual polish unless the slice
explicitly requires both.

### Guardrail 2

Do not "just fix nearby things" that belong to a later screen family.

### Guardrail 3

If a slice fails its narrow validation, repair that slice before widening scope.

### Guardrail 4

If a proposed change cannot point to a contract clause and a checklist item, it
does not belong in the current slice.

## Review Template Per Slice

Use this format in implementation notes or review summaries.

```text
Slice:
Primary intention:

Contracts:
-

Checklist targets:
-

Files touched:
-

Validation:
-

Out of scope held back:
-
```

## Definition Of Slice Planning Done

This implementation-slice plan is ready when:

1. every slice has one dominant intention
2. every slice names exact contract and checklist targets
3. the order prevents structural and stylistic work from collapsing together
4. a future implementation agent can pick up one slice without reopening design
   intent questions
