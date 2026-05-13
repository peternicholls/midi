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
  assert(strcmp(snapshot[1].line, "two 2") == 0);

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
  test_snapshot_respects_capacity();
  test_wraparound_keeps_newest_entries();
  return 0;
}
