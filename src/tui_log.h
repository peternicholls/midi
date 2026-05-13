#ifndef TUI_LOG_H
#define TUI_LOG_H

#include <stdarg.h>
#include <stddef.h>

#define TUI_LOG_CAPACITY 128
#define TUI_LOG_LINE_LENGTH 160

typedef struct TuiLog TuiLog;

typedef struct TuiLogEntry {
  char line[TUI_LOG_LINE_LENGTH];
} TuiLogEntry;

TuiLog *tui_log_create(void);
void tui_log_destroy(TuiLog *log);

int tui_log_append(TuiLog *log, const char *format, ...);
int tui_log_vappend(TuiLog *log, const char *format, va_list args);
size_t tui_log_snapshot(TuiLog *log, TuiLogEntry *out_entries, size_t capacity);

#endif
