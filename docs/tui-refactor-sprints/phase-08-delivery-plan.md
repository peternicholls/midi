# Phase 8 Delivery Plan: TUI UX Redesign

**Depends on:** Phase 7

**Status:** Implemented (verified after Phase 8 remediation 2026-05-14)

## Purpose

Turn the Phase 8 redesign brief into an execution plan that can be delivered in
reviewable slices. The design document remains the product brief; this file is
the engineering plan for sequencing the work.

## Planning Constraints

- Keep the Signal Desk visual direction from the redesign brief.
- Use `docs/tui-refactor-sprints/phase-08-mockups-v3/index.html` as the
      primary layout reference for the full-height file pane and right-side
      mode-aware work pane.
- Preserve the compact supported minimum of 90x20 while treating 90x24 as the
  preferred redesign minimum.
- Keep the file browser persistent and full height in normal use.
- Keep all Phase 8 work buildable and testable after each phase.
- Avoid broad rewrites inside `command_tui.c`; prefer incremental extraction and
  explicit data contracts.
- Defer optional details-strip ideas until after the main work pane redesign is
  complete.

## Delivery Strategy

Phase 8 should ship in five engineering phases. Each phase ends with a build,
tests, and a small manual TUI pass before the next phase starts.

Sequencing logic:

1. Establish layout and status hierarchy first so later stories target a stable
      geometry contract grounded in the V3 mockup.
2. Replace formatted row strings with clipped table rendering before adding more
   modes or richer live data.
3. Add right-pane mode control and structured live data before attaching new
   settings or directory workflows.
4. Add file/settings flows only after the rendering model is stable.
5. Finish with transport, accessibility, and verification hardening.

## Phase 8A: Layout Foundation And Status Hierarchy

Objective: establish a stable layout contract and top-of-screen hierarchy that
the rest of Phase 8 can build on.

### Story P8A.1: Introduce `TuiLayout`

Goal: make all panel geometry come from one renderer-owned layout structure.

Files:

- `src/tui_render.c`
- `src/tui_render.h`

Tasks:

- [x] Add an internal `TuiLayout` struct covering the status rail, command
      strip, file pane, work pane, footer, and overlay rectangle.
- [x] Centralise width and height calculations in `tui_render.c`.
- [x] Clamp file-pane width to 28-36 columns.
- [x] Preserve the existing resize guard below 90x20.
- [x] Add compact layout behaviour for 90x20 through 90x23.
- [x] Document preferred behaviour for 90x24, 120x36, and wide terminals in
      code comments where the math is not obvious.

Verification:

- [x] `make`
- [x] `make test`
- [x] Manual resize checks at 90x20, 90x24, 120x36, and 150x40+

### Story P8A.2: Add Status Rail And Command Strip

Goal: move high-value transport and navigation state into a persistent top rail.

Files:

- `src/tui_render.c`
- `src/tui_render.h`
- `src/command_tui.c`
- `src/app_support.h`
- `src/app_support.c`

Tasks:

- [x] Define a single app-version source for the renderer.
- [x] Render app label, mode label, RX/TX activity, source, destination, and
      clipped recordings path in the status rail.
- [x] Render a mode-aware command strip below the rail.
- [x] Move repeated key hints out of the footer.
- [x] Keep footer content focused on current status and transport summary.

Verification:

- [x] `make`
- [x] `make test`
- [x] Manual check that recording/playback state is readable without relying on
      the footer

### Story P8A.3: Lock The Persistent File Pane Behaviour

Goal: keep the file browser visible and predictable across modes.

Files:

- `src/tui_render.c`
- `src/command_tui.c`
- `src/tui_files.c`

Tasks:

- [x] Keep the file pane full height beneath the top rail.
- [x] Confirm reverse-video selection remains limited to the file list.
- [x] Clip long file names cleanly in the list.
- [x] Keep directory path presentation in the status rail rather than the file
      pane.
- [x] Preserve current keyboard navigation while decoupling selection from file
      loading.

Verification:

- [x] `make test`
- [x] Manual file navigation check with mixed short and long `.mid` names

Exit criteria:

- Layout math lives in one renderer contract.
- The top rail and command strip are present.
- The file pane remains persistent and visually stable.

## Phase 8B: Table Rendering And Dense Data Presentation

Objective: replace monolithic formatted rows with clipped, per-cell table
rendering, starting with the reusable cell helper and the Sequence table.

### Story P8B.1: Add Reusable Clipped-Cell Drawing

Goal: stop long cell content from corrupting adjacent columns.

Files:

- `src/tui_render.c`

Tasks:

- [x] Add a helper that renders text inside a fixed cell width.
- [x] Support optional attributes per cell rather than per row.
- [x] Ensure clipping is deterministic in compact and default layouts.
- [x] Keep ASCII-safe rendering behaviour.

Verification:

- [x] `make`
- [x] Manual check with long paths, descriptions, and byte strings

### Story P8B.2: Render The Sequence Table By Columns

Goal: make loaded sequence inspection readable and stable.

Files:

- `src/tui_render.c`
- `src/tui_render.h`

Tasks:

- [x] Add a sequence header row.
- [x] Render marker, `#/`, time, channel, event, target, value, and raw columns
      separately.
- [x] Keep selected/current indication on a leading `>` marker.
- [x] Keep raw bytes visible but secondary.
- [x] Avoid a direction column in Sequence mode.

Verification:

- [x] `make test`
- [x] Manual check that long raw bytes do not overwrite the value or footer

Exit criteria:

- Sequence rows render cell by cell.
- Clipping prevents cross-column corruption.
- Dense table selection stays readable in color and monochrome terminals.

## Phase 8C: Work Pane Modes And Structured Live Data

Objective: make the right pane mode-aware and give it structured live data
instead of text-only log rendering.

### Story P8C.1: Add Right-Pane Mode State

Goal: make Sequence, Live Player, and Live Diagnostic explicit application
states.

Files:

- `src/command_tui.c`
- `src/tui_render.h`
- `src/tui_render.c`

Tasks:

- [x] Add a right-pane mode enum.
- [x] Add keyboard flow for switching or cycling the active work-pane mode.
- [x] Feed the renderer the active mode and mode-specific headers.
- [x] Keep file-pane focus behaviour separate from work-pane mode.

Verification:

- [x] `make`
- [x] Manual mode-switch check across all three panes and overlays

### Story P8C.2: Extend `TuiLogEntry` With Structured MIDI Fields

Goal: stop reparsing formatted text in the renderer.

Files:

- `src/tui_log.c`
- `src/tui_log.h`
- `src/command_tui.c`
- `tests/test_tui_log.c`

Tasks:

- [x] Add optional structured fields for time, direction, channel, event,
      target, value, bytes, description, and category.
- [x] Preserve plain-text compatibility for non-MIDI log rows.
- [x] Update `log_midi_bytes()` or equivalent live append paths to populate the
      structured fields.
- [x] Keep ring-buffer ownership and snapshot behaviour unchanged.

Verification:

- [x] `make`
- [x] `make test`
- [x] Add or update tests for structured snapshot content

### Story P8C.3: Implement Live Diagnostic And Live Player Rendering

Goal: render both live modes only after the structured data contract exists.

Files:

- `src/command_tui.c`
- `src/tui_render.c`
- `src/tui_render.h`
- possibly `src/tui_model.c`

Tasks:

- [x] Add a lightweight live-note state snapshot owned outside the renderer.
- [x] Render Live Diagnostic rows from structured fields instead of formatted
      log strings.
- [x] Render Live Diagnostic time, direction, channel, event, target, value,
      raw, and description columns.
- [x] Render `#/`, note, state, velocity, pressure, bend/mod, and age columns.
- [x] Keep numeric values present for every bar or compact visual treatment.
- [x] Support fading and expiry without introducing unreadable churn.
- [x] Represent channel pressure, poly pressure, modulation, and bend clearly.

Verification:

- [x] `make`
- [x] `make test`
- [x] Manual live-input check with note on/off, modulation, pressure, and pitch
      bend

Exit criteria:

- The work pane has explicit Sequence, Live Player, and Live Diagnostic modes.
- Live Diagnostic uses structured rows.
- Live Player shows performance state rather than raw log text.

## Phase 8D: File, Settings, And Directory Workflows

Objective: add the user workflows that make the redesigned TUI practical rather
than decorative, while leaving the full directory-browser implementation in
Phase 9.

### Story P8D.1: Separate Selection From File Actions

Goal: make load, unload, append, and rename explicit actions.

Files:

- `src/command_tui.c`
- `src/tui_render.c`
- `src/tui_render.h`

Tasks:

- [x] Keep moving the file selection from implicitly reloading the sequence.
- [x] Add explicit load on `enter`.
- [x] Add unload flow on `u`.
- [x] Add append-to-loaded-file flow on `a` with explicit user intent.
- [x] Add rename flow on `r` that preserves the `.mid` suffix.
- [x] Update command-strip hints to match the new actions.

Verification:

- [x] `make test`
- [x] Manual check for load, unload, append, and rename flows

### Story P8D.2: Add Settings Overlay Model

Goal: make core settings visible and keyboard-adjustable in-place.

Files:

- `src/command_tui.c`
- `src/tui_render.c`
- `src/tui_render.h`
- possibly `src/tui_settings.c`
- possibly `src/tui_settings.h`

Tasks:

- [x] Define overlay state for recordings directory, middle C convention, note
      format, fade timeout, tempo, and metronome.
- [x] Render a centred keyboard-first popover.
- [x] Add selection and adjustment controls.
- [x] Reflect settings values in footer or rail text where useful.

Verification:

- [x] `make`
- [x] `make test`
- [x] Manual settings interaction check

### Story P8D.3: Prepare Phase 9 Directory Browser Hooks

Goal: make the browser entry point and layout contract real without duplicating
Phase 9.

Files:

- `src/command_tui.c`
- `src/tui_render.c`
- `src/tui_render.h`

Tasks:

- [x] Keep `d` as the discoverable directory-browser entry point.
- [x] Reserve overlay and footer/status surfaces for the future browser.
- [x] Preserve the manual-path escape hatch until Phase 9 replaces it.
- [x] Document that column rendering, browser navigation, and destination apply
      flow are owned by Phase 9.

Verification:

- [x] `make test`
- [x] Manual check that the browser entry point and placeholder/status handling
      do not regress existing directory changes

Exit criteria:

- File actions are explicit and reversible.
- Settings are discoverable from the command strip.
- Directory-browser entry and layout hooks are in place for Phase 9.

## Phase 8E: Transport, Accessibility, And Final Hardening

Objective: complete the redesign with transport polish, semantic colour, and
accessibility checks.

### Story P8E.1: Add DAW-Style Transport Controls

Goal: make playback and recording controls consistent with the redesign brief.

Files:

- `src/command_tui.c`
- `src/tui_render.c`

Tasks:

- [x] Add `space` play/stop or pause/resume behaviour.
- [x] Add `0` and `home` to return the playhead to the start.
- [x] Keep `p` for play-from-selected-event in Sequence mode.
- [x] Keep status text aligned with the active transport state.

Verification:

- [x] `make test`
- [x] Manual transport-flow check during sequence playback and live monitoring

### Story P8E.2: Add Tempo And Metronome Support

Goal: make tempo and metronome visible controls rather than deferred ideas.

Files:

- `src/command_tui.c`
- `src/midi_recorder.c`
- `src/midi_sequence.c`
- `src/tui_render.c`

Tasks:

- [x] Add tempo state for playback scaling.
- [x] Keep playback event order intact under tempo changes.
- [x] Add a simple recording metronome toggle.
- [x] Surface tempo and metronome state in the UI.

Verification:

- [x] `make`
- [x] `make test`
- [x] Manual tempo and metronome check

### Story P8E.3: Complete Semantic Colour And Accessibility Pass

Goal: ensure the redesign stays readable in both colour and monochrome modes.

Files:

- `src/tui_render.c`
- `src/tui_render.h`

Tasks:

- [x] Finalise semantic colour pairs for note on, note off, control change,
      program or pressure, pitch bend, SysEx, unsupported messages, active
      recording, active playback, and muted raw bytes.
- [x] Replace any remaining SysEx `COLOR_WHITE` treatment with a neutral-safe
      fallback.
- [x] Keep colour on foreground text only for data tables.
- [x] Ensure mode, focus, state, and bars all remain understandable as text.
- [x] Confirm no rapid flashing behaviour is introduced.

Verification:

- [x] `make`
- [x] `make test`
- [x] Manual colour-terminal check
- [x] Manual monochrome or limited-colour terminal check

### Story P8E.4: Final Phase-8 Verification Pass

Goal: close the phase with explicit evidence instead of informal confidence.

Tasks:

- [x] Run `make clean`
- [x] Run `make`
- [x] Run `make test`
- [x] Manually verify 90x20, 90x24, comfortable desktop, and wide-terminal
      layouts.
- [x] Manually verify recording, playback, navigation, settings, directory
      browsing, and live monitoring.
- [x] Capture verification notes back into the Phase 8 redesign document.

Exit criteria:

- Transport behaviour matches the redesign brief.
- Tempo and metronome controls are visible and non-destructive.
- Colour remains semantic, and monochrome fallback is still usable.
- Phase 8 evidence is recorded in the design-phase document.

## Cross-Phase Risks And Controls

| Risk | Control |
| --- | --- |
| Visual redesign sprawls into open-ended polish | Keep each story tied to a user-visible behavioural change or a renderer contract. |
| Structured live data destabilises the log ring buffer | Extend `TuiLogEntry` incrementally and keep snapshot tests updated before adding more live modes. |
| File and directory workflows become a full file manager | Limit scope to recordings selection, file actions, and explicit status messaging. |
| Accessibility degrades while colours improve | Require text labels, numeric values, and monochrome checks in every phase that touches rendering. |
| Transport features force wider MIDI engine changes | Keep tempo/metronome late in the sequence so renderer and workflow work is already stable. |

## Definition Of Done

Phase 8 is done when all five phases complete and the following statements are
true:

- The Signal Desk layout is visible in the shipped TUI.
- The file pane remains persistent while the right pane switches modes.
- Sequence, Live Player, and Live Diagnostic each render their own structured
  columns.
- Load, unload, append, rename, settings, and directory workflows are explicit.
- Semantic colour improves scanability without becoming required for use.
- Verification evidence is recorded in the Phase 8 redesign document.