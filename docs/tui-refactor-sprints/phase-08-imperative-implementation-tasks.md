# Phase 8 Imperative Implementation Tasks

**Date:** 2026-05-20

**Status:** Ready for execution

**Primary purpose:** turn the Phase 8 implementation slices into explicit,
imperative task lists that can be executed one slice at a time without
reopening design intent.

## How To Use This Document

Use this document as the execution companion to
`phase-08-contract-driven-implementation-slices.md`.

For every slice:

1. Restate the slice intention before you edit.
2. Read only the listed contract clauses and checklist targets.
3. Touch only the listed files unless a nearby dependency forces one extra
   local change.
4. Make the smallest coherent change set that satisfies the slice.
5. Run the narrowest validation that can falsify the slice outcome.
6. Stop as soon as the slice stop condition is true.

Use these references in every slice note or review summary:

- `phase-08-screen-contracts.md`
- `phase-08-parity-checklist.md`
- `tui-c-interop-language.md`

## Execution Order

Run the slices in this order:

```text
S0 -> S1 -> S2 -> S3 -> S4 -> S5 -> S6
```

Do not pull structural work forward from a later slice.

## S0: Shared Primitive And Evidence Foundation

**Intention:** make the renderer capable of expressing the frozen screen
contracts without improvising spacing, clipping, or token behavior later.

**Contracts to read:**

- `phase-08-screen-contracts.md`: Shared Structural Contract
- `phase-08-screen-contracts.md`: Global Width Priorities
- `phase-08-screen-contracts.md`: Rail Contract
- `phase-08-screen-contracts.md`: Keybar Contract
- `phase-08-screen-contracts.md`: File Pane Contract
- `tui-c-interop-language.md`: Rendering Primitives
- `tui-c-interop-language.md`: The Key Interop Rule

**Checklist targets to review:**

- `phase-08-parity-checklist.md`: Global Parity Checklist
- `phase-08-parity-checklist.md`: Compact Stress Checklist
- `phase-08-parity-checklist.md`: Wide-Terminal Sanity Checklist

**Files to touch:**

- `src/tui_render.c`
- `src/tui_render.h`
- `src/tui_model.c` only if shared primitive support requires it

**Execution checklist:**

1. Restate that S0 is a shared-primitive slice, not a screen-specific polish
   pass.
2. Read only the shared structural, width, rail, keybar, and file-pane
   contract clauses before editing.
3. Inspect the current shared rendering helpers in `src/tui_render.c` and
   `src/tui_render.h` before adding new ones.
4. Identify one missing or unstable primitive at a time.
5. Change the shared primitive before changing any individual screen layout.
6. Keep render-state changes out of scope unless a shared primitive cannot be
   expressed without one small supporting field.

**Tasks:**

1. Fix the compact layout threshold: change `rows < 24` to `rows <= 24` in
   `calculate_layout` in `src/tui_render.c` — `90x24` is the contract's
   compact review target.
2. Update the comment above `calculate_layout` to match the corrected threshold
   and remove the stale "Compact 90x20-90x23" reference.
3. Implement or tighten shared cell clipping helpers.
4. Verify that clipping behavior is deterministic at narrow widths.
5. Normalize token spacing so shared regions stop improvising per screen.
6. Remove ad hoc token padding that conflicts with the shared contract.
7. Align rail token rendering to the frozen rail contract.
8. Confirm rail token alignment survives both compact and wide layouts.
9. Align keybar token rendering to the frozen keybar contract.
10. Confirm keybar token spacing remains stable when labels vary in length.
11. Add or finish file-pane path-row support if the shared contract requires it.
12. Verify that the file-pane path row clips cleanly instead of collapsing the
    pane hierarchy.
13. Introduce a mini-bar rendering helper only if later slices need one shared
    primitive.
14. Keep Live Player scope semantics unchanged.
15. Keep Sequence-specific content unchanged. Do not touch the `description`
    column in Sequence mode — that work belongs to S2.
16. Keep Diagnostic-specific row logic unchanged.
17. Keep Settings wording unchanged.
18. Run `make`.
19. Run `make test`.
20. Review shared regions at `90x24` and `120x36`.
21. Stop immediately if a tempting screen-specific polish item cannot be tied
    back to a shared primitive.

**Stop when:** shared primitives and shared region rules are stable enough for
later slices to reuse without redefining them.

**Deferred to S5:** rename the `"Live fade"` settings label to `"Fade timeout"`
to match the parity checklist. Do not make this change in S0.

## S1: Explicit Live Player Scope Ownership

**Intention:** remove the biggest interaction-spec gap by making Live Player
scope a real app-owned concept instead of renderer inference.

**Contracts to read:**

- `phase-08-screen-contracts.md`: Live Player @90x24
- `phase-08-screen-contracts.md`: Live Player @120x36
- `phase-08-screen-contracts.md`: ownership rule for explicit `channel-scope`
- `tui-c-interop-language.md`: Canonical TUI Nouns And Their C Owners
- `tui-c-interop-language.md`: Canonical TUI Verbs And Their C Owners
- `tui-c-interop-language.md`: Recommended State Additions for explicit Live
  Player channel scope

**Checklist targets to review:**

- `phase-08-parity-checklist.md`: Live Player @90x24
- `phase-08-parity-checklist.md`: Interaction Reality Checklist

**Files to touch:**

- `src/command_tui.c`
- `src/tui_render.h`
- `src/tui_render.c`

**Tasks:**

1. Add explicit Live Player scope state in app-owned C state.
2. Implement `c` key handling that cycles channel scope.
3. Expose the scope state through the render snapshot.
4. Render the controls row from explicit scope state instead of renderer
   inference.
5. Keep the keybar honest about `c channel` behavior.
6. Do not rebalance the full Live Player layout yet.
7. Do not add extra bar or pill polish beyond what the new scope state needs.
8. Run `make`.
9. Run `make test`.
10. Confirm that the Live Player keybar no longer advertises a fake `c channel`
    action.

**Stop when:** `channel-scope` is owned by app state and visible in render
state.

## S2: Sequence Contract Pass

**Intention:** make Sequence mode match the frozen contract exactly, especially
around column content and hierarchy.

**Contracts to read:**

- `phase-08-screen-contracts.md`: Sequence @90x24
- `phase-08-screen-contracts.md`: Sequence @120x36
- `phase-08-screen-contracts.md`: File Pane Contract
- `phase-08-screen-contracts.md`: Rail Contract
- `tui-c-interop-language.md`: Sequence contract

**Checklist targets to review:**

- `phase-08-parity-checklist.md`: Sequence @90x24
- `phase-08-parity-checklist.md`: Sequence @120x36

**Files to touch:**

- `src/tui_render.c`
- `src/tui_render.h`
- `src/command_tui.c`

**Tasks:**

1. Remove any non-approved Sequence columns.
2. Preserve only the Sequence columns named by the frozen contract.
3. Lock the `#/` presentation to `current/total`.
4. Align the Sequence title row to the contract.
5. Align event count presentation to the contract.
6. Restore the file-pane path row if S0 did not already cover it.
7. Rebalance `event`, `target`, `value`, and `raw` widths to the contract.
8. Keep Live Player behavior unchanged.
9. Keep Diagnostic row behavior unchanged.
10. Keep Settings overlay behavior unchanged.
11. Run `make`.
12. Run `make test`.
13. Review Sequence at `90x24` and `120x36` against the parity checklist.

**Stop when:** Sequence matches the frozen screen contract without relying on a
later slice to correct structure or hierarchy.

## S3: Live Player Visual Contract Pass

**Intention:** make Live Player feel like the approved task surface now that
scope ownership is real.

**Contracts to read:**

- `phase-08-screen-contracts.md`: Live Player @90x24
- `phase-08-screen-contracts.md`: Live Player @120x36
- `phase-08-screen-contracts.md`: Rail Contract
- `phase-08-screen-contracts.md`: Keybar Contract
- `tui-c-interop-language.md`: Live Player contract

**Checklist targets to review:**

- `phase-08-parity-checklist.md`: Live Player @90x24
- `phase-08-parity-checklist.md`: Live Player @120x36

**Files to touch:**

- `src/tui_render.c`
- `src/tui_render.h`
- `src/command_tui.c`

**Tasks:**

1. Rebalance controls row spacing and widths to the contract.
2. Rebalance note row widths to the contract.
3. Align file-pane info-row behavior in live mode.
4. Update footer summary wording for live context if the contract requires it.
5. Render velocity, pressure, bend, and mod bars only if S0 already supplied
   the needed primitive.
6. Keep Diagnostic-specific row logic unchanged.
7. Keep Settings overlay adjustments out of this slice except for shared
   primitive reuse.
8. Run `make`.
9. Run `make test`.
10. Review Live Player at `90x24` and `120x36` against the parity checklist.

**Stop when:** Live Player matches the frozen contract at both target sizes and
no longer depends on later visual reinterpretation.

## S4: Live Diagnostic Contract Pass

**Intention:** align the debug and inspection surface with the V3 stream table
contract while keeping raw bytes and descriptions readable together.

**Contracts to read:**

- `phase-08-screen-contracts.md`: Live Diagnostic @90x24
- `phase-08-screen-contracts.md`: Live Diagnostic @120x36
- `tui-c-interop-language.md`: Live Diagnostic contract

**Checklist targets to review:**

- `phase-08-parity-checklist.md`: Live Diagnostic @90x24
- `phase-08-parity-checklist.md`: Live Diagnostic @120x36

**Files to touch:**

- `src/tui_render.c`
- `src/tui_render.h`
- `src/tui_log.c` only if row content support requires it
- `src/tui_log.h` only if row content support requires it

**Tasks:**

1. Rebalance `time`, `dir`, `ch`, `event`, `target`, `value`, `raw`, and
   `description` widths to the contract.
2. Implement compact clipping behavior for `description` that still preserves
   readability.
3. Improve skipped and non-MIDI row readability without changing the screen
   family.
4. Keep Settings overlay content unchanged.
5. Avoid unrelated shared polish that Diagnostic does not need.
6. Run `make`.
7. Run `make test`.
8. Review Diagnostic at `90x24` and `120x36` against the parity checklist.

**Stop when:** Diagnostic matches the contract and preserves both raw bytes and
semantic explanation.

## S5: Settings And Shared Polish Pass

**Intention:** finish the remaining Phase 8 parity details that are shared
across screens or isolated to the overlay.

**Contracts to read:**

- `phase-08-screen-contracts.md`: Settings Overlay @90x24
- `phase-08-screen-contracts.md`: Settings Overlay @120x36
- `phase-08-screen-contracts.md`: Rail Contract
- `phase-08-screen-contracts.md`: Keybar Contract
- `phase-08-screen-contracts.md`: File Pane Contract

**Checklist targets to review:**

- `phase-08-parity-checklist.md`: Settings @90x24
- `phase-08-parity-checklist.md`: Settings @120x36
- `phase-08-parity-checklist.md`: Global Parity Checklist

**Files to touch:**

- `src/tui_render.c`
- `src/tui_render.h`
- `src/command_tui.c`

**Tasks:**

1. Compare every label string in `draw_settings_overlay` in
   `src/tui_render.c` against the parity checklist row names and align
   word-for-word where they differ.
2. Rename `"Live fade"` to `"Fade timeout"` as the first confirmed mismatch.
3. Verify the remaining labels (`Recordings directory`, `Middle C`, `Note
   format`, `Tempo`, `Metronome`) match the checklist exactly.
4. Improve notes-column readability to the approved overlay contract.
5. Tighten overlay spacing and hierarchy.
6. Apply the final approved rail, keybar, and pill polish.
7. Apply any remaining approved file-pane polish.
8. Do not reopen the frozen screen contracts.
9. Do not add Phase 9 directory-browser columns.
10. Run `make`.
11. Run `make test`.
12. Review Settings at `90x24` and `120x36` against the parity checklist.

**Stop when:** Settings and the shared visual language are no longer the reason
parity fails.

## S6: Verification Harness And Signoff

**Intention:** make parity provable and freeze the result.

**Contracts to read:**

- all frozen screen contracts in `phase-08-screen-contracts.md`

**Checklist targets to review:**

- all sections in `phase-08-parity-checklist.md`

**Files to touch:**

- `Makefile`
- new test or harness files only if the verification path needs them
- documentation files that capture evidence

**Tasks:**

1. Add a renderer snapshot harness if it is feasible within the current C
   boundary.
2. Add a fixed-size capture workflow if a harness is not enough by itself.
3. Capture parity evidence for each required screen and size.
4. Record final checklist completion in documentation.
5. Avoid slipping new UI redesign work into verification.
6. Run `make`.
7. Run `make test`.
8. Run the executable verification path that proves parity per screen and size.

**Stop when:** parity is demonstrated with evidence instead of asserted.

## Slice Guardrails

Apply these rules while executing the tasks:

1. Keep semantic ownership changes separate from broad visual polish unless the
   slice explicitly requires both.
2. Refuse to fix nearby issues that belong to a later screen family.
3. Repair the current slice before widening scope if narrow validation fails.
4. Reject any change that cannot point to both a contract clause and a checklist
   item.

## Review Note Template

Use this template when you execute or review a slice:

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