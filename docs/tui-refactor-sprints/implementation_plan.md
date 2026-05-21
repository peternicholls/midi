# Phase 8 TUI Parity Implementation Plan

This implementation plan details the step-by-step refactoring of the terminal user interface (TUI) to align with the approved V3 mockup specifications, screen contracts, and parity checklists. The refactoring is structured into seven logical slices (S0 to S6) executed sequentially under the existing C-first boundary (`command_tui.c` -> `TuiRenderState` -> `tui_render.c`).

## User Review Required

> [!IMPORTANT]
> - All changes maintain the current C-first architecture. Go or Bubble Tea transitions are not part of this plan.
> - Keyboard shortcut layouts will be updated to match the frozen keybar contract, representing a change in the displayed command help relative to legacy mockups.
> - The legacy `description` column in Sequence mode will be completely removed to align with the frozen contracts.

## Proposed Changes

### Component: Core TUI Rendering & Layout

#### [MODIFY] [tui_render.h](file:///Users/peternicholls/code/midi/src/tui_render.h)
- Expose the explicit channel scope state in the render snapshot state structure (e.g. `TuiRenderState.channel_scope` or `TuiRenderLiveControlRow` updates).
- Add any needed helper declarations or updates.

#### [MODIFY] [tui_render.c](file:///Users/peternicholls/code/midi/src/tui_render.c)
- **S0 (Shared Primitives)**:
  - Fix compact layout threshold logic (`rows <= 24`).
  - Implement a standardized `draw_keycap_token` helper to render key-command pairs in the format `[key] label` with deterministic spacing.
  - Wrap rail `mode`, `rx`, and `tx` in brackets to render them as bounded token elements (e.g. `[SEQUENCE]`, `[RX*]`).
  - Add the directory path row (`state->recordings_dir`) to `draw_files_panel`, clipping it cleanly to the pane width, and adjust file list vertical offset.
  - Implement reusable `draw_mini_bar` and `draw_bend_bar` helpers using portable reverse-video character blocks (`' ' | A_REVERSE`).
- **S2 (Sequence)**:
  - Adjust Sequence column widths to exactly match `#/ time ch event target value raw`.
  - Remove the legacy `description` column in Sequence mode.
  - Lock `#/` column to format `current/total`.
- **S3 (Live Player)**:
  - Render controls row spacing and note row column widths per the Live Player contract.
  - Incorporate `draw_mini_bar` and `draw_bend_bar` for velocity, pressure, and bend/mod columns.
- **S4 (Live Diagnostic)**:
  - Adjust diagnostic column widths (`time dir ch event target value raw description`) and clip descriptions cleanly.
- **S5 (Settings)**:
  - Align all settings overlay labels (`Recordings directory`, `Middle C`, `Note format`, `Fade timeout`, `Tempo`, `Metronome`) and implement the three-column layout (`setting | value | notes`).

---

### Component: App Controller & Key Handling

#### [MODIFY] [command_tui.c](file:///Users/peternicholls/code/midi/src/command_tui.c)
- **S1 (Explicit Scope)**:
  - Add an explicit `channel_scope` field to `TuiApp` state (representing `0` for ALL/AUTO, or `1` to `16` for individual channels).
  - Implement a key handler for `'c'` inside `handle_keypress` to cycle the channel scope.
  - Update `render_tui` snapshot compilation to copy the new C-owned scope state into `TuiRenderState`.
  - Update controls row calculation to render values based strictly on the selected scope channel.

---

### Component: Build & Test Configuration

#### [MODIFY] [Makefile](file:///Users/peternicholls/code/midi/Makefile)
- Ensure compilation targets and tests include any updated files.

## Verification Plan

### Automated Tests
- Run `make test` to verify that existing core functionality, parsing, recording, and playback tests continue to pass.
- Run any regression tests on the TUI renderer model.

### Manual Verification
- Resize the terminal to `90x24` and manually verify visual alignment, spacing, column visibility, and margins against the V3 screen contracts.
- Resize the terminal to `120x36` and manually check layout scaling and breathing room.
- Exercise keyboard controls (specifically the new `'c'` key to cycle channel scope, pane focus, play, stop) to verify they trigger active C operations and update the UI accordingly.
