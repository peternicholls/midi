# Phase 8: TUI Visual and UX Redesign — Plan Review by Claude Sonnet

This document expands on the task list in `phase-08-tui-visual-ux-redesign.md` with
analysis of the current codebase, hidden complexity, open design questions, and
implementation notes. It is intended to be reviewed and answered before work begins.

---

## Foundation

Phase 7 is complete. `tui_render.c` is a stateless draw function that consumes a
`TuiRenderState` snapshot. `command_tui.c` owns all mutable state and assembles the
snapshot before each call to `tui_render()`. The separation is clean and Phase 8 has
a solid base to build on.

---

## Task-by-Task Analysis

### 1. Define target layout at common terminal sizes

**Current state:**

- Minimum enforced at `tui_render.c:238`: `rows < 20 || cols < 90`
- `file_width = cols / 3`, minimum 28 (`tui_render.c:247-250`)
- `log_height = 8` hardcoded (`tui_render.c:230`)
- No special wide-terminal behavior — both panels simply stretch

**Hidden complexity:**

Three areas compete for vertical space: header (5 rows fixed), content, and log+footer
(10 rows minimum). Making the log taller directly reduces content area height. At the
current minimum of 24 rows that leaves only 9 content rows; increasing log height to
12 would leave 5 — barely usable.

`file_width = cols / 3` has no upper bound. On a 220-column terminal the file panel
grows to 73 columns with nothing to show beyond a filename, wasting space that the
event panel could use.

**Design choices needed:**

- What is the "comfortable default" size? (80x24? 120x40? 132x40?)
- Should `log_height` be proportional, e.g. `rows / 4` clamped to `[6, 14]`?
- Should `file_width` have a maximum (e.g. 40 cols) for wide terminals?
- Should the minimum terminal size increase to accommodate the new layout?
- Should a named layout struct or constants be introduced now so Phase 9's directory
  browser overlay can reference named regions rather than recompute geometry?

**Open question:** What terminal size do you primarily develop and test in?

---

### 2. File browser as persistent side column

**Current state:**

The file panel is already a left side column (`tui_render.c:255`). The task is likely
about ensuring it remains correctly bounded and stable as the rest of the layout
changes across Phase 8 and Phase 9.

**Hidden complexity:**

Phase 9 adds a directory browser overlay. Phase 8 needs to ensure the base layout is
structured so Phase 9's overlay can land cleanly. If layout geometry is recomputed
again in Phase 9 there is a risk of divergence between the two phases.

**Design choice:** Should Phase 8 introduce named layout constants or a computed
`TuiLayout` struct passed into each draw function, so Phase 9 can reference the same
geometry rather than duplicate the calculation?

---

### 3. Live stream as taller primary monitoring area

**Current state:**

Fixed at 8 rows (`tui_render.c:230`).

**Hidden complexity:**

"Taller" is in direct tension with the content area. At common terminal heights:

| Terminal rows | Header | Footer | Current log | Content left |
|---|---|---|---|---|
| 24 | 5 | 2 | 8 | 9 |
| 30 | 5 | 2 | 8 | 15 |
| 40 | 5 | 2 | 8 | 25 |
| 40 (log=12) | 5 | 2 | 12 | 21 |

Increasing log height degrades the sequence inspection experience at small terminals.

**Design choices needed:**

- Proportional: `log_height = rows / 4` clamped `[6, 14]`
- Priority-based: log gets a fixed N rows, content gets the rest (minimum content = 8)
- Is the live stream more important than the file/event panels? What is the primary
  use case — monitoring live MIDI, or inspecting recorded files?

---

### 4. Tabular columns for event rows

**Current state** (`tui_render.c:161-163`):

```c
snprintf(line, sizeof(line), "%c %4zu  %s  %-12s  %s",
         marker, index+1, clock_text, byte_text, description);
mvaddnstr(screen_row, left + 1, line, width - 2);
```

A single string is formatted and then clipped with `mvaddnstr`. This is the largest
hidden complexity in the entire phase.

**Hidden complexity:**

- Single-string clipping corrupts columns — `mvaddnstr` cuts wherever the character
  count lands, mid-column and without regard for content boundaries.
- Making bytes "visually secondary" (task 6) requires different attributes on the
  bytes column. A single `attron`/`attroff` pair cannot do this — the row must be
  drawn column-by-column with separate attribute calls per segment.
- Column headers require one extra row in `draw_events_panel`, reducing `visible`
  from `height - 3` to `height - 4`.
- A "direction" column (listed in the task) does not currently exist in
  `TuiRenderSequenceRow` (`tui_render.h:15`). Adding it cascades into
  `fill_sequence_render_row()` in `command_tui.c:931`. Note: sequence file events
  have no inherent directionality — they are all events to be played back. Direction
  is only meaningful for the live monitor stream.

**Proposed column layout** (relative to panel left edge):

| Column | Width | X offset | Notes |
|---|---|---|---|
| Selector | 2 | 0 | `>` or space |
| Index | 5 | 2 | `%4zu` + space |
| Time | 9 | 8 | `MM:SS.s` |
| Bytes | 14 | 18 | `%-12s` hex, visually secondary |
| Description | remaining | 33 | category color |

**Open question:** Should "Direction" be a column? If so:
- Where does that data come from? The sequence view has no per-event direction.
- If direction only applies to the live stream, should it be a log panel column
  rather than a sequence panel column?

---

### 5. Semantic color pairs

**Current state** (`tui_render.c:8-16`, `tui_render.c:206-220`):

| Pair | Foreground | Category |
|---|---|---|
| `TUI_COLOR_NOTE_ON` (1) | `COLOR_GREEN` | `MIDI_DESCRIPTION_NOTE_ON` |
| `TUI_COLOR_NOTE_OFF` (2) | `COLOR_CYAN` | `MIDI_DESCRIPTION_NOTE_OFF` |
| `TUI_COLOR_CONTROL` (3) | `COLOR_YELLOW` | `MIDI_DESCRIPTION_CONTROL_CHANGE` |
| `TUI_COLOR_PROGRAM` (4) | `COLOR_MAGENTA` | `PROGRAM_CHANGE`, `CHANNEL_PRESSURE`, `POLY_PRESSURE` |
| `TUI_COLOR_BEND` (5) | `COLOR_BLUE` | `MIDI_DESCRIPTION_PITCH_BEND` |
| `TUI_COLOR_SYSTEM` (6) | `COLOR_WHITE` | `MIDI_DESCRIPTION_SYSEX` |
| `TUI_COLOR_WARNING` (7) | `COLOR_RED` | `UNSUPPORTED`, `INCOMPLETE` |

The task adds two more: "selected row" and "active recording/playback".

**Hidden complexity:**

- `A_REVERSE` for selection combined with a category foreground color already works
  and is terminal-universal. Replacing it with a dedicated color pair would require
  explicit combination logic to avoid the pair overriding the category color.
- `TUI_COLOR_SYSTEM` is `COLOR_WHITE` which is invisible on light-background
  terminals. This is a latent bug.
- "Active recording/playback" as a color pair — it is unclear where it applies.
  The header mode label? The footer? Individual log rows during live recording?
  This needs a concrete definition before implementation.

**Design choices needed:**

- Keep `A_REVERSE` for selection or add a dedicated pair?
- Fix `TUI_COLOR_SYSTEM` (`COLOR_WHITE`) in this phase or leave for Phase 11?
- Define exactly where "active recording/playback" color is applied.

---

### 6. Keep raw bytes visible but visually secondary

**Current state:**

The bytes column (`byte_text`) renders with the same category attributes as the
description. There is no visual distinction between them.

**Hidden complexity:**

This is not achievable without the column-by-column draw approach described in task 4.
The two tasks are coupled: task 6 is a prerequisite of, or co-requirement with, task 4.

**Proposed approach:** Draw the bytes column with `A_DIM` or a neutral/default color
pair, and the description column with the category color. No changes to
`TuiRenderSequenceRow` are required for this alone.

---

### 7. Improve active transport status readability

**Current state** (`tui_render.c:63-65`):

```c
snprintf(transport_label, sizeof(transport_label), "%s  RX%s TX%s",
         mode_label, rx_active ? "." : " ", tx_active ? "." : " ");
```

Displayed top-right in `A_BOLD`. The distinction between `RX.` (active) and `RX `
(inactive) is a single character and easy to miss.

**Design choices needed:**

- Color the mode label using a color pair (e.g. red for RECORDING, green for PLAYING)?
- Change `RX.`/`TX.` to `[RX]`/`[TX]` for more visual weight?
- Use a bracket or separator between mode and activity indicators?

Note: `A_BLINK` is explicitly ruled out by the phase risks (conservative color usage,
prefer plain ASCII).

---

### 8. Ensure text clipping does not corrupt columns

Fully addressed by the column-by-column draw approach in task 4. Each column segment
gets its own `mvaddnstr(row, x_offset, text, col_width)` call with an independent
width clamp.

This is currently impossible to fix without restructuring the row draw loop.

---

## Struct Changes Required

**`TuiRenderSequenceRow` (`tui_render.h:15`):**

- No changes required for basic column layout improvement or secondary byte styling.
- A `direction` field would be needed if direction becomes a sequence column, but
  this is questionable (see task 4 open question).
- A separate `is_current_playback_position` bool may be useful to distinguish the
  currently playing event from the user-selected event — these are two different
  concepts that `selected` currently conflates.

**`TuiRenderState` (`tui_render.h:27`):**

- A transport state enum or bool flags may be needed if the mode label gains a color
  pair, so the renderer can choose the right pair without re-parsing the label string.

---

## Open Questions Summary

| # | Question | Blocks |
|---|---|---|
| 1 | What is the primary/comfortable terminal size? | Layout constants (task 1) |
| 2 | Proportional or fixed log height? | Live stream sizing (task 3) |
| 3 | Is per-event direction available or meaningful in the sequence view? | Direction column (task 4) |
| 4 | Should column widths be constants or derived from terminal width? | Column layout (task 4) |
| 5 | Keep `A_REVERSE` for selection or add a dedicated color pair? | Color pairs (task 5) |
| 6 | Where exactly is "active recording/playback" color applied? | Color pairs (task 5) |
| 7 | Fix `TUI_COLOR_SYSTEM = COLOR_WHITE` in this phase? | Color pairs (task 5) |
| 8 | Introduce a `TuiLayout` struct now for Phase 9 compatibility? | Layout (task 1, Phase 9 prep) |
| 9 | Should `is_current_playback_position` be separate from `selected` in the row struct? | Row struct, playback UX |

---

## Suggested Implementation Order

Given the coupling between tasks, this order minimises re-work:

1. Define and document layout constants / `TuiLayout` struct (task 1)
2. Restructure `draw_events_panel` to column-by-column draw (task 4 + task 8)
3. Apply secondary styling to bytes column (task 6)
4. Add column headers to sequence panel (task 4)
5. Adjust log height (task 3)
6. Add remaining color pairs and fix `TUI_COLOR_SYSTEM` (task 5)
7. Improve transport status label (task 7)
8. Enforce file panel max width on wide terminals (task 1)
9. Manual testing at minimum and comfortable sizes (task 9)
