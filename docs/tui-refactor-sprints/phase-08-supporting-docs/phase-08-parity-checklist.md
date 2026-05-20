# Phase 8 Parity Checklist

**Date:** 2026-05-20

**Status:** Draft for implementation review

**Primary purpose:** give reviewers and implementers a screen-by-screen parity
checklist so the C TUI can be judged against the frozen Phase 8 contracts and
the V3 mockup intent.

## References

Use these together, in this order:

1. `docs/tui-refactor-sprints/phase-08-supporting-docs/phase-08-screen-contracts.md`
2. `docs/tui-refactor-sprints/phase-08-supporting-docs/phase-08-mockups-v3/index.html`
3. `docs/tui-refactor-sprints/phase-08-supporting-docs/phase-08-mockups-v3/styles.css`
4. `docs/tui-refactor-sprints/phase-08-supporting-docs/phase-08-mockups-v3/Phase 8 TUI Mockups V3.pdf`

Use the PDF as a visual cross-check only. Do not use it to invent behavior not
present in the HTML/CSS or the screen contracts.

## How To Use This Checklist

For each screen and size target:

1. render the current TUI at the named size
2. compare it to the frozen screen contract first
3. compare it to the V3 HTML/CSS for hierarchy, density, and visual intent
4. mark each item as `pass`, `fail`, or `deferred`
5. record evidence for every `fail` and every `deferred`

The checklist is designed to reject "roughly similar".

## Review Targets

Required review sizes:

- `90x24`
- `120x36`

Required screens:

- Sequence
- Live Player
- Live Diagnostic
- Settings overlay

Supporting review targets:

- compact stress check near `90x20`
- wide-terminal sanity check above `120x36`

## Pass Rules

### A screen passes only if:

- the visible regions match the frozen contract
- the visible content matches the intended mockup content
- the interaction hints map to real C-owned behavior
- compact clipping is deliberate rather than accidental
- no major hierarchy cue is missing

### A screen fails if:

- it adds or removes a major region without approval
- it hides required state that the contract says must remain visible
- it implies interaction that the code does not actually support
- it falls back to arbitrary clipping rather than contract-driven compact rules

### A screen may be marked deferred only if:

- the item is already declared out of scope in the planning docs
- the deferred item is recorded explicitly
- the remaining screen still reads correctly without it

## Global Parity Checklist

Apply this before the screen-specific lists.

- [ ] The TUI is reviewed against the frozen contract, not memory.
- [ ] The review size is explicitly recorded.
- [ ] The rail, keybar, file pane, work pane, and footer are all visibly present.
- [ ] The footer is summarizing status rather than carrying the main command help.
- [ ] The file pane title reads `Files`.
- [ ] The file pane includes the path row.
- [ ] Selected file state is readable through reverse-video equivalent.
- [ ] The rail ordering reads as `brand | mode | rx | tx | source | destination-or-scope | version`.
- [ ] The version is right-aligned when width allows.
- [ ] Mode, RX, and TX read as compact state tokens rather than plain prose.
- [ ] Keybar commands match real interactions in the code.
- [ ] No color is required to understand the screen.
- [ ] Numeric values remain visible wherever bars or magnitude cues are used.
- [ ] No column corrupts adjacent columns when content is long.

## Sequence Checklist

### Sequence @90x24

- [ ] The right work pane is clearly Sequence mode.
- [ ] The rail shows `MIDI Capture`, mode, RX/TX, source, destination, and version.
- [ ] The keybar shows `tab`, `enter`, `space`, `0`, `,`, and `d` actions consistent with the contract.
- [ ] The file pane shows title, path row, selected file, and surrounding file list.
- [ ] The work pane title shows sequence name and event count.
- [ ] The table header reads `#/ time ch event target value raw` with a leading marker column.
- [ ] The `#/` cell renders `current/total`.
- [ ] The marker column makes the current row immediately visible.
- [ ] The table does not include an extra Sequence `description` column.
- [ ] `time` remains readable.
- [ ] `event` remains readable.
- [ ] `target` remains readable.
- [ ] `value` keeps numeric content visible even when clipped.
- [ ] `raw` remains visible but secondary.
- [ ] Long raw data clips inside its own cell.
- [ ] The footer shows loaded summary plus tempo/playhead state.

### Sequence @120x36

- [ ] Wider size increases breathing room rather than introducing new product-level content.
- [ ] `value` has visibly more room than at `90x24`.
- [ ] `raw` has visibly more room than at `90x24`.
- [ ] The table density still reads like the V3 mockup rather than becoming sparse prose.
- [ ] The file pane does not over-expand beyond its planned role.

## Live Player Checklist

### Live Player @90x24

- [ ] The rail clearly reads `LIVE PLAYER`.
- [ ] The rail uses source plus explicit channel scope instead of destination.
- [ ] The keybar includes `c channel` and that interaction is real in the code.
- [ ] The work pane title shows source identity and fade policy.
- [ ] The controls header reads `scope mod pressure pitch last`.
- [ ] The controls row is present.
- [ ] The controls row is driven by explicit app-owned scope state, not inferred implicitly from recent note rows alone.
- [ ] The note table header reads `#/ note state velocity pressure bend/mod age`.
- [ ] The `#/` column reads as `current/total-visible`.
- [ ] `state` distinguishes active vs fading/off textually.
- [ ] `velocity` keeps numeric text visible.
- [ ] `pressure` keeps numeric text visible.
- [ ] `bend/mod` keeps readable textual meaning.
- [ ] `age` remains readable.
- [ ] The file pane still provides orientation even if file selection is not driving live playback.
- [ ] The footer summarizes live note state and fade/note-format context.

### Live Player @120x36

- [ ] `velocity` gains visible width relative to `90x24`.
- [ ] `pressure` gains visible width relative to `90x24`.
- [ ] The controls row reads like a distinct summary strip rather than collapsing into the note table.
- [ ] The screen visually echoes the mockup proportions from `.controls` and `.player`.

## Live Diagnostic Checklist

### Live Diagnostic @90x24

- [ ] The rail clearly reads `LIVE DIAGNOSTIC`.
- [ ] Both RX and TX activity tokens are visible.
- [ ] The keybar includes filter/pause-log actions consistent with the contract.
- [ ] The title shows diagnostic identity and log window size.
- [ ] The header reads `time dir ch event target value raw description`.
- [ ] `time` remains readable.
- [ ] `dir` remains readable.
- [ ] `event` remains readable even if clipped.
- [ ] `raw` remains visible.
- [ ] `description` remains present, even when it is the first field to lose width.
- [ ] Non-MIDI or skipped rows still read coherently.
- [ ] The footer summarizes diagnostic state rather than duplicating command help.

### Live Diagnostic @120x36

- [ ] `description` recovers more readable explanatory text than at `90x24`.
- [ ] `event`, `target`, and `value` have visibly more breathing room than at `90x24`.
- [ ] `raw` stays secondary but readable.

## Settings Overlay Checklist

### Settings @90x24

- [ ] The overlay is centered over the base screen.
- [ ] The base screen is visually subdued behind the overlay.
- [ ] The overlay title reads `Settings`.
- [ ] The header reads `setting value notes`.
- [ ] The selected row is obvious.
- [ ] `Recordings directory` row includes a human-facing hint for directory browsing.
- [ ] `Middle C` row includes naming guidance.
- [ ] `Note format` row includes allowed format guidance.
- [ ] `Fade timeout` row includes allowed values.
- [ ] `Tempo` row explains playback-speed intent.
- [ ] `Metronome` row explains recording-click intent.
- [ ] The notes column remains present if width allows.
- [ ] If notes are clipped, clipping is orderly and still readable.

### Settings @120x36

- [ ] The notes column has comfortable width.
- [ ] The overlay feels like the V3 popover hierarchy rather than a generic dialog.
- [ ] The row spacing and hint text remain dense and useful.

## Compact Stress Checklist

Use this at or near `90x20` as a stress review.

- [ ] The screen still reads as a deliberate layout.
- [ ] Required rail tokens remain visible.
- [ ] Keybar still shows the highest-value actions.
- [ ] Marker and `#/` remain visible.
- [ ] The file pane remains identifiable.
- [ ] No table devolves into unreadable clipping noise.

## Wide-Terminal Sanity Checklist

Use this above `120x36`.

- [ ] Extra width improves legibility rather than changing product structure.
- [ ] File pane width stays bounded.
- [ ] The work pane receives the majority of added width.
- [ ] Tables become easier to read without introducing new unsupported regions.

## Interaction Reality Checklist

Use this after visual review.

- [ ] Every keybar token maps to a real key handler path.
- [ ] Every visible state token maps to explicit app state or a render field.
- [ ] Live Player `channel-scope` is state-owned.
- [ ] Sequence selection and playback state are not conflated unintentionally.
- [ ] Overlay actions match real keyboard behavior.

## Evidence Capture

For each screen review, record:

- size reviewed
- branch/commit or worktree state
- `pass`, `fail`, or `deferred`
- screenshot or terminal capture reference when available
- one short note for each failed item

## Review Summary Template

```text
Screen:
Size:
Status: pass | fail | deferred

Fails:
-

Deferred:
-

Notes:
-
```

## Exit Criteria For Phase 8 Parity

Phase 8 parity is ready to claim only when:

- all required screens at `90x24` pass
- all required screens at `120x36` pass
- compact stress review has no unreadable layout failures
- keybar and visible-state tokens map to real C-owned behavior
- any deferred items are explicitly documented as out of scope