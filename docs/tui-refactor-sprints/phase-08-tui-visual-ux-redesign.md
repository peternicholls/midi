# Phase 8: TUI Visual And UX Redesign

**Depends on:** Phase 7

**Status:** Not started

## Objective

Give the TUI a clear visual language and a more useful monitoring layout while
preserving the fast, dense utility feel of a terminal instrument panel.

The current app is functional but plain: a header, side-by-side files/events
area, short live stream, and footer. Phase 8 should turn that into a
purposeful MIDI workbench: files stay available at full height, the right-side
work pane switches between task-specific modes, and color communicates message
meaning rather than decoration.

Static HTML/CSS mockups live alongside this plan:

- [`phase-08-mockups-codex/index.html`](phase-08-mockups-codex/index.html)
- [`phase-08-mockups-gemini/index.html`](phase-08-mockups-gemini/index.html)
- [`phase-08-mockups-v2/index.html`](phase-08-mockups-v2/index.html)
- [`phase-08-mockups-v3/index.html`](phase-08-mockups-v3/index.html)
- Formal execution breakdown: [`phase-08-delivery-plan.md`](phase-08-delivery-plan.md)

Treat them as visual direction, not an implementation constraint: the curses
implementation should preserve the layout, hierarchy, and semantic color intent
while staying portable across terminal themes.

## Design Language

### Recommended Direction: Signal Desk

Signal Desk is a restrained studio-utility design. It should feel like a piece
of reliable audio/MIDI equipment: quiet dark background, sharp alignment,
semantic color, compact labels, and no decorative chrome.

Principles:

- Data first: the right pane should always make its columns explicit. Sequence
  and diagnostic modes show full event data; Live Player mode shows performance
  data without raw-byte clutter.
- State is visible at a glance: recording, playback, RX, and TX should be
  readable from the top status rail without scanning the footer.
- Color is semantic: every color means a MIDI category or active transport
  state.
- Raw bytes remain present in Sequence and Live Diagnostic modes, but visually
  secondary to semantic descriptions.
- Borders divide work areas but do not consume space with heavy decoration.
- Monochrome mode must still work through labels, alignment, reverse video, and
  status text.

### Alternate Concepts Considered

1. **Timeline Console**
   - Stronger horizontal timeline feel with sequence and live stream stacked as
     two time-based tables.
   - Better for playback analysis, weaker for browsing files while monitoring.
   - Rejected for Phase 8 because it demotes the persistent file browser.

2. **Performance HUD**
   - Large transport counters and status blocks, with events as a secondary
     detail view.
   - Better for stage/live use, weaker for inspecting MIDI takes.
   - Rejected for Phase 8 because it sacrifices utility density.

3. **Signal Desk**
   - Persistent file browser and a mode-aware work pane in one stable grid.
   - Best match for the current renderer boundary and phase goals.
   - Recommended for implementation.

## Palette

The first implementation should stay compatible with curses' common 8/16-color
world. The mockup uses richer hex values to show intent, but the implementation
should map them conservatively through `init_pair()`.

| Role | Intent | Mockup color | Curses fallback |
| --- | --- | --- | --- |
| Background | Quiet work surface | `#101411` | default background |
| Panel | Slightly raised work area | `#151b17` | default background |
| Text | Primary readable content | `#dce3d6` | default foreground |
| Muted | Paths, raw bytes, separators | `#879182` | dim/default |
| Note on | New musical onset | `#62d77a` | `COLOR_GREEN` |
| Note off | Release/end state | `#71b7df` | `COLOR_CYAN` |
| Control change | Continuous control | `#e7ba52` | `COLOR_YELLOW` |
| Program/pressure | Configuration/performance pressure | `#b897e8` | `COLOR_MAGENTA` |
| Pitch bend | Expressive movement | `#ef866c` | `COLOR_BLUE` or `COLOR_MAGENTA` |
| SysEx | System/vendor data | `#a5adb0` | default/neutral + `A_DIM` or `A_BOLD` |
| Unsupported/incomplete | Needs attention | `#ff6b61` | `COLOR_RED` |
| Selected row | Current navigation target | leading `>` marker | plain text + `A_BOLD` marker |
| Recording active | Capture state | `#ff5a52` | `COLOR_RED` + `A_BOLD` |
| Playback active | Output state | `#f0c84b` | `COLOR_YELLOW` + `A_BOLD` |

Color-pair rules:

- Use foreground color only for event category text. Avoid colored backgrounds
  in data tables.
- Use a leading `>` marker for selected/current rows in tables. Do not use a
  full-row highlight in Sequence mode; it hurts readability.
- Reserve `A_REVERSE` for file-list selection and modal/list selections where
  the text is short enough to remain readable.
- Use `A_DIM` for raw bytes where available.
- Do not use `COLOR_WHITE` for SysEx/system rows; it disappears on light
  terminal themes. Prefer neutral/default foreground with a label or attribute.
- Apply active recording/playback color to the mode label and `RX`/`TX`
  indicators only. Do not tint whole panels.
- In monochrome mode, prefix or align category/direction labels so color is not
  the only differentiator.

## Target Layouts

### Minimum And Preferred Terminal Sizes

Supported compact minimum: **90 columns x 20 rows**.

Preferred redesign minimum: **90 columns x 24 rows**.

The current renderer accepts 90x20, but the redesigned layout needs enough
height for a useful full-height work pane. Keep 90x20 as a compact supported mode, but
allow the layout to breathe at 90x24 and above.

- Header/status rail plus key command strip: 4-5 rows
- Files column: 24-28 columns
- Work pane: remaining right side
- Footer/status help: 2 rows

At less than 90x20, keep the existing resize message. At 90x20 through 90x23,
use compact labels and fewer visible live rows.

### Comfortable Default Terminal

Target: **120 columns x 36 rows**.

Layout:

- Top status rail: title, mode, RX/TX activity, source, destination, and version
  label.
- Key command strip below the status rail. This replaces the old top path row
  with readable commands such as `tab pane`, `enter load`, `space play/stop`,
  `0 start`, `, settings`, and `d directory`.
- Left file browser: fixed 28-34 columns, full height below the rail.
- Right work pane: full height below the rail, switching between Sequence, Live
  Player, and Live Diagnostic modes.
- Footer: current status and transport summary, for example
  `Loaded 20260514000738.mid with 128 events`.

### Wide Terminal

Target: **150+ columns x 40+ rows**.

Layout:

- File browser widens modestly but caps at 36 columns.
- Sequence and Live Diagnostic tables gain wider description columns.
- Do not add the optional right-side details strip in Phase 8. Keep it as a
  future idea after the main table and live monitor work lands.

### Layout Algorithm

Phase 8 should introduce an internal `TuiLayout` struct in `tui_render.c` before
redrawing panels. The struct should name the status rail, key command strip,
file browser, work pane, footer, and overlay rectangle. This avoids scattering
geometry math, and it gives the directory browser one layout source to reuse.

Recommended sizing rules:

- Minimum supported terminal remains 90x20.
- Preferred minimum is 90x24.
- File column width is clamped to 28-36 columns.
- Work pane gets the remaining width and full available height.
- When vertical space is tight, keep the current mode usable instead of trying
  to show multiple data panes at once.

## Screen Anatomy

### Status Rail

Purpose: make app state visible without reading the footer.

Content:

- App label: `MIDI Capture`
- Version label in the top-right corner, for example `v0.8.0` or
  `dev-20260514`
- Mode pill text: `SEQUENCE`, `LIVE PLAYER`, `LIVE DIAGNOSTIC`, `RECORDING`,
  `PLAYING`, or related paused states as needed.
- RX/TX activity indicators
- Source and destination labels
- Recordings directory, clipped from the left when long so the final directory
  name remains visible

Implementation notes:

- Keep this in `tui_render.c`; do not move transport logic into the renderer.
- `command_tui.c` already provides mode/source/destination/activity labels.
- The version label should come from a single compile-time app version source,
  not a string literal duplicated inside the renderer. If the project has not
  defined one by implementation time, add a small `APP_VERSION` macro or
  `app_version()` helper as part of the status-rail slice. Prefer
  `app_support.*` or one build-time definition as the owning source.
- Add explicit render fields only if the footer string becomes too overloaded.

### Key Command Strip

Purpose: keep common commands discoverable without crowding the footer.

Content should be mode-aware, compact, and readable:

- Global examples: `tab pane`, `arrows move`, `enter load`, `space play/stop`,
  `0 start`, `, settings`, `d directory`, `q quit`.
- Sequence examples: `p play from here`, `u unload`, `a append`, `r rename`.
- Diagnostic examples: `f filter`, `space pause log`.

### File Browser

Purpose: persistent orientation across recording, playback, and monitoring.

Behavior:

- Fixed side column.
- Selected file uses reverse video because file names are short and the affordance
  is familiar.
- Empty state says `No .mid files`.
- Directory path appears in the status rail, not inside the file list.
- Long file names clip from the right in the list, but full selected name is
  visible in the sequence header when space allows.
- File selection and file loading are distinct. Moving the selection should not
  silently replace the loaded sequence; `enter` loads the selected file.
- Loaded files can be unloaded for a fresh recording.
- Recording can append to an already loaded file only through an explicit append
  command.
- Rename must preserve the `.mid` suffix automatically.

### Sequence Table

Purpose: inspect the selected MIDI file and choose playback start position.

Columns:

| Column | Compact width | Default width | Notes |
| --- | ---: | ---: | --- |
| marker | 1 | 1 | `>` selected/current row |
| #/ | 6-8 | 8 | event number / event count, e.g. `21/128` |
| time | 8 | 9 | `mm:ss.t` or current helper output |
| ch | 2-3 | 3 | MIDI channel or `-` |
| event | 10-14 | 14 | `Note On`, `CC 1`, `Pitch Bend`, etc. |
| target | 8-12 | 12 | note name, controller name, bend, channel, device |
| value | 10-22 | 16-22 | velocity, pressure, CC value, bend value, etc. |
| raw | 10-18 | remaining or fixed | dim, clipped raw bytes |

Renderer behavior:

- Draw a column header row.
- Clip each cell independently so long bytes or descriptions cannot corrupt
  adjacent columns.
- Use neutral/dim styling on bytes and category color on the description, not
  one attribute across the whole row.
- Keep selected/current row readable with the leading `>` marker and optional
  bold marker. Avoid full-row reverse video in dense tables.
- Do not add a direction column to sequence rows. Loaded sequence events do not
  have meaningful per-row direction; `SEQ` would consume width without adding
  information.

### Work Pane Modes

The right pane has three modes.

#### Sequence

Sequence mode is the file-inspection mode. It uses the structured sequence
columns above and allows playback from the selected event.

#### Live Player

Live Player mode is a performance monitor. It should not show `time`, `dir`, or
raw bytes by default. Instead it shows normal-height rows with explicit columns:

| Column | Notes |
| --- | --- |
| #/ | visible row number / visible row count |
| note | note name or configured note number format |
| state | `ON`, `OFF`, `HOLD`, or `FADING` |
| velocity | numeric value plus horizontal bar |
| pressure | channel pressure or per-note poly pressure value plus bar |
| bend/mod | compact expressive state, e.g. `mod 37`, `bend +530` |
| age | time since event or fade age |

Rules:

- Note-on rows are active.
- Note-off rows become muted and then fade out after the configured timeout.
- Rows below a faded note move up when it expires.
- Channel pressure appears in a channel-level control row and may be repeated on
  note rows only when useful.
- Poly pressure appears on the affected note row.
- Modulation and other continuous controllers use horizontal bars.
- Pitch bend uses a centered bipolar bar in visual treatments, but the curses
  implementation may start with a signed numeric value if the bar is too costly.

#### Live Diagnostic

Live Diagnostic mode is the full stream/debug view. It should show all data
needed to debug devices, routing, timing, and unsupported messages.

Columns:

| Column | Width | Notes |
| --- | ---: | --- |
| time | 9 | elapsed or event time |
| dir | 4 | `RX`, `TX`, `REC`, `PLAY`, `ERR`, `MON` |
| ch | 2-3 | MIDI channel or `-` |
| event | 10-14 | `Note On`, `CC 1`, `SysEx`, etc. |
| target | 8-12 | note/controller/device/system target |
| value | 10-18 | event value, with bars where useful |
| raw | 18-28 | dim raw bytes |
| description | remaining | category-colored for MIDI rows |

Current constraint: `TuiLogEntry` stores a formatted line only. The intended end
state is structured live rows with time, direction, event fields, raw bytes,
description, and category.

### Footer

Purpose: current action summary and compact key help.

Rules:

- First segment: current status or transport summary.
- Last segment: secondary state, such as tempo, playhead, filters, or recording
  append state.
- Do not repeat source/destination or key hints if already visible above.
- Preserve useful status messages such as
  `Loaded 20260514000738.mid with 128 events`.

## Interaction Model

Use DAW-like transport behavior where it fits:

- `tab`: move focus between file pane, work pane, and overlay columns.
- Arrow keys: move within the focused pane or column.
- `enter`: load selected file, open selected directory, or apply focused
  setting.
- `space`: play/stop or pause/resume depending on active transport state.
- `0` or `home`: return playhead to the start.
- `p`: play from selected event in Sequence mode.
- `s`: stop active playback/recording.
- `u`: unload the current file so the next recording starts fresh.
- `a`: append recording to the loaded file, only after explicit confirmation or
  an unambiguous command state.
- `r`: rename selected recording, preserving `.mid`.
- `,`: open settings.
- `d`: open directory browser.
- `q`: quit, saving or stopping active recording using the existing safety
  behavior.

## Settings Overlay

Settings should appear as a centered popover over the current layout. It should
be keyboard-first and should not require users to guess paths or formats.

Initial settings:

- Recordings directory.
- Middle C naming convention, e.g. `C3`, `C4`, or `C5`.
- Note display format: note name, decimal note number, or hex note number.
- Live Player fade timeout: `2`, `5`, `10`, or `never`.
- Tempo in BPM for playback speed scaling.
- Metronome during recording: on/off, single-tone click only for now.

Deferred:

- Themes. Keep theme selection out of Phase 8 implementation unless the core
  usability work is complete.

## Directory Browser

Full column-based directory-browser implementation belongs to Phase 9. Phase 8
should reserve the layout, key-command, overlay, and footer/status hooks that
Phase 9 will plug into so the browser lands inside the approved redesign
without reopening the geometry work.

Rules:

- Phase 8 keeps the `d` entry point and the overlay/layout contract; Phase 9
  owns the full column renderer, navigation model, and destination-apply flow.
- Columns show each path level.
- `tab` moves between columns.
- Arrow keys move within a column.
- `enter` opens a directory or applies the selected final destination.
- `esc` cancels.
- `/` or another explicit command can open manual path entry as an escape hatch.
- The browser should show the selected absolute path in the footer.
- Invalid or inaccessible directories should produce readable status text.

## Tempo And Metronome

Tempo is useful even when users ignore it:

- Playback tempo can scale playback speed slower or faster.
- Default tempo should be conservative, for example `120 BPM`.
- Recording can enable a simple metronome click.
- No bar/measure grid is needed in Phase 8; use a single tone only.

## Accessibility

The TUI needs to remain usable without color or visual-only effects.

- Color must never be the only carrier of state.
- Every bar must also show a numeric value.
- Live Player fade must have textual state such as `OFF`, `FADING`, or an age
  value.
- Mode names, selected file, loaded file, and current focus should be available
  as plain text.
- Keep the layout ASCII-safe. Avoid relying on decorative glyphs.
- Maintain a monochrome fallback with labels, alignment, and reverse video.
- Avoid rapid flashing. RX/TX indicators should be simple state changes, not
  blink effects.
- Consider a future screen-reader-friendly status dump mode, but for Phase 8
  ensure all important state exists as text in the terminal buffer.

## Decisions From Design Review

The review in `phase-08-review-claude-sonnet.md` identified implementation
constraints that should be folded into the redesign before code starts:

- Keep Signal Desk as the visual direction.
- Introduce `TuiLayout` in Phase 8 so Phase 9 does not duplicate geometry.
- Keep 90x20 supported as compact mode; treat 90x24 as the preferred minimum.
- Clamp file browser width to 28-36 columns.
- Draw sequence and live rows cell-by-cell, not as one formatted string.
- Drop the Sequence `direction` column; keep direction in Live Diagnostic.
- Use a leading `>` marker for selected/current rows in dense tables.
- Keep `A_REVERSE` for file-list and overlay selections only.
- Avoid `COLOR_WHITE` for SysEx/system rows.
- Apply active transport color only to the mode label and RX/TX indicators.
- Defer the wide-terminal details strip until after the main redesign lands.
- Use the V3 two-pane mode model as the current target: full-height file pane
  plus Sequence, Live Player, or Live Diagnostic in the work pane.
- Reintroduce the V1 color palette and readable table spacing.
- Put key commands in the top command strip and keep footer space for status.

## Implementation Plan

The slice order is intentional:

- land the status rail and command strip before validating table-heavy layouts
- establish structured live data before rendering Live Player or Live
  Diagnostic panes
- keep full directory-browser implementation in Phase 9, while Phase 8 prepares
  the entry point and layout contract

### Slice 1: Layout, Status Rail, And Version Contract

Files:

- `src/tui_render.h`
- `src/tui_render.c`
- `src/command_tui.c`
- possibly `src/app_support.h`
- possibly `src/app_support.c`

Tasks:

- Introduce an internal `TuiLayout` struct for status rail, key command strip,
  file column, work pane, footer, and overlay rectangles.
- Keep 90x20 supported as compact mode and use 90x24 as the preferred minimum.
- Cap file-browser width between 28 and 36 columns.
- Keep the file pane full height and give the work pane the remaining width and
  full available height.
- Move source/destination prominence to the status rail.
- Add a top-right version label sourced from a single app-version definition.
- Add the key command strip below the rail.
- Use active recording/playback color on the mode label and RX/TX indicators
  when available.
- Move common key hints out of the footer and into the command strip.
- Clip long paths and status messages deliberately.
- Keep all calculations in `tui_render.c`.

Verification:

- Build succeeds.
- Manual resize checks at 90x20, 90x24, 120x36, and a wide terminal.
- Active recording and playback states are readable within one glance.

### Slice 2: Sequence Table Rendering Foundation

Files:

- `src/tui_render.c`
- possibly `src/tui_render.h`

Tasks:

- Add reusable clipped-cell drawing helper.
- Add sequence table header.
- Render sequence rows by columns instead of one formatted line.
- Keep sequence columns to marker, `#/`, time, channel, event, target, value,
  and raw bytes.
- Make raw bytes dim where supported.
- Keep bars paired with numeric values for accessibility.
- Keep selected/current row visible via the marker column without changing the
  readability of the rest of the row.

Verification:

- Long raw-byte strings do not overwrite descriptions.
- Long descriptions do not overwrite footer or borders.
- Selected row remains visible in color and monochrome terminals.

### Slice 3: Mode And Live Data Structure

Files:

- `src/tui_render.h`
- `src/tui_render.c`
- `src/command_tui.c`
- possibly `src/tui_log.c`
- possibly `src/tui_log.h`

Tasks:

- Add a right-pane mode enum for Sequence, Live Player, and Live Diagnostic.
- Decide whether `tui_log` should remain text-only with a parallel structured
  snapshot, or whether `TuiLogEntry` should gain optional structured fields.
- Prefer the smallest compatible change: extend `TuiLogEntry` with `kind`,
  `time_text`, `direction`, `channel_text`, `event_text`, `target_text`,
  `value_text`, `byte_text`, `description`, and `category`, while preserving
  plain text for non-MIDI entries.
- Update `log_midi_bytes()` to store structured live rows.
- Add a lightweight live-note state snapshot owned outside the renderer.
- Feed the renderer the active right-pane mode and mode-specific headers.

Verification:

- Existing `tests/test_tui_log.c` still covers append/snapshot behavior.
- Add or update tests for structured live-row snapshot if the log type changes.
- RX and TX diagnostic rows expose direction and description without renderer
  reparsing.
- Live Player state carries numeric values for every visual bar.

### Slice 4: Live Pane Rendering And Semantic Color

Files:

- `src/tui_render.h`
- `src/tui_render.c`
- `src/command_tui.c`

Tasks:

- Render Live Diagnostic rows as a full stream table with time, direction,
  channel, event, target, value, raw bytes, and description columns.
- Render Live Player rows from current/recent note state, including fade age,
  velocity, pressure, modulation, and pitch bend values.
- Rename color-pair constants around semantic roles where that improves
  readability.
- Keep dense table selection on the `>` marker; do not add a selected color pair
  yet. File lists and overlays may continue to use `A_REVERSE`.
- Add or refine color pairs for active recording, active playback, muted bytes,
  and warning/status text where curses support allows.
- Replace the SysEx/system `COLOR_WHITE` fallback with a neutral/default
  treatment that works on light and dark terminals.
- Ensure `has_colors() == false` uses no color-only cues.

Verification:

- Live Diagnostic rows render from structured fields rather than formatted log
  text.
- Live Player rows show numeric values for every visual bar.
- Note on/off/control/program/pitch/SysEx/unsupported rows are visually
  distinct enough in color mode.
- Monochrome capture remains readable.

### Slice 5: Footer, Settings, File Actions, And Phase 9 Browser Hooks

Files:

- `src/command_tui.c`
- `src/tui_render.h`
- `src/tui_render.c`
- possibly `src/tui_settings.c`

Tasks:

- Keep the footer useful for loaded-file, tempo, playhead, filters, and
  append/unload state without duplicating the status rail.
- Add a settings overlay model for recordings directory, middle C, note format,
  fade timeout, tempo, and metronome.
- Make file selection and file loading distinct.
- Add unload, append-to-loaded-file, and rename flows.
- Preserve `.mid` suffix during rename.
- Add the `d` entry point, overlay rectangle usage, and footer/status text
  hooks needed by the Phase 9 directory browser.
- Keep the existing manual path flow as the fallback until Phase 9 lands.

Verification:

- Settings values are visible and keyboard-adjustable.
- Footer stays useful for status such as loaded file, tempo, playhead, filters,
  and append/unload state.
- Rename cannot remove the `.mid` suffix.
- Append requires explicit user intent.
- Directory-browser entry and layout hooks are in place without duplicating
  Phase 9's browser implementation.

### Slice 6: Tempo, Transport, Metronome, And Accessibility

Files:

- `src/command_tui.c`
- `src/tui_render.c`
- possibly playback/recording support modules if behavior becomes shared

Tasks:

- Add DAW-like transport behavior for `space`, `0`, and `home`.
- Add tempo state for playback speed scaling.
- Add a recording metronome toggle using a simple single-tone click.
- Ensure all bar visuals have numeric text alternatives.
- Ensure mode, focus, selected file, loaded file, and current status are
  available as plain text.
- Verify monochrome and no-flash behavior.

Verification:

- Playback speed can change via tempo without corrupting event order.
- Metronome can be toggled on/off while recording setup is active.
- `space`, `0`, and `home` match the documented behavior.
- Live Player remains understandable without color.
- Accessibility notes are reflected in the renderer behavior.

## Inputs

- `src/tui_render.c`
- `src/tui_render.h`
- `src/command_tui.c`
- `src/tui_log.c`
- `src/tui_log.h`
- `src/tui_model.c`
- `src/midi_describe.c`
- `src/midi_describe.h`
- `src/app_support.c`
- `src/app_support.h`
- Phase 5 MIDI description categories
- Phase 7 renderer isolation
- Static mockups in `docs/tui-refactor-sprints/phase-08-mockups-codex/`
- Static mockups in `docs/tui-refactor-sprints/phase-08-mockups-gemini/`
- Static mockups in `docs/tui-refactor-sprints/phase-08-mockups-v2/`
- Static mockups in `docs/tui-refactor-sprints/phase-08-mockups-v3/`
- Design review notes in `docs/tui-refactor-sprints/phase-08-review-claude-sonnet.md`
- future `src/tui_settings.c` if settings are split out
- future `src/tui_settings.h` if settings are split out

## Tasks

- [ ] Confirm the Signal Desk design direction against the mockups.
- [ ] Define terminal layout behavior for 90x20, 90x24, 120x36, and wide
      terminals.
- [ ] Place the file browser as a persistent full-height side column with a
      capped width.
- [ ] Replace the separate live stream pane with a right-side work pane mode:
  - [ ] Sequence
  - [ ] Live Player
  - [ ] Live Diagnostic
- [ ] Render sequence rows as clipped table columns:
  - [ ] marker/#/
  - [ ] time
  - [ ] channel
  - [ ] event
  - [ ] target
  - [ ] value
  - [ ] raw bytes
- [ ] Render Live Player rows as clipped table columns:
  - [ ] #/
  - [ ] note
  - [ ] state
  - [ ] velocity
  - [ ] pressure
  - [ ] bend/mod
  - [ ] age
- [ ] Render Live Diagnostic rows as clipped table columns:
  - [ ] time
  - [ ] direction
  - [ ] channel
  - [ ] event
  - [ ] target
  - [ ] value
  - [ ] raw bytes
  - [ ] description
- [ ] Add semantic color pairs:
  - [ ] note on
  - [ ] note off
  - [ ] control/program/pitch messages
  - [ ] SysEx
  - [ ] unsupported/incomplete
  - [ ] active recording/playback
  - [ ] muted raw bytes
- [ ] Preserve dense-table selection with a leading `>` marker.
- [ ] Preserve file-list and overlay selection with readable `A_REVERSE`.
- [ ] Keep raw bytes visible but visually secondary.
- [ ] Define a single app-version source for the status rail.
- [ ] Improve active transport status readability in the status rail.
- [ ] Add a top-right software version label to the status rail.
- [ ] Add a readable key command strip below the status rail.
- [ ] Preserve footer space for loaded file/status/tempo/playhead state.
- [ ] Extend live data structures before rendering Live Player and Live
      Diagnostic panes.
- [ ] Add settings overlay:
  - [ ] recordings directory
  - [ ] middle C convention
  - [ ] note format
  - [ ] Live Player fade timeout
  - [ ] tempo
  - [ ] metronome
- [ ] Add the directory-browser entry point and layout/status hooks for Phase 9.
- [ ] Add load/unload/append/rename file actions.
- [ ] Add DAW-style transport keys: `space`, `0`, and `home`.
- [ ] Add tempo playback scaling.
- [ ] Add recording metronome toggle.
- [ ] Preserve accessibility text for colors, bars, focus, mode, and fade state.
- [ ] Ensure text clipping cannot corrupt adjacent columns.
- [ ] Preserve keyboard flows from the current TUI.
- [ ] Preserve monochrome fallback readability.
- [ ] Manually test with representative live MIDI input.

## Suggested Files

- `src/tui_render.c`
- `src/tui_render.h`
- `src/command_tui.c`
- `src/tui_log.c` if live rows become structured
- `src/tui_log.h` if live rows become structured
- `src/tui_model.c` if display formatter responsibilities change
- `tests/test_tui_log.c` if live rows become structured

## Verification

Implementation evidence captured 2026-05-16:

- `make clean && make && make test` passed after implementing the renderer,
  log, and TUI command-loop changes.
- A scripted 120x24 TUI smoke check rendered the Signal Desk status rail,
  command strip, persistent Recordings pane, Sequence work pane, and footer.
- Directory-browser scope remains intentionally limited to the Phase 9 entry
  overlay and manual-path escape hatch.

- [ ] `make clean`
- [ ] `make`
- [ ] `make test`
- [ ] Manual TUI check at 90x20.
- [ ] Manual TUI check at 90x24.
- [ ] Manual TUI check at a comfortable desktop terminal size.
- [ ] Manual TUI check at a wide terminal size.
- [ ] Note on and note off are visually distinct.
- [ ] Raw bytes are visible but secondary.
- [ ] Long file names, byte strings, descriptions, paths, and status messages
      clip cleanly.
- [ ] Monochrome fallback is still readable.
- [ ] Recording, playback, navigation, settings, and live monitoring still
  work.
- [ ] Live Diagnostic RX and TX rows show direction, bytes, and description.
- [ ] Live Player rows show note state, velocity, pressure, bend/mod, and age
      with numeric values.
- [ ] Settings are reachable and keyboard-adjustable.
- [ ] Directory-browser entry and overlay/footer hooks are in place for Phase 9.
- [ ] Tempo and metronome controls are visible and non-destructive.
- [ ] Accessibility checks confirm color is not the only carrier of state.

## Exit Criteria

- The TUI has a named design language and an implemented layout matching the
  approved Signal Desk direction.
- The file browser stays visible in normal use.
- The right work pane supports Sequence, Live Player, and Live Diagnostic modes.
- Sequence rows include aligned `#/`, time, channel, event, target, value, and
  raw-byte columns.
- Live Player rows include aligned `#/`, note, state, velocity, pressure,
  bend/mod, and age columns.
- Live Diagnostic rows include aligned time, direction, channel, event, target,
  value, raw bytes, and description columns.
- Settings are discoverable from the key command strip, and directory-browser
  entry remains discoverable while the full browser flow lands in Phase 9.
- Load, unload, append, rename, tempo, and metronome behaviors are documented in
  the UI and implemented without surprising side effects.
- Color communicates event type and transport state without reducing
  readability.
- Monochrome terminals remain usable.

## Risks

- Visual polish can sprawl. Keep Phase 8 focused on scannability, layout,
  semantic color, core modes, and keyboard workflows.
- Terminals vary in color and glyph support. Prefer ASCII labels, conservative
  color pairs, and reverse video over ornate borders or glyphs.
- Structured live rows may require touching `tui_log`. Keep that change small
  and preserve existing ring-buffer behavior with tests.
- Raising the preferred minimum terminal height could surprise users. Preserve
  a compact 90x20 fallback unless implementation proves it too cramped.
- Settings, directory browsing, tempo, metronome, and append flows add scope.
  Keep each workflow text-first and reversible; split modules only if the
  command coordinator becomes hard to read.
