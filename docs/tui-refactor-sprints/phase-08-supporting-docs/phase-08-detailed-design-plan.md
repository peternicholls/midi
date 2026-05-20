# Phase 8 Detailed Design Plan

**Date:** 2026-05-19

**Status:** Planning

**Decision context:** Phase 8 remains C-first. The implementation boundary stays
`command_tui.c` -> `TuiRenderState` -> `tui_render.c`.

## Objective

Turn the V3 mockups into a terminal-exact planning package so the final TUI
mirrors the approved intent rather than only approximating it.

This plan is intentionally more detailed than the original redesign brief and
the remediation plan. It exists to remove ambiguity before the next C-side
implementation pass.

## Planning Outcome

This planning phase is complete only when the project has all of the following:

1. a locked reference hierarchy
2. a terminal-native design language
3. fixed-size screen contracts
4. explicit interaction/state ownership for each screen
5. renderer primitive rules for translating mockup affordances into terminal
   output
6. review gates that can reject parity drift before code is called "done"

## Reference Hierarchy

When references disagree, use this order:

1. `docs/tui-refactor-sprints/phase-08-supporting-docs/phase-08-mockups-v3/index.html`
2. `docs/tui-refactor-sprints/phase-08-supporting-docs/phase-08-mockups-v3/styles.css`
3. `docs/tui-refactor-sprints/phase-08-supporting-docs/phase-08-mockups-v3/Phase 8 TUI Mockups V3.pdf`
4. `docs/tui-refactor-sprints/phase-08-supporting-docs/tui-c-interop-language.md`
5. `docs/tui-refactor-sprints/phase-08-supporting-docs/phase-08-mockup-parity-investigation.md`
6. `docs/tui-refactor-sprints/phase-08-supporting-docs/phase-08-remediation-plan.md`
7. `docs/tui-refactor-sprints/phase-08-supporting-docs/phase-08-tui-visual-ux-redesign.md`

Reference rules:

- HTML defines screen content and sequencing.
- CSS defines proportions, density, and semantic styling intent.
- PDF is a single-page review artifact used as a final visual cross-check, not
  as a source of new behavior.
- The C-interop language translates those references into implementation-owned
  nouns and verbs.

## Fidelity Policy

The planning phase should classify every visual or interaction detail into one
of three buckets.

### Bucket A: Must mirror directly

These should survive from the mockup into the terminal with only medium-driven
adaptation, not product-level reinterpretation.

- screen composition
- title and header content
- table columns
- keybar intent
- file pane presence and hierarchy
- mode structure
- overlay structure
- selected/current row semantics
- visible counts, statuses, and labels

### Bucket B: Must preserve intent, but may adapt to terminal primitives

- pills -> padded bounded tokens
- bars -> numeric values plus inline mini bars
- muted metadata -> dim/default secondary text
- row emphasis -> marker, dimming, reverse video, or sparse highlight
- CSS spacing -> column width and text clipping rules

### Bucket C: Explicitly deferred or out of scope

- Phase 9 directory column browser
- decorative web-only effects such as box shadows and gradients
- theme selection
- non-portable styling dependencies

If a detail is not assigned to one of these buckets, the plan is incomplete.

## Planning Stages

### Stage P0: Reference Lock

Purpose:

- stop future debates about which artifact is authoritative

Inputs:

- V3 HTML
- V3 CSS
- V3 PDF
- current C-first contract language

Outputs:

- locked hierarchy
- explicit fidelity policy

Acceptance criteria:

- the team can answer "which artifact wins?" without discussion
- the team can answer "what may adapt vs what must match?" without discussion

### Stage P1: Screen Contract Freeze

Purpose:

- define exact terminal targets for the primary supported sizes

Required contract variants:

- `sequence @90x24`
- `sequence @120x36`
- `live-player @90x24`
- `live-player @120x36`
- `live-diagnostic @90x24`
- `live-diagnostic @120x36`
- `settings-overlay @90x24`
- `settings-overlay @120x36`

Outputs:

- fixed-size screen contracts
- declared width budgets for major columns
- explicit compact fallbacks

Acceptance criteria:

- each screen contract names all visible regions and columns
- each region maps to existing or planned C-owned state
- each compact fallback is deliberate rather than "clip whatever happens"

### Stage P2: Interaction Contract Freeze

Purpose:

- guarantee that the UI model and the code model describe the same actions

Required interaction mapping per screen:

- key
- user intent
- state owner
- operation owner
- render impact

Special focus areas:

- Live Player channel scope
- sequence row selection vs playback position
- overlay entry/exit behavior
- transport state vs work-pane mode

Acceptance criteria:

- every keybar token maps to a concrete operation or an explicit planned gap
- no interaction is implied visually without a C owner

### Stage P3: Terminal Primitive Translation

Purpose:

- define how web-style visual intent becomes terminal output

Required primitive mappings:

- rail token
- mode pill
- RX/TX activity token
- keycap
- selected file
- current row marker
- muted metadata
- magnitude bar
- overlay frame

Acceptance criteria:

- each mockup affordance has one chosen terminal primitive
- no implementation slice needs to improvise the translation later

### Stage P4: Implementation Slice Planning

Purpose:

- convert the frozen contracts into a C-first execution order

Required slices:

1. screen contract parity foundations
2. explicit Live Player scope state
3. Sequence parity pass
4. Live Player parity pass
5. Live Diagnostic parity pass
6. settings and shared polish
7. verification and signoff

The authoritative slice breakdown now lives in:

- `docs/tui-refactor-sprints/phase-08-contract-driven-implementation-slices.md`

Acceptance criteria:

- each slice has named files, named contract clauses, and named validation
- no slice spans unrelated screens unless it is a shared primitive change

### Stage P5: Verification Design

Purpose:

- define how parity will be proven, not just argued

Required checks:

- `make`
- `make test`
- fixed-size renderer snapshots once harness exists
- manual visual review against V3 HTML/CSS and PDF export
- mode-by-mode parity checklist

Acceptance criteria:

- every slice has a corresponding validation step
- final parity claims require both executable validation and visual review

## Deliverables From This Planning Phase

This planning phase should yield these working artifacts:

1. `phase-08-detailed-design-plan.md`
2. `phase-08-screen-contracts.md`
3. `phase-08-contract-driven-implementation-slices.md`
4. a future renderer snapshot plan or test doc when the harness lands
5. updates to implementation plans only after the contracts are frozen

## Design Topics That Must Be Frozen Before Coding

### 1. Status rail composition

Freeze:

- token ordering
- mode token behavior during playback/recording
- source/destination presentation
- right-aligned version behavior

### 2. File pane behavior

Freeze:

- title text
- path row behavior
- loaded-file indicator behavior
- whether live modes keep file pane purely orienting or partially informative

### 3. Sequence table contract

Freeze:

- exact visible columns
- whether the extra `description` column exists at all
- `#/` formatting
- `value` and `raw` width priorities

### 4. Live Player contract

Freeze:

- explicit channel-scope behavior
- controls row ownership
- note row ordering rules
- active vs fading row treatment
- bar syntax for velocity/pressure/mod/bend

### 5. Live Diagnostic contract

Freeze:

- whether `description` is always present at 90x24 or only at wider sizes
- handling of non-MIDI log rows
- how filtered or skipped rows should read

### 6. Settings overlay contract

Freeze:

- compact behavior when width is constrained
- notes-column clipping rules
- wording of each notes hint

## Review Gates

The planning process should use four gates.

### Gate G1: Reference fidelity gate

Question:

- does the contract still match the V3 mockup surfaces?

Fail if:

- any screen contract introduces or removes product-level content without
  explicit approval

### Gate G2: C ownership gate

Question:

- can every noun and verb be owned by current or planned C state?

Fail if:

- a visual behavior requires renderer inference instead of explicit state

### Gate G3: compact-screen gate

Question:

- does the `90x24` contract still feel intentional and readable?

Fail if:

- compact mode is just accidental clipping of the wide design

### Gate G4: parity-proof gate

Question:

- how will the team prove the coded TUI matches the planned contract?

Fail if:

- the answer is only "manual glance testing"

## Implementation Sequencing After Planning

Once the planning package is approved, implementation should proceed in this
order:

1. add explicit Live Player scope state in `command_tui.c`
2. align Sequence contract and remove any non-approved columns
3. align file pane hierarchy and path row behavior
4. rebalance Live Player and Diagnostic widths from the frozen contracts
5. add terminal mini-bar primitives
6. add fixed-size renderer verification

## Planning Risks

### Risk: over-planning without freezing decisions

Mitigation:

- every planning artifact must end in explicit frozen choices, not open prose

### Risk: treating compact mode as a second-class screen

Mitigation:

- write compact contracts first, then widen them for 120x36

### Risk: confusing transport state with work-pane mode

Mitigation:

- freeze both concepts separately in the interaction contract

### Risk: HTML-specific visuals leaking into implementation language

Mitigation:

- permit only terminal primitives in the implementation contract

## Definition Of Planning Done

Planning is done when:

1. the screen contracts are frozen for `90x24` and `120x36`
2. Live Player channel scope has an explicit ownership plan
3. the Sequence, Live Player, Live Diagnostic, and Settings screens each have a
   parity checklist
4. the next implementation slice can be described in contract clauses rather
   than mockup interpretation language
