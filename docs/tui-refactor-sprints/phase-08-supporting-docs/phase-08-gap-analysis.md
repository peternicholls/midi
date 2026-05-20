# Phase 8: Post-Implementation Gap Analysis

**Date:** 2026-05-16
**Branch:** phase-08-ux-plan
**Reviewer:** GitHub Copilot / Claude Sonnet 4.6

This document is a post-implementation review of the Phase 8 TUI UX redesign.
It compares the current code in `src/tui_render.c`, `src/command_tui.c`, and
related files against:

- `phase-08-delivery-plan.md` — the engineering execution plan (P8A–P8E)
- `phase-08-tui-visual-ux-redesign.md` — the UX design brief
- `phase-08-mockups-v3/index.html` — the primary visual reference

---

## Delivery Plan Coverage (P8A–P8E)

### P8A — Layout Foundation ✅ Complete

All stories done. `TuiLayout` struct present in `tui_render.c` with named
rectangles for `status_rail`, `command_strip`, `file_pane`, `work_pane`,
`footer`, and `overlay`. File width clamped to 28–36 columns. 90×20 resize
guard active. `compact` flag computed for rows < 24. `app_version()` used as
single version source in the status rail.

**One open item:** `layout.compact` is computed and stored but no drawing
function currently alters behaviour when it is true. The compact rendering
path (shorter labels, fewer visible rows at 90×20–90×23) is a stub.

---

### P8B — Table Rendering ✅ Complete

`draw_cell()` helper clips each column independently. Sequence table has a
header row and all eight columns: marker, `#/`, time, ch, event, target,
value, raw (+ description when width permits). Leading `>` marker for the
selected row. Raw bytes rendered with `A_DIM`.

**One open item:** The `#/` column renders only the event number (`%zu`) in a
5-character cell. The V3 mockup shows `21/128` (event number / total count).
`state->sequence_event_count` is available and the total count is never
displayed anywhere on screen.

---

### P8C — Work Pane Modes ✅ Complete

`TuiRenderWorkPaneMode` enum with SEQUENCE, LIVE_PLAYER, LIVE_DIAGNOSTIC.
`v` key cycles modes. `TuiLogMidiFields` provides structured fields for all
Live Diagnostic columns (time, dir, ch, event, target, value, raw, description).
`fill_live_note_rows()` populates Live Player rows with per-note fade and
expiry.

**One open item:** The V3 mockup's Live Player contains a **channel-scope
control row** above the per-note rows: a single row showing `CH 1`, mod bar,
pressure bar, pitch bend value, and last-RX age. The implementation only
renders per-note rows; the channel-level aggregate row is absent.

---

### P8D — File, Settings, Directory Workflows ✅ Complete

`enter`=load, `u`=unload, `a`=append-arm toggle, `r`=rename (`.mid`
auto-appended), `R`=record. Settings overlay has all six rows (recordings
directory, middle C, note format, fade timeout, tempo, metronome), box
drawing, and arrow-key navigation. `d` opens a directory placeholder overlay
that preserves the Phase 9 layout hooks.

**One open item:** The settings overlay renders a 2-column `"%-22s %s"` line
per row. The V3 mockup shows a 3-column grid with a `notes` hint column (for
example `press d for browser` next to the recordings directory row,
`C3/C4/C5` next to middle C). The third column is absent.

---

### P8E — Transport, Accessibility, Final Hardening ✅ Complete

`space`=play/pause/start, `s`=stop, `0`/`KEY_HOME`=return to start, `p`=play
from selected, `m`=metronome toggle. Tempo scaling applied in
`playback_position_seconds()`. Metronome fires `beep()` on each beat. All
eight semantic color pairs initialised with `use_default_colors()` and
`init_pair`. SysEx uses `A_DIM` rather than `COLOR_WHITE`.

**Doc open item:** `phase-08-tui-visual-ux-redesign.md` still shows
`**Status:** Not started`. The verification evidence section should be
populated and the status updated to `Implemented`.

---

## UX Redesign Doc Compliance

| Requirement | Status |
|---|---|
| Signal Desk layout — persistent file pane + mode-aware right pane | ✅ |
| Status rail: app label, mode, RX/TX, source, dest, dir path | ✅ |
| `app_version()` as single version source | ✅ |
| Mode-aware command strip with per-mode hints | ✅ |
| Directory path in status rail, not repeated in file pane | ✅ |
| `No .mid files` empty state | ✅ |
| Sequence table: column header, `>` marker, `A_DIM` raw bytes | ✅ |
| No direction column in Sequence mode | ✅ |
| Live Diagnostic: time, dir, ch, event, target, value, raw, description | ✅ |
| Live Player: #/, note, state, vel, pressure, bend/mod, age, fade/expire | ✅ |
| Structured `TuiLogEntry` / `TuiLogMidiFields` | ✅ |
| Centred settings popover, keyboard-first | ✅ |
| All six settings exposed | ✅ |
| `d` entry point + layout hooks for Phase 9 | ✅ |
| `space`/`0`/`HOME`/`p`/`s`/`u`/`a`/`r` all wired | ✅ |
| Tempo scaling + metronome toggle | ✅ |
| SysEx neutral (not `COLOR_WHITE`) | ✅ |
| `A_REVERSE` limited to file list | ✅ |
| Monochrome usable via labels and alignment | ✅ |
| 90×20 resize guard | ✅ |
| Verification evidence in redesign doc | ⚠️ Status field still says "Not started" |
| `#/` shows `event/total` | ❌ Shows event number only |
| Live Player channel-scope row | ❌ Missing |

---

## Visual Consistency with Mockups-V3

### Status Rail

| Element | V3 mockup | Implementation |
|---|---|---|
| Version position | Far right (margin-left: auto) | Inline after "MIDI Capture" |
| Mode label style | Bordered pill | Wrapped in `[...]` brackets |
| RX/TX format | `RX*` active / `RX.` idle | `RX:ON` / `RX:--` |
| Source/dest labels | `SRC [0] KeyLab 88` | `Source [0]: KeyLab 88` |
| LIVE PLAYER label colour | Green (`--note-on`) | `A_BOLD` only |
| LIVE DIAGNOSTIC label colour | Yellow (`--control`) | `A_BOLD` only |

The overall structure is correct. Format diverges from the mockup's
pill/indicator aesthetic but is functional.

### File Pane

| Element | V3 mockup | Implementation |
|---|---|---|
| Pane title | `Files` | `Recordings` |
| Directory path row | Second row below title (in-pane) | Status rail only |
| Selected file | Full reverse-video | `A_REVERSE` ✅ |
| Loaded-file indicator | Not shown | `* filename` prefix |

The plan deliberately places the directory path in the status rail rather
than the file pane. The mockup shows it in both places; the implementation
is plan-compliant.

### Sequence Table Column Widths

| Column | Mockup (ch) | Implementation (chars) | Note |
|---|---|---|---|
| marker | 2 | 2 | ✅ |
| #/ | 8 | 5 | ❌ No `/total`; may clip `21/128` |
| time | 10 | 8 | Minor |
| ch | 4 | 4 | ✅ |
| event | 15 | 10 | `"Ch Pressure"` clips to 10 ch |
| target | 12 | 8 | Minor |
| value | 22 | 7 | Narrow; no bar visual (text only) |
| raw | remaining | 12 | ✅ |

### Live Diagnostic Column Widths

| Column | Mockup (ch) | Implementation (chars) |
|---|---|---|
| time | 10 | 8 |
| dir | 5 | 4 |
| ch | 4 | 4 |
| event | 13 | 10 |
| target | 12 | 8 |
| value | 18 | 7 |
| raw | 15 | 14 |
| description | remaining | remaining |

### Live Player

The V3 mockup has two visual sections:

1. **Channel-scope row** (`controls` grid): scope, mod bar, pressure bar,
   pitch bar, last-RX age. **Absent from implementation.**
2. **Per-note rows** (`player` grid): #/, note, state, velocity, pressure,
   bend/mod, age. Present. ✅

`HOLD` state (sustain-pedal note hold) shown in the mockup spec is not
tracked; CC 64 is not handled. Implementation has `ON`, `OFF`, `FADING`.

### Settings Overlay

Mockup shows a 3-column grid (setting | value | notes/hint). Implementation
renders a flat `"%-22s %s"` string. Box, title, `A_REVERSE` selection
highlight, and all six rows are correct. Notes column absent.

### Directory Overlay

Correct deliberate stub. Phase 9 owns the full column renderer and
navigation model.

---

## Prioritised Gaps

### High — affects correctness or day-to-day readability

1. **`#/` column missing `/total`** — render `event+1/total` (e.g. `21/128`)
   and widen the cell to 8 characters. `state->sequence_event_count` is
   already in scope in `draw_sequence_row`.

2. **Live Player channel-scope row absent** — add one row above the note rows
   showing channel (CH 1), current mod value, channel pressure, pitch-bend
   value, and last-RX age. The data is available in `TuiLiveNoteState`.

3. **`phase-08-tui-visual-ux-redesign.md` status field** — update
   `**Status:** Not started` to `Implemented` and add a brief verification
   note.

### Medium — visible divergence from the mockup

4. **RX/TX format** — `RX*`/`RX.` is more compact and less ambiguous than
   `RX:ON`/`RX:--`. Change in `draw_status_rail`.

5. **Mode label colouring for work pane** — `LIVE PLAYER` should render with
   `COLOR_GREEN`, `LIVE DIAGNOSTIC` with `COLOR_YELLOW`, consistent with the
   mockup pill colours. Currently `A_BOLD` only in non-transport states.
   Apply colour in `draw_sequence_panel`, `draw_live_player_panel`, and
   `draw_live_diagnostic_panel` (or pass it through `work_pane_mode_text`).

6. **Sequence event/target/value column widths** — at 120+ columns the work
   pane has room to spare. Widening event to 14, target to 12, and value to
   10 chars would reduce clipping and better match the mockup proportions
   without breaking compact terminals.

7. **File pane title** — `"Files"` matches the mockup; `"Recordings"` does
   not. One-character change in `draw_files_panel`.

### Low — cosmetic or doc-only

8. **Delivery plan checkboxes unticked** — no story checkboxes in
   `phase-08-delivery-plan.md` have been marked complete despite all five
   phases being implemented.

9. **Compact rendering path** — `layout.compact` flag is computed but no
   drawing function alters label length or column hints at 90×20–90×23.

10. **Settings overlay notes column** — third column with discoverability
    hints (`press d for browser`, `C3/C4/C5`, etc.) would improve first-use
    experience.

11. **Version label position** — moving the version string to the right end
    of the status rail would match the mockup exactly.

---

## Build And Test Status

`make clean && make && make test` pass on the current branch. The 90×20 and
90×24 layout paths are exercised by the existing render-state unit tests. No
regressions observed from this analysis.

## Remediation Closure (2026-05-14)

Remediation phases R0–R4 from `phase-08-remediation-plan.md` were executed and
verified with `make && make test` (clean). Gap status after remediation:

- Gaps 1, 2, 3, 4, 5, 6, 7, 9, 10, 11 — **closed** by R1/R2/R3/R4 code in
  `src/tui_render.c` and `src/command_tui.c` (sequence event/total formatting,
  Live Player channel-scope row, status doc updated, `RX*`/`RX.` and `TX*`/`TX.`
  rail, mode-label colouring across all three work-pane titles, widened
  Sequence columns at default width with compact fallback, `Files` title,
  compact-mode shorter command strip, settings overlay notes column, version
  pinned to right edge of rail).
- Gap 8 — **closed**; `phase-08-delivery-plan.md` checkboxes ticked after
  remediation verification.
- Phase 9 directory-browser remains deferred to its own sprint (not in scope
  here).
- V3 PDF cross-check skipped during remediation; HTML/CSS V3 source treated
  as authoritative reference.
