#include "tui_log.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_append_and_snapshot_preserve_order(void) {
  TuiLog *log = tui_log_create();
  TuiLogEntry snapshot[4];

  assert(log != NULL);
  assert(tui_log_append(log, "one") == 1);
  assert(tui_log_append(log, "two %d", 2) == 1);

  assert(tui_log_snapshot(log, snapshot, 4) == 2);
  assert(strcmp(snapshot[0].line, "one") == 0);
  assert(!snapshot[0].has_midi_fields);
  assert(strcmp(snapshot[1].line, "two 2") == 0);
  assert(!snapshot[1].has_midi_fields);

  tui_log_destroy(log);
}

static void test_midi_fields_are_snapshotted(void) {
  TuiLog *log = tui_log_create();
  TuiLogEntry snapshot[2];
  TuiLogMidiFields fields;

  assert(log != NULL);
  memset(&fields, 0, sizeof(fields));
  snprintf(fields.time, sizeof(fields.time), "00:01.234");
  snprintf(fields.direction, sizeof(fields.direction), "RX");
  snprintf(fields.channel, sizeof(fields.channel), "1");
  snprintf(fields.event, sizeof(fields.event), "note on");
  snprintf(fields.target, sizeof(fields.target), "C4");
  snprintf(fields.value, sizeof(fields.value), "100");
  snprintf(fields.bytes, sizeof(fields.bytes), "90 3C 64");
  snprintf(fields.description, sizeof(fields.description),
           "Note on C4 v100 ch1");
  fields.category = MIDI_DESCRIPTION_NOTE_ON;

  assert(tui_log_append_midi(
             log, &fields, "00:01.234 RX  90 3C 64  Note on C4 v100 ch1") == 1);
  assert(tui_log_snapshot(log, snapshot, 2) == 1);
  assert(snapshot[0].has_midi_fields);
  assert(strcmp(snapshot[0].midi.time, "00:01.234") == 0);
  assert(strcmp(snapshot[0].midi.direction, "RX") == 0);
  assert(strcmp(snapshot[0].midi.channel, "1") == 0);
  assert(strcmp(snapshot[0].midi.event, "note on") == 0);
  assert(strcmp(snapshot[0].midi.target, "C4") == 0);
  assert(strcmp(snapshot[0].midi.value, "100") == 0);
  assert(strcmp(snapshot[0].midi.bytes, "90 3C 64") == 0);
  assert(snapshot[0].midi.category == MIDI_DESCRIPTION_NOTE_ON);

  tui_log_destroy(log);
}

static void test_snapshot_respects_capacity(void) {
  TuiLog *log = tui_log_create();
  TuiLogEntry snapshot[1];

  assert(log != NULL);
  assert(tui_log_append(log, "first") == 1);
  assert(tui_log_append(log, "second") == 1);

  assert(tui_log_snapshot(log, snapshot, 1) == 1);
  assert(strcmp(snapshot[0].line, "first") == 0);

  tui_log_destroy(log);
}

static void test_wraparound_keeps_newest_entries(void) {
  TuiLog *log = tui_log_create();
  TuiLogEntry snapshot[TUI_LOG_CAPACITY];
  char expected[32];

  assert(log != NULL);
  for (size_t i = 0; i < TUI_LOG_CAPACITY + 3; ++i) {
    assert(tui_log_append(log, "line %zu", i) == 1);
  }

  assert(tui_log_snapshot(log, snapshot, TUI_LOG_CAPACITY) == TUI_LOG_CAPACITY);
  assert(strcmp(snapshot[0].line, "line 3") == 0);
  snprintf(expected, sizeof(expected), "line %zu",
           (size_t)TUI_LOG_CAPACITY + 2);
  assert(strcmp(snapshot[TUI_LOG_CAPACITY - 1].line, expected) == 0);

  tui_log_destroy(log);
}

int main(void) {
  test_append_and_snapshot_preserve_order();
  test_midi_fields_are_snapshotted();
  test_snapshot_respects_capacity();
  test_wraparound_keeps_newest_entries();
  return 0;
}
