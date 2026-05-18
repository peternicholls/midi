# Phase 8 Remediation Plan: Close Post-Implementation Gaps

**Depends on:** Phase 8 implementation and `phase-08-gap-analysis.md`

**Status:** Planned

**Primary handoff target:** next implementation agent

## Objective

Close the Phase 8 post-implementation gaps identified on 2026-05-16 while
preserving the shipped Signal Desk layout and the Phase 7 renderer boundary.
This is a remediation pass, not a second redesign.

The work should tighten the current implementation against the Phase 8 brief and
V3 mockups in small, reviewable changes:

- Fix the correctness/readability gaps that affect daily use.
- Bring the most visible V3 divergences closer to the approved visual target.
- Update the Phase 8 docs so status and verification evidence match reality.
- Keep Phase 9 directory-browser work out of scope.

## Source Context

Use these inputs together:

- `docs/tui-refactor-sprints/phase-08-gap-analysis.md`
  - Prioritised gaps are listed under "High", "Medium", and "Low".
  - Key gaps: `#/` lacks `/total`, Live Player lacks the channel-scope row,
    settings lacks the notes column, compact mode is a stub, and the Phase 8 docs
    still report stale status.
- `docs/tui-refactor-sprints/phase-08-delivery-plan.md`
  - P8A-P8E remain the intended engineering structure.
  - The plan requires 90x20 support, 90x24 preferred behavior, a persistent file
    pane, mode-aware work pane, and verification after each slice.
- `docs/tui-refactor-sprints/phase-08-tui-visual-ux-redesign.md`
  - The approved visual direction is Signal Desk.
  - Treat mockups as visual direction, not as a demand for non-portable terminal
    styling.
- `docs/tui-refactor-sprints/phase-08-mockups-v3/index.html`
  - Primary visual reference for Sequence, Live Player, Live Diagnostic,
    Settings, and Directory placeholder states.
- `docs/tui-refactor-sprints/phase-08-mockups-v3/styles.css`
  - Primary reference for column proportions, status rail compactness, and
    semantic color intent.
- Current implementation files:
  - `src/tui_render.c`
  - `src/tui_render.h`
  - `src/command_tui.c`
  - `src/tui_log.c`
  - `src/tui_log.h`
  - `tests/test_tui_log.c`
  - `tests/test_tui_model.c`
  - `Makefile`

## Non-Goals

- Do not implement the Phase 9 column directory browser. Keep the existing
  placeholder/manual-path hook.
- Do not add new dependencies.
- Do not introduce theme selection.
- Do not move CoreMIDI, playback, or recording ownership as part of this pass.
- Do not add sustain-pedal `HOLD` semantics unless it falls out naturally from a
  very small CC64 handling change. The gap analysis notes `HOLD`, but it is not
  one of the priority remediation items.
- Do not make color the only way to read state.

## Current Code Map

Renderer:

- `src/tui_render.c:26` defines internal `TuiLayout`; `compact` is computed at
  `src/tui_render.c:59` but not consumed by draw functions.
- `src/tui_render.c:186` draws the status rail. It currently prints
  `MIDI Capture <version> [` inline, uses `RX:ON`/`RX:--`, and reserves the
  right edge for the directory path.
- `src/tui_render.c:256` draws the file pane title as `Recordings`.
- `src/tui_render.c:304` and `src/tui_render.c:322` draw the Sequence header and
  rows with fixed narrow widths. The `#/` cell is 5 chars and only prints the
  event number.
- `src/tui_render.c:390` and `src/tui_render.c:407` draw Live Diagnostic.
- `src/tui_render.c:471` and `src/tui_render.c:487` draw Live Player note rows.
  There is no channel-scope controls row.
- `src/tui_render.c:563` draws settings as a single formatted two-column string.
- `src/tui_render.c:613` initializes eight semantic color pairs.

Command/UI state:

- `src/command_tui.c:56` defines per-note live state. It stores modulation,
  channel pressure, pitch bend, and last seen time only on notes that have
  already been seen.
- `src/command_tui.c:314` updates live note state from incoming MIDI bytes.
- `src/command_tui.c:390` logs structured MIDI fields and calls
  `update_live_note_state()`.
- `src/command_tui.c:1444` fills `TuiRenderLiveNoteRow` rows. It currently
  formats `number` as `1`, `2`, etc., not `1/5`.
- `src/command_tui.c:1497` assembles the renderer snapshot.
- `src/command_tui.c:1514` and `src/command_tui.c:1515` format source and
  destination labels as `Source [0]: ...` / `Destination [0]: ...`, while V3
  uses `SRC [0] ...` / `DST [0] ...`.

Tests:

- There are no dedicated curses renderer tests today.
- `tests/test_tui_log.c` covers structured log snapshot behavior.
- `tests/test_tui_model.c` covers pure formatting helpers.
- `Makefile` has no renderer test target.

## Remediation Strategy

Work in five small passes. Each pass should build and test cleanly before the
next pass starts.

1. **Safety and table correctness**
   - Fix `#/` rendering and table widths first because this is the clearest
     correctness/readability gap.
   - Add small pure helpers only if they make the behavior testable without
     contorting curses rendering.
2. **Live Player channel controls**
   - Add a minimal channel-level snapshot contract owned by `command_tui.c`.
   - Keep the renderer snapshot explicit; do not make the renderer infer channel
     state from note rows.
3. **Visible polish**
   - Tighten status rail labels, mode coloring, file pane title, and compact
     layout behavior.
   - Prefer small formatting changes over broad renderer rewrites.
4. **Settings discoverability**
   - Render settings as a three-column grid with notes/hints.
   - Keep the existing keyboard model and overlay state.
5. **Docs and verification**
   - Update stale Phase 8 status/checklists only after code verification.
   - Capture exactly what was tested and what remains manual or deferred.

## Detailed Work Plan

### Pass 0: Baseline And Guardrails

Goal: confirm the branch state and decide where test coverage is practical before
editing rendering code.

Tasks:

- Run `git status --short` and preserve existing untracked or user-owned files.
- Run `make clean`, `make`, and `make test` before code changes if time allows.
- Confirm whether the current environment can run a scripted TUI capture:
  `TERM=xterm ./midi-capture tui <recordings-dir>` under `script` or an
  equivalent terminal capture.
- If a scripted capture is not reliable, record that as a manual verification
  gap and still run the build/test suite.

Acceptance criteria:

- Starting verification state is known.
- No unrelated files are reverted or reformatted.
- The next pass has a clear verification route.

### Pass 1: Sequence `#/` And Column Widths

Goal: make Sequence rows show `event/total` and reduce clipping in the columns
called out by the gap analysis.

Relevant references:

- Gap: `#/` should show `21/128`, not just `21`.
- V3 mockup: `phase-08-mockups-v3/index.html` Sequence rows show `21/128`.
- V3 CSS: `.seq` uses `2ch 8ch 10ch 4ch 15ch 12ch 22ch ...`.
- Design brief: Sequence default `#/` width is 8, event width 14, target width
  12, value width 16-22.

Implementation tasks:

- In `src/tui_render.c`, change `draw_sequence_row()` to receive the total event
  count, or add a total field to `TuiRenderSequenceRow`.
  - Preferred: pass `state->sequence_event_count` from `draw_sequence_panel()`
    into `draw_sequence_row()` to avoid expanding the public render row unless
    later tests need it.
- Format the number cell as `%zu/%zu` using one-based row index and total count.
- Widen the `#/` column from 5 to 8 chars in both `draw_sequence_header()` and
  `draw_sequence_row()`.
- Replace the hardcoded `desc_width = width - 56` with named width calculations.
  Keep a compact width set for 90-column terminals and a default width set for
  wider terminals.
- Suggested minimum Sequence widths for compact terminals:
  - marker: 2
  - `#/`: 8
  - time: 8
  - ch: 3 or 4
  - event: 10
  - target: 8
  - value: 7
  - raw: 10
  - description: remaining
- Suggested default widths when `inner_width >= 80`:
  - marker: 2
  - `#/`: 8
  - time: 9 or 10
  - ch: 4
  - event: 14
  - target: 12
  - value: 12 or more
  - raw: 12-15
  - description: remaining
- Do not add a Sequence `dir` column.
- Keep raw bytes `A_DIM`.
- Keep the `>` marker as the selected/current affordance.

Acceptance criteria:

- A sequence with 128 events renders selected row numbers as `21/128`.
- At 90 columns, `#/` still has enough width for common totals.
- At about 120 columns, event/target/value clipping is visibly reduced.
- Raw bytes do not overwrite the value or description columns.

Verification:

- `make`
- `make test`
- Manual or scripted TUI capture with a loaded file containing more than 99
  events.
- Check 90x20, 90x24, and about 120x36 layouts.

### Pass 2: Live Player Channel-Scope Row

Goal: add the V3 channel controls row above Live Player note rows.

Relevant references:

- Gap: Live Player is missing a channel-scope row.
- V3 mockup: Live Player has a `scope / mod / pressure / pitch / last` header
  and a `CH 1 / 37 / 41 / +530 / RX 18ms` row before note rows.
- Design brief: channel pressure appears in a channel-level control row; pitch
  bend may start as a signed numeric value if a bar is too costly.

Implementation tasks:

- In `src/tui_render.h`, add a renderer-facing channel controls struct, for
  example:

  ```c
  typedef struct TuiRenderLiveControlRow {
    char scope[TUI_RENDER_FIELD_TEXT_LENGTH];
    char modulation[TUI_RENDER_FIELD_TEXT_LENGTH];
    char pressure[TUI_RENDER_FIELD_TEXT_LENGTH];
    char pitch[TUI_RENDER_FIELD_TEXT_LENGTH];
    char last_rx[TUI_RENDER_FIELD_TEXT_LENGTH];
    bool has_activity;
  } TuiRenderLiveControlRow;
  ```

- Add a `TuiRenderLiveControlRow live_controls;` field to `TuiRenderState`.
- In `src/command_tui.c`, add channel-level live state separate from
  `live_notes[16][128]`.
  - Suggested shape:

    ```c
    typedef struct TuiLiveChannelState {
      bool seen;
      uint8_t channel;
      uint8_t modulation;
      uint8_t pressure;
      int bend;
      uint64_t last_seen_nanos;
    } TuiLiveChannelState;
    ```

  - Store `TuiLiveChannelState live_channels[16];` in `TuiApp`.
- Update `update_live_note_state()` so:
  - note on/off/poly pressure updates `live_channels[channel].seen`,
    `.channel`, and `.last_seen_nanos`;
  - CC 1 updates `.modulation` and `.last_seen_nanos`;
  - channel pressure updates `.pressure` and `.last_seen_nanos`;
  - pitch bend updates `.bend` and `.last_seen_nanos`.
- Keep the existing per-note propagation if note rows still need modulation,
  pressure, and bend values. Do not remove note-row behavior while adding the
  aggregate row.
- Add a helper near `fill_live_note_rows()` to choose the most recent seen
  channel and format the renderer `live_controls` row.
  - If there is no channel activity, render `CH -`, `mod -`, `pressure -`,
    `bend +0`, and `RX -` or omit the row body but keep the header.
  - Use age text consistent with the rest of the TUI, e.g. `RX 18ms`, `RX 1.2s`.
- In `draw_live_player_panel()`, draw:
  - title row;
  - channel controls header;
  - channel controls row;
  - note rows header;
  - note rows.
- Adjust `visible` note-row calculation so the controls section does not overrun
  compact terminals.
- Update live note row numbering to `1/5`, `2/5`, etc. after the row count is
  known.
- Keep the row text-first. ASCII bars may be added only if compact-safe and
  numeric values remain visible.

Acceptance criteria:

- Live Player shows one channel-scope row above note rows.
- The row shows channel, modulation, pressure, pitch bend, and last RX age.
- The data can update from CC 1, channel pressure, pitch bend, and note events
  even when no currently active note carries that value.
- Note rows still show note, state, velocity, pressure, bend/mod, and age.
- Compact terminals continue to show a useful subset without overlapping rows.

Verification:

- `make`
- `make test`
- Manual live-input check with:
  - note on/off;
  - CC 1 modulation;
  - channel pressure;
  - pitch bend.
- Manual check with no recent MIDI activity.

### Pass 3: Status Rail, Mode Color, File Title, And Compact Behavior

Goal: close visible V3 divergences that are low-risk formatting changes.

Relevant references:

- V3 status rail uses `MIDI Capture`, a mode pill, `RX*`/`RX.`, `TX*`/`TX.`,
  compact `SRC [0] ...` / `DST [0] ...`, and right-aligned version.
- Gap analysis marks RX/TX format, mode coloring, file title, compact behavior,
  and version position as open items.

Implementation tasks:

- In `src/command_tui.c`, format endpoint labels as:
  - `SRC [0] <name>`
  - `DST [0] <name>`
  Keep `none` behavior for missing endpoints.
- In `draw_status_rail()`:
  - Print `MIDI Capture` without the version appended inline.
  - Render `state->app_version` near the right edge when width permits.
  - Use `RX*` when active and `RX.` when idle.
  - Use `TX*` when active and `TX.` when idle.
  - Keep directory path visible, but do not let it crowd out source/destination
    at 90 columns. If necessary, clip or omit the `DIR` segment before endpoint
    labels.
- Keep the status rail mode text text-first. Brackets are acceptable, but do not
  rely on color or reverse video alone.
- Add a helper for work-pane title attributes:
  - Sequence: bold/default.
  - Live Player: green when colors are available.
  - Live Diagnostic: yellow when colors are available.
  - Active recording/playback may still override the status rail mode label.
- Apply work-pane title attributes in `draw_sequence_panel()`,
  `draw_live_player_panel()`, and `draw_live_diagnostic_panel()`.
- Change the file pane title from `Recordings` to `Files`.
- Consume `layout.compact` in at least one real rendering branch:
  - shorten command-strip text at 90x20 through 90x23;
  - reduce optional Live Player detail width or row count;
  - shorten status rail labels if endpoint/path text would collide.
- Keep `A_REVERSE` limited to file list and overlay selection.

Acceptance criteria:

- Status rail shows compact `RX*`/`RX.` and `TX*`/`TX.` indicators.
- Version is no longer inline after `MIDI Capture`; it is right-positioned when
  space allows.
- Source/destination labels match the V3 compact shape.
- File pane title is `Files`.
- Live Player and Live Diagnostic work-pane titles use the intended semantic
  colors when colors are available.
- `layout.compact` changes at least one visible behavior in 90x20-90x23.

Verification:

- `make`
- `make test`
- Manual 90x20 and 90x24 checks.
- Manual no-color or limited-color terminal check, for example `TERM=vt100`,
  if supported by the local terminal environment.

### Pass 4: Settings Overlay Notes Column

Goal: make the settings overlay match the V3 three-column grid and improve
discoverability.

Relevant references:

- V3 settings grid columns are `setting`, `value`, and `notes`.
- Gap analysis calls out the missing third column.
- Existing settings keyboard behavior is already complete and should be kept.

Implementation tasks:

- In `draw_settings_overlay()`, replace the single `snprintf("%-22s %s", ...)`
  line with per-cell drawing using `draw_cell()`.
- Add a `notes[]` array next to the existing labels/values:
  - Recordings directory: `press d for browser`
  - Middle C: `C3/C4/C5 naming preference`
  - Note format: `name / number / hex`
  - Live fade: `2 / 5 / 10 / never`
  - Tempo: `playback speed reference`
  - Metronome: `recording click, single tone`
- Draw a header row: `setting`, `value`, `notes`.
- Keep the selected setting highlighted with `A_REVERSE`.
- Ensure long directory values clip inside the value column and do not overwrite
  notes.
- If the overlay is too narrow, prioritize setting and value columns and clip the
  notes column first.
- Do not change settings navigation or application behavior in this pass.

Acceptance criteria:

- Settings overlay has visible `setting`, `value`, and `notes` columns.
- Selected row remains readable.
- Long recordings directory values do not corrupt adjacent columns.
- Existing `up/down`, `left/right`, `enter`, `,`, and `esc` behavior remains
  unchanged.

Verification:

- `make`
- `make test`
- Manual settings overlay check at 90x20, 90x24, and about 120x36.

### Pass 5: Documentation Closure

Goal: make Phase 8 docs reflect the actual implementation and remediation state.

Implementation tasks:

- Update `docs/tui-refactor-sprints/phase-08-tui-visual-ux-redesign.md`:
  - Change `**Status:** Not started` to the correct post-remediation status.
    Use `Implemented` only if the remediation code has landed and verification
    evidence is recorded.
  - Add a short remediation evidence note under the existing Verification
    section.
  - Keep the Phase 9 directory-browser limitation explicit.
- Update `docs/tui-refactor-sprints/phase-08-delivery-plan.md`:
  - Mark completed P8A-P8E checkboxes only after verification.
  - If a task remains deliberately deferred, annotate it rather than silently
    checking it off.
- Update `docs/tui-refactor-sprints/phase-08-gap-analysis.md` or create a short
  follow-up note:
  - Mark remediated gaps with evidence.
  - Leave unresolved gaps explicit with owner/phase.

Acceptance criteria:

- Docs no longer claim Phase 8 is "Not started" after implementation evidence is
  present.
- Delivery-plan checkboxes and status text match verified code, not intent.
- Remaining deferred work is named and scoped.

Verification:

- Read the rendered Markdown diff.
- Run `git diff --check`.

## Recommended Implementation Order

1. Baseline build/test.
2. Sequence `#/` and width remediation.
3. Live Player channel controls data model and rendering.
4. Status rail and compact visible polish.
5. Settings notes column.
6. Docs/status/checklist updates.
7. Final `make clean`, `make`, `make test`, and manual TUI pass.

Do not reorder docs closure before code verification. The current docs already
overstate and understate different parts of Phase 8; close them only with fresh
evidence.

## Acceptance Criteria For The Whole Remediation

- Sequence mode renders `event/total` in the `#/` column.
- Sequence columns remain aligned and clipped at 90x20, 90x24, and about 120x36.
- Live Player renders a channel-scope row with channel, modulation, pressure,
  pitch bend, and last RX age.
- Live Player note rows still render with numeric values and readable state.
- Status rail uses compact RX/TX indicators and no longer inlines the version
  after the app label.
- File pane title matches V3: `Files`.
- Settings overlay shows setting, value, and notes columns.
- `layout.compact` has visible behavior at 90x20-90x23.
- Monochrome/limited-color terminals still have text labels for every important
  state.
- Phase 8 docs reflect the remediated state and list any remaining deferrals.
- `make clean`, `make`, and `make test` pass.

## Manual Verification Checklist

Use this checklist for the final pass:

- 90x20:
  - resize guard does not trigger;
  - content does not overlap;
  - compact command/status behavior is visible;
  - file pane and work pane remain usable.
- 90x24:
  - preferred minimum has readable Sequence table;
  - Settings overlay fits;
  - Live Player controls row plus at least a useful subset of note rows is shown.
- About 120x36:
  - Sequence `#/` shows examples like `21/128`;
  - event/target/value columns are less clipped than before;
  - Live Diagnostic still shows all eight columns.
- Wide terminal:
  - file pane remains capped;
  - extra width benefits work-pane descriptions, not the file list.
- Live MIDI input:
  - note on/off changes Live Player note rows;
  - CC 1 changes channel modulation;
  - channel pressure changes channel pressure;
  - pitch bend changes signed pitch value;
  - last RX age updates.
- Settings:
  - `,` opens settings;
  - up/down changes selection;
  - left/right changes values;
  - recordings directory row hints `press d for browser`;
  - selected row remains readable.
- Directory placeholder:
  - `d` still opens the Phase 9 placeholder/manual-path hook;
  - no Phase 9 browser behavior is accidentally introduced.
- Color/accessibility:
  - Live Player and Live Diagnostic labels have semantic color in color terminals;
  - no-color/limited-color mode remains understandable through labels and layout.

## Risks And Mitigations

| Risk | Mitigation |
| --- | --- |
| Live controls row expands scope into a full channel mixer | Add only one aggregate row for the most recent channel. Defer channel selection and mixer behavior. |
| Column width changes break 90-column terminals | Use compact/default width sets and verify 90x20 and 90x24 before widening defaults. |
| Channel state duplicated between notes and aggregate row | Treat channel state as authoritative for aggregate controls; keep per-note copied values only for note-row display. |
| Curses rendering is hard to unit test | Test pure formatting helpers where practical, then require scripted/manual TUI captures for visual layout. |
| Docs get checked off prematurely | Update status/checklists only after final build/test/manual evidence is recorded. |

## Remaining Deferrals After This Plan

These should stay out of this remediation unless explicitly reprioritized:

- Full Phase 9 directory browser columns/navigation/apply flow.
- Sustain pedal `HOLD` state.
- Theme selection.
- Full visual ASCII bars for every numeric value if they make compact layouts
  worse. Numeric text is required; bars are optional polish.
- Future screen-reader-friendly status dump mode.
