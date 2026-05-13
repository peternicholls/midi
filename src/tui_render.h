#ifndef TUI_RENDER_H
#define TUI_RENDER_H

#include "midi_describe.h"
#include "tui_files.h"
#include "tui_log.h"

#include <stdbool.h>
#include <stddef.h>

#define TUI_RENDER_CLOCK_TEXT_LENGTH 32
#define TUI_RENDER_BYTE_TEXT_LENGTH 64
#define TUI_RENDER_DESCRIPTION_TEXT_LENGTH 96

typedef struct TuiRenderSequenceRow {
  size_t index;
  bool selected;
  char clock_text[TUI_RENDER_CLOCK_TEXT_LENGTH];
  char byte_text[TUI_RENDER_BYTE_TEXT_LENGTH];
  char description[TUI_RENDER_DESCRIPTION_TEXT_LENGTH];
  MidiDescriptionCategory category;
} TuiRenderSequenceRow;

typedef int (*TuiRenderSequenceRowProvider)(void *context, size_t index,
                                            TuiRenderSequenceRow *row);

typedef struct TuiRenderState {
  const char *recordings_dir;
  const char *source_label;
  const char *destination_label;
  const char *mode_label;
  bool rx_active;
  bool tx_active;

  const TuiFileList *files;

  bool sequence_loaded;
  const char *sequence_name;
  size_t sequence_event_count;
  size_t sequence_selected;
  TuiRenderSequenceRowProvider sequence_row_provider;
  void *sequence_context;

  const TuiLogEntry *logs;
  size_t log_count;

  const char *footer;
} TuiRenderState;

void tui_render_setup_colors(void);
void tui_render(const TuiRenderState *state);

#endif
