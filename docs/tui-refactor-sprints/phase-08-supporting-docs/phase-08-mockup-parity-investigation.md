# Phase 8 Mockup Parity Investigation

**Date:** 2026-05-19

## Goal

Explain why the shipped Phase 8 TUI still feels materially different from the
V3 mockups, even after the recent remediation work, and outline realistic ways
to move the curses UI closer to the intended design.

## Evidence Reviewed

- `docs/tui-refactor-sprints/phase-08-supporting-docs/phase-08-mockups-v3/index.html`
- `docs/tui-refactor-sprints/phase-08-supporting-docs/phase-08-mockups-v3/styles.css`
- `docs/tui-refactor-sprints/phase-08-supporting-docs/phase-08-tui-visual-ux-redesign.md`
- `docs/tui-refactor-sprints/phase-08-supporting-docs/phase-08-delivery-plan.md`
- `docs/tui-refactor-sprints/phase-08-supporting-docs/phase-08-gap-analysis.md`
- `docs/tui-refactor-sprints/phase-08-supporting-docs/phase-08-remediation-plan.md`
- `src/tui_render.c`
- `src/tui_render.h`
- `src/command_tui.c`
- `docs/manual-smoke-test.md`

Validation baseline used for this investigation:

- `make test` passes on the current worktree.
- There are still no renderer-specific automated tests or fixed-size terminal
  snapshots.

## Short Diagnosis

The main problem is not a single bad implementation choice. It is a chain of
small specification losses:

1. The mockups were treated as inspiration in some docs and as the primary
   reference in others.
2. The renderer was improved in incremental, low-risk slices, but there was no
   fixed text-mode contract that translated the HTML/CSS mockup into exact
   terminal output expectations.
3. Manual verification focused on behavior and broad layout, not mockup parity
   at named terminal sizes.
4. The documentation set drifted behind the code, so it is now hard to tell
   which gaps are still real and which were already closed locally.

The result is a TUI that is structurally closer to the mockups than the older
implementation, but still not visually or interactionally faithful enough to be
described as "the mockup in code".

## What Seems To Have Gone Wrong

### 1. The spec relaxed exact parity too early

The redesign brief says to treat the mockups as visual direction rather than as
an implementation constraint. That is a reasonable portability guard, but it
also creates room to stop at "same rough layout" instead of "same screen
contract".

That relaxation shows up in a few ways:

- File path moved out of the file pane because the plan preferred a cleaner
  status rail, even though the V3 mockup shows the path inside the file pane.
- Sequence gained an extra `description` column in the renderer, even though
  the V3 Sequence table ends at `raw`.
- Several mockup affordances based on bars, pills, and keycap rhythm were
  reduced to plain text because no terminal-native substitute was specified.

None of those decisions is individually unreasonable. Together, they move the
product away from the mockup language.

### 2. There is no terminal-native reference artifact

The HTML/CSS mockup is rich enough to communicate intent, but it is still a web
artifact. The project never introduced a second, terminal-native design spec
that answers questions like:

- What is the exact 90x24 screen text for Sequence mode?
- Which mockup elements become box drawing, dim text, reverse video, or inline
  ASCII/Unicode bars?
- Which elements are intentionally omitted rather than simply not implemented?

Without that intermediate contract, the renderer naturally drifted toward what
was easiest to express in curses.

### 3. Verification never closed the loop on visual parity

The current manual smoke test checks behavior, transport, file loading, and live
MIDI flow. It does not verify that the TUI matches the approved V3 screen
composition at fixed terminal sizes.

There are also no renderer tests that capture a 90x24 or 120x36 frame and
compare it against a golden text snapshot. That means parity regressions are
easy to miss, especially when the UI is changing in small passes.

### 4. The docs are now partially stale relative to the code

The current worktree already closes several gaps that are still described as
open elsewhere:

- Live Player now has a rendered controls row.
- Settings now has a `notes` column when width allows.
- Status rail labels are already using the compact `RX.` / `RX*` style.
- Source and destination labels are already formatted as `SRC [0] ...` and
  `DST [0] ...`.

That doc drift matters because it obscures the real remaining fidelity issues.

### 5. Some remaining mismatches are interaction mismatches, not paint mismatches

The most important example is Live Player scope.

The V3 mockup implies a user-controlled channel scope (`c` to change channel,
scope row showing `CH 1`). The current implementation renders a scope row, but
it chooses the most recently active channel rather than a user-selected scope.

That means the screen can look closer to the mockup while still not behaving the
way the mockup suggests.

## Remaining Gaps That Still Matter

These are the most visible or meaningful differences still present in the
current implementation.

### Sequence mode

- The renderer still spends width on a `description` column that is not present
  in the V3 Sequence table.
- That extra column makes the visible data columns feel thinner than the mockup,
  especially `value`.
- The file pane still does not show the in-pane path row from V3.

### Live Player mode

- The controls row exists, but it is not tied to an explicit channel-selection
  model.
- The command strip does not expose the mockup's `c channel` interaction.
- The per-note columns are materially narrower than the V3 proportions.
- The mockup's bar-based feel is reduced to plain numeric text.

### Live Diagnostic mode

- Column widths are still tighter than the V3 grid, especially for `event`,
  `target`, and `value`.
- The mockup's visual hierarchy relies on wider breathing room than the current
  renderer gives the table.

### Shared visual language

- Mode, RX, and TX read correctly, but they do not yet feel like mockup-style
  pills.
- The command strip is informative, but it does not visually echo the mockup's
  keycap rhythm.
- The mockup uses bar-like cues heavily; the curses UI mostly renders numbers
  and labels without an equivalent density cue.

## Ways To Get Closer To The Mockups

### Option A: Parity-first pass inside the current curses renderer

This is the lowest-risk path and the one that best fits the existing codebase.

Recommended changes:

1. Remove the extra Sequence `description` column and reallocate that width to
   `value` and `raw`.
2. Add the file-pane path row back under `Files`, even if the path also remains
   in the status rail.
3. Introduce an explicit Live Player channel-scope state in `command_tui.c`,
   add `c` to cycle it, and render the controls row from that state instead of
   "most recently seen channel".
4. Rework Live Player and Live Diagnostic column widths directly from the V3 CSS
   `ch` proportions rather than from ad hoc fixed widths.
5. Add a tiny terminal bar helper for modulation, pressure, velocity, and bend.
   Even a text-first format like `37 [###....]` would recover some of the V3
   density.
6. Tighten the status rail and keybar styling so they read more like bordered
   pills and keycaps rather than plain inline labels.

Why this helps:

- It keeps the current architecture.
- It fixes the most obvious fidelity losses.
- It addresses both visual and interaction drift.

Cost:

- Moderate.
- Mostly renderer work, plus a small `command_tui.c` state addition for channel
  scope.

### Option B: Add a terminal-native wireframe spec before more code changes

This is the best path if the team wants to stop arguing with the HTML mockups.

Recommended artifacts:

- One exact 90x24 ASCII wireframe per mode.
- One exact 120x36 ASCII wireframe per mode.
- A short mapping table from mockup elements to terminal primitives:
  - pill -> bordered text token
  - bar -> numeric value plus mini bar
  - selected file -> reverse video
  - muted metadata -> `A_DIM`

Why this helps:

- It removes ambiguity between "same design language" and "same screen".
- It gives implementation and review a shared target that matches the actual
  rendering medium.

Cost:

- Low to moderate.
- Mostly documentation, but it should happen before another parity pass.

### Option C: Add fixed-size renderer snapshots and golden tests

This is the strongest way to keep parity once the target is agreed.

Approach:

- Render deterministic `TuiRenderState` fixtures at 90x24 and 120x36.
- Capture the terminal buffer into plain text.
- Compare against checked-in golden outputs.

Why this helps:

- It would catch column drift immediately.
- It would make future mockup-fidelity work reviewable without relying only on
  human memory.

Cost:

- Moderate.
- Requires either a curses capture helper or a small renderer abstraction that
  can target an off-screen text buffer for tests.

### Option D: Split the design into portable baseline and enhanced terminal mode

If the concern is portability across weak terminals, another path is to keep the
current conservative baseline and add an enhanced rendering mode for terminals
that support Unicode box drawing and richer glyphs.

Possible split:

- Baseline mode: current ASCII-safe renderer.
- Enhanced mode: mini bars, cleaner pill borders, heavier panel rhythm.

Why this helps:

- It preserves portability.
- It allows a much closer approximation to the mockups where the terminal can
  support it.

Cost:

- Higher ongoing complexity.
- More branches to test and maintain.

### Option E: Align the docs to the current implementation and stop chasing parity

This is valid if the team decides the current TUI is "good enough" and the
remaining differences are not worth another pass.

Why this helps:

- Lowest immediate cost.

Why it is probably the wrong answer for the current concern:

- The user-visible complaint is specifically that the design still is not in the
  code as laid out in the mockups.
- Updating docs alone will not fix that perception.

## Recommended Path

If the goal is to make the TUI actually feel like the V3 mockups, the best next
step is a combination of Option B and Option A:

1. Write terminal-native reference wireframes for 90x24 and 120x36.
2. Use those wireframes to drive one parity-first renderer pass.
3. During that pass, fix the Live Player scope model, remove the extra Sequence
   description column, and rebalance widths from the V3 proportions.
4. After that, add at least one renderer snapshot test so the gains stick.

That sequence fixes the current breakdown at the root: ambiguous design intent,
not just isolated rendering bugs.

## Concrete Next Slice

If this becomes implementation work, the most leverage comes from this order:

1. Sequence: remove `description`, widen `value`, add file-pane path row.
2. Live Player: add explicit scoped channel state and `c` key flow.
3. Live Player and Diagnostic: rebalance widths to match V3 more closely.
4. Shared styling: add small text bars and tighter pill/keycap formatting.
5. Add fixed-size visual verification.