# Code Review — midi-capture

**Reviewed**: 11 May 2026  
**Build**: passes cleanly (`make && make test`) with no compiler warnings under `-Wall -Wextra -Wpedantic`.

---

## Summary

The codebase is tidy, well-structured, and intentionally small. Error paths are consistently handled, CoreMIDI/AudioToolbox resources are properly disposed, and the use of atomics for cross-thread state is correct. The issues below are ordered roughly by severity.

---

## Bugs / Correctness

### 1. NULL dereference before NULL check in `parse_long_arg` — `src/main.c`

```c
static int parse_long_arg(const char *value, long *result) {
    char *end = NULL;
    long parsed = strtol(value, &end, 10);          // ← value dereferenced here
    if (value == NULL || *value == '\0' || ...) {   // ← NULL checked too late
```

`strtol` dereferences `value` before the `value == NULL` guard is reached. If a NULL pointer is ever passed (the callers are currently safe, but nothing enforces this), this is undefined behaviour. Fix: move the NULL check before the `strtol` call.

```c
static int parse_long_arg(const char *value, long *result) {
    if (value == NULL || *value == '\0') {
        return 0;
    }
    char *end = NULL;
    long parsed = strtol(value, &end, 10);
    if (end == NULL || *end != '\0') {
        return 0;
    }
    *result = parsed;
    return 1;
}
```

---

### 2. Running status treated as UNSUPPORTED — `src/midi_parser.c`

Any byte below `0x80` is classified as `MIDI_PARSED_UNSUPPORTED` with a consumed length of 1:

```c
if (status < 0x80) {
    out->kind = MIDI_PARSED_UNSUPPORTED;
    out->length = 1;
    return 1;
}
```

MIDI 1.0 running status allows a device to omit the status byte when sending repeated messages of the same type (e.g., a stream of Note On events). Treating these data bytes as one-byte "unsupported" messages silently discards each one. The README notes this as a known edge case, but it could cause silent data loss with hardware that uses running status aggressively.

**Minimum fix**: document the limitation explicitly in the header.  
**Better fix**: thread a `last_status` byte through the parser so running-status sequences are reconstructed correctly.

---

### 3. Order counter incremented before allocation check — `src/main.c`

In `append_playback_event`:

```c
event->order = events->next_order++;   // incremented unconditionally

if (length <= sizeof(event->inline_bytes)) {
    ...
} else {
    event->data = (uint8_t *)malloc(length);
    if (event->data == NULL) {
        ...
        return 0;                      // next_order already incremented
    }
}
```

If the `malloc` fails, `events->next_order` is left incremented but `events->count` is not, so the sequence number is permanently out of step. In practice, an OOM here would likely abort the whole operation, but the ordering guarantee is technically broken. Move the increment after the success path, or initialise order at the end just before `events->count += 1`.

---

## Design / API Issues

### 4. Duplicated MIDI message length logic

`midi_channel_message_length` in `src/midi_parser.c` and `midi_message_length_from_status` in `src/main.c` are identical switch statements:

```c
// midi_parser.c
static size_t midi_channel_message_length(uint8_t status) { ... }

// main.c
static size_t midi_message_length_from_status(uint8_t status) { ... }
```

The internal version in `midi_parser.c` should either be exposed via `midi_parser.h` or `main.c` should derive the length from the `MidiParsedMessage` it already has. Keeping both in sync is a maintenance burden.

---

### 5. `print_osstatus_and_return` is misleadingly named

The function is `void` — it prints an error message and exits, but despite its name it does not `return` from the calling function. Callers still have to write their own `return`:

```c
// Current pattern at every call site:
print_osstatus_and_return("NewMusicSequence", status);
return 1;
```

Rename to `log_osstatus_error`, or — if the intent is to stop execution — change it to a macro that also issues the `return`.

---

### 6. `append_timestamp` name is misleading — `src/status_line.c`

The function writes (formats) into the buffer from position 0; it does not append to existing content. `format_timestamp` or `format_mmss` would be clearer.

---

### 7. Heap allocation on the playback hot path — `src/main.c`

`send_midi_bytes` allocates and frees a `MIDIPacketList` on the heap for every MIDI message sent, including 3-byte channel messages:

```c
MIDIPacketList *packet_list = (MIDIPacketList *)malloc(packet_list_size);
```

For short messages a stack-allocated buffer (or a fixed-size array on the stack) avoids the malloc/free cost on what is a timing-sensitive playback loop. Example:

```c
// For messages up to 256 bytes, stack-allocate is fine:
uint8_t buf[sizeof(MIDIPacketList) + 256];
MIDIPacketList *packet_list = (MIDIPacketList *)buf;
```

For SysEx that can exceed the stack limit, keep the heap path as a fallback.

---

### 8. `get_endpoint_name` fallback uses `%u` for `MIDIEndpointRef` — `src/main.c`

```c
snprintf(buffer, buffer_size, "endpoint-%u", (unsigned int)endpoint);
```

`MIDIEndpointRef` is typedef'd to `MIDIObjectRef` which is `UInt32` in current SDK headers, but casting it through `unsigned int` and using `%u` is fragile if that typedef ever widens. Use `%lu` with `(unsigned long)` to be safe.

---

### 9. `-Wno-deprecated-declarations` suppresses all deprecation warnings globally — `Makefile`

```makefile
APP_CFLAGS := $(CFLAGS) -Wno-deprecated-declarations
```

This silently masks any future deprecation in `src/main.c`. The two APIs that prompted this flag are `MIDIInputPortCreate` and `MusicSequenceSetMIDIEndpoint`. Either document which symbols are deprecated and why the workaround is acceptable, or suppress only the specific call sites with a targeted `#pragma clang diagnostic`.

---

## Test Coverage Gaps

The existing tests cover the happy path well. The following cases are unexercised:

| Missing test | Why it matters |
|---|---|
| `NULL` bytes input to `midi_next_message` | Documents contract; would catch future refactors |
| `NULL` out-parameter to `midi_next_message` | Same |
| Sysex without terminator (`0xF0` … no `0xF7`) | Should return 0 with `MIDI_PARSED_INCOMPLETE`; currently untested |
| Running-status byte (< `0x80`) | Documents current behaviour explicitly so any future change is intentional |
| `0xF2` Song Position Pointer (3-byte system) | `MIDI_PARSED_UNSUPPORTED` with length 3 |
| `0xF1` MTC Quarter Frame (2-byte system) | `MIDI_PARSED_UNSUPPORTED` with length 2 |
| `format_status_line` with `buffer_size = 0` | Defensive edge case |
| `format_status_line` with `buffer_size` too small | Confirm truncation does not crash |

---

## Minor / Hygiene

### `output.mid` is not cleaned by `make clean` and likely not gitignored

The recorded test file in the repo root is a build artefact. Add it to the clean target and/or `.gitignore`:

```makefile
clean:
    rm -f $(APP) $(TESTS) output.mid
```

### `command_list` works without a `MIDIClientRef`

`MIDIGetNumberOfSources()` and `MIDIGetDestination()` function without creating a client, but on some macOS versions the MIDI server may not have fully initialised yet, causing endpoint names to come back empty. Creating a temporary client just for the list command guarantees the daemon is running before querying endpoints.

### `(void)event_size` in `collect_track_playback_events`

The `event_size` return value from `MusicEventIteratorGetEventInfo` is silently discarded. A short comment explaining why (the event pointer/length is only used for known event types; raw size is unused) would clarify intent for future readers.

---

## Quick Reference

| # | File | Severity | Issue |
|---|---|---|---|
| 1 | `src/main.c` | **Bug** | `parse_long_arg` dereferences before NULL check |
| 2 | `src/midi_parser.c` | **Bug / Limitation** | Running status silently dropped |
| 3 | `src/main.c` | **Bug (minor)** | `next_order` incremented before malloc check |
| 4 | `src/main.c` + `src/midi_parser.c` | Design | Duplicated message length switch |
| 5 | `src/main.c` | Design | `print_osstatus_and_return` misleading name |
| 6 | `src/status_line.c` | Design | `append_timestamp` misleading name |
| 7 | `src/main.c` | Performance | Heap allocation per message on playback path |
| 8 | `src/main.c` | Portability | `%u` for `MIDIEndpointRef` in fallback name |
| 9 | `Makefile` | Maintainability | Global `-Wno-deprecated-declarations` |
| 10 | `tests/` | Testing | Multiple missing edge-case tests |
| 11 | Root | Hygiene | `output.mid` not cleaned / not gitignored |
