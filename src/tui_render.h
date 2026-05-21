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
#define TUI_RENDER_FIELD_TEXT_LENGTH 32
#define TUI_RENDER_LIVE_NOTE_MAX 128

typedef enum TuiRenderWorkPaneMode {
  TUI_RENDER_WORK_PANE_SEQUENCE = 0,
  TUI_RENDER_WORK_PANE_LIVE_PLAYER,
  TUI_RENDER_WORK_PANE_LIVE_DIAGNOSTIC
} TuiRenderWorkPaneMode;

typedef enum TuiRenderFocus {
  TUI_RENDER_FOCUS_FILES = 0,
  TUI_RENDER_FOCUS_WORK_PANE
} TuiRenderFocus;

typedef enum TuiRenderOverlayKind {
  TUI_RENDER_OVERLAY_NONE = 0,
  TUI_RENDER_OVERLAY_SETTINGS,
  TUI_RENDER_OVERLAY_DIRECTORY
} TuiRenderOverlayKind;

typedef struct TuiRenderSequenceRow {
  size_t index;
  bool selected;
  char clock_text[TUI_RENDER_CLOCK_TEXT_LENGTH];
  char channel[TUI_RENDER_FIELD_TEXT_LENGTH];
  char event[TUI_RENDER_FIELD_TEXT_LENGTH];
  char target[TUI_RENDER_FIELD_TEXT_LENGTH];
  char value[TUI_RENDER_FIELD_TEXT_LENGTH];
  char byte_text[TUI_RENDER_BYTE_TEXT_LENGTH];
  char description[TUI_RENDER_DESCRIPTION_TEXT_LENGTH];
  MidiDescriptionCategory category;
} TuiRenderSequenceRow;

typedef int (*TuiRenderSequenceRowProvider)(void *context, size_t index,
                                            TuiRenderSequenceRow *row);

typedef struct TuiRenderLiveNoteRow {
  char number[TUI_RENDER_FIELD_TEXT_LENGTH];
  char note[TUI_RENDER_FIELD_TEXT_LENGTH];
  char state[TUI_RENDER_FIELD_TEXT_LENGTH];
  char velocity[TUI_RENDER_FIELD_TEXT_LENGTH];
  char pressure[TUI_RENDER_FIELD_TEXT_LENGTH];
  char bend_mod[TUI_RENDER_FIELD_TEXT_LENGTH];
  char age[TUI_RENDER_FIELD_TEXT_LENGTH];
  MidiDescriptionCategory category;
  bool active;
  int raw_velocity;
  int raw_pressure;
} TuiRenderLiveNoteRow;

typedef struct TuiRenderLiveControlRow {
  char scope[TUI_RENDER_FIELD_TEXT_LENGTH];
  char modulation[TUI_RENDER_FIELD_TEXT_LENGTH];
  char pressure[TUI_RENDER_FIELD_TEXT_LENGTH];
  char pitch[TUI_RENDER_FIELD_TEXT_LENGTH];
  char last_rx[TUI_RENDER_FIELD_TEXT_LENGTH];
  bool active;
  int raw_modulation;
  int raw_pressure;
  int raw_pitch;
} TuiRenderLiveControlRow;

typedef struct TuiRenderSettingsState {
  int selected_index;
  const char *recordings_dir;
  const char *middle_c;
  const char *note_format;
  const char *fade_timeout;
  const char *tempo;
  const char *metronome;
} TuiRenderSettingsState;

typedef struct TuiRenderState {
  const char *app_version;
  const char *recordings_dir;
  const char *source_label;
  const char *destination_label;
  const char *mode_label;
  TuiRenderWorkPaneMode work_pane_mode;
  TuiRenderFocus focus;
  bool rx_active;
  bool tx_active;
  bool record_active;
  bool playback_active;
  bool append_armed;

  const TuiFileList *files;

  bool sequence_loaded;
  const char *sequence_name;
  size_t sequence_event_count;
  size_t sequence_selected;
  TuiRenderSequenceRowProvider sequence_row_provider;
  void *sequence_context;

  const TuiLogEntry *logs;
  size_t log_count;

  const TuiRenderLiveNoteRow *live_notes;
  size_t live_note_count;
  TuiRenderLiveControlRow live_controls;

  TuiRenderOverlayKind overlay;
  TuiRenderSettingsState settings;
  const char *directory_overlay_message;

  const char *footer;
} TuiRenderState;

void tui_render_setup_colors(void);
void tui_render(const TuiRenderState *state);

#endif
