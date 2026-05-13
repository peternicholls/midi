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
  TUI_COLOR_SYSTEM = 6,
  TUI_COLOR_WARNING = 7
} TuiColorPair;

static const char *text_or_empty(const char *text) {
  return text != NULL ? text : "";
}

static void draw_clipped_text(int row, int col, int width, const char *text,
                              int attr) {
  if (width <= 0 || text == NULL) {
    return;
  }
  attron(attr);
  mvaddnstr(row, col, text, width);
  attroff(attr);
}

static int category_attrs(MidiDescriptionCategory category) {
  if (!has_colors()) {
    return 0;
  }

  switch (category) {
  case MIDI_DESCRIPTION_NOTE_ON:
    return COLOR_PAIR(TUI_COLOR_NOTE_ON);
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
    return COLOR_PAIR(TUI_COLOR_SYSTEM);
  case MIDI_DESCRIPTION_UNSUPPORTED:
  case MIDI_DESCRIPTION_INCOMPLETE:
    return COLOR_PAIR(TUI_COLOR_WARNING);
  }

  return 0;
}

static void draw_header(const TuiRenderState *state, int cols) {
  char transport_label[128];

  snprintf(transport_label, sizeof(transport_label), "%s  RX%s TX%s",
           text_or_empty(state->mode_label), state->rx_active ? "." : " ",
           state->tx_active ? "." : " ");

  attron(A_BOLD);
  mvaddnstr(0, 2, "MIDI Capture TUI", cols - 4);
  mvaddnstr(0, cols - (int)strlen(transport_label) - 2, transport_label,
            (int)strlen(transport_label));
  attroff(A_BOLD);
  mvaddnstr(1, 2, "Recordings: ", 12);
  mvaddnstr(1, 14, text_or_empty(state->recordings_dir), cols - 16);
  mvaddnstr(2, 2, text_or_empty(state->source_label), cols / 2 - 3);
  mvaddnstr(2, cols / 2, text_or_empty(state->destination_label),
            cols - cols / 2 - 2);
  mvaddnstr(3, 2,
            "r record  p play  space pause/resume  s stop  o output dir  "
            "arrows navigate  q quit",
            cols - 4);
  mvhline(4, 0, ACS_HLINE, cols);
}

static void draw_files_panel(const TuiRenderState *state, int top, int left,
                             int width, int height) {
  const TuiFileList *files = state->files;
  int visible = height - 2;
  size_t scroll = 0;
  size_t count = files != NULL ? files->count : 0;
  size_t selected = files != NULL ? files->selected : 0;

  mvaddnstr(top, left + 1, "Files", width - 2);
  if (visible <= 0) {
    return;
  }
  if (selected >= (size_t)visible) {
    scroll = selected - (size_t)visible + 1;
  }

  for (int row = 0; row < visible; ++row) {
    int screen_row = top + 1 + row;
    size_t index = scroll + (size_t)row;

    move(screen_row, left + 1);
    clrtoeol();
    if (files == NULL || index >= count) {
      continue;
    }

    if (index == selected) {
      attron(A_REVERSE);
    }
    mvaddnstr(screen_row, left + 1, files->items[index].name, width - 2);
    if (index == selected) {
      attroff(A_REVERSE);
    }
  }

  if (count == 0) {
    mvaddnstr(top + 1, left + 1, "No .mid files in destination", width - 2);
  }
}

static void draw_events_panel(const TuiRenderState *state, int top, int left,
                              int width, int height) {
  int visible = height - 3;
  size_t scroll = 0;

  mvaddnstr(top, left + 1, "Sequence", width - 2);
  if (!state->sequence_loaded) {
    mvaddnstr(top + 1, left + 1, "Select a MIDI file to inspect", width - 2);
    return;
  }

  mvprintw(top + 1, left + 1, "%s  %zu event(s)",
           text_or_empty(state->sequence_name), state->sequence_event_count);
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
    int screen_row = top + 2 + row;
    char line[192];
    int attrs;

    move(screen_row, left + 1);
    clrtoeol();
    if (index >= state->sequence_event_count ||
        !state->sequence_row_provider(state->sequence_context, index,
                                      &event_row)) {
      continue;
    }

    snprintf(line, sizeof(line), "%c %4zu  %s  %-12s  %s",
             event_row.selected ? '>' : ' ', event_row.index + 1,
             event_row.clock_text, event_row.byte_text, event_row.description);

    attrs = category_attrs(event_row.category);
    if (event_row.selected) {
      attrs |= A_REVERSE;
    }
    attron(attrs);
    mvaddnstr(screen_row, left + 1, line, width - 2);
    attroff(attrs);
  }
}

static void draw_log_panel(const TuiRenderState *state, int top, int left,
                           int width, int height) {
  int visible = height - 2;
  size_t start = 0;

  mvaddnstr(top, left + 1, "Live Stream", width - 2);
  if (visible <= 0) {
    return;
  }
  if (state->log_count > (size_t)visible) {
    start = state->log_count - (size_t)visible;
  }

  for (int row = 0; row < visible; ++row) {
    int screen_row = top + 1 + row;
    size_t index = start + (size_t)row;

    move(screen_row, left + 1);
    clrtoeol();
    if (state->logs == NULL || index >= state->log_count) {
      continue;
    }
    mvaddnstr(screen_row, left + 1, state->logs[index].line, width - 2);
  }
}

static void draw_footer(const TuiRenderState *state, int row, int cols) {
  mvhline(row - 1, 0, ACS_HLINE, cols);
  draw_clipped_text(row, 2, cols - 4, text_or_empty(state->footer), A_BOLD);
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
  init_pair(TUI_COLOR_SYSTEM, COLOR_WHITE, -1);
  init_pair(TUI_COLOR_WARNING, COLOR_RED, -1);
}

void tui_render(const TuiRenderState *state) {
  int rows;
  int cols;
  int top;
  int file_width;
  int event_left;
  int content_height;
  int log_top;
  int log_height = 8;

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

  draw_header(state, cols);
  top = 5;
  content_height = rows - top - log_height - 2;
  file_width = cols / 3;
  if (file_width < 28) {
    file_width = 28;
  }
  event_left = file_width + 1;
  log_top = top + content_height + 1;

  mvvline(top, file_width, ACS_VLINE, content_height);
  draw_files_panel(state, top, 0, file_width, content_height);
  draw_events_panel(state, top, event_left, cols - event_left, content_height);
  draw_log_panel(state, log_top, 0, cols, log_height);
  draw_footer(state, rows - 1, cols);
  refresh();
}
