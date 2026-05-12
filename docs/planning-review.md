# Planning Docs Review

Review of `docs/tui-refactor-plan.md` and `docs/tui-refactor-sprints/` for
completeness, gaps, issues, and improvements. Conducted against the current
source tree on 2026-05-12.

## Overall Assessment

The structure is solid. The phased approach is logical, phases are well-scoped,
and the principle of leaving the project buildable after every phase is correctly
stated. The main problems are around unacknowledged existing code, a few
dangerous ambiguities, and no status tracking.

---

## Gaps

### 1. `tui_model.c/.h` is not addressed anywhere in the plan

It already exists with five pure helpers: `tui_is_midi_filename`,
`tui_format_recording_name`, `tui_join_path`, `tui_format_midi_bytes`, and
`tui_format_clock_time`. It has tests. The target shape in
`tui-refactor-plan.md` does not mention it.

Phase 6 would naturally absorb `tui_is_midi_filename` and `tui_join_path` into
`tui_files.c`. Phase 5 might absorb `tui_format_midi_bytes` into
`midi_describe.c`. Neither phase acknowledges this. Without explicit guidance,
these functions either get duplicated, orphaned, or moved inconsistently.

**Resolution needed:** Add a section to `tui-refactor-plan.md` describing
`tui_model.c`'s fate. Either it becomes the home for remaining pure formatters,
or its functions migrate into the new modules and the file is deleted or
narrowed.

---

### 2. Phase 1 exits without a concrete deliverable

The outcome is "possible `midi_error.h`" and "possible docs note." This is too
open. Phase 2 immediately starts extracting shared modules. If the error
convention is not decided before Phase 2 begins, each phase will invent its own
style and Phase 1's goal is defeated.

**Resolution needed:** Phase 1 should have a required output: at minimum, a
short header or small `src/midi_result.h` that makes the convention concrete.
Mark it required, not possible.

---

### 3. `status_line.c/.h` is never mentioned

It exists, is tested, and has 48 lines. It is unclear whether it stays untouched
or whether some of its behavior overlaps with what Phase 6 (`tui_log.c`) will
do. Phase 10 lists `app_support.*` for review but not `status_line.*`.

---

### 4. No status tracking across phases

Every checkbox in every phase file is unchecked. The project already has working
code and tests, so some Phase 0 baseline work is implicitly done. There is no
way to tell what has actually been completed. The lack of any completed or
in-progress markers makes the docs misleading if this is an active project.

---

### 5. The integration point between Phase 2 and Phase 5 is undefined

Phase 2 produces a playback event struct. Phase 5 produces a description struct.
Phases 7 and 8 will need renderer rows that combine both. The plan does not
define the data contract between them. Does `midi_sequence` store raw bytes and
`midi_describe` processes those bytes? Or does Phase 2 already include
description data? This interface matters for Phases 6–8 and should be explicit
before Phase 2 is implemented.

---

## Issues

### 6. `-Iinclude` in the Makefile but no `include/` directory exists

This is a live inconsistency in the current Makefile. It does not cause a build
failure because it is an include search path, not a required directory, but it
is misleading and should either be removed or the directory created with intent.

---

### 7. Phase 4 async recorder risk is noted but not tasked

"Recorder callbacks may run asynchronously. Preserve atomic counter behavior."
This appears only as a risk. CoreMIDI input callbacks run on a real-time thread.
The counter behavior needs an explicit design decision (atomics, lock, etc.)
before extraction begins, not just a warning note.

---

### 8. Phase 2–4 tests are marked "if practical"

Phrases like "add tests if fixtures are available" and "possible tests if a
generated sequence can be inspected reliably" create an escape hatch that could
lead to zero new test coverage for the most complex extracted modules. For
`midi_sequence.c` and `midi_recorder.c`, the plan should assert minimum required
test coverage, even if that means a small synthetic fixture. The current
permissive language undermines the refactor's safety net.

---

### 9. Phase 5 category list diverges from the sprint summary

The phase-05 file adds `channel pressure`, `poly pressure`, and `incomplete` to
the event category list. The summary doc in `tui-refactor-sprints.md` only lists
note on/off, control change, program change, pitch bend, SysEx, and unsupported.
The two should match.

---

### 10. Final Makefile shape is never described

Each new module and test binary (there will be at least six new ones by Phase 6)
needs manual Makefile entries. No phase describes the expected end state. The
`TESTS` variable and per-test rules will need explicit expansion. Worth spelling
out in Phase 0 or in a note on the Makefile pattern so phases do not each handle
it differently.

---

## Improvements

### 11. Add explicit dependency lines per phase file

Currently dependencies are only implicit. Phase 7 lists "Phase 5 MIDI
description model, Phase 6 TUI support modules" as inputs, but this could be a
short formal line at the top of each file:

```
Depends on: Phase 1, Phase 5, Phase 6
```

This makes the sequencing unambiguous and prevents phases being started out of
order.

---

### 12. Phase 11 verification does not cover the no-MIDI-device case

The final checklist covers normal operation but does not include: what happens
when no MIDI source or destination is available. This is a regression risk. The
TUI should degrade gracefully when MIDI devices are absent, not crash or hang.

---

### 13. Milestone dependency chain is not stated

The milestone labels (A–D) are defined, but their dependencies are not. Someone
batching work could start Milestone C (visual redesign, Phases 8–9) before Phase
7 renderer isolation is complete, which is the wrong order. The milestone section
should explicitly state that each milestone depends on the previous one being
complete.

---

## Summary

| # | Category    | Item                                               | Severity |
|---|-------------|----------------------------------------------------|----------|
| 1 | Gap         | `tui_model.c` fate unaddressed                     | High     |
| 2 | Gap         | Phase 1 has no required concrete output            | High     |
| 5 | Gap         | Phase 2/5 data contract undefined                  | Medium   |
| 3 | Gap         | `status_line.c` never mentioned                    | Low      |
| 4 | Gap         | No status/progress tracking in phase files         | Low      |
| 7 | Issue       | Async recorder risk noted but not tasked           | Medium   |
| 8 | Issue       | Phase 2–4 tests "if practical" escape hatch        | Medium   |
| 12| Improvement | Phase 11 no-device error path missing              | Medium   |
| 6 | Issue       | Makefile `-Iinclude` with no directory             | Low      |
| 9 | Issue       | Phase 5 category list diverges from summary        | Low      |
| 10| Issue       | Final Makefile shape never described               | Low      |
| 11| Improvement | Explicit dependency lines per phase file           | Medium   |
| 13| Improvement | Milestone dependency chain not stated              | Low      |

## Priority Actions Before Starting Phase 2

1. Decide what happens to `tui_model.c` and record it in `tui-refactor-plan.md`.
2. Make Phase 1's output required: commit to a concrete error/result convention.
3. Define the interface contract between `midi_sequence` and `midi_describe`
   before implementing either.
