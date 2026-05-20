#include "tui_render.h"

#include <curses.h>

#include <stdio.h>
#include <string.h>

typedef enum TuiColorPair {
  TUI_COLOR_NOTE_ON = 1,
  TUI_COLOR_NOTE_OFF = 2,
  TUI_COLOR_CONTROL = 3,
  TUI_COLOR_PROGRAM = 4,
  TUI_COLOR_BEND = 5,
  TUI_COLOR_WARNING = 6,
  TUI_COLOR_RECORDING = 7,
  TUI_COLOR_PLAYBACK = 8
} TuiColorPair;

typedef struct TuiRect {
  int top;
  int left;
  int width;
  int height;
} TuiRect;

typedef struct TuiLayout {
  TuiRect status_rail;
  TuiRect command_strip;
  TuiRect file_pane;
  TuiRect work_pane;
  TuiRect footer;
  TuiRect overlay;
  bool compact;
} TuiLayout;

static const char *text_or_empty(const char *text) {
  return text != NULL ? text : "";
}

static int clamp_int(int value, int min_value, int max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

static TuiLayout calculate_layout(int rows, int cols) {
  TuiLayout layout;
  int content_top = 3;
  int content_bottom = rows - 3;
  int file_width;
  int overlay_width;
  int overlay_height;

  memset(&layout, 0, sizeof(layout));
  layout.compact = rows < 24;
  layout.status_rail = (TuiRect){0, 0, cols, 1};
  layout.command_strip = (TuiRect){1, 0, cols, 1};
  layout.footer = (TuiRect){rows - 1, 0, cols, 1};

  /*
   * Preferred geometry: 90x24 keeps a 28-column browser and a 57-column work
   * pane; 120x36 grows the browser to 30 columns; wide terminals cap the
   * browser at 36 so the work pane receives all additional inspection width.
   * Compact 90x20-90x23 keeps the same horizontal contract and spends the
   * reduced height entirely on the two persistent panes.
   */
  file_width = clamp_int(cols / 4, 28, 36);
  if (content_bottom < content_top) {
    content_bottom = content_top;
  }
  layout.file_pane =
      (TuiRect){content_top, 0, file_width, content_bottom - content_top + 1};
  layout.work_pane =
      (TuiRect){content_top, file_width + 1, cols - file_width - 1,
                content_bottom - content_top + 1};

  overlay_width = clamp_int(cols - 10, 52, 76);
  overlay_height = 10;
  layout.overlay =
      (TuiRect){(rows - overlay_height) / 2, (cols - overlay_width) / 2,
                overlay_width, overlay_height};
  return layout;
}

static void draw_clipped_text(int row, int col, int width, const char *text,
                              int attr) {
  if (width <= 0 || text == NULL) {
    return;
  }
  if (attr != 0) {
    attron(attr);
  }
  mvaddnstr(row, col, text, width);
  if (attr != 0) {
    attroff(attr);
  }
}

static void draw_cell(int row, int *col, int width, const char *text,
                      int attr) {
  if (width <= 0 || col == NULL) {
    return;
  }
  mvhline(row, *col, ' ', width);
  draw_clipped_text(row, *col, width, text_or_empty(text), attr);
  *col += width;
}

static int category_attrs(MidiDescriptionCategory category) {
  if (!has_colors()) {
    return 0;
  }

  switch (category) {
  case MIDI_DESCRIPTION_NOTE_ON:
    return COLOR_PAIR(TUI_COLOR_NOTE_ON) | A_BOLD;
  case MIDI_DESCRIPTION_NOTE_OFF:
    return COLOR_PAIR(TUI_COLOR_NOTE_OFF);
  case MIDI_DESCRIPTION_CONTROL_CHANGE:
    return COLOR_PAIR(TUI_COLOR_CONTROL);
  case MIDI_DESCRIPTION_PROGRAM_CHANGE:
  case MIDI_DESCRIPTION_CHANNEL_PRESSURE:
  case MIDI_DESCRIPTION_POLY_PRESSURE:
    return COLOR_PAIR(TUI_COLOR_PROGRAM);
  case MIDI_DESCRIPTION_PITCH_BEND:
    return COLOR_PAIR(TUI_COLOR_BEND);
  case MIDI_DESCRIPTION_SYSEX:
    return A_DIM;
  case MIDI_DESCRIPTION_UNSUPPORTED:
  case MIDI_DESCRIPTION_INCOMPLETE:
    return COLOR_PAIR(TUI_COLOR_WARNING) | A_BOLD;
  }

  return 0;
}

static const char *work_pane_mode_text(TuiRenderWorkPaneMode mode) {
  switch (mode) {
  case TUI_RENDER_WORK_PANE_SEQUENCE:
    return "SEQUENCE";
  case TUI_RENDER_WORK_PANE_LIVE_PLAYER:
    return "LIVE PLAYER";
  case TUI_RENDER_WORK_PANE_LIVE_DIAGNOSTIC:
    return "LIVE DIAGNOSTIC";
  }
  return "SEQUENCE";
}

static int mode_label_attrs(TuiRenderWorkPaneMode mode) {
  if (!has_colors()) {
    return A_BOLD;
  }
  switch (mode) {
  case TUI_RENDER_WORK_PANE_LIVE_PLAYER:
    return COLOR_PAIR(TUI_COLOR_NOTE_ON) | A_BOLD;
  case TUI_RENDER_WORK_PANE_LIVE_DIAGNOSTIC:
    return COLOR_PAIR(TUI_COLOR_CONTROL) | A_BOLD;
  case TUI_RENDER_WORK_PANE_SEQUENCE:
  default:
    return A_BOLD;
  }
}

static void draw_status_rail(const TuiRenderState *state,
                             const TuiLayout *layout) {
  const char *brand = "MIDI Capture";
  int row = layout->status_rail.top;
  int col = 1;
  int width = layout->status_rail.width;
  const char *rx_text = state->rx_active ? "RX*" : "RX.";
  const char *tx_text = state->tx_active ? "TX*" : "TX.";
  int rx_attrs = (has_colors() && state->rx_active)
                     ? COLOR_PAIR(TUI_COLOR_NOTE_ON) | A_BOLD
                     : A_DIM;
  int tx_attrs = (has_colors() && state->tx_active)
                     ? COLOR_PAIR(TUI_COLOR_PLAYBACK) | A_BOLD
                     : A_DIM;
  const char *source = text_or_empty(state->source_label);
  const char *destination = text_or_empty(state->destination_label);
  const char *version = text_or_empty(state->app_version);
  int version_width = (int)strlen(version);
  int right_limit;
  int mode_width = layout->compact ? 14 : 18;
  int src_width;
  int dst_width;

  mvhline(row, 0, ' ', width);

  /* version pinned to right edge when wide enough */
  if (version_width > 0 && width > version_width + 24) {
    draw_clipped_text(row, width - version_width - 1, version_width, version,
                      A_DIM);
    right_limit = width - version_width - 3;
  } else {
    right_limit = width - 1;
  }

  /* brand */
  draw_clipped_text(row, col, (int)strlen(brand), brand, A_BOLD);
  col += (int)strlen(brand) + 1;

  /* work-pane mode (colored by mode type); overlay with transport when active */
  {
    const char *mode_text;
    int m_attr;
    if (state->record_active) {
      mode_text = "RECORDING";
      m_attr = has_colors() ? (COLOR_PAIR(TUI_COLOR_RECORDING) | A_BOLD) : A_BOLD;
    } else if (state->playback_active) {
      mode_text = "PLAYING";
      m_attr = has_colors() ? (COLOR_PAIR(TUI_COLOR_PLAYBACK) | A_BOLD) : A_BOLD;
    } else {
      mode_text = work_pane_mode_text(state->work_pane_mode);
      m_attr = mode_label_attrs(state->work_pane_mode);
    }
    draw_clipped_text(row, col, mode_width, mode_text, m_attr);
  }
  col += mode_width;

  /* RX / TX */
  draw_clipped_text(row, col, 3, rx_text, rx_attrs);
  col += 4;
  draw_clipped_text(row, col, 3, tx_text, tx_attrs);
  col += 4;

  /* endpoints: split remaining space between SRC and DST */
  if (col < right_limit) {
    int remaining = right_limit - col;
    if (destination[0] != '\0') {
      src_width = remaining / 2 - 1;
      dst_width = remaining - src_width - 1;
    } else {
      src_width = remaining;
      dst_width = 0;
    }
    if (src_width > 0) {
      draw_clipped_text(row, col, src_width, source, 0);
      col += src_width + 1;
    }
    if (dst_width > 0) {
      draw_clipped_text(row, col, dst_width, destination, 0);
    }
  }
}

static void draw_command_strip(const TuiRenderState *state,
                               const TuiLayout *layout) {
  const char *command_text;
  int row = layout->command_strip.top;
  bool compact = layout->compact;

  if (state->overlay == TUI_RENDER_OVERLAY_SETTINGS) {
    command_text = compact ? "settings: arrows  enter apply  esc close  q quit"
                           : "settings: arrows choose/change  enter apply  "
                             ",/esc close  q quit";
  } else if (state->overlay == TUI_RENDER_OVERLAY_DIRECTORY) {
    command_text = compact
                       ? "directory: enter path  esc close  q quit"
                       : "directory: enter/o manual path  esc close  q quit";
  } else if (state->work_pane_mode == TUI_RENDER_WORK_PANE_SEQUENCE) {
    command_text =
        compact
            ? "tab pane  v mode  enter load  space play  R rec  , settings  q"
            : "tab pane  v mode  arrows move  enter load  space "
              "play/pause  p play from here  u unload  a append  r rename "
              " R record  , settings  d dir  q quit";
  } else if (state->work_pane_mode == TUI_RENDER_WORK_PANE_LIVE_PLAYER) {
    command_text =
        compact ? "tab pane  v mode  space play  R rec  , settings  q"
                : "tab pane  v mode  space play/pause  s stop  R record  m "
                  "metronome  , settings  d dir  q quit";
  } else {
    command_text =
        compact ? "tab pane  v mode  space play  R rec  , settings  q"
                : "tab pane  v mode  live diagnostic  space play/pause  s "
                  "stop  R record  , settings  d dir  q quit";
  }

  mvhline(row, 0, ' ', layout->command_strip.width);
  draw_clipped_text(row, 1, layout->command_strip.width - 2, command_text,
                    A_DIM);
  mvhline(row + 1, 0, ACS_HLINE, layout->command_strip.width);
}

static void draw_files_panel(const TuiRenderState *state, const TuiRect *rect) {
  const TuiFileList *files = state->files;
  int visible = rect->height - 2;
  size_t scroll = 0;
  size_t count = files != NULL ? files->count : 0;
  size_t selected = files != NULL ? files->selected : 0;
  int title_attr = state->focus == TUI_RENDER_FOCUS_FILES ? A_BOLD : 0;

  draw_clipped_text(rect->top, rect->left + 1, rect->width - 2, "Files",
                    title_attr);
  if (visible <= 0) {
    return;
  }
  if (selected >= (size_t)visible) {
    scroll = selected - (size_t)visible + 1;
  }

  for (int row = 0; row < visible; ++row) {
    int screen_row = rect->top + 1 + row;
    size_t index = scroll + (size_t)row;
    char line[NAME_MAX + 4];
    bool loaded = false;

    move(screen_row, rect->left + 1);
    clrtoeol();
    if (files == NULL || index >= count) {
      continue;
    }

    loaded = state->sequence_loaded && state->sequence_name != NULL &&
             strcmp(files->items[index].name, state->sequence_name) == 0;
    snprintf(line, sizeof(line), "%c %s", loaded ? '*' : ' ',
             files->items[index].name);

    if (index == selected) {
      attron(A_REVERSE | A_BOLD);
      mvhline(screen_row, rect->left + 1, ' ', rect->width - 2);
      mvaddnstr(screen_row, rect->left + 1, line, rect->width - 2);
      attroff(A_REVERSE | A_BOLD);
    } else if (loaded) {
      attron(A_BOLD);
      mvaddnstr(screen_row, rect->left + 1, line, rect->width - 2);
      attroff(A_BOLD);
    } else {
      mvaddnstr(screen_row, rect->left + 1, line, rect->width - 2);
    }
  }

  if (count == 0) {
    mvaddnstr(rect->top + 1, rect->left + 1, "No .mid files", rect->width - 2);
  }
}

/*
 * Sequence column widths. Two sets: compact for narrow work panes (< 80 inner
 * cols) and default for wider panes. Description fills the remaining width.
 * Order: marker, #/, time, ch, event, target, value, raw, description.
 * The V3 mockup (phase-08-mockups-v3/styles.css `.seq`) targets roughly
 * 2 / 8 / 10 / 4 / 14 / 12 / 12-22 / 12 / remaining.
 */
typedef struct TuiSequenceWidths {
  int marker;
  int number;
  int time;
  int channel;
  int event;
  int target;
  int value;
  int raw;
} TuiSequenceWidths;

static TuiSequenceWidths sequence_widths_for(int inner_width) {
  TuiSequenceWidths w;
  if (inner_width < 80) {
    w.marker = 2;
    w.number = 8;
    w.time = 8;
    w.channel = 4;
    w.event = 11;
    w.target = 9;
    w.value = 8;
    w.raw = 11;
  } else {
    w.marker = 2;
    w.number = 8;
    w.time = 9;
    w.channel = 4;
    w.event = 14;
    w.target = 12;
    w.value = 12;
    w.raw = 12;
  }
  return w;
}

static int sequence_desc_width(const TuiSequenceWidths *w, int inner_width) {
  int used = w->marker + w->number + w->time + w->channel + w->event +
             w->target + w->value + w->raw;
  int desc = inner_width - used;
  return desc < 0 ? 0 : desc;
}

static void draw_sequence_header(int row, int left, int width) {
  int col = left;
  TuiSequenceWidths w = sequence_widths_for(width);
  int desc_width = sequence_desc_width(&w, width);

  draw_cell(row, &col, w.marker, ">", A_DIM);
  draw_cell(row, &col, w.number, "#/", A_DIM);
  draw_cell(row, &col, w.time, "time", A_DIM);
  draw_cell(row, &col, w.channel, "ch", A_DIM);
  draw_cell(row, &col, w.event, "event", A_DIM);
  draw_cell(row, &col, w.target, "target", A_DIM);
  draw_cell(row, &col, w.value, "value", A_DIM);
  draw_cell(row, &col, w.raw, "raw", A_DIM);
  draw_cell(row, &col, desc_width, "description", A_DIM);
}

static void draw_sequence_row(const TuiRenderSequenceRow *event_row,
                              size_t total_events, int row, int left,
                              int width) {
  int col = left;
  int attrs = category_attrs(event_row->category);
  int selected_attrs = event_row->selected ? (attrs | A_BOLD | A_REVERSE) : attrs;
  int marker_attr = event_row->selected ? (A_BOLD | A_REVERSE) : A_DIM;
  TuiSequenceWidths w = sequence_widths_for(width);
  int desc_width = sequence_desc_width(&w, width);
  char marker[2] = {event_row->selected ? '>' : ' ', '\0'};
  char number[24];

  snprintf(number, sizeof(number), "%zu/%zu", event_row->index + 1,
           total_events);
  draw_cell(row, &col, w.marker, marker, marker_attr);
  draw_cell(row, &col, w.number, number, selected_attrs);
  draw_cell(row, &col, w.time, event_row->clock_text, attrs);
  draw_cell(row, &col, w.channel, event_row->channel, attrs);
  draw_cell(row, &col, w.event, event_row->event, attrs);
  draw_cell(row, &col, w.target, event_row->target, attrs);
  draw_cell(row, &col, w.value, event_row->value, attrs);
  draw_cell(row, &col, w.raw, event_row->byte_text, A_DIM);
  draw_cell(row, &col, desc_width, event_row->description, A_DIM);
}

/* Draw a split title: left-aligned label, right-aligned detail on same row. */
static void draw_split_title(int row, int left, int width,
                             const char *left_text, int left_attr,
                             const char *right_text, int right_attr) {
  int right_len = right_text ? (int)strlen(right_text) : 0;
  int left_width = right_len > 0 ? width - right_len - 1 : width;

  mvhline(row, left, ' ', width);
  if (left_width > 0) {
    draw_clipped_text(row, left, left_width, text_or_empty(left_text),
                      left_attr);
  }
  if (right_len > 0 && right_len < width) {
    draw_clipped_text(row, left + width - right_len, right_len,
                      text_or_empty(right_text), right_attr);
  }
}

static void draw_sequence_panel(const TuiRenderState *state,
                                const TuiRect *rect) {
  int inner_left = rect->left + 1;
  int inner_width = rect->width - 2;
  int visible = rect->height - 3;
  size_t scroll = 0;
  int title_attr = state->focus == TUI_RENDER_FOCUS_WORK_PANE ? A_BOLD : 0;
  int mode_attr = mode_label_attrs(state->work_pane_mode) | title_attr;
  char left_title[128];
  char right_title[32];

  if (!state->sequence_loaded) {
    snprintf(left_title, sizeof(left_title), "%s",
             work_pane_mode_text(state->work_pane_mode));
    draw_split_title(rect->top, inner_left, inner_width, left_title, mode_attr,
                     NULL, 0);
    mvaddnstr(rect->top + 2, inner_left, "Select a recording and press enter",
              inner_width);
    return;
  }

  snprintf(left_title, sizeof(left_title), "%s \xe2\x80\x93 %s",
           work_pane_mode_text(state->work_pane_mode),
           text_or_empty(state->sequence_name));
  snprintf(right_title, sizeof(right_title), "%zu events",
           state->sequence_event_count);
  draw_split_title(rect->top, inner_left, inner_width, left_title, mode_attr,
                   right_title, A_DIM);
  draw_sequence_header(rect->top + 1, inner_left, inner_width);
  if (visible <= 0 || state->sequence_event_count == 0 ||
      state->sequence_row_provider == NULL) {
    return;
  }

  if (state->sequence_selected >= (size_t)visible) {
    scroll = state->sequence_selected - (size_t)visible + 1;
  }

  for (int row = 0; row < visible; ++row) {
    TuiRenderSequenceRow event_row;
    size_t index = scroll + (size_t)row;
    int screen_row = rect->top + 2 + row;

    move(screen_row, inner_left);
    clrtoeol();
    if (index >= state->sequence_event_count ||
        !state->sequence_row_provider(state->sequence_context, index,
                                      &event_row)) {
      continue;
    }
    draw_sequence_row(&event_row, state->sequence_event_count, screen_row,
                      inner_left, inner_width);
  }
}


static void draw_live_diagnostic_header(int row, int left, int width) {
  int col = left;
  int desc_width = width - 55;

  if (desc_width < 0) {
    desc_width = 0;
  }
  draw_cell(row, &col, 8, "time", A_DIM);
  draw_cell(row, &col, 4, "dir", A_DIM);
  draw_cell(row, &col, 4, "ch", A_DIM);
  draw_cell(row, &col, 10, "event", A_DIM);
  draw_cell(row, &col, 8, "target", A_DIM);
  draw_cell(row, &col, 7, "value", A_DIM);
  draw_cell(row, &col, 14, "raw", A_DIM);
  draw_cell(row, &col, desc_width, "description", A_DIM);
}

static void draw_live_diagnostic_row(const TuiLogEntry *entry, int row,
                                     int left, int width) {
  int col = left;
  int desc_width = width - 55;
  int attrs;

  if (desc_width < 0) {
    desc_width = 0;
  }
  if (!entry->has_midi_fields) {
    draw_cell(row, &col, 8, "-", 0);
    draw_cell(row, &col, 4, "LOG", A_DIM);
    draw_cell(row, &col, 4, "-", 0);
    draw_cell(row, &col, 10, "message", 0);
    draw_cell(row, &col, 8, "-", 0);
    draw_cell(row, &col, 7, "-", 0);
    draw_cell(row, &col, 14, "", A_DIM);
    draw_cell(row, &col, desc_width, entry->line, 0);
    return;
  }

  attrs = category_attrs(entry->midi.category);
  draw_cell(row, &col, 8, entry->midi.time, A_DIM);
  draw_cell(row, &col, 4, entry->midi.direction, attrs);
  draw_cell(row, &col, 4, entry->midi.channel, attrs);
  draw_cell(row, &col, 10, entry->midi.event, attrs);
  draw_cell(row, &col, 8, entry->midi.target, attrs);
  draw_cell(row, &col, 7, entry->midi.value, attrs);
  draw_cell(row, &col, 14, entry->midi.bytes, A_DIM);
  draw_cell(row, &col, desc_width, entry->midi.description, A_DIM);
}

static void draw_live_diagnostic_panel(const TuiRenderState *state,
                                       const TuiRect *rect) {
  int inner_left = rect->left + 1;
  int inner_width = rect->width - 2;
  int visible = rect->height - 3;
  size_t start = 0;
  int title_attr = state->focus == TUI_RENDER_FOCUS_WORK_PANE ? A_BOLD : 0;
  int mode_attr = mode_label_attrs(state->work_pane_mode) | title_attr;
  char right_title[32];

  snprintf(right_title, sizeof(right_title), "last %zu msgs",
           state->log_count);
  draw_split_title(rect->top, inner_left, inner_width,
                   work_pane_mode_text(state->work_pane_mode), mode_attr,
                   right_title, A_DIM);
  draw_live_diagnostic_header(rect->top + 1, inner_left, inner_width);
  if (visible <= 0) {
    return;
  }
  if (state->log_count > (size_t)visible) {
    start = state->log_count - (size_t)visible;
  }

  for (int row = 0; row < visible; ++row) {
    int screen_row = rect->top + 2 + row;
    size_t index = start + (size_t)row;

    move(screen_row, inner_left);
    clrtoeol();
    if (state->logs == NULL || index >= state->log_count) {
      continue;
    }
    draw_live_diagnostic_row(&state->logs[index], screen_row, inner_left,
                             inner_width);
  }
}

static void draw_live_player_header(int row, int left, int width) {
  int col = left;
  int spare_width = width - 59;

  if (spare_width < 0) {
    spare_width = 0;
  }
  draw_cell(row, &col, 4, "#/", A_DIM);
  draw_cell(row, &col, 8, "note", A_DIM);
  draw_cell(row, &col, 9, "state", A_DIM);
  draw_cell(row, &col, 9, "velocity", A_DIM);
  draw_cell(row, &col, 10, "pressure", A_DIM);
  draw_cell(row, &col, 14, "bend/mod", A_DIM);
  draw_cell(row, &col, 5 + spare_width, "age", A_DIM);
}

static void draw_live_controls_header(int row, int left, int width) {
  int col = left;
  int last_width = width - 8 - 12 - 12 - 12;

  if (last_width < 6) {
    last_width = 6;
  }
  draw_cell(row, &col, 8, "scope", A_DIM);
  draw_cell(row, &col, 12, "mod", A_DIM);
  draw_cell(row, &col, 12, "pressure", A_DIM);
  draw_cell(row, &col, 12, "pitch", A_DIM);
  draw_cell(row, &col, last_width, "last", A_DIM);
}

static void draw_live_controls_row(const TuiRenderLiveControlRow *controls,
                                   int row, int left, int width) {
  int col = left;
  int last_width = width - 8 - 12 - 12 - 12;
  int attrs = controls->active ? 0 : A_DIM;

  if (last_width < 6) {
    last_width = 6;
  }
  draw_cell(row, &col, 8, controls->scope, attrs);
  draw_cell(row, &col, 12, controls->modulation, attrs);
  draw_cell(row, &col, 12, controls->pressure, attrs);
  draw_cell(row, &col, 12, controls->pitch, attrs);
  draw_cell(row, &col, last_width, controls->last_rx, attrs);
}

static void draw_live_player_panel(const TuiRenderState *state,
                                   const TuiRect *rect) {
  int inner_left = rect->left + 1;
  int inner_width = rect->width - 2;
  int header_offset = 5;
  int visible = rect->height - header_offset;
  int title_attr = state->focus == TUI_RENDER_FOCUS_WORK_PANE ? A_BOLD : 0;
  int mode_attr = mode_label_attrs(state->work_pane_mode) | title_attr;
  size_t active_count = 0;
  size_t fading_count = 0;
  char right_title[48];

  for (size_t i = 0; i < state->live_note_count; ++i) {
    if (state->live_notes[i].active) {
      active_count++;
    } else {
      fading_count++;
    }
  }
  if (active_count > 0 || fading_count > 0) {
    snprintf(right_title, sizeof(right_title), "%zu on  %zu fading",
             active_count, fading_count);
  } else {
    snprintf(right_title, sizeof(right_title), "no active notes");
  }

  draw_split_title(rect->top, inner_left, inner_width,
                   work_pane_mode_text(state->work_pane_mode), mode_attr,
                   right_title, A_DIM);
  draw_live_controls_header(rect->top + 1, inner_left, inner_width);
  draw_live_controls_row(&state->live_controls, rect->top + 2, inner_left,
                         inner_width);
  draw_live_player_header(rect->top + 4, inner_left, inner_width);
  if (visible <= 0) {
    return;
  }

  for (int row = 0; row < visible; ++row) {
    int screen_row = rect->top + header_offset + row;
    int col = inner_left;
    const TuiRenderLiveNoteRow *note_row;
    int attrs;

    move(screen_row, inner_left);
    clrtoeol();
    if (state->live_notes == NULL || (size_t)row >= state->live_note_count) {
      continue;
    }

    note_row = &state->live_notes[row];
    attrs = note_row->active ? category_attrs(note_row->category) : A_DIM;
    draw_cell(screen_row, &col, 4, note_row->number, attrs);
    draw_cell(screen_row, &col, 8, note_row->note, attrs);
    draw_cell(screen_row, &col, 9, note_row->state, attrs);
    draw_cell(screen_row, &col, 9, note_row->velocity, attrs);
    draw_cell(screen_row, &col, 10, note_row->pressure, attrs);
    draw_cell(screen_row, &col, 14, note_row->bend_mod, attrs);
    draw_cell(screen_row, &col, inner_left + inner_width - col, note_row->age,
              attrs);
  }

  if (state->live_note_count == 0) {
    mvaddnstr(rect->top + header_offset, inner_left, "No recent note activity",
              inner_width);
  }
}

static void draw_work_panel(const TuiRenderState *state, const TuiRect *rect) {
  switch (state->work_pane_mode) {
  case TUI_RENDER_WORK_PANE_SEQUENCE:
    draw_sequence_panel(state, rect);
    break;
  case TUI_RENDER_WORK_PANE_LIVE_PLAYER:
    draw_live_player_panel(state, rect);
    break;
  case TUI_RENDER_WORK_PANE_LIVE_DIAGNOSTIC:
    draw_live_diagnostic_panel(state, rect);
    break;
  }
}

static void draw_footer(const TuiRenderState *state, const TuiLayout *layout) {
  mvhline(layout->footer.top - 1, 0, ACS_HLINE, layout->footer.width);
  draw_clipped_text(layout->footer.top, 2, layout->footer.width - 4,
                    text_or_empty(state->footer), A_BOLD);
}

static void draw_box_rect(const TuiRect *rect) {
  mvhline(rect->top, rect->left, ACS_HLINE, rect->width);
  mvhline(rect->top + rect->height - 1, rect->left, ACS_HLINE, rect->width);
  mvvline(rect->top, rect->left, ACS_VLINE, rect->height);
  mvvline(rect->top, rect->left + rect->width - 1, ACS_VLINE, rect->height);
  mvaddch(rect->top, rect->left, ACS_ULCORNER);
  mvaddch(rect->top, rect->left + rect->width - 1, ACS_URCORNER);
  mvaddch(rect->top + rect->height - 1, rect->left, ACS_LLCORNER);
  mvaddch(rect->top + rect->height - 1, rect->left + rect->width - 1,
          ACS_LRCORNER);
}

static void draw_settings_overlay(const TuiRenderState *state,
                                  const TuiRect *rect) {
  const char *labels[] = {"Recordings directory",
                          "Middle C",
                          "Note format",
                          "Live fade",
                          "Tempo",
                          "Metronome"};
  const char *values[] = {text_or_empty(state->settings.recordings_dir),
                          text_or_empty(state->settings.middle_c),
                          text_or_empty(state->settings.note_format),
                          text_or_empty(state->settings.fade_timeout),
                          text_or_empty(state->settings.tempo),
                          text_or_empty(state->settings.metronome)};
  const char *notes[] = {"press d for browser",
                         "C3/C4/C5 naming preference",
                         "name / number",
                         "2s / 5s / 10s / never",
                         "playback speed reference",
                         "recording click, single tone"};
  int count = (int)(sizeof(labels) / sizeof(labels[0]));
  int inner_width = rect->width - 4;
  int setting_width = 22;
  int value_width = 18;
  int notes_width;

  if (inner_width < setting_width + value_width + 12) {
    /* compact: smaller setting column, drop notes if necessary */
    setting_width = inner_width >= 38 ? 20 : (inner_width / 2);
    value_width = inner_width - setting_width - 1;
    notes_width = 0;
  } else {
    notes_width = inner_width - setting_width - value_width - 2;
  }

  draw_box_rect(rect);
  draw_clipped_text(rect->top + 1, rect->left + 2, inner_width, "Settings",
                    A_BOLD);

  /* header */
  {
    int row = rect->top + 2;
    int col = rect->left + 2;
    draw_cell(row, &col, setting_width, "setting", A_BOLD);
    col += 1;
    draw_cell(row, &col, value_width, "value", A_BOLD);
    if (notes_width > 0) {
      col += 1;
      draw_cell(row, &col, notes_width, "notes", A_BOLD);
    }
  }

  for (int i = 0; i < count && i + 4 < rect->height; ++i) {
    int row = rect->top + 3 + i;
    int attr = i == state->settings.selected_index ? A_REVERSE : 0;
    int col = rect->left + 2;

    draw_cell(row, &col, setting_width, labels[i], attr);
    col += 1;
    draw_cell(row, &col, value_width, values[i], attr);
    if (notes_width > 0) {
      col += 1;
      draw_cell(row, &col, notes_width, notes[i], attr | A_DIM);
    }
  }
}

static void draw_directory_overlay(const TuiRenderState *state,
                                   const TuiRect *rect) {
  draw_box_rect(rect);
  draw_clipped_text(rect->top + 1, rect->left + 2, rect->width - 4,
                    "Directory Browser", A_BOLD);
  draw_clipped_text(rect->top + 3, rect->left + 2, rect->width - 4,
                    text_or_empty(state->directory_overlay_message), 0);
  draw_clipped_text(rect->top + 5, rect->left + 2, rect->width - 4,
                    "Phase 9 owns column navigation and apply flow.", A_DIM);
  draw_clipped_text(rect->top + 7, rect->left + 2, rect->width - 4,
                    "Press enter or o to set a manual path.", A_DIM);
}

static void draw_overlay(const TuiRenderState *state, const TuiLayout *layout) {
  if (state->overlay == TUI_RENDER_OVERLAY_SETTINGS) {
    draw_settings_overlay(state, &layout->overlay);
  } else if (state->overlay == TUI_RENDER_OVERLAY_DIRECTORY) {
    draw_directory_overlay(state, &layout->overlay);
  }
}

void tui_render_setup_colors(void) {
  if (!has_colors()) {
    return;
  }

  start_color();
  use_default_colors();
  init_pair(TUI_COLOR_NOTE_ON, COLOR_GREEN, -1);
  init_pair(TUI_COLOR_NOTE_OFF, COLOR_CYAN, -1);
  init_pair(TUI_COLOR_CONTROL, COLOR_YELLOW, -1);
  init_pair(TUI_COLOR_PROGRAM, COLOR_MAGENTA, -1);
  init_pair(TUI_COLOR_BEND, COLOR_BLUE, -1);
  init_pair(TUI_COLOR_WARNING, COLOR_RED, -1);
  init_pair(TUI_COLOR_RECORDING, COLOR_RED, -1);
  init_pair(TUI_COLOR_PLAYBACK, COLOR_GREEN, -1);
}

void tui_render(const TuiRenderState *state) {
  int rows;
  int cols;
  TuiLayout layout;

  if (state == NULL) {
    return;
  }

  erase();
  getmaxyx(stdscr, rows, cols);
  if (rows < 20 || cols < 90) {
    mvaddstr(1, 2, "Resize terminal to at least 90x20 for TUI mode.");
    refresh();
    return;
  }

  layout = calculate_layout(rows, cols);
  draw_status_rail(state, &layout);
  draw_command_strip(state, &layout);
  mvvline(layout.file_pane.top, layout.file_pane.width, ACS_VLINE,
          layout.file_pane.height);
  draw_files_panel(state, &layout.file_pane);
  draw_work_panel(state, &layout.work_pane);
  draw_footer(state, &layout);
  draw_overlay(state, &layout);
  refresh();
}
