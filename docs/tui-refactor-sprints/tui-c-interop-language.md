# C-Interoperable TUI Contract Language

**Date:** 2026-05-19

## Purpose

Define a language for the TUI that is native to the terminal, but directly
interoperable with the existing C code.

This language is not a second renderer and not a web-style mockup format. It is
a shared contract vocabulary for three things:

1. what the user sees
2. what the user can do
3. which C state and C operation owns that behavior

The goal is to stop losing intent between mockup, documentation, and
implementation.

## Why This Needs To Be C-First

The current app already has the right architectural split:

- `command_tui.c` owns mutable state and operations
- `tui_render.h` defines the render snapshot contract
- `tui_render.c` draws from that snapshot

What is missing is a stable language that names the TUI in the same terms as the
code.

HTML/CSS mockups are useful for visual direction, but they are not directly
interoperable with:

- `TuiRenderState`
- `TuiRenderSequenceRow`
- `TuiRenderLiveNoteRow`
- `TuiRenderLiveControlRow`
- `handle_keypress()` and the operation helpers in `command_tui.c`

So the contract language should be based on C nouns and C verbs, not DOM nouns
and CSS behaviors.

## Design Rules

This language should follow five rules.

1. Every visible concept must map to an existing or planned field in
   `TuiRenderState` or a nested render struct.
2. Every interactive concept must map to a concrete operation in
   `command_tui.c`.
3. No visual term is allowed if the renderer cannot express it using terminal
   primitives such as text, spacing, reverse video, dim text, color pair, box
   drawing, or inline bars.
4. No operation term is allowed if the state owner is ambiguous.
5. The language must be usable in docs, tests, and implementation notes without
   requiring a parser.

## The Language Model

The language has three layers.

### 1. Surface language

This defines what appears on screen.

Core surface nouns:

- `rail`
- `keybar`
- `files`
- `workpane`
- `footer`
- `overlay`
- `title`
- `table`
- `row`
- `cell`
- `token`
- `status`
- `hint`

### 2. State language

This defines what the screen is expressing.

Core state nouns:

- `mode`
- `focus`
- `overlay-kind`
- `transport`
- `rx`
- `tx`
- `source`
- `destination`
- `recordings-dir`
- `file-selection`
- `sequence-selection`
- `channel-scope`
- `settings-selection`

### 3. operation language

This defines what the user can do and which C operation handles it.

Core verbs:

- `cycle`
- `move`
- `select`
- `load`
- `unload`
- `play`
- `pause`
- `stop`
- `record`
- `rename`
- `append`
- `open`
- `close`
- `apply`
- `toggle`
- `set`

## Grammar

The contract language should be written in a compact sentence form:

`surface := state -> render-owner -> operation-owner`

Examples:

- `rail.mode := work-pane-mode -> TuiRenderState.work_pane_mode -> cycle_work_pane_mode()`
- `files.selection := file-selection -> TuiFileList.selected -> move_file_selection()`
- `sequence.current := sequence-selection -> playback.selected_event -> move_event_selection()`
- `overlay.settings := overlay-kind(settings) -> TuiRenderState.overlay -> handle_settings_key()`

For larger screens, use block form:

```text
screen sequence {
  rail {
    brand app-version mode rx tx source destination
  }
  keybar {
    tab pane | enter load | space play-stop | 0 start | , settings | d directory
  }
  files {
    title recordings-dir file-selection
  }
  workpane sequence {
    title sequence-name event-count
    table marker number time ch event target value raw
    selection sequence-selection
  }
  footer {
    transport-summary
  }
}
```

This is documentation syntax only. It should map directly to C fields and C
functions.

## Canonical TUI Nouns And Their C Owners

| Contract noun | Meaning | C owner |
| --- | --- | --- |
| `mode` | active right-pane mode | `TuiRenderState.work_pane_mode` |
| `focus` | files vs work pane focus | `TuiRenderState.focus` |
| `overlay-kind` | none, settings, directory | `TuiRenderState.overlay` |
| `transport` | idle, playing, recording, paused | `TuiRenderState.record_active`, `TuiRenderState.playback_active`, operation state in `command_tui.c` |
| `rx` | recent input activity | `TuiRenderState.rx_active` |
| `tx` | recent output activity | `TuiRenderState.tx_active` |
| `file-selection` | selected `.mid` in file list | `TuiFileList.selected` |
| `sequence-selection` | selected event row | `playback.selected_event` via `TuiRenderState.sequence_selected` |
| `channel-scope` | current Live Player channel scope | should be explicit app state, not inferred from last activity |
| `settings-selection` | selected settings row | `TuiSettings.selected_index` via `TuiRenderState.settings.selected_index` |

## Canonical TUI Verbs And Their C Owners

| Contract verb | Meaning | Current or intended C owner |
| --- | --- | --- |
| `cycle(mode)` | move to next work pane mode | `cycle_work_pane_mode()` |
| `cycle(focus)` | toggle files/work pane focus | `cycle_focus()` |
| `move(file-selection, up/down)` | move selected file | `move_file_selection()` |
| `move(sequence-selection, up/down)` | move selected event | `move_event_selection()` |
| `load(selected-file)` | load current file into playback state | `playback_load_selected_file()` |
| `unload(sequence)` | unload loaded sequence | `unload_loaded_file()` |
| `play(sequence)` | start or resume playback | `start_playback_session()` / `toggle_playback_pause()` |
| `stop(transport)` | stop record or playback | `stop_record_session()` / `stop_playback_session()` |
| `record(start)` | start recording | `start_record_session()` |
| `record(toggle-pause)` | pause or resume recording | `toggle_record_pause()` |
| `append(toggle)` | arm append recording | `arm_append_recording()` |
| `rename(selected-file)` | rename selected file | `prompt_for_rename()` |
| `open(settings)` | open settings overlay | `app->overlay = TUI_RENDER_OVERLAY_SETTINGS` |
| `open(directory)` | open directory overlay | `app->overlay = TUI_RENDER_OVERLAY_DIRECTORY` |
| `close(overlay)` | dismiss current overlay | overlay handlers |
| `apply(setting)` | confirm settings choice | `handle_settings_key()` |
| `set(directory)` | choose or type recordings path | `prompt_for_output_directory()` / directory overlay flow |
| `cycle(channel-scope)` | choose Live Player channel | should be added explicitly |

## Rendering Primitives

The language should only use terminal-safe rendering primitives.

| Primitive | Meaning in the contract | Curses expression |
| --- | --- | --- |
| `plain` | normal content | default attr |
| `muted` | secondary metadata | `A_DIM` |
| `selected` | currently targeted short item | `A_REVERSE` |
| `accent(note-on)` | note-on semantic emphasis | category color pair |
| `accent(control)` | CC / diagnostic semantic emphasis | category color pair |
| `pill` | compact state token | bounded text with padding and optional color |
| `keycap` | command token | short bordered token or spaced literal token |
| `bar` | compact magnitude cue | numeric text plus inline mini bar |
| `box` | overlay or modal boundary | ACS line drawing |

This matters because mockup language such as gradient, shadow, radius, and CSS
gap must be translated into terminal equivalents before implementation.

## Screen Contracts

The following screen contracts should become the canonical reference.

### Sequence contract

```text
screen sequence {
  rail { brand mode rx tx source destination version }
  keybar { tab-pane enter-load space-play-stop 0-start ,-settings d-directory }
  files { title recordings-dir file-selection }
  workpane sequence {
    title sequence-name event-count
    table marker number time ch event target value raw
    selection sequence-selection
  }
  footer { transport-summary }
}
```

Direct C mapping:

- `mode` -> `TuiRenderState.work_pane_mode`
- `file-selection` -> `TuiFileList.selected`
- `sequence-selection` -> `TuiRenderState.sequence_selected`
- `table` rows -> `TuiRenderSequenceRowProvider`

### Live Player contract

```text
screen live-player {
  rail { brand mode rx tx source channel-scope version }
  keybar { tab-pane space-hold c-channel ,-settings d-directory q-quit }
  files { title recordings-dir file-selection }
  workpane live-player {
    title source-name fade-policy
    controls scope mod pressure pitch last-rx
    table number note state velocity pressure bend-mod age
  }
  footer { live-summary }
}
```

Direct C mapping:

- `controls` -> `TuiRenderLiveControlRow`
- `table` rows -> `TuiRenderLiveNoteRow[]`
- `channel-scope` -> needs explicit app state

### Live Diagnostic contract

```text
screen live-diagnostic {
  rail { brand mode rx tx source destination version }
  keybar { tab-pane f-filter space-pause-log ,-settings d-directory }
  files { title recordings-dir file-selection }
  workpane live-diagnostic {
    title log-window-size
    table time dir ch event target value raw description
  }
  footer { diagnostic-summary }
}
```

Direct C mapping:

- `table` rows -> `TuiLogEntry[]`
- structured cells -> `TuiLogEntry.midi.*`

### Settings contract

```text
overlay settings {
  title Settings
  table setting value notes
  selection settings-selection
  actions up-down left-right enter-apply esc-close
}
```

Direct C mapping:

- `selection` -> `TuiRenderSettingsState.selected_index`
- `actions` -> `handle_settings_key()`

## The Key Interop Rule

Every contract clause must be writable in this form:

`user intent -> app state -> render field -> draw function -> key handler`

Example:

`cycle channel scope -> app.live_player_scope_channel -> state.live_controls.scope -> draw_live_controls_row() -> handle_keypress('c')`

If any step in that chain is missing, the design is not yet interoperable with
the C code.

## Recommended State Additions

To make this language fully usable, a few state concepts should become explicit.

### 1. Explicit Live Player channel scope

Add app-owned state rather than inferring scope from the most recently active
channel.

Suggested concept:

```text
live-player-scope := auto | ch-1 | ch-2 | ... | ch-16
```

Suggested ownership:

- app state in `command_tui.c`
- surfaced into `TuiRenderState` as either:
  - an explicit scope enum and channel number, or
  - a fully formatted `live_controls.scope` plus a separate `scope_is_auto`

### 2. Explicit transport mode text ownership

The contract should treat `mode` and `transport` as related but distinct.

- `mode` means which pane is active
- `transport` means whether playback or recording is active or paused

That avoids the current ambiguity where one rail token sometimes stands for
work mode and sometimes for transport state.

### 3. Fixed-size screen variants

The language should require named variants:

- `screen sequence @90x24`
- `screen sequence @120x36`
- `screen live-player @90x24`
- `screen live-diagnostic @120x36`

That turns parity into a testable contract instead of a broad visual opinion.

## How To Use This Language

Use it in four places.

### 1. Design docs

Write each screen in contract form before or alongside mockups.

### 2. Implementation plans

Reference contract clauses instead of broad phrases like "match the mockup more
closely".

Example:

- good: `Implement live-player.controls.channel-scope with explicit cycle action`
- weak: `Make Live Player closer to V3`

### 3. Code review

Review by contract breakage.

Example questions:

- Did this change remove a contract noun?
- Did this change split ownership across renderer and command logic?
- Did this introduce a visual term with no terminal primitive?

### 4. Tests

Golden tests should render contract fixtures at fixed sizes and compare screen
text against expected output.

## Recommendation

The project should adopt this as its TUI language:

- C-owned nouns
- C-owned verbs
- terminal-safe rendering primitives
- fixed-size screen contracts
- one direct ownership chain from user intent to render output

This is the right interoperability boundary for this repo. It keeps design,
operations, and rendering in one shared vocabulary instead of translating
between HTML language and C language after the fact.

## Adopted Project Direction

As of 2026-05-19, this contract language is the chosen TUI design language for
the Phase 8 C-first path.

That means:

- new TUI design notes should describe screens in this contract language
- renderer changes should add or change explicit C-owned nouns rather than
  inventing renderer-local shorthand
- interaction changes should be described as contract verbs mapped to concrete
  `command_tui.c` owners
- mockups remain visual references, but this contract is the implementation
  reference

## Immediate Use

Use this document next for:

1. fixed-size `90x24` screen contracts
2. fixed-size `120x36` screen contracts
3. parity-driven renderer changes in `tui_render.c`
4. renderer snapshot tests aligned to those contracts