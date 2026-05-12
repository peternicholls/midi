# TUI Refactor Plan

## Problem

The project has grown a terminal TUI on top of the original CLI commands. The
TUI currently lives mostly in `src/command_tui.c`, which is much larger than the
rest of the codebase and mixes several responsibilities:

- curses rendering and keyboard handling
- recordings directory scanning
- live MIDI input monitoring
- recording state and packet-to-sequence conversion
- playback event extraction and MIDI output
- status and log buffering
- the main TUI lifecycle loop

This is hard to read because one translation unit now represents an entire
application, not one command boundary. It is also hard to maintain because parts
of the TUI duplicate behavior already present in `src/command_record.c` and
`src/command_play.c`.

## Cause

The main cause is architectural, not the C language itself. The project appears
to follow a simple "one command, one source file" shape. That worked for `list`,
`record`, and `play`, but `tui` is a composite feature that embeds record, play,
file browsing, rendering, and live monitoring.

C makes this easier to get wrong because it has limited namespacing and no
built-in module system beyond headers and translation units. However, clean C is
still practical here if the code is split around stable responsibilities.

## Direction

Keep the project in C for now. Do not rewrite in Swift as the first cleanup
step. A Swift rewrite would not automatically solve the design problem, and this
project still depends heavily on CoreMIDI, AudioToolbox, curses, callbacks, byte
parsing, and resource lifetimes. Those concerns need clearer boundaries whether
the implementation language is C or Swift.

The better first step is to extract shared MIDI engine modules from the CLI and
TUI paths, then keep command files thin.

Use this work as an opportunity to improve architecture across the project, not
only to make `src/command_tui.c` shorter. When moving code, prefer boundaries
that make the CLI and TUI share behavior naturally, reduce duplicated CoreMIDI
handling, and leave command files as adapters around reusable application
services.

The refactor should also improve the TUI experience. This is a utility, so it
should stay dense and efficient, but it should also be pleasant to use: clearer
columns, meaningful color, a more useful live stream, and less guesswork when
choosing output directories.

## Target Shape

Suggested modules:

- `midi_describe.c/.h`
  - convert parsed MIDI messages into short English descriptions
  - expose event categories such as note on, note off, control change, program
    change, pitch bend, SysEx, and unsupported
  - avoid putting MIDI interpretation rules directly in the TUI renderer

- `midi_sequence.c/.h`
  - load and save `MusicSequence` files
  - collect timestamped playback events from a sequence
  - own playback event list allocation and disposal

- `midi_output.c/.h`
  - open and close CoreMIDI output ports
  - send raw MIDI byte buffers to a destination

- `midi_recorder.c/.h`
  - own recorder state for `MusicSequence` capture
  - convert parsed packet bytes into sequence events
  - track captured and ignored message counts

- `tui_log.c/.h`
  - own the TUI ring buffer and mutex
  - provide append and snapshot operations

- `tui_files.c/.h`
  - scan a recordings directory
  - own file list allocation, sorting, and selection preservation

- `tui_directory_browser.c/.h`
  - provide an in-TUI directory picker for choosing the recordings destination
  - show current directory, parent navigation, child directories, and the
    selected path before applying it

- `tui_render.c/.h`
  - contain curses-only rendering functions
  - read from TUI state but avoid owning MIDI behavior
  - own layout calculation and color-pair selection

- `command_tui.c`
  - remain the coordinator for TUI lifecycle, key handling, and mode transitions
  - call the extracted modules instead of embedding their internals

## Existing Modules

Two source files already exist, are tested, and must be explicitly accounted for
during the refactor. Without a stated plan their functions risk being duplicated,
orphaned, or moved inconsistently.

### `tui_model.c` / `tui_model.h`

Contains five pure helpers: `tui_is_midi_filename`, `tui_format_recording_name`,
`tui_join_path`, `tui_format_midi_bytes`, and `tui_format_clock_time`. All are
covered by tests in `tests/test_tui_model.c`.

Planned fate during the refactor:

- `tui_is_midi_filename` and `tui_join_path` migrate to `tui_files.c` (Phase 6),
  because they serve the file-scanning responsibility.
- `tui_format_midi_bytes` stays in `tui_model.c` as a display formatter; it
  formats raw bytes as hex for presentation and is not a semantic describer.
- `tui_format_recording_name` and `tui_format_clock_time` remain in
  `tui_model.c`, which narrows to a home for pure display formatters that do not
  belong to a specific engine module.
- If all functions migrate out during Phase 6 or Phase 8, delete `tui_model.c`
  and move its tests at that point. Do not delete it speculatively before its
  callers are updated.

### `status_line.c` / `status_line.h`

A focused 48-line file providing single-line status rendering. It is tested and
has a distinct responsibility from `tui_log.c`'s ring buffer: `status_line`
owns one visible status bar row, while `tui_log` owns the multi-line rolling
log. Both can coexist.

Leave `status_line.*` unchanged through Phases 1–9. Revisit it explicitly in
Phase 10 alongside `app_support.*` to confirm its responsibility does not
overlap with any new module.

## TUI UX Goals

- Use a stable column layout with the file browser on the side. The file browser
  should stay visible while recording, playback, and live monitoring continue.
- Give the live stream more vertical space than it has today. It should feel like
  the main monitoring surface, not a short footer log.
- Add a `Description` column for MIDI events and live stream rows. Examples:
  `Note On C4 velocity 96 channel 1`, `Note Off C4 channel 1`, `Control Change
  64 value 127 channel 1`, `Program Change 12 channel 2`.
- Use color intentionally:
  - note on: one clear color
  - note off: a different clear color
  - control/program/pitch messages: separate but quieter colors
  - SysEx and unsupported messages: distinct warning/neutral treatments
  - selected rows and active transport state: visible without overwhelming the
    content
- Provide monochrome fallback behavior for terminals without color support.
- Keep raw MIDI bytes visible, but make them secondary to time, direction, and
  English description.
- Preserve utility density. Avoid decorative borders that steal space from MIDI
  data; use color, alignment, headings, and spacing to make scanning easier.
- Make the output-directory flow browser-like. Instead of asking for a blank text
  path only, show the current directory, parent directory, available child
  directories, and a clear selected destination. Allow manual path entry as an
  advanced escape hatch.
- Keep keyboard flows predictable: arrows move selection, enter confirms, escape
  cancels, and status text explains validation failures such as non-directory
  paths or permission errors.

## Architecture Principles

- Prefer reusable domain modules over command-local helper copies.
- Keep `command_*.c` files thin: parse command arguments, call shared behavior,
  and translate results into CLI or TUI presentation.
- Separate domain behavior from presentation. MIDI parsing, sequence loading,
  event collection, recording, and output should not know whether they are used
  by the CLI or TUI.
- Return structured errors from shared modules. CLI code can print them; TUI code
  can turn them into status messages and log entries.
- Make ownership explicit in public APIs. Each module should clearly document
  which side allocates, disposes, opens, closes, or frees a resource.
- Prefer small headers. Expose only types and functions needed by other modules;
  keep implementation-only details private in `.c` files.
- Avoid broad rewrites while extracting. If a module boundary exposes a deeper
  design issue, fix it when the fix is local and behavior can be verified.
- Keep TUI rendering data-driven. The renderer should receive event rows with
  category, timestamp, bytes, and description instead of re-parsing MIDI bytes.
- Treat color as semantic state, not decoration. Color choices should help users
  distinguish MIDI event types and active transport state quickly.
- Keep the Makefile pattern consistent. Each new shared module adds its sources
  to `APP_SRCS`. Each new test binary adds an entry in `TESTS`, a dedicated
  build rule pairing the test source with the module under test, and a run line
  in the `test` target. Do not add new include search paths (e.g., `-Iinclude`)
  without creating the corresponding directory.

## Migration Plan

1. Lock current behavior with focused regression tests before moving logic.
   Start with non-hardware code: file scanning, playback event ordering, path
   formatting, clock formatting, and any pure MIDI sequence/event conversion that
   can be tested from fixture `.mid` files.

2. Define shared error and ownership conventions before the first extraction.
   Keep this lightweight: enough to prevent every new module from inventing its
   own reporting and cleanup style.

3. Extract playback event handling first. This removes duplication between
   `command_play.c` and `command_tui.c` and creates an obvious shared boundary.
   The CLI can keep printing to stderr while the TUI maps errors into status/log
   messages.

   Before implementing `midi_sequence.h`, agree the event struct shape with the
   Phase 5 MIDI description model design. The contract: `midi_sequence` produces
   events carrying timestamps and raw MIDI bytes; `midi_describe` (Phase 5)
   reads those bytes and returns a category and text description; Phases 7–8
   renderer rows carry both. Define the shared event row struct in Phase 2 so
   Phase 5 can extend it without requiring a breaking change.

4. Extract MIDI output next. Keep CoreMIDI port ownership explicit so both CLI
   playback and TUI playback use the same send path.

5. Extract recording sequence construction. Separate the engine that converts
   MIDI packets into `MusicSequence` events from the CLI run loop and the TUI
   pause/resume UI behavior.

6. Add MIDI event description support. Build this as shared domain logic, then
   use it for both loaded sequence rows and live stream rows.

7. Split TUI-only support code: log buffer, recordings file list, directory
   browser, and renderer.
   These moves should be mostly mechanical once MIDI engine code is shared.

8. Redesign the TUI layout after the renderer is isolated. Introduce the side
   file browser, taller live stream, description column, semantic color pairs,
   and monochrome fallback in one coherent UI pass.

9. Replace the raw output-directory prompt with the directory browser flow.
   Preserve manual path entry for users who already know the exact location.

10. Shrink `command_tui.c` last. It should read as a state machine and event loop,
   not as the place where MIDI parsing, CoreMIDI setup, file scanning, and curses
   drawing all live.

11. Revisit the remaining command files. After the shared modules exist,
   simplify `command_play.c`, `command_record.c`, and `command_list.c` where they
   can become thinner adapters without losing clarity.

## Swift Assessment

Moving to Swift may be useful later if the product direction becomes a native
macOS app or SwiftUI frontend. It is not the best immediate readability fix for
the current CLI/TUI tool.

Practical concerns with a Swift rewrite now:

- rewrite risk is high compared with targeted extraction
- CoreMIDI and AudioToolbox still require callback and pointer interop
- curses support would remain C-oriented
- tests and build setup would need to be recreated
- behavior could regress before the architecture improves

Revisit Swift after the C boundaries are clean. At that point, a Swift rewrite
would have a clearer set of modules to port.

## Success Criteria

- `command_tui.c` is reduced to TUI orchestration and input handling.
- CLI and TUI share playback event extraction, MIDI output, and recording
  sequence construction.
- New modules have small public headers with narrow responsibilities.
- The TUI uses a column layout with a side file browser, a taller live stream,
  semantic event colors, and a human-readable description column.
- The output-directory change flow is browser-like and validates the selected
  destination before applying it.
- `make test` continues to pass after each extraction step.
- Hardware-dependent behavior is manually smoke-tested on macOS after shared
  CoreMIDI code is moved.

## Risks

- CoreMIDI behavior is hardware-dependent, so automated tests will not cover the
  full runtime path.
- Error handling differs between CLI and TUI; shared modules should report
  structured errors instead of printing directly.
- Pause/resume timing in the TUI is more complex than CLI recording and should
  stay explicitly tested or manually verified during recorder extraction.
- Terminal size and color support vary. The redesigned layout needs explicit
  minimum-size behavior and a readable monochrome fallback.
