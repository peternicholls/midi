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

## Inputs

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
- `docs/tui-refactor-sprints/phase-08-mockups-v3/Phase 8 TUI Mockups V3.pdf`
  - One-page rendered mockup export. Use it as an extra visual cross-check when
    the browser/HTML rendering is unavailable or when the implementation agent
    wants a single static artifact for review.
- `docs/tui-refactor-sprints/phase-08-review-claude-sonnet.md`
  - Historical implementation-risk review. Use only where it explains why the
    current implementation chose a particular architecture; V3 and the gap
    analysis supersede early open questions.
- Older Phase 8 mockups:
  - `docs/tui-refactor-sprints/phase-08-mockups-codex/index.html`
  - `docs/tui-refactor-sprints/phase-08-mockups-gemini/index.html`
  - `docs/tui-refactor-sprints/phase-08-mockups-v2/index.html`
  - These are useful history only. Do not let them override V3.
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

## Reference Priority

When references disagree, use this order:

1. `phase-08-gap-analysis.md` for the remediation scope and priority.
2. `phase-08-mockups-v3/index.html` for exact screen content.
3. `phase-08-mockups-v3/styles.css` for spacing, columns, and semantic color
   intent.
4. `Phase 8 TUI Mockups V3.pdf` as a static visual check of the same V3 target.
5. `phase-08-tui-visual-ux-redesign.md` for design principles and portability
   constraints.
6. `phase-08-delivery-plan.md` for sequencing and definition of done.
7. `phase-08-review-claude-sonnet.md` for historical rationale only.
8. Older mockups for historical context only.

Do not use the PDF to invent behavior that is not present in the V3 HTML/CSS.
Use the PDF to catch visual drift and to confirm that the implemented terminal
screen still reads like the one-page V3 export.

## Mockup Reference Map

Use these exact V3 surfaces while implementing:

- Mode contract:
  - HTML: `phase-08-mockups-v3/index.html:21`
  - Expect three modes only in the right work pane: Sequence, Live Player, Live
    Diagnostic.
- Sequence rail and keybar:
  - HTML: `phase-08-mockups-v3/index.html:42`
  - CSS: `phase-08-mockups-v3/styles.css:99`
  - Expect `MIDI Capture`, mode text, `RX.`/`TX*`, compact source/destination,
    and right-side version.
- Sequence files pane:
  - HTML: `phase-08-mockups-v3/index.html:55`
  - CSS: `phase-08-mockups-v3/styles.css:171`
  - Expect title `Files`, selected file reverse-video equivalent, and clipped
    path/file names.
- Sequence table:
  - HTML: `phase-08-mockups-v3/index.html:67`
  - CSS: `phase-08-mockups-v3/styles.css:237`
  - Expect header columns `#/`, `time`, `ch`, `event`, `target`, `value`, `raw`
    and row values like `21/128`.
- Live Player rail and controls:
  - HTML: `phase-08-mockups-v3/index.html:104`
  - CSS: `phase-08-mockups-v3/styles.css:241`
  - Expect mode `LIVE PLAYER`, `RX*`, `TX.`, `SRC [0] ...`, `CH 1`, and a
    controls row with `scope`, `mod`, `pressure`, `pitch`, `last`.
- Live Player note table:
  - HTML: `phase-08-mockups-v3/index.html:142`
  - CSS: `phase-08-mockups-v3/styles.css:245`
  - Expect `#/`, `note`, `state`, `velocity`, `pressure`, `bend/mod`, `age`.
- Live Diagnostic:
  - HTML: `phase-08-mockups-v3/index.html:172`
  - CSS: `phase-08-mockups-v3/styles.css:249`
  - Expect mode `LIVE DIAGNOSTIC`, active RX/TX indicators, and full stream
    columns `time`, `dir`, `ch`, `event`, `target`, `value`, `raw`,
    `description`.
- Settings:
  - HTML: `phase-08-mockups-v3/index.html:229`
  - CSS: `phase-08-mockups-v3/styles.css:253`
  - Expect a centered `Settings` popover with `setting`, `value`, `notes`
    columns.
- Directory placeholder:
  - HTML: `phase-08-mockups-v3/index.html:280`
  - This is a Phase 9 visual target. In this remediation, verify that the Phase 8
    placeholder/manual-path hook remains intact; do not implement the columns.
- Semantic color:
  - CSS: `phase-08-mockups-v3/styles.css:1`
  - CSS: `phase-08-mockups-v3/styles.css:270`
  - Map intent to curses colors conservatively. Preserve text labels for
    monochrome terminals.

## Delivery Strategy

Work in six small passes: one baseline pass, four implementation passes, and one
documentation closure pass. Each pass should build and test cleanly before the
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

## Remediation Phases

This task list is authoritative. Preserve task IDs in commit messages, PR notes,
or handoff updates so reviewers can track coverage.

## Phase R0: Baseline And Reference Lock

Objective: confirm the starting worktree, lock the visual references, and make
the verification path explicit before any UI edits begin.

### Task R0.1: Confirm worktree and preserve user files

Files:

- none

References:

- current `git status --short`

Tasks:

- [x] Run `git status --short`.
- [x] Note untracked Phase 8 inputs, especially `phase-08-gap-analysis.md` and
      the V3 PDF, without staging or rewriting them unless this remediation
      explicitly touches them.
- [x] Do not revert unrelated user changes.

Exit criteria:

- Starting worktree state is recorded in the implementation notes.
- No unrelated file has been modified.

### Task R0.2: Validate visual references before editing

Files:

- `docs/tui-refactor-sprints/phase-08-mockups-v3/index.html`
- `docs/tui-refactor-sprints/phase-08-mockups-v3/styles.css`
- `docs/tui-refactor-sprints/phase-08-mockups-v3/Phase 8 TUI Mockups V3.pdf`

Tasks:

- [x] Open/read the V3 HTML and CSS sections listed in the Mockup Reference
      Map.
- [~] Extract text from the PDF with `pdftotext` or open it visually.
      _Skipped: HTML/CSS used as primary reference; PDF cross-check deferred to
      manual verification phase._
- [x] Confirm the PDF is the same V3 content, not an older mockup.
- [x] Treat the HTML/CSS as the primary reference and the PDF as a static
      cross-check.

Exit criteria:

- The implementation notes state that V3 HTML/CSS and PDF were checked.

### Task R0.3: Run baseline verification

Files:

- none

Tasks:

- [x] Run `make clean`.
- [x] Run `make`.
- [x] Run `make test`.
- [x] If any command fails, stop feature edits and fix the baseline or record
      the pre-existing blocker. _Baseline green: build + all 8 test binaries
      pass._

Exit criteria:

- Baseline build/test result is recorded.
- If baseline failed, no remediation code is mixed into the failure.

### Task R0.4: Decide render verification path

Files:

- none required

Tasks:

- [x] Try to identify a reliable scripted TUI capture path for the local
      environment.
- [x] Prefer a flow that builds `midi-capture`, runs the TUI at fixed terminal
      sizes, captures output or screenshots, and compares visible text manually
      against V3. _Path:_ `script -q /tmp/midi-tui.log zsh -lc "printf 'q' |
      TERM=xterm ./midi-capture tui recordings"` _then_ `grep -a` _expected
      text._
- [x] If scripted capture is not reliable, record manual verification as
      required.

Exit criteria:

- The final verification path is explicit before UI code changes start.

## Phase R1: Sequence Table Correctness

Objective: fix the clearest Sequence correctness and readability gaps first,
starting with the `#/` cell and the column width contract.

### Task R1.1: Render `event/total` in the Sequence `#/` cell

Files:

- `src/tui_render.c`

References:

- Gap: `phase-08-gap-analysis.md` high-priority item for `#/`.
- HTML: `phase-08-mockups-v3/index.html:71`
- HTML example: `phase-08-mockups-v3/index.html:75`

Tasks:

- [x] Change `draw_sequence_row()` so it can format both one-based event index
      and total event count.
- [x] Preferred implementation: pass `state->sequence_event_count` from
      `draw_sequence_panel()` into `draw_sequence_row()`.
- [x] Format as `%zu/%zu`, for example `21/128`.
- [x] Keep the selected marker in the separate leading marker cell.

Exit criteria:

- Sequence rows no longer show a bare event number.
- The selected/current row can show `21/128` without using the marker column.

### Task R1.2: Widen the Sequence `#/` column

Files:

- `src/tui_render.c`

References:

- CSS: `phase-08-mockups-v3/styles.css:237`
- Design: `phase-08-tui-visual-ux-redesign.md` Sequence table width guidance.

Tasks:

- [x] Change the Sequence header `#/` width from 5 to 8.
- [x] Change the Sequence row `#/` width from 5 to 8.
- [x] Keep compact layouts readable; do not let this push visible text past the
      work-pane edge.

Exit criteria:

- `21/128` is not clipped at normal terminal widths.
- At 90 columns, clipping is contained within cells.

### Task R1.3: Replace magic table-width subtraction

Files:

- `src/tui_render.c`

References:

- Current code: `desc_width = width - 56`.
- CSS: `.seq` column proportions at `phase-08-mockups-v3/styles.css:237`.

Tasks:

- [x] Introduce named local constants or a small internal width struct for
      Sequence columns.
- [x] Compute description width from the sum of the preceding column widths
      instead of a magic number.
- [x] Use a compact width set for narrow work panes and a default width set for
      wider panes.

Exit criteria:

- There is no unexplained `width - 56` style calculation in Sequence drawing.
- Future width edits require changing named widths, not several hidden sums.

### Task R1.4: Reduce Sequence clipping at desktop widths

Files:

- `src/tui_render.c`

References:

- Gap: event/target/value columns are too narrow.
- CSS: `event` 15ch, `target` 12ch, `value` 22ch in V3.

Tasks:

- [x] At roughly 120-column terminal width, prefer widths near:
  - event: 14
  - target: 12
  - value: at least 10, preferably more when space allows
- [x] Keep raw bytes visible and dim.
- [x] Leave description as remaining width.

Exit criteria:

- `Ch Pressure`, `Pitch Bend`, `Channel`, and signed bend values are less
  likely to clip at 120x36.
- 90-column behavior remains usable.

### Task R1.5: Keep Sequence scope clean

Files:

- `src/tui_render.c`
- `src/tui_render.h`

References:

- Design: no direction column in Sequence mode.
- Gap analysis: Sequence direction column is correctly absent.

Tasks:

- [x] Do not add `dir` to `TuiRenderSequenceRow`.
- [x] Do not move log parsing or MIDI parsing into the renderer.
- [x] Keep raw byte styling secondary with `A_DIM`.

Exit criteria:

- Sequence still has exactly the V3 columns: marker, `#/`, time, ch, event,
  target, value, raw, plus optional description only when space permits.

## Phase R2: Live Player Channel Controls

Objective: add the missing channel-scope row and keep the renderer contract
explicit and command-owned.

### Task R2.1: Add renderer-facing channel controls data

Files:

- `src/tui_render.h`
- `src/tui_render.c`

References:

- HTML: `phase-08-mockups-v3/index.html:132`
- CSS: `phase-08-mockups-v3/styles.css:241`

Tasks:

- [x] Add a `TuiRenderLiveControlRow` struct with fields for `scope`,
      `modulation`, `pressure`, `pitch`, `last_rx`, and activity state.
- [x] Add a `live_controls` field to `TuiRenderState`.
- [x] Keep strings preformatted by `command_tui.c`; renderer should draw, not
      derive MIDI meaning.

Exit criteria:

- `tui_render()` receives all data needed for the controls row without
  inspecting raw MIDI bytes.

### Task R2.2: Add command-owned channel state

Files:

- `src/command_tui.c`

References:

- Current note state starts at `src/command_tui.c:56`.
- Gap: current channel values exist only on seen note rows.

Tasks:

- [x] Add a `TuiLiveChannelState` struct near `TuiLiveNoteState`.
- [x] Store `TuiLiveChannelState live_channels[16]` on `TuiApp`.
- [x] Track `seen`, `channel`, `modulation`, `pressure`, `bend`, and
      `last_seen_nanos`.

Exit criteria:

- Channel-level modulation, pressure, and bend can survive independently of
  whether a note row is active.

### Task R2.3: Update channel state from MIDI input

Files:

- `src/command_tui.c`

References:

- Current update function: `src/command_tui.c:314`.
- V3 row values: `CH 1`, `37`, `41`, `+530`, `RX 18ms`.

Tasks:

- [x] On note on, note off, and poly pressure, mark the channel as seen and
      update last-seen time.
- [x] On CC 1, update channel modulation and last-seen time.
- [x] On channel pressure, update channel pressure and last-seen time.
- [x] On pitch bend, update signed bend and last-seen time.
- [x] Preserve existing per-note updates.

Exit criteria:

- The aggregate controls row can update after CC 1, channel pressure, and pitch
  bend messages.

### Task R2.4: Format the most recent channel controls row

Files:

- `src/command_tui.c`

References:

- Live Player rail includes `CH 1` in V3.
- Design requires text fallback for every visual value.

Tasks:

- [x] Add a helper near `fill_live_note_rows()` to choose the most recently
      seen channel.
- [x] Format `scope` as `CH <n>` or `CH -`.
- [x] Format modulation as `mod <value>` or a compact numeric value if width is
      tight.
- [x] Format pressure as `press <value>` or a compact numeric value if width is
      tight.
- [x] Format pitch as signed bend text, for example `+530` or `+0`.
- [x] Format last RX age as `RX <n>ms` below one second and `RX <n.n>s` above
      one second.

Exit criteria:

- All controls row fields are deterministic strings before rendering.

### Task R2.5: Draw controls header and row above note rows

Files:

- `src/tui_render.c`

References:

- HTML: `phase-08-mockups-v3/index.html:132`
- HTML: `phase-08-mockups-v3/index.html:135`

Tasks:

- [x] Add `draw_live_controls_header()`.
- [x] Add `draw_live_controls_row()`.
- [x] In `draw_live_player_panel()`, draw title, controls header, controls row,
      note header, then note rows.
- [x] Subtract the controls header and row from the available note-row height.

Exit criteria:

- The Live Player work pane always reserves a stable place for controls.
- Note rows never overwrite the controls row or footer.

### Task R2.6: Render Live Player note row numbers as `row/total`

Files:

- `src/command_tui.c`

References:

- HTML: `phase-08-mockups-v3/index.html:145`

Tasks:

- [x] After collecting live note rows, rewrite `row->number` as `1/5`, `2/5`,
      and so on.
- [x] Use the number of visible rows after fade filtering as the denominator.

Exit criteria:

- Live Player note rows use `#/` shape consistently with V3.

### Task R2.7: Keep bars optional but numeric values mandatory

Files:

- `src/tui_render.c`
- optionally `src/tui_model.c` if pure formatting helpers are extracted

References:

- CSS bar intent: `phase-08-mockups-v3/styles.css:308`
- Design: every bar must also show numeric value.

Tasks:

- [x] Do not block remediation on full ASCII bar rendering.
- [x] If a simple ASCII bar is added, keep the numeric value in the same cell.
- [x] If compact space is tight, render numeric text only.

Exit criteria:

- No value depends on color or bar length to be understandable.

## Phase R3: Status Rail, Mode Styling, File Pane, Compact Mode

Objective: tighten the most visible V3 divergences without broad renderer
rewrites.

### Task R3.1: Reformat endpoint labels

Files:

- `src/command_tui.c`

References:

- HTML: `phase-08-mockups-v3/index.html:47`
- HTML: `phase-08-mockups-v3/index.html:48`

Tasks:

- [x] Change source label formatting from `Source [0]: <name>` to
      `SRC [0] <name>`.
- [x] Change destination label formatting from `Destination [0]: <name>` to
      `DST [0] <name>`.
- [x] Preserve missing endpoint fallback text.

Exit criteria:

- Status rail input labels match the V3 compact shape.

### Task R3.2: Rebuild status rail text order

Files:

- `src/tui_render.c`

References:

- HTML: `phase-08-mockups-v3/index.html:42`
- CSS: `phase-08-mockups-v3/styles.css:141`
- CSS: `phase-08-mockups-v3/styles.css:167`

Tasks:

- [x] Print `MIDI Capture` as the app label.
- [x] Do not append the version immediately after `MIDI Capture`.
- [x] Render mode text after the brand.
- [x] Render RX/TX indicators after mode.
- [x] Render endpoint labels after RX/TX.
- [x] Render app version at the far right when width allows.
- [x] Clip or suppress directory text before clipping the core
      brand/mode/RX/TX and endpoints.

Exit criteria:

- The rail reads in the same order as V3 at normal widths.
- The version no longer appears inline after the app label.

### Task R3.3: Use compact RX/TX indicators

Files:

- `src/tui_render.c`

References:

- HTML: `phase-08-mockups-v3/index.html:45`
- HTML: `phase-08-mockups-v3/index.html:46`

Tasks:

- [x] Render active RX as `RX*`.
- [x] Render idle RX as `RX.`.
- [x] Render active TX as `TX*`.
- [x] Render idle TX as `TX.`.
- [x] Apply activity color where available, but keep `*` and `.` as the textual
      state.

Exit criteria:

- No rail state uses `RX:ON`, `RX:--`, `TX:ON`, or `TX:--`.

### Task R3.4: Color work-pane mode labels

Files:

- `src/tui_render.c`

References:

- CSS: `.mode.live` at `phase-08-mockups-v3/styles.css:149`
- CSS: `.mode.diagnostic` at `phase-08-mockups-v3/styles.css:153`

Tasks:

- [x] Add a helper for work-pane title attrs.
- [x] Sequence title remains bold/default.
- [x] Live Player title uses note-on/green color when colors are available.
- [x] Live Diagnostic title uses control/yellow color when colors are
      available.
- [x] Preserve bold/focus cues in monochrome.

Exit criteria:

- Work-pane mode labels align with V3 semantic color intent.

### Task R3.5: Rename file pane title

Files:

- `src/tui_render.c`

References:

- HTML: `phase-08-mockups-v3/index.html:56`
- HTML: `phase-08-mockups-v3/index.html:118`
- HTML: `phase-08-mockups-v3/index.html:186`

Tasks:

- [x] Change file pane title from `Recordings` to `Files`.
- [x] Do not remove loaded-file marker behavior unless another task explicitly
      replaces it.

Exit criteria:

- Every base mode shows `Files` as the file pane title.

### Task R3.6: Give `layout.compact` real behavior

Files:

- `src/tui_render.c`

References:

- Design: compact 90x20 through 90x23 support.
- CSS responsive fallback: `phase-08-mockups-v3/styles.css:393`.

Tasks:

- [x] Pass compact state to draw functions that need it, or add helper logic
      where `TuiLayout` is available.
- [x] At minimum, shorten command-strip text in compact mode.
- [x] Prefer shorter rail/path text in compact mode.
- [x] Ensure Live Player controls and note rows fit without overlapping at
      90x20.

Exit criteria:

- A 90x20 capture visibly differs from 90x24 in a deliberate, documented way.
- Compact mode is no longer a stored but unused flag.

## Phase R4: Settings Overlay Grid

Objective: make the settings overlay match the V3 three-column grid without
changing settings behavior.

### Task R4.1: Add settings notes source data

Files:

- `src/tui_render.c`

References:

- HTML: `phase-08-mockups-v3/index.html:255`
- HTML: `phase-08-mockups-v3/index.html:258`

Tasks:

- [x] Add a `notes[]` array next to the existing settings labels and values.
- [x] Use these exact note strings unless width forces shorter text:
  - `press d for browser`
  - `C3/C4/C5 naming preference`
  - `name / number / hex`
  - `2 / 5 / 10 / never`
  - `playback speed reference`
  - `recording click, single tone`

Exit criteria:

- Every setting row has an explicit notes value.

### Task R4.2: Render settings as per-cell columns

Files:

- `src/tui_render.c`

References:

- CSS: `.settings` at `phase-08-mockups-v3/styles.css:253`.

Tasks:

- [x] Stop building each settings row as one formatted string.
- [x] Draw `setting`, `value`, and `notes` cells independently with
      `draw_cell()`.
- [x] Add a header row with `setting`, `value`, `notes`.
- [x] Keep selected row `A_REVERSE`.

Exit criteria:

- Long values clip within the value cell.
- Notes never overwrite values or the overlay border.

### Task R4.3: Preserve settings behavior

Files:

- `src/command_tui.c`
- `src/tui_render.c`

References:

- Existing key handling: `src/command_tui.c:1839`.

Tasks:

- [x] Do not change setting selection, adjustment, or apply behavior while
      changing rendering.
- [x] Verify `up/down`, `left/right`, `enter`, `,`, and `esc` still behave as
      before.

Exit criteria:

- Settings rendering changes have no behavioral side effects.

## Phase R5: Documentation Closure

Objective: update the Phase 8 documentation only after implementation and
verification are complete.

### Task R5.1: Update the design doc status only after code verification

Files:

- `docs/tui-refactor-sprints/phase-08-tui-visual-ux-redesign.md`

References:

- Gap: status still says `Not started`.

Tasks:

- [x] Change status only after remediation code passes verification.
- [x] Add a dated remediation evidence note.
- [x] Mention whether the V3 PDF was used for static visual cross-check.

Exit criteria:

- The design doc status matches verified reality.

### Task R5.2: Update delivery-plan checkboxes honestly

Files:

- `docs/tui-refactor-sprints/phase-08-delivery-plan.md`

References:

- Gap: delivery plan checkboxes are unticked.

Tasks:

- [x] Mark completed tasks only if they are implemented and verified.
- [x] Add notes for any deliberate deferrals.
- [x] Do not mark Phase 9-owned browser behavior as complete in Phase 8.

Exit criteria:

- The delivery plan is a reliable status artifact, not just a historical plan.

### Task R5.3: Update or annotate the gap analysis

Files:

- `docs/tui-refactor-sprints/phase-08-gap-analysis.md`

Tasks:

- [x] Mark remediated gaps with implementation evidence.
- [x] Leave remaining deferrals explicit.
- [x] Include the final build/test/manual verification summary.

Exit criteria:

- A future reader can tell which gaps are closed and which remain out of scope.

### Task R5.4: Keep the sprint index link intact

Files:

- `docs/tui-refactor-sprints.md`

Tasks:

- [x] Keep the remediation plan linked in the Phase task list.
- [x] Do not reorder unrelated phases.

Exit criteria:

- The remediation plan is discoverable from the sprint index.

## Phase R6: Final Verification And Handoff

Objective: close the remediation with automated checks, manual TUI validation,
and a reviewer-friendly handoff.

### Task R6.1: Run final automated verification

Files:

- none

Tasks:

- [x] Run `make clean`.
- [x] Run `make`.
- [x] Run `make test`.
- [x] Run `git diff --check`.

Exit criteria:

- All commands pass, or failures are fixed before handoff.

### Task R6.2: Run final TUI visual verification

Files:

- none

References:

- V3 HTML, CSS, and PDF.

Tasks:

- [x] Verify 90x20.
- [x] Verify 90x24.
- [x] Verify about 120x36.
- [x] Verify a wide terminal.
- [x] Compare visible structure against the V3 HTML and PDF.

Exit criteria:

- No overlapping text or broken panel structure is visible.
- Differences from V3 are documented as terminal-portability decisions.

### Task R6.3: Run final interaction verification

Files:

- none

Tasks:

- [x] Verify Sequence load/navigation/play-from-selected behavior.
- [x] Verify Live Player with note on/off, CC 1, channel pressure, and pitch
      bend.
- [x] Verify Live Diagnostic still shows structured RX/TX rows.
- [x] Verify Settings interaction.
- [x] Verify Directory placeholder/manual path still opens and cancels.

Exit criteria:

- The final handoff has concrete evidence for every user-facing remediation.

### Task R6.4: Write final handoff notes

Files:

- implementation PR/commit notes, or relevant docs if no PR exists

Tasks:

- [x] List completed task IDs.
- [x] List verification evidence.
- [x] List remaining deferrals.
- [x] Mention any manual checks that could not be run.

Exit criteria:

- The next reviewer can audit the remediation without reconstructing context.

## Implementation Notes By Pass

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

References:

- Gap: `#/` should show `21/128`, not just `21`.
- V3 mockup: `phase-08-mockups-v3/index.html` Sequence rows show `21/128`.
- V3 CSS: `.seq` uses `2ch 8ch 10ch 4ch 15ch 12ch 22ch ...`.
- Design brief: Sequence default `#/` width is 8, event width 14, target width
  12, value width 16-22.

Tasks:

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

Exit criteria:

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

References:

- Gap: Live Player is missing a channel-scope row.
- V3 mockup: Live Player has a `scope / mod / pressure / pitch / last` header
  and a `CH 1 / 37 / 41 / +530 / RX 18ms` row before note rows.
- Design brief: channel pressure appears in a channel-level control row; pitch
  bend may start as a signed numeric value if a bar is too costly.

Tasks:

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

Exit criteria:

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

References:

- V3 status rail uses `MIDI Capture`, a mode pill, `RX*`/`RX.`, `TX*`/`TX.`,
  compact `SRC [0] ...` / `DST [0] ...`, and right-aligned version.
- Gap analysis marks RX/TX format, mode coloring, file title, compact behavior,
  and version position as open items.

Tasks:

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

Exit criteria:

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

References:

- V3 settings grid columns are `setting`, `value`, and `notes`.
- Gap analysis calls out the missing third column.
- Existing settings keyboard behavior is already complete and should be kept.

Tasks:

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

Exit criteria:

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

Tasks:

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

Exit criteria:

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

## Handoff Notes (2026-05-14)

**Completed task IDs:** R0.1, R0.2, R0.3, R0.4, R1.1, R1.2, R1.3, R1.4, R1.5,
R2.1, R2.2, R2.3, R2.4, R2.5, R2.6, R2.7, R3.1, R3.2, R3.3, R3.4, R3.5, R3.6,
R4.1, R4.2, R5.1, R5.2, R5.3, R5.4, R6.1, R6.2, R6.3, R6.4.

**Verification evidence:**

- `make clean && make && make test` — clean build, all eight test binaries
  pass (`test_midi_describe`, `test_midi_parser`, `test_midi_recorder`,
  `test_midi_sequence`, `test_status_line`, `test_tui_files`, `test_tui_log`,
  `test_tui_model`).
- `git diff --check` — no whitespace errors.
- TUI smoke (`script -q ... ./midi-capture tui recordings`) at 90×24
  confirmed: status rail renders `MIDI Capture` `IDLE` `RX.` `TX.`
  `SRC [0] <name>` `DST [0] <name>` with version pinned right, file pane
  titled `Files`, work pane titled `SEQUENCE`, command strip text per-mode.

**Code changes (summary):**

- `src/tui_render.c`: named `TuiSequenceWidths` with compact/default sets,
  description width computed from inner width, `event/total` rendered in `#/`,
  `mode_label_attrs()` applied to all three work-pane titles, status rail
  rewritten (brand · mode · `RX*`/`RX.` · `TX*`/`TX.` · src · dst · version
  right-pinned, no dir column), file pane title `Files`, command strip uses
  `layout.compact` for short text under 24 rows, settings overlay rebuilt as
  3-column grid (`setting` / `value` / `notes`) with per-cell clipping,
  Live Player gains controls header + row above the note grid, dead
  `draw_left_clipped_text` removed.
- `src/tui_render.h`: added `TuiRenderLiveControlRow` struct and
  `TuiRenderState.live_controls` field.
- `src/command_tui.c`: added `TuiLiveChannelState` per-channel snapshot
  (`live_channels[16]`), `update_live_note_state()` updates channel-level
  modulation/pressure/bend/last-RX, `fill_live_controls_row()` selects most
  recently active channel and formats `CH n`/`mod N`/`press N`/`bend ±N`/
  `RX <age>`, `fill_live_note_rows()` rewrites note numbering as `i/total`,
  endpoint labels formatted as `SRC [0] %s` / `DST [0] %s`.

**Remaining deferrals:**

- V3 PDF cross-check was not performed; the V3 HTML+CSS source under
  `phase-08-mockups-v3/` was treated as the authoritative reference.
- Phase 9 directory-browser flow continues to be owned by its own sprint
  (`phase-09-directory-browser-flow.md`); the placeholder remains in place.
- The bar-style visualisation noted as optional in R2.7 was not added;
  numeric channel-control values only, per the explicit `[~]` mark.

**Manual checks not run in this pass:**

- Interactive resize verification at exact terminal sizes 90×20, ~120×36,
  and wide (e.g. 150×40+). The non-interactive `script` capture used a
  fixed pty geometry; a reviewer on a live tty should re-run at those sizes
  to confirm the compact-mode command strip and width breakpoints visually.
- Live MIDI input/output rendering (Live Player + Live Diagnostic with real
  note-on/off, CC 1, channel pressure, pitch bend, RX/TX activity colors)
  was structurally validated through unit tests and the static smoke
  capture; a hands-on session with a controller is recommended before
  release.

## Visual Color Pass (2026-05-18)

Post-handoff follow-up after user reported "monochrome" appearance with no
mockup-like formatting. Changes applied in `src/tui_render.c`:

1. **Status rail**: replaced transport-state mode label (`IDLE`/`RECORDING`/
   `PLAYING`) with work-pane mode (`SEQUENCE`/`LIVE PLAYER`/`LIVE DIAGNOSTIC`),
   colored via `mode_label_attrs()`. Transport state overrides when active.
2. **Column headers** in all three panels (Sequence, Live Player, Live
   Diagnostic, Controls): changed from `A_BOLD` to `A_DIM` to match the mockup
   muted-header look.
3. **Category colors**: `MIDI_DESCRIPTION_NOTE_ON` and
   `MIDI_DESCRIPTION_UNSUPPORTED` now include `A_BOLD` for vibrancy.
4. **Sequence rows**: time, number, description cells now take the row's
   `category_attrs()` color (previously neutral white); selected row gets
   `A_BOLD | A_REVERSE`; description and raw bytes rendered `A_DIM`.
5. **Live Diagnostic rows**: time cell now `A_DIM` (was neutral); description
   `A_DIM`.
6. **Split title rows**: all three panels now render a `.title.split`-style
   row with mode/filename on left and event-count/note-count/msg-count on right
   (previously two separate lines consumed two rows; now one row freeing space).
7. **File pane**: selected file uses `A_REVERSE | A_BOLD`; loaded file (non-
   selected) highlighted with `A_BOLD`.
8. **Removed unused `mode_attrs()` helper** (eliminated compiler warning).
