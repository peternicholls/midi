#include "tui_log.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

struct TuiLog {
  pthread_mutex_t mutex;
  TuiLogEntry entries[TUI_LOG_CAPACITY];
  size_t start;
  size_t count;
};

TuiLog *tui_log_create(void) {
  TuiLog *log = (TuiLog *)calloc(1, sizeof(*log));

  if (log == NULL) {
    return NULL;
  }
  if (pthread_mutex_init(&log->mutex, NULL) != 0) {
    free(log);
    return NULL;
  }
  return log;
}

void tui_log_destroy(TuiLog *log) {
  if (log == NULL) {
    return;
  }
  pthread_mutex_destroy(&log->mutex);
  free(log);
}

int tui_log_vappend(TuiLog *log, const char *format, va_list args) {
  char line[TUI_LOG_LINE_LENGTH];
  size_t slot;

  if (log == NULL || format == NULL) {
    return 0;
  }

  vsnprintf(line, sizeof(line), format, args);

  pthread_mutex_lock(&log->mutex);
  slot = (log->start + log->count) % TUI_LOG_CAPACITY;
  if (log->count == TUI_LOG_CAPACITY) {
    log->start = (log->start + 1) % TUI_LOG_CAPACITY;
    slot = (log->start + log->count - 1) % TUI_LOG_CAPACITY;
  } else {
    log->count += 1;
  }
  snprintf(log->entries[slot].line, sizeof(log->entries[slot].line), "%s",
           line);
  pthread_mutex_unlock(&log->mutex);
  return 1;
}

int tui_log_append(TuiLog *log, const char *format, ...) {
  va_list args;
  int result;

  va_start(args, format);
  result = tui_log_vappend(log, format, args);
  va_end(args);
  return result;
}

size_t tui_log_snapshot(TuiLog *log, TuiLogEntry *out_entries,
                        size_t capacity) {
  size_t copied = 0;

  if (log == NULL || out_entries == NULL || capacity == 0) {
    return 0;
  }

  pthread_mutex_lock(&log->mutex);
  while (copied < log->count && copied < capacity) {
    size_t index = (log->start + copied) % TUI_LOG_CAPACITY;
    out_entries[copied] = log->entries[index];
    copied += 1;
  }
  pthread_mutex_unlock(&log->mutex);
  return copied;
}
