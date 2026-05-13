#ifndef MIDI_RESULT_H
#define MIDI_RESULT_H

#include <CoreMIDI/CoreMIDI.h>

typedef enum MidiResultCode {
  MIDI_RESULT_OK = 0,
  MIDI_RESULT_INVALID_ARGUMENT,
  MIDI_RESULT_NO_MEMORY,
  MIDI_RESULT_OSSTATUS
} MidiResultCode;

typedef struct MidiResult {
  MidiResultCode code;
  OSStatus os_status;
  const char *operation;
} MidiResult;

static inline MidiResult midi_result_ok(void) {
  MidiResult result = {MIDI_RESULT_OK, noErr, NULL};
  return result;
}

static inline MidiResult midi_result_invalid_argument(const char *operation) {
  MidiResult result = {MIDI_RESULT_INVALID_ARGUMENT, noErr, operation};
  return result;
}

static inline MidiResult midi_result_no_memory(const char *operation) {
  MidiResult result = {MIDI_RESULT_NO_MEMORY, noErr, operation};
  return result;
}

static inline MidiResult midi_result_osstatus(const char *operation,
                                              OSStatus status) {
  MidiResult result = {MIDI_RESULT_OSSTATUS, status, operation};
  return result;
}

static inline int midi_result_is_ok(MidiResult result) {
  return result.code == MIDI_RESULT_OK;
}

/*
 * Shared module conventions:
 *
 * - Modules return MidiResult for recoverable failures and do not print, write
 *   TUI status, or append TUI logs themselves.
 * - operation points to a static label owned by the caller or module; callers
 *   must not free it.
 * - *_init initializes caller-owned structs.
 * - *_dispose releases internal resources without freeing the struct.
 * - *_free releases heap-owned collections returned by a module.
 * - *_open and *_close own CoreMIDI client/port lifetimes.
 */

#endif
