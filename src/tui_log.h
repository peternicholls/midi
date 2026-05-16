#ifndef TUI_LOG_H
#define TUI_LOG_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

#include "midi_describe.h"

#define TUI_LOG_CAPACITY 128
#define TUI_LOG_LINE_LENGTH 160
#define TUI_LOG_FIELD_LENGTH 32
#define TUI_LOG_DESCRIPTION_LENGTH 96

typedef struct TuiLogMidiFields {
  char time[TUI_LOG_FIELD_LENGTH];
  char direction[TUI_LOG_FIELD_LENGTH];
  char channel[TUI_LOG_FIELD_LENGTH];
  char event[TUI_LOG_FIELD_LENGTH];
  char target[TUI_LOG_FIELD_LENGTH];
  char value[TUI_LOG_FIELD_LENGTH];
  char bytes[TUI_LOG_FIELD_LENGTH * 2];
  char description[TUI_LOG_DESCRIPTION_LENGTH];
  MidiDescriptionCategory category;
} TuiLogMidiFields;

typedef struct TuiLog TuiLog;

typedef struct TuiLogEntry {
  char line[TUI_LOG_LINE_LENGTH];
  bool has_midi_fields;
  TuiLogMidiFields midi;
} TuiLogEntry;

TuiLog *tui_log_create(void);
void tui_log_destroy(TuiLog *log);

int tui_log_append(TuiLog *log, const char *format, ...);
int tui_log_vappend(TuiLog *log, const char *format, va_list args);
int tui_log_append_midi(TuiLog *log, const TuiLogMidiFields *fields,
                        const char *line);
size_t tui_log_snapshot(TuiLog *log, TuiLogEntry *out_entries, size_t capacity);

#endif
