# Phase 8 TUI Refactor: Slice Plan Evaluation Report

This report evaluates [phase-08-contract-driven-implementation-slices.md](../phase-08-contract-driven-implementation-slices.md) based on a **surface layer check** (structure, formatting, clarity, and completeness) and a **deep dive check** (technical alignment with the codebase, contracts, checklists, dependencies, and boundaries).

---

## 1. Surface Layer Check

The surface layer check evaluates the document's structure, formatting, readability, and adherence to its own planning definition.

### Readability & Structure
- **Formatting**: The document uses clean, standardized GitHub-flavored Markdown. Spacing, headers, lists, code blocks, and bold tags are correct and consistent.
- **Completeness**: All 7 slices (`S0` through `S6`) are completely specified with:
  - Primary Intention
  - Contextual Justification ("Why this slice exists")
  - Primary Contract Targets
  - Primary Checklist Targets
  - Likely Code Files
  - Allowed Work vs. "Do Not Do Yet"
  - Validation Focus & Stop Conditions
- **Date & Status**: Current date (`2026-05-20`) and status (`Planning`) are correctly stated at the top.
- **Review Template**: A standard `Review Template Per Slice` is provided, promoting standard output formatting across refactor reviews.

### Reference Link Verification
All referenced paths are valid and point to existing local resources:
- Folder [phase-08-supporting-docs/](./)
- File [README.md](./README.md)
- File [phase-08-screen-contracts.md](./phase-08-screen-contracts.md)
- File [phase-08-parity-checklist.md](./phase-08-parity-checklist.md)
- File [tui-c-interop-language.md](./tui-c-interop-language.md)
- File [phase-08-detailed-design-plan.md](./phase-08-detailed-design-plan.md)
- Folder [phase-08-mockups-v3/](./phase-08-mockups-v3/)

### Evaluation Against "Definition Of Slice Planning Done"
1. **Dominant Intention**: *Passed.* Each slice focuses on one core design problem or screen family.
2. **Contract and Checklist Targets**: *Passed.* Every slice references explicit sections of `phase-08-screen-contracts.md` and `phase-08-parity-checklist.md`.
3. **Prevention of Churn/Collapse**: *Passed.* Foundations (S0) and semantic state (S1) are decoupled from screen-specific visual passes (S2-S4).
4. **Actionable for Agents**: *Passed.* The division of "Allowed Work" and "Do Not Do Yet" prevents scope creep.

---

## 2. Deep Dive Check

The deep dive check compares the slice plan against the actual state of the C codebase and references to verify technical correctness and dependency integrity.

### Codebase Alignment & Gap Analysis

An inspection of the C source files (`src/command_tui.c`, `src/tui_render.c`, `src/tui_render.h`) highlights specific implementation gaps that the slices must address:

1. **Compact Layout Threshold Bug in `calculate_layout`**
   - *Current Code* (`src/tui_render.c:59`):
     ```c
     layout.compact = rows < 24;
     ```
   - *Issue*: The screen contracts define `90x24` as the compact review target. If the terminal height is exactly 24 rows, `rows < 24` evaluates to `false`, meaning `90x24` will NOT use the compact layout.
   - *Alignment*: `S0` is allowed to work on "shared primitives and region rules." The compact logic belongs here. Changing this threshold to `rows <= 24` (or similar) is a critical task for `S0`.

2. **Missing Path Row in File Pane**
   - *Current Code* (`src/tui_render.c` `draw_files_panel`): Renders the title "Files" and the file list immediately below it, but completely lacks rendering logic for the directory path row (`path`).
   - *Alignment*: `S0` and `S2` identify this gap and specify "restore/add file-pane path row."

3. **Hardcoded Column Widths & Lack of Mode Responsiveness**
   - *Current Code* (`src/tui_render.c` `sequence_widths_for`): Has only two hardcoded states (pane width < 80 vs >= 80) and continues to draw the disallowed `description` column in Sequence mode.
   - *Current Code* (`src/tui_render.c` `draw_live_player_header` / `draw_live_diagnostic_header`): Column widths are completely hardcoded and do not adapt dynamically to the screen size (e.g., Live Player age / Diagnostic desc are just allocated whatever is left).
   - *Alignment*: `S2` (Sequence), `S3` (Live Player), and `S4` (Live Diagnostic) target these exact layout rules and specify column rebalancing.

4. **Implicit / Inferred Live Player Channel Scope**
   - *Current Code* (`src/command_tui.c` `fill_live_controls_row`): Automatically finds the channel with the most recent MIDI event to display in the controls row. There is no `c` keypress handler and no explicit `channel_scope` state variable.
   - *Alignment*: `S1` explicitly targets this by introducing app-owned C state and a `'c'` key event cycle in `handle_keypress`.

---

## 3. Dependency Path Validation

The proposed dependency path is:
```text
S0 (Primitives) -> S1 (Scope State) -> S2 (Sequence UI) -> S3 (Player UI) -> S4 (Diagnostic UI) -> S5 (Settings) -> S6 (Harness)
```

This sequence is logically sound and structurally optimal:
- **S0** provides the layout primitives (e.g., clipping, token spacing) required by all screens.
- **S1** establishes the backing state for the Live Player, which is necessary before S3 can align the Player visual contract.
- **S2** tackles Sequence mode first, which is the default view.
- **S3** and **S4** follow to polish the active Live screens.
- **S5** addresses Settings overlay, which overlays all these base modes.
- **S6** freezes and validates the final state.

---

## 4. Specific Recommendations & Corrections

To make the slice plan completely airtight before execution, the following minor edits and clarifications should be incorporated:

1. **Clarify Compact Threshold in S0**:
   - Add a task to `S0` (Shared Primitives) in `phase-08-imperative-implementation-tasks.md` to update the compact check in `calculate_layout` so that `90x24` is correctly categorized as compact (e.g., checking `rows <= 24` or `cols < 100`).

2. **Explicitly Mention the Disallowed Sequence `description` Column in S0 "Do not do yet"**:
   - In `S0`, explicitly reinforce that the `description` column in Sequence mode must not be modified or worked on until `S2` removes/rebalances it.

3. **Confirm Settings Note Wordings in S5**:
   - Note hints in `src/tui_render.c` are currently close but not identical to the contracts. S5 should explicitly check the string alignment (e.g., labeling `"Live fade"` vs `"Fade timeout"`).

---

## 5. Summary Evaluation Matrix

| Slice | Primary Objective | Code Files Involved | Contract Alignment | Status |
| :--- | :--- | :--- | :--- | :--- |
| **S0** | Primitives, Spacing, Clipping, Path-row | `tui_render.*`, `tui_model.*` | Structural, Widths, Rail, Keybar | Ready |
| **S1** | Explicit Channel Scope State & `'c'` key | `command_tui.c`, `tui_render.*` | Live Player @90x24/120x36 | Ready |
| **S2** | Sequence column & hierarchy alignment | `tui_render.*`, `command_tui.c` | Sequence @90x24/120x36 | Ready |
| **S3** | Live Player visual/column rebalance | `tui_render.*`, `command_tui.c` | Live Player @90x24/120x36 | Ready |
| **S4** | Diagnostic column & description alignment | `tui_render.*`, `tui_log.*` | Diagnostic @90x24/120x36 | Ready |
| **S5** | Settings overlay & global UI polish | `tui_render.*`, `command_tui.c` | Settings @90x24/120x36 | Ready |
| **S6** | Snapshots, verification & checklist signs | `Makefile`, test files, docs | All screen contracts | Ready |

### Conclusion
`docs/tui-refactor-sprints/phase-08-contract-driven-implementation-slices.md` is **highly complete, logically coherent, and technically aligned** with the current C boundary and the target contracts. With the recommendations above (particularly the compact threshold alignment in `S0`), the plan is ready to be fully accepted for execution.
