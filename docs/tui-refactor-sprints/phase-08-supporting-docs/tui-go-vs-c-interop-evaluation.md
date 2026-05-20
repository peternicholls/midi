# TUI Option Evaluation: C-Interop Contract vs Go Bubble Tea

**Date:** 2026-05-19

## Question

Should this project continue with the C-interoperable TUI contract language and
existing C renderer boundary, or move the design rendering into Go using a TUI
framework such as Bubble Tea?

## Current Repo Reality

This repo is currently:

- one C application binary built by `Makefile`
- linked directly against macOS frameworks such as CoreMIDI and CoreFoundation
- tested with focused C test binaries
- structured so `command_tui.c` owns mutable state and operations
- structured so `tui_render.c` renders a snapshot from `tui_render.h`

That matters because a Go renderer would not be landing in a greenfield app. It
would be entering a project that already has a working state boundary and a
single-language build system.

## The Two Real Options

There are really only two credible strategies.

### Option 1: Stay C-first

Keep:

- application state in C
- CoreMIDI integration in C
- renderer in C
- TUI design language as a C-interoperable contract

Improve:

- the design contract
- render fidelity
- screen verification
- layout primitives

### Option 2: Move the TUI into Go

Use Bubble Tea or a similar Go framework for:

- model/update/view loop
- terminal rendering
- key and resize handling
- higher-level composition

Keep some or all operational logic in C via `cgo`, or rewrite larger parts of
the app into Go.

## What Bubble Tea Is Good At

Bubble Tea is strong when you want:

- fast iteration on full-screen TUI layout
- a clear model/update/view loop
- easier composition of UI components
- better support for modern terminal behavior than raw curses-style drawing
- an ecosystem around components and styling

For design-heavy TUIs, Bubble Tea is often a better authoring experience than
manual curses-style rendering.

If this repo were starting from scratch, Bubble Tea would be a serious option.

## What Bubble Tea Costs In This Repo

### 1. You introduce a second runtime and build system

Today the repo builds with:

- `clang`
- one `Makefile`
- one binary

Moving the renderer into Go means introducing:

- `go.mod`
- `go build`
- `cgo` if the C code remains in the loop
- a mixed-language release path

That is not just a UI choice. It changes the project’s toolchain identity.

### 2. `cgo` becomes part of the critical path

If Go owns the UI but C still owns CoreMIDI, playback, recording, parsing, and
app state, then Go and C must communicate through `cgo`.

That introduces:

- pointer-passing rules
- data-copy boundaries
- ownership questions for strings, arrays, and structs
- callback complexity if either side needs to notify the other asynchronously
- more complicated debugging and profiling

This is especially relevant here because the current TUI is live and stateful.
It is not a static menu. RX/TX activity, playback position, logs, recordings,
and live note state all update continuously.

### 3. Darwin/CoreFoundation interop is not trivial

This app depends on CoreMIDI and CoreFoundation on macOS.

That is already handled directly in C. If Go is added on top, one of two things
happens:

1. C continues to own the Apple framework integration, which means Go talks to C
   through `cgo`.
2. Go starts owning more of the Apple integration, which means the project now
   needs a stable Go-to-Darwin interop layer.

Neither is impossible, but both are materially more complex than staying in C.

### 4. You risk duplicating the state model

Bubble Tea wants a Go `model` that drives `Update` and `View`.

This repo already has a C-side app model inside `command_tui.c` plus a render
snapshot in `tui_render.h`.

If only the rendering moves to Go, there is a strong risk of ending up with:

- one app model in C
- one presentation model in Go
- one event loop in C
- one UI event loop in Go

That is the exact kind of translation gap that caused the current mockup drift,
just in a new language boundary.

### 5. The best Bubble Tea version is probably not a partial migration

Bubble Tea is strongest when it owns the TUI application loop.

A partial split where:

- C owns operational state
- Go owns rendering
- both sides coordinate key events and UI transitions

is usually the awkward version.

If Bubble Tea is chosen, the cleaner architecture is usually:

- Go owns the TUI event loop and UI model
- C becomes a thin library for MIDI capture/playback logic
- or the MIDI layer also gets rewritten or wrapped more fully

That is much closer to a rewrite than a renderer swap.

## What The C-Interop Contract Option Costs

The C-interop contract option is less exciting, but cheaper and more direct.

It keeps:

- one language in the app core
- one binary build path
- the current `command_tui.c` -> `TuiRenderState` -> `tui_render.c` ownership
- all existing tests and build assumptions

Its cost is mostly in design discipline:

- write fixed-size screen contracts
- tighten naming and ownership
- add renderer snapshot verification
- improve terminal primitives so the mockup language survives in text mode

That is real work, but it is local work.

## Comparison

| Axis | C-interop contract | Go + Bubble Tea |
| --- | --- | --- |
| Build complexity | Low | High |
| Fit with current repo | High | Low to medium |
| UI authoring ergonomics | Medium | High |
| Design iteration speed | Medium | High |
| Interop risk | Low | High if C remains owner |
| CoreMIDI integration disruption | Low | Medium to high |
| Risk of split-brain state | Low | High in partial migration |
| Time to first visible improvement | Short | Medium to long |
| Long-term rewrite potential | Limited but solid | High |

## Where Bubble Tea Actually Makes Sense

Bubble Tea becomes attractive if one of these becomes true:

1. The project is willing to move the whole TUI application loop into Go.
2. The project is willing to treat the current C code as a lower-level library,
   not the application shell.
3. The team expects substantial future UI complexity where Bubble Tea’s model
   and component ecosystem will keep paying for themselves.
4. The project wants to invest in a larger modernization, not just fix Phase 8
   parity.

If that is the plan, the right question is not "Should rendering move to Go?"
The right question is:

`Should the TUI application boundary move to Go?`

That is a much larger decision.

## Where The C-Interop Contract Clearly Wins

The current option wins if the goal is:

- get the design intent into code faster
- preserve the current C operational model
- avoid mixed-language runtime complexity
- improve the TUI without rewriting the application shell

That is the situation this repo is in right now.

## Recommended Direction

For this repo, right now, the better path is:

1. keep the app C-first
2. keep the renderer boundary in C
3. use the C-interoperable TUI contract language as the design system
4. add fixed-size screen contracts and renderer snapshot validation
5. only reconsider Go if the project later chooses to move the full TUI app
   boundary, not just the view layer

## Adopted Direction

As of 2026-05-19, the project is choosing the C-first path.

This means:

- the TUI application boundary stays in C
- the renderer boundary stays `command_tui.c` -> `TuiRenderState` ->
   `tui_render.c`
- the C-interoperable contract language becomes the design/reference language
- Bubble Tea is not in scope for Phase 8 parity work

Revisit conditions:

- only revisit a Go/Bubble Tea direction if the project decides to move the
   full TUI application loop into Go
- do not reopen the question just to change rendering syntax or styling

## Practical Recommendation

### Best near-term choice

Use the new C-interoperable TUI language and push Phase 8 parity through the
existing C renderer.

Reason:

- shortest path to visible improvement
- lowest architectural risk
- no new language boundary in the hot path

### Best long-term Go choice

If Go is still attractive, do not move only the drawing code.

Instead, prototype one of these:

1. a separate Go spike that owns a full Bubble Tea TUI model and talks to a
   very thin C MIDI layer
2. a fuller rewrite plan where Go becomes the TUI application shell and C is
   reduced to narrow MIDI-facing modules

That will tell you whether Bubble Tea is actually worth it here.

## Bottom Line

Bubble Tea is a strong TUI framework, but in this repo it is not a free upgrade.
It is a boundary shift.

If the goal is better design implementation, the C-interop contract is the
better move.

If the goal is broader platform and architecture modernization, then Bubble Tea
is worth exploring, but only as part of a larger application-boundary rewrite,
not as a narrow rendering substitution.

## Immediate Next Steps

The next C-first slice should be:

1. write fixed-size screen contracts for `90x24` and `120x36`
2. use those contracts to drive Phase 8 parity fixes in `tui_render.c`
3. make Live Player channel scope explicit app state in `command_tui.c`
4. add renderer-level snapshot verification for fixed-size screens