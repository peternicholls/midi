#include "command_tui.h"

#include "app_support.h"
#include "midi_describe.h"
#include "midi_output.h"
#include "midi_parser.h"
#include "midi_recorder.h"
#include "midi_sequence.h"
#include "tui_files.h"
#include "tui_log.h"
#include "tui_model.h"
#include "tui_render.h"

#include <curses.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define TUI_MAX_STATUS 256
#define TUI_MAX_NAME 256
#define TUI_INPUT_TIMEOUT_MS 30
#define TUI_MIDDLE_C_DEFAULT 4
#define TUI_TEMPO_DEFAULT_BPM 120

typedef enum TuiNoteFormat {
  TUI_NOTE_FORMAT_NAME = 0,
  TUI_NOTE_FORMAT_NUMBER
} TuiNoteFormat;

typedef enum TuiFadeTimeout {
  TUI_FADE_TIMEOUT_2S = 0,
  TUI_FADE_TIMEOUT_5S,
  TUI_FADE_TIMEOUT_10S,
  TUI_FADE_TIMEOUT_NEVER
} TuiFadeTimeout;

typedef struct TuiSettings {
  int middle_c_octave;
  TuiNoteFormat note_format;
  TuiFadeTimeout fade_timeout;
  int tempo_bpm;
  bool metronome_enabled;
  int selected_index;
} TuiSettings;

typedef struct TuiLiveNoteState {
  bool seen;
  bool active;
  uint8_t note;
  uint8_t channel;
  uint8_t velocity;
  uint8_t pressure;
  uint8_t modulation;
  int bend;
  uint64_t last_seen_nanos;
} TuiLiveNoteState;

typedef struct TuiLiveChannelState {
  bool seen;
  uint8_t channel;
  uint8_t modulation;
  uint8_t pressure;
  int bend;
  uint64_t last_seen_nanos;
} TuiLiveChannelState;

typedef struct TuiApp TuiApp;

typedef struct TuiMonitorSession {
  TuiApp *app;
  MIDIClientRef client;
  MIDIPortRef input_port;
  long source_index;
  char source_name[TUI_MAX_NAME];
  uint64_t started_nanos;
  atomic_ullong last_rx_nanos;
  MidiParserState parser_state;
  bool connected;
} TuiMonitorSession;

typedef struct TuiRecordSession {
  TuiApp *app;
  MidiRecorder recorder;
  long source_index;
  char source_name[TUI_MAX_NAME];
  char output_path[PATH_MAX];
  uint64_t wall_started_nanos;
  uint64_t paused_nanos_total;
  uint64_t pause_started_nanos;
  uint64_t first_adjusted_host_time;
  bool active;
  bool paused;
  bool has_first_host_time;
  bool append_to_loaded;
  double append_offset_seconds;
  char append_path[PATH_MAX];
  char append_name[NAME_MAX + 1];
} TuiRecordSession;

typedef struct TuiPlaybackSession {
  TuiApp *app;
  MidiSequenceEventList events;
  MidiOutput output;
  long destination_index;
  char destination_name[TUI_MAX_NAME];
  char loaded_path[PATH_MAX];
  char loaded_name[NAME_MAX + 1];
  double total_seconds;
  double start_offset_seconds;
  uint64_t wall_started_nanos;
  uint64_t paused_nanos_total;
  uint64_t pause_started_nanos;
  uint64_t last_tx_nanos;
  size_t selected_event;
  size_t current_index;
  size_t start_index;
  bool loaded;
  bool active;
  bool paused;
} TuiPlaybackSession;

struct TuiApp {
  TuiFileList files;
  TuiLog *log;
  TuiMonitorSession monitor;
  TuiRecordSession record;
  TuiPlaybackSession playback;
  TuiSettings settings;
  TuiLiveNoteState live_notes[16][128];
  TuiLiveChannelState live_channels[16];
  TuiRenderWorkPaneMode work_pane_mode;
  TuiRenderFocus focus;
  TuiRenderOverlayKind overlay;
  char recordings_dir[PATH_MAX];
  char status_message[TUI_MAX_STATUS];
  time_t last_scan_time;
  bool append_armed;
  bool live_hold;
  bool diagnostic_log_paused;
  bool diagnostic_filter_midi_only;
  unsigned long metronome_next_beat;
  int channel_scope;
};

static void set_osstatus_error(TuiApp *app, const char *label, OSStatus status);
static void record_packet_bytes(TuiRecordSession *record, const uint8_t *bytes,
                                size_t byte_count, uint64_t host_time);

static void describe_endpoint(bool is_source, long index, char *buffer,
                              size_t buffer_size) {
  MIDIEndpointRef endpoint;

  if (buffer == NULL || buffer_size == 0) {
    return;
  }

  endpoint =
      is_source ? get_source_by_index(index) : get_destination_by_index(index);
  if (endpoint == 0) {
    snprintf(buffer, buffer_size, "none");
    return;
  }

  get_endpoint_name(endpoint, buffer, buffer_size);
}

static void set_status(TuiApp *app, const char *format, ...) {
  va_list args;

  if (app == NULL || format == NULL) {
    return;
  }

  va_start(args, format);
  vsnprintf(app->status_message, sizeof(app->status_message), format, args);
  va_end(args);
}

static void log_line(TuiApp *app, const char *format, ...) {
  va_list args;

  if (app == NULL || format == NULL) {
    return;
  }

  va_start(args, format);
  tui_log_vappend(app->log, format, args);
  va_end(args);
}

typedef struct TuiMidiDisplayFields {
  char channel[TUI_RENDER_FIELD_TEXT_LENGTH];
  char event[TUI_RENDER_FIELD_TEXT_LENGTH];
  char target[TUI_RENDER_FIELD_TEXT_LENGTH];
  char value[TUI_RENDER_FIELD_TEXT_LENGTH];
} TuiMidiDisplayFields;

static void format_note_label(const TuiSettings *settings, uint8_t note,
                              char *buffer, size_t buffer_size) {
  static const char *names[] = {"C",  "C#", "D",  "D#", "E",  "F",
                                "F#", "G",  "G#", "A",  "A#", "B"};
  int middle_c_octave = TUI_MIDDLE_C_DEFAULT;
  int octave;

  if (buffer == NULL || buffer_size == 0) {
    return;
  }
  if (settings != NULL) {
    if (settings->note_format == TUI_NOTE_FORMAT_NUMBER) {
      snprintf(buffer, buffer_size, "%u", note);
      return;
    }
    middle_c_octave = settings->middle_c_octave;
  }

  octave = (int)(note / 12u) + middle_c_octave - 5;
  snprintf(buffer, buffer_size, "%s%d", names[note % 12u], octave);
}

static void fill_midi_display_fields(const TuiSettings *settings,
                                     const uint8_t *bytes, size_t length,
                                     const MidiDescription *description,
                                     TuiMidiDisplayFields *fields) {
  uint8_t status;
  uint8_t data1;
  uint8_t data2;
  unsigned int channel;

  if (fields == NULL) {
    return;
  }
  memset(fields, 0, sizeof(*fields));
  snprintf(fields->channel, sizeof(fields->channel), "-");
  snprintf(fields->event, sizeof(fields->event), "message");
  snprintf(fields->target, sizeof(fields->target), "-");
  snprintf(fields->value, sizeof(fields->value), "-");

  if (bytes == NULL || length == 0) {
    return;
  }

  status = bytes[0];
  data1 = length > 1 ? bytes[1] : 0;
  data2 = length > 2 ? bytes[2] : 0;
  if (status >= 0x80 && status <= 0xEF) {
    channel = (unsigned int)(status & 0x0Fu) + 1u;
    snprintf(fields->channel, sizeof(fields->channel), "%u", channel);

    switch (status & 0xF0u) {
    case 0x80:
      snprintf(fields->event, sizeof(fields->event), "note off");
      format_note_label(settings, data1, fields->target,
                        sizeof(fields->target));
      snprintf(fields->value, sizeof(fields->value), "%u", data2);
      break;
    case 0x90:
      snprintf(fields->event, sizeof(fields->event),
               data2 == 0 ? "note off" : "note on");
      format_note_label(settings, data1, fields->target,
                        sizeof(fields->target));
      snprintf(fields->value, sizeof(fields->value), "%u", data2);
      break;
    case 0xA0:
      snprintf(fields->event, sizeof(fields->event), "poly prs");
      format_note_label(settings, data1, fields->target,
                        sizeof(fields->target));
      snprintf(fields->value, sizeof(fields->value), "%u", data2);
      break;
    case 0xB0:
      snprintf(fields->event, sizeof(fields->event), "control");
      snprintf(fields->target, sizeof(fields->target),
               data1 == 1 ? "mod" : "CC%u", data1);
      snprintf(fields->value, sizeof(fields->value), "%u", data2);
      break;
    case 0xC0:
      snprintf(fields->event, sizeof(fields->event), "program");
      snprintf(fields->target, sizeof(fields->target), "program");
      snprintf(fields->value, sizeof(fields->value), "%u", data1);
      break;
    case 0xD0:
      snprintf(fields->event, sizeof(fields->event), "ch pressure");
      snprintf(fields->target, sizeof(fields->target), "pressure");
      snprintf(fields->value, sizeof(fields->value), "%u", data1);
      break;
    case 0xE0: {
      int bend = ((int)data2 << 7) | (int)data1;
      bend -= 8192;
      snprintf(fields->event, sizeof(fields->event), "pitch bend");
      snprintf(fields->target, sizeof(fields->target), "bend");
      snprintf(fields->value, sizeof(fields->value), "%+d", bend);
      break;
    }
    default:
      break;
    }
    return;
  }

  if (description != NULL) {
    switch (description->category) {
    case MIDI_DESCRIPTION_SYSEX:
      snprintf(fields->event, sizeof(fields->event), "sysex");
      snprintf(fields->target, sizeof(fields->target), "system");
      snprintf(fields->value, sizeof(fields->value), "%zuB", length);
      break;
    case MIDI_DESCRIPTION_UNSUPPORTED:
      snprintf(fields->event, sizeof(fields->event), "unsupported");
      break;
    case MIDI_DESCRIPTION_INCOMPLETE:
      snprintf(fields->event, sizeof(fields->event), "incomplete");
      break;
    default:
      break;
    }
  }
}

static void update_live_note_state(TuiApp *app, const uint8_t *bytes,
                                   size_t length) {
  uint8_t status;
  uint8_t data1;
  uint8_t data2;
  unsigned int channel;
  TuiLiveNoteState *note_state;
  TuiLiveChannelState *channel_state;
  uint64_t now_nanos;

  if (app == NULL || bytes == NULL || length == 0) {
    return;
  }

  status = bytes[0];
  if (status < 0x80 || status > 0xEF) {
    return;
  }

  data1 = length > 1 ? bytes[1] : 0;
  data2 = length > 2 ? bytes[2] : 0;
  channel = (unsigned int)(status & 0x0Fu);
  now_nanos = host_time_now_nanos();

  channel_state = &app->live_channels[channel];
  channel_state->seen = true;
  channel_state->channel = (uint8_t)(channel + 1u);
  channel_state->last_seen_nanos = now_nanos;

  switch (status & 0xF0u) {
  case 0x80:
  case 0x90:
  case 0xA0:
    note_state = &app->live_notes[channel][data1];
    note_state->seen = true;
    note_state->note = data1;
    note_state->channel = (uint8_t)(channel + 1u);
    note_state->last_seen_nanos = now_nanos;
    if ((status & 0xF0u) == 0x80 || ((status & 0xF0u) == 0x90 && data2 == 0)) {
      note_state->active = false;
      note_state->velocity = 0;
    } else if ((status & 0xF0u) == 0x90) {
      note_state->active = true;
      note_state->velocity = data2;
    } else {
      note_state->pressure = data2;
    }
    break;
  case 0xB0:
    if (data1 == 1) {
      channel_state->modulation = data2;
      for (size_t note = 0; note < 128; ++note) {
        if (app->live_notes[channel][note].seen) {
          app->live_notes[channel][note].modulation = data2;
          app->live_notes[channel][note].last_seen_nanos = now_nanos;
        }
      }
    }
    break;
  case 0xD0:
    channel_state->pressure = data1;
    for (size_t note = 0; note < 128; ++note) {
      if (app->live_notes[channel][note].seen) {
        app->live_notes[channel][note].pressure = data1;
        app->live_notes[channel][note].last_seen_nanos = now_nanos;
      }
    }
    break;
  case 0xE0: {
    int bend = ((int)data2 << 7) | (int)data1;
    bend -= 8192;
    channel_state->bend = bend;
    for (size_t note = 0; note < 128; ++note) {
      if (app->live_notes[channel][note].seen) {
        app->live_notes[channel][note].bend = bend;
        app->live_notes[channel][note].last_seen_nanos = now_nanos;
      }
    }
    break;
  }
  default:
    break;
  }
}

static void log_midi_bytes(TuiApp *app, const char *label, double seconds,
                           const uint8_t *bytes, size_t length) {
  char clock_text[32];
  char byte_text[64];
  char line[TUI_LOG_LINE_LENGTH];
  MidiDescription description;
  TuiMidiDisplayFields display_fields;
  TuiLogMidiFields log_fields;

  tui_format_clock_time(clock_text, sizeof(clock_text), seconds);
  tui_format_midi_bytes(byte_text, sizeof(byte_text), bytes, length);
  midi_describe_bytes(bytes, length, &description);
  fill_midi_display_fields(&app->settings, bytes, length, &description,
                           &display_fields);
  if (!app->live_hold) {
    update_live_note_state(app, bytes, length);
  }

  if (app->diagnostic_log_paused) {
    return;
  }

  memset(&log_fields, 0, sizeof(log_fields));
  snprintf(log_fields.time, sizeof(log_fields.time), "%s", clock_text);
  snprintf(log_fields.direction, sizeof(log_fields.direction), "%s", label);
  snprintf(log_fields.channel, sizeof(log_fields.channel), "%s",
           display_fields.channel);
  snprintf(log_fields.event, sizeof(log_fields.event), "%s",
           display_fields.event);
  snprintf(log_fields.target, sizeof(log_fields.target), "%s",
           display_fields.target);
  snprintf(log_fields.value, sizeof(log_fields.value), "%s",
           display_fields.value);
  snprintf(log_fields.bytes, sizeof(log_fields.bytes), "%s", byte_text);
  snprintf(log_fields.description, sizeof(log_fields.description), "%s",
           description.text);
  log_fields.category = description.category;

  snprintf(line, sizeof(line), "%s %s  %s  %s", clock_text, label, byte_text,
           description.text);
  tui_log_append_midi(app->log, &log_fields, line);
}

static void dispose_monitor_input(TuiMonitorSession *monitor) {
  if (monitor == NULL) {
    return;
  }
  if (monitor->input_port != 0) {
    MIDIPortDispose(monitor->input_port);
    monitor->input_port = 0;
  }
  if (monitor->client != 0) {
    MIDIClientDispose(monitor->client);
    monitor->client = 0;
  }
  monitor->connected = false;
  monitor->source_name[0] = '\0';
}

static void log_monitored_packet_bytes(TuiApp *app, const uint8_t *bytes,
                                       size_t byte_count, uint64_t host_time) {
  size_t offset = 0;
  double elapsed_seconds = 0.0;
  uint64_t packet_nanos;

  if (app == NULL || bytes == NULL || byte_count == 0) {
    return;
  }

  packet_nanos = AudioConvertHostTimeToNanos(host_time);
  if (app->monitor.started_nanos == 0 ||
      packet_nanos < app->monitor.started_nanos) {
    app->monitor.started_nanos = packet_nanos;
  }
  atomic_store_explicit(&app->monitor.last_rx_nanos, packet_nanos,
                        memory_order_relaxed);
  elapsed_seconds =
      (double)(packet_nanos - app->monitor.started_nanos) / 1000000000.0;

  while (offset < byte_count) {
    MidiParsedMessage parsed;
    size_t used =
        midi_next_stream_message(&app->monitor.parser_state, bytes + offset,
                                 byte_count - offset, &parsed);
    if (used == 0) {
      log_line(app, "stream RX incomplete packet fragment");
      return;
    }

    switch (parsed.kind) {
    case MIDI_PARSED_CHANNEL:
      log_midi_bytes(app, "RX", elapsed_seconds, parsed.channel_bytes,
                     parsed.channel_length);
      break;
    case MIDI_PARSED_SYSEX:
      log_midi_bytes(app, "RX", elapsed_seconds, bytes + offset, parsed.length);
      break;
    case MIDI_PARSED_UNSUPPORTED:
      log_midi_bytes(app, "SKIP", elapsed_seconds, bytes + offset,
                     parsed.length);
      break;
    case MIDI_PARSED_INCOMPLETE:
      log_line(app, "stream RX incomplete message");
      return;
    }

    offset += used;
  }
}

static void tui_midi_read_proc(const MIDIPacketList *packet_list,
                               void *read_proc_ref_con,
                               void *src_conn_ref_con) {
  const MIDIPacket *packet;
  TuiApp *app = (TuiApp *)read_proc_ref_con;

  (void)src_conn_ref_con;

  if (app == NULL || packet_list == NULL) {
    return;
  }

  packet = &packet_list->packet[0];
  for (UInt32 i = 0; i < packet_list->numPackets; ++i) {
    uint64_t packet_time =
        packet->timeStamp != 0 ? packet->timeStamp : AudioGetCurrentHostTime();
    log_monitored_packet_bytes(app, packet->data, packet->length, packet_time);
    if (app->record.active) {
      record_packet_bytes(&app->record, packet->data, packet->length,
                          packet_time);
    }
    packet = MIDIPacketNext(packet);
  }
}

static int open_monitor_input(TuiApp *app, MIDIEndpointRef source) {
  OSStatus status;

  if (app == NULL || source == 0) {
    return 0;
  }
  if (app->monitor.connected) {
    return 1;
  }

  status = MIDIClientCreate(CFSTR("midi-capture-tui-monitor"), NULL, NULL,
                            &app->monitor.client);
  if (status != noErr) {
    set_osstatus_error(app, "MIDIClientCreate", status);
    return 0;
  }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  status = MIDIInputPortCreate(
      app->monitor.client, CFSTR("midi-capture-tui-monitor-input"),
      tui_midi_read_proc, app, &app->monitor.input_port);
#pragma clang diagnostic pop
  if (status != noErr) {
    MIDIClientDispose(app->monitor.client);
    app->monitor.client = 0;
    set_osstatus_error(app, "MIDIInputPortCreate", status);
    return 0;
  }

  status = MIDIPortConnectSource(app->monitor.input_port, source, NULL);
  if (status != noErr) {
    MIDIPortDispose(app->monitor.input_port);
    MIDIClientDispose(app->monitor.client);
    app->monitor.input_port = 0;
    app->monitor.client = 0;
    set_osstatus_error(app, "MIDIPortConnectSource", status);
    return 0;
  }

  app->monitor.connected = true;
  app->monitor.source_index = 0;
  app->monitor.started_nanos = host_time_now_nanos();
  get_endpoint_name(source, app->monitor.source_name,
                    sizeof(app->monitor.source_name));
  log_line(app, "MON source[0] %s connected", app->monitor.source_name);
  return 1;
}

static void maybe_refresh_monitor_input(TuiApp *app) {
  MIDIEndpointRef source;

  if (app == NULL) {
    return;
  }

  source = get_source_by_index(0);
  if (source == 0) {
    if (app->monitor.connected) {
      dispose_monitor_input(&app->monitor);
      set_status(app, "Source [0] is not available");
      log_line(app, "MON source[0] unavailable");
    }
    return;
  }

  if (!app->monitor.connected) {
    open_monitor_input(app, source);
  }
}

static void set_osstatus_error(TuiApp *app, const char *label,
                               OSStatus status) {
  if (label == NULL) {
    label = "operation";
  }
  set_status(app, "%s failed: OSStatus=%d", label, (int)status);
  log_line(app, "ERR %s failed: OSStatus=%d", label, (int)status);
}

static void set_midi_result_error(TuiApp *app, MidiResult result) {
  const char *operation =
      result.operation != NULL ? result.operation : "operation";

  switch (result.code) {
  case MIDI_RESULT_OK:
    return;
  case MIDI_RESULT_INVALID_ARGUMENT:
    set_status(app, "%s failed: invalid argument", operation);
    log_line(app, "ERR %s failed: invalid argument", operation);
    return;
  case MIDI_RESULT_NO_MEMORY:
    set_status(app, "%s failed: out of memory", operation);
    log_line(app, "ERR %s failed: out of memory", operation);
    return;
  case MIDI_RESULT_OSSTATUS:
    set_osstatus_error(app, operation, result.os_status);
    return;
  }
}

static void trim_ascii_whitespace(char *text) {
  size_t start = 0;
  size_t end;

  if (text == NULL) {
    return;
  }

  while (text[start] == ' ' || text[start] == '\t' || text[start] == '\n' ||
         text[start] == '\r') {
    start += 1;
  }

  end = strlen(text);
  while (end > start && (text[end - 1] == ' ' || text[end - 1] == '\t' ||
                         text[end - 1] == '\n' || text[end - 1] == '\r')) {
    end -= 1;
  }

  if (start > 0) {
    memmove(text, text + start, end - start);
  }
  text[end - start] = '\0';
}

static int ensure_directory_exists(const char *path) {
  struct stat info;

  if (path == NULL || path[0] == '\0') {
    return 0;
  }

  if (stat(path, &info) == 0) {
    return S_ISDIR(info.st_mode) ? 1 : 0;
  }

  if (errno != ENOENT) {
    return 0;
  }

  return mkdir(path, 0755) == 0;
}

static int scan_recording_files(TuiApp *app) {
  TuiFileScanResult result;

  if (app == NULL) {
    return 0;
  }

  result = tui_file_list_scan(&app->files, app->recordings_dir);
  app->last_scan_time = time(NULL);
  if (tui_file_scan_result_is_ok(result)) {
    return 1;
  }

  switch (result.code) {
  case TUI_FILE_SCAN_INVALID_ARGUMENT:
    set_status(app, "Could not scan recordings: invalid argument");
    break;
  case TUI_FILE_SCAN_NO_MEMORY:
    set_status(app, "Out of memory while scanning %s", app->recordings_dir);
    break;
  case TUI_FILE_SCAN_OPEN_FAILED:
    set_status(app, "Could not scan %s: %s", app->recordings_dir,
               strerror(result.system_error));
    break;
  case TUI_FILE_SCAN_PATH_TOO_LONG:
    set_status(app, "Recording path is too long in %s", app->recordings_dir);
    break;
  case TUI_FILE_SCAN_OK:
    break;
  }
  return 0;
}

static void select_file_by_name(TuiApp *app, const char *name) {
  if (app == NULL || name == NULL) {
    return;
  }
  for (size_t i = 0; i < app->files.count; ++i) {
    if (strcmp(app->files.items[i].name, name) == 0) {
      app->files.selected = i;
      return;
    }
  }
}

static void playback_reset_runtime(TuiPlaybackSession *playback) {
  playback->active = false;
  playback->paused = false;
  playback->wall_started_nanos = 0;
  playback->paused_nanos_total = 0;
  playback->pause_started_nanos = 0;
  playback->start_offset_seconds = 0.0;
  playback->last_tx_nanos = 0;
  playback->start_index = 0;
  playback->current_index = playback->selected_event;
}

static void playback_dispose_transport(TuiPlaybackSession *playback) {
  midi_output_close(&playback->output);
}

static void playback_clear_loaded(TuiPlaybackSession *playback) {
  if (playback == NULL) {
    return;
  }

  playback_dispose_transport(playback);
  midi_sequence_event_list_free(&playback->events);
  playback->loaded = false;
  playback->loaded_path[0] = '\0';
  playback->loaded_name[0] = '\0';
  playback->total_seconds = 0.0;
  playback->selected_event = 0;
  playback_reset_runtime(playback);
}

static int playback_load_file(TuiApp *app, const char *path, const char *name,
                              bool force_reload) {
  MidiSequenceEventList events;
  double total_seconds = 0.0;
  MidiResult result;

  midi_sequence_event_list_init(&events);
  if (app == NULL || path == NULL || name == NULL) {
    return 0;
  }

  if (!force_reload && app->playback.loaded &&
      strcmp(app->playback.loaded_path, path) == 0) {
    return 1;
  }

  result = midi_sequence_load_events_from_file(path, &events, &total_seconds);
  if (!midi_result_is_ok(result)) {
    midi_sequence_event_list_free(&events);
    set_midi_result_error(app, result);
    return 0;
  }

  playback_clear_loaded(&app->playback);
  app->playback.events = events;
  app->playback.total_seconds = total_seconds;
  app->playback.loaded = true;
  app->playback.selected_event = 0;
  app->playback.current_index = 0;
  snprintf(app->playback.loaded_path, sizeof(app->playback.loaded_path), "%s",
           path);
  snprintf(app->playback.loaded_name, sizeof(app->playback.loaded_name), "%s",
           name);
  set_status(app, "Loaded %s with %zu event(s)", app->playback.loaded_name,
             app->playback.events.count);
  return 1;
}

static int playback_load_selected_file(TuiApp *app) {
  const TuiFileEntry *selected_file;

  if (app == NULL) {
    return 0;
  }

  selected_file = tui_file_list_selected(&app->files);
  if (selected_file == NULL) {
    set_status(app, "No recording selected");
    return 0;
  }

  return playback_load_file(app, selected_file->path, selected_file->name,
                            false);
}

static int initialize_record_state(TuiApp *app, TuiRecordSession *record) {
  memset(record, 0, sizeof(*record));
  record->app = app;
  MidiResult result = midi_recorder_init(&record->recorder);
  if (!midi_result_is_ok(result)) {
    set_midi_result_error(app, result);
    return 0;
  }

  return 1;
}

static void dispose_record_resources(TuiRecordSession *record) {
  if (record == NULL) {
    return;
  }

  midi_recorder_dispose(&record->recorder);
}

static void record_packet_bytes(TuiRecordSession *record, const uint8_t *bytes,
                                size_t byte_count, uint64_t host_time) {
  double elapsed_seconds = 0.0;
  uint64_t adjusted_host_time;

  if (record == NULL || bytes == NULL || byte_count == 0 || !record->active ||
      record->paused) {
    return;
  }

  adjusted_host_time = host_time - record->paused_nanos_total;
  if (!record->has_first_host_time) {
    record->first_adjusted_host_time = adjusted_host_time;
    record->has_first_host_time = true;
  }
  if (adjusted_host_time >= record->first_adjusted_host_time) {
    elapsed_seconds =
        (double)AudioConvertHostTimeToNanos(adjusted_host_time -
                                            record->first_adjusted_host_time) /
        1000000000.0;
  }

  MidiResult result = midi_recorder_record_packet_bytes(
      &record->recorder, bytes, byte_count, elapsed_seconds);
  if (!midi_result_is_ok(result)) {
    set_midi_result_error(record->app, result);
    g_stop_requested = 1;
  }
}

static OSStatus save_sequence_to_file(MusicSequence sequence,
                                      const char *output_path) {
  CFURLRef url = NULL;
  OSStatus status;

  if (!create_file_url(output_path, &url)) {
    return -50;
  }

  status = MusicSequenceFileCreate(sequence, url, kMusicSequenceFile_MIDIType,
                                   kMusicSequenceFileFlags_EraseFile, 480);
  CFRelease(url);
  return status;
}

static MidiResult add_sequence_event_to_track(MusicSequence sequence,
                                              MusicTrack track,
                                              const MidiSequenceEvent *event,
                                              double offset_seconds) {
  MusicTimeStamp beats = 0;
  OSStatus status;

  if (sequence == NULL || track == NULL || event == NULL ||
      event->data == NULL) {
    return midi_result_invalid_argument("add_sequence_event_to_track");
  }

  status = MusicSequenceGetBeatsForSeconds(
      sequence, offset_seconds + event->seconds, &beats);
  if (status != noErr) {
    return midi_result_osstatus("MusicSequenceGetBeatsForSeconds", status);
  }

  if (event->length <= 3 && event->data[0] >= 0x80 && event->data[0] <= 0xEF) {
    MIDIChannelMessage message;

    memset(&message, 0, sizeof(message));
    message.status = event->data[0];
    message.data1 = event->length > 1 ? event->data[1] : 0;
    message.data2 = event->length > 2 ? event->data[2] : 0;
    status = MusicTrackNewMIDIChannelEvent(track, beats, &message);
    if (status != noErr) {
      return midi_result_osstatus("MusicTrackNewMIDIChannelEvent", status);
    }
    return midi_result_ok();
  }

  MIDIRawData *raw =
      (MIDIRawData *)malloc(sizeof(MIDIRawData) + event->length - 1);
  if (raw == NULL) {
    return midi_result_no_memory("add_sequence_event_to_track");
  }
  raw->length = (UInt32)event->length;
  memcpy(raw->data, event->data, event->length);
  status = MusicTrackNewMIDIRawDataEvent(track, beats, raw);
  free(raw);
  if (status != noErr) {
    return midi_result_osstatus("MusicTrackNewMIDIRawDataEvent", status);
  }
  return midi_result_ok();
}

static MidiResult append_recording_to_loaded_file(TuiRecordSession *record) {
  MusicSequence base_sequence = NULL;
  MusicTrack append_track = NULL;
  MidiSequenceEventList recorded_events;
  double recorded_total_seconds = 0.0;
  MidiResult result;
  OSStatus status;

  if (record == NULL || record->append_path[0] == '\0') {
    return midi_result_invalid_argument("append_recording_to_loaded_file");
  }

  midi_sequence_event_list_init(&recorded_events);
  result = midi_sequence_load_file(record->append_path, &base_sequence);
  if (!midi_result_is_ok(result)) {
    return result;
  }

  result =
      midi_sequence_collect_events(midi_recorder_sequence(&record->recorder),
                                   &recorded_events, &recorded_total_seconds);
  (void)recorded_total_seconds;
  if (!midi_result_is_ok(result)) {
    DisposeMusicSequence(base_sequence);
    return result;
  }

  status = MusicSequenceNewTrack(base_sequence, &append_track);
  if (status != noErr) {
    midi_sequence_event_list_free(&recorded_events);
    DisposeMusicSequence(base_sequence);
    return midi_result_osstatus("MusicSequenceNewTrack", status);
  }

  for (size_t i = 0; i < recorded_events.count; ++i) {
    result = add_sequence_event_to_track(base_sequence, append_track,
                                         &recorded_events.items[i],
                                         record->append_offset_seconds);
    if (!midi_result_is_ok(result)) {
      midi_sequence_event_list_free(&recorded_events);
      DisposeMusicSequence(base_sequence);
      return result;
    }
  }

  status = save_sequence_to_file(base_sequence, record->append_path);
  midi_sequence_event_list_free(&recorded_events);
  DisposeMusicSequence(base_sequence);
  if (status != noErr) {
    return midi_result_osstatus("MusicSequenceFileCreate", status);
  }
  return midi_result_ok();
}

static double record_elapsed_seconds(const TuiRecordSession *record) {
  uint64_t now_nanos;
  uint64_t paused_nanos;

  if (record == NULL || !record->active) {
    return 0.0;
  }

  now_nanos = host_time_now_nanos();
  paused_nanos = record->paused_nanos_total;
  if (record->paused && now_nanos > record->pause_started_nanos) {
    paused_nanos += now_nanos - record->pause_started_nanos;
  }
  if (now_nanos <= record->wall_started_nanos + paused_nanos) {
    return 0.0;
  }
  return (double)(now_nanos - record->wall_started_nanos - paused_nanos) /
         1000000000.0;
}

static int stop_record_session(TuiApp *app, bool save_file) {
  OSStatus status = noErr;
  MidiResult append_result = midi_result_ok();
  unsigned int captured;
  unsigned int ignored;
  bool append_to_loaded;
  char output_path[PATH_MAX];
  char output_name[NAME_MAX + 1];

  if (!app->record.active) {
    return 1;
  }

  append_to_loaded = app->record.append_to_loaded;
  snprintf(output_path, sizeof(output_path), "%s", app->record.output_path);
  snprintf(output_name, sizeof(output_name), "%s",
           app->record.append_name[0] != '\0' ? app->record.append_name
                                              : app->record.output_path);

  if (save_file) {
    if (append_to_loaded) {
      append_result = append_recording_to_loaded_file(&app->record);
    } else {
      status =
          save_sequence_to_file(midi_recorder_sequence(&app->record.recorder),
                                app->record.output_path);
    }
  }
  captured = midi_recorder_captured_events(&app->record.recorder);
  ignored = midi_recorder_ignored_messages(&app->record.recorder);

  dispose_record_resources(&app->record);
  app->record.active = false;
  app->record.paused = false;

  if (save_file && status != noErr) {
    set_osstatus_error(app, "MusicSequenceFileCreate", status);
    return 0;
  }
  if (save_file && !midi_result_is_ok(append_result)) {
    set_midi_result_error(app, append_result);
    return 0;
  }

  if (save_file) {
    log_line(app, "%s %u event(s) to %s",
             append_to_loaded ? "Appended" : "Saved", captured, output_path);
    if (ignored > 0) {
      log_line(app, "Ignored %u unsupported/incomplete message(s)", ignored);
    }
    set_status(app, "%s %s", append_to_loaded ? "Appended to" : "Saved",
               output_path);
    scan_recording_files(app);
    if (append_to_loaded) {
      select_file_by_name(app, output_name);
      playback_load_file(app, output_path, output_name, true);
    }
  }

  return 1;
}

static int start_record_session(TuiApp *app) {
  time_t now_time;
  struct tm local_time;
  char file_name[64];
  char source_name[TUI_MAX_NAME];
  TuiRecordSession record;

  if (app == NULL) {
    return 0;
  }
  if (app->record.active) {
    set_status(app, "Recording is already active");
    return 0;
  }
  if (app->playback.active) {
    set_status(app, "Stop playback before recording");
    return 0;
  }
  if (app->append_armed && !app->playback.loaded) {
    app->append_armed = false;
    set_status(app, "Load a recording before arming append");
    return 0;
  }
  if (MIDIGetNumberOfSources() == 0) {
    set_status(app, "No MIDI source is available");
    return 0;
  }
  if (!ensure_directory_exists(app->recordings_dir)) {
    set_status(app, "Could not create or access %s", app->recordings_dir);
    return 0;
  }

  now_time = time(NULL);
  if (localtime_r(&now_time, &local_time) == NULL ||
      !tui_format_recording_name(&local_time, file_name, sizeof(file_name))) {
    set_status(app, "Could not build recording name");
    return 0;
  }

  maybe_refresh_monitor_input(app);
  if (!app->monitor.connected) {
    set_status(app, "Source [0] is not available");
    return 0;
  }

  memset(&record, 0, sizeof(record));
  if (!initialize_record_state(app, &record)) {
    return 0;
  }

  snprintf(source_name, sizeof(source_name), "%s", app->monitor.source_name);
  record.app = app;
  record.active = true;
  record.wall_started_nanos = host_time_now_nanos();
  record.source_index = 0;
  snprintf(record.source_name, sizeof(record.source_name), "%s", source_name);
  if (app->append_armed && app->playback.loaded) {
    record.append_to_loaded = true;
    record.append_offset_seconds = app->playback.total_seconds;
    snprintf(record.append_path, sizeof(record.append_path), "%s",
             app->playback.loaded_path);
    snprintf(record.append_name, sizeof(record.append_name), "%s",
             app->playback.loaded_name);
    snprintf(record.output_path, sizeof(record.output_path), "%s",
             app->playback.loaded_path);
    app->append_armed = false;
  } else {
    tui_join_path(record.output_path, sizeof(record.output_path),
                  app->recordings_dir, file_name);
  }
  app->record = record;
  app->metronome_next_beat = 1;
  set_status(app, "%s %s",
             app->record.append_to_loaded ? "Appending to" : "Recording to",
             app->record.output_path);
  log_line(app, "REC source[0] %s -> %s%s", app->record.source_name,
           app->record.output_path,
           app->record.append_to_loaded ? " append" : "");
  return 1;
}

static void toggle_record_pause(TuiApp *app) {
  uint64_t now_nanos;

  if (!app->record.active) {
    return;
  }

  now_nanos = host_time_now_nanos();
  if (!app->record.paused) {
    app->record.paused = true;
    app->record.pause_started_nanos = now_nanos;
    set_status(app, "Recording paused");
    log_line(app, "REC paused");
    return;
  }

  if (now_nanos > app->record.pause_started_nanos) {
    app->record.paused_nanos_total +=
        now_nanos - app->record.pause_started_nanos;
  }
  app->record.pause_started_nanos = 0;
  app->record.paused = false;
  set_status(app, "Recording resumed");
  log_line(app, "REC resumed");
}

static double playback_elapsed_seconds(const TuiPlaybackSession *playback) {
  uint64_t now_nanos;
  uint64_t paused_nanos;

  if (playback == NULL || !playback->active) {
    return 0.0;
  }
  now_nanos = host_time_now_nanos();
  paused_nanos = playback->paused_nanos_total;
  if (playback->paused && now_nanos > playback->pause_started_nanos) {
    paused_nanos += now_nanos - playback->pause_started_nanos;
  }
  if (now_nanos <= playback->wall_started_nanos + paused_nanos) {
    return 0.0;
  }
  return (double)(now_nanos - playback->wall_started_nanos - paused_nanos) /
         1000000000.0;
}

static double playback_position_seconds(const TuiApp *app) {
  const TuiPlaybackSession *playback;
  double tempo_scale;

  if (app == NULL) {
    return 0.0;
  }
  playback = &app->playback;
  if (!playback->active) {
    if (playback->events.count == 0 ||
        playback->selected_event >= playback->events.count) {
      return 0.0;
    }
    return playback->events.items[playback->selected_event].seconds;
  }
  tempo_scale = app->settings.tempo_bpm > 0 ? (double)app->settings.tempo_bpm /
                                                  (double)TUI_TEMPO_DEFAULT_BPM
                                            : 1.0;
  return playback->start_offset_seconds +
         playback_elapsed_seconds(playback) * tempo_scale;
}

static void stop_playback_session(TuiApp *app, bool completed) {
  if (!app->playback.active) {
    return;
  }

  playback_dispose_transport(&app->playback);
  app->playback.active = false;
  app->playback.paused = false;
  app->playback.paused_nanos_total = 0;
  app->playback.pause_started_nanos = 0;
  app->playback.wall_started_nanos = 0;
  app->playback.start_offset_seconds = 0.0;
  app->playback.last_tx_nanos = 0;
  if (completed) {
    set_status(app, "Playback complete");
    log_line(app, "PLAY complete");
  } else {
    set_status(app, "Playback stopped");
    log_line(app, "PLAY stopped");
  }
}

static int start_playback_session(TuiApp *app) {
  MIDIEndpointRef destination;
  MidiResult result;

  if (app->playback.active) {
    set_status(app, "Playback is already active");
    return 0;
  }

  if (!app->playback.loaded) {
    set_status(app, "Load a MIDI file first");
    return 0;
  }
  if (app->record.active) {
    set_status(app, "Stop recording before playback");
    return 0;
  }
  if (MIDIGetNumberOfDestinations() == 0) {
    set_status(app, "No MIDI destination is available");
    return 0;
  }

  destination = get_destination_by_index(0);
  if (destination == 0) {
    set_status(app, "Destination [0] is not available");
    return 0;
  }
  result = midi_output_open(&app->playback.output,
                            CFSTR("midi-capture-tui-playback"),
                            CFSTR("midi-capture-tui-output"));
  if (!midi_result_is_ok(result)) {
    set_midi_result_error(app, result);
    return 0;
  }

  app->playback.destination_index = 0;
  get_endpoint_name(destination, app->playback.destination_name,
                    sizeof(app->playback.destination_name));
  app->playback.start_index = app->playback.selected_event;
  if (app->playback.start_index >= app->playback.events.count) {
    app->playback.start_index = 0;
  }
  app->playback.current_index = app->playback.start_index;
  app->playback.start_offset_seconds =
      app->playback.events.count == 0
          ? 0.0
          : app->playback.events.items[app->playback.start_index].seconds;
  app->playback.wall_started_nanos = host_time_now_nanos();
  app->playback.paused_nanos_total = 0;
  app->playback.pause_started_nanos = 0;
  app->playback.last_tx_nanos = 0;
  app->playback.active = true;
  app->playback.paused = false;
  set_status(app, "Playing %s to [%ld] %s", app->playback.loaded_name,
             app->playback.destination_index, app->playback.destination_name);
  log_line(app, "PLAY %s -> dest[0] %s from event %zu",
           app->playback.loaded_name, app->playback.destination_name,
           app->playback.start_index + 1);
  return 1;
}

static void toggle_playback_pause(TuiApp *app) {
  uint64_t now_nanos;

  if (!app->playback.active) {
    return;
  }

  now_nanos = host_time_now_nanos();
  if (!app->playback.paused) {
    app->playback.paused = true;
    app->playback.pause_started_nanos = now_nanos;
    set_status(app, "Playback paused");
    log_line(app, "PLAY paused");
    return;
  }

  if (now_nanos > app->playback.pause_started_nanos) {
    app->playback.paused_nanos_total +=
        now_nanos - app->playback.pause_started_nanos;
  }
  app->playback.pause_started_nanos = 0;
  app->playback.paused = false;
  set_status(app, "Playback resumed");
  log_line(app, "PLAY resumed");
}

static void playback_tick(TuiApp *app) {
  MIDIEndpointRef destination;
  double current_position;
  MidiResult result;

  if (!app->playback.active || app->playback.paused) {
    return;
  }

  destination = get_destination_by_index(app->playback.destination_index);
  if (destination == 0) {
    stop_playback_session(app, false);
    set_status(app, "Destination [0] disappeared");
    return;
  }

  current_position = playback_position_seconds(app);
  while (app->playback.current_index < app->playback.events.count &&
         app->playback.events.items[app->playback.current_index].seconds <=
             current_position) {
    MidiSequenceEvent *event =
        &app->playback.events.items[app->playback.current_index];
    result = midi_output_send(&app->playback.output, destination, event->data,
                              event->length);
    if (!midi_result_is_ok(result)) {
      set_midi_result_error(app, result);
      stop_playback_session(app, false);
      return;
    }
    app->playback.last_tx_nanos = host_time_now_nanos();
    app->playback.selected_event = app->playback.current_index;
    log_midi_bytes(app, "TX", event->seconds, event->data, event->length);
    app->playback.current_index += 1;
  }

  if (app->playback.current_index >= app->playback.events.count &&
      current_position >= app->playback.total_seconds) {
    stop_playback_session(app, true);
  }
}

static const char *current_mode_label(const TuiApp *app) {
  if (app->record.active) {
    return app->record.paused ? "RECORD PAUSED" : "RECORDING";
  }
  if (app->playback.active) {
    return app->playback.paused ? "PLAY PAUSED" : "PLAYING";
  }
  return "IDLE";
}

static double fade_timeout_seconds(TuiFadeTimeout timeout);
static const char *fade_timeout_label(TuiFadeTimeout timeout);

static void live_scope_channel_range(const TuiApp *app, size_t *start_channel,
                                     size_t *end_channel) {
  if (start_channel == NULL || end_channel == NULL) {
    return;
  }
  *start_channel = 0;
  *end_channel = 16;
  if (app != NULL && app->channel_scope > 0 && app->channel_scope <= 16) {
    *start_channel = (size_t)app->channel_scope - 1;
    *end_channel = (size_t)app->channel_scope;
  }
}

static void format_channel_scope_label(const TuiApp *app, char *buffer,
                                       size_t buffer_size) {
  if (buffer == NULL || buffer_size == 0) {
    return;
  }
  if (app != NULL && app->channel_scope > 0 && app->channel_scope <= 16) {
    snprintf(buffer, buffer_size, "CH %d", app->channel_scope);
  } else {
    snprintf(buffer, buffer_size, "CH AUTO");
  }
}

static bool live_note_is_visible(const TuiLiveNoteState *state,
                                 uint64_t now_nanos, double timeout_seconds,
                                 double *age_seconds) {
  double age = 0.0;

  if (state == NULL || !state->seen || state->last_seen_nanos == 0) {
    return false;
  }
  if (now_nanos >= state->last_seen_nanos) {
    age = (double)(now_nanos - state->last_seen_nanos) / 1000000000.0;
  }
  if (age_seconds != NULL) {
    *age_seconds = age;
  }
  if (!state->active && timeout_seconds >= 0.0 && age > timeout_seconds) {
    return false;
  }
  return true;
}

static size_t filter_diagnostic_log_snapshot(TuiLogEntry *entries, size_t count,
                                             bool midi_only) {
  size_t kept = 0;

  if (entries == NULL || !midi_only) {
    return count;
  }
  for (size_t index = 0; index < count; ++index) {
    if (!entries[index].has_midi_fields) {
      continue;
    }
    entries[kept++] = entries[index];
  }
  return kept;
}

static void format_footer_summary(TuiApp *app, char *summary,
                                  size_t summary_size) {
  char clock_text[32];

  if (app == NULL || summary == NULL || summary_size == 0) {
    return;
  }

  if (app->record.active) {
    tui_format_clock_time(clock_text, sizeof(clock_text),
                          record_elapsed_seconds(&app->record));
    snprintf(summary, summary_size,
             "REC %s  %u event(s)  %u ignored  %s  tempo %d BPM  metro %s",
             clock_text, midi_recorder_captured_events(&app->record.recorder),
             midi_recorder_ignored_messages(&app->record.recorder),
             app->record.append_to_loaded ? "append armed"
                                          : app->record.output_path,
             app->settings.tempo_bpm,
             app->settings.metronome_enabled ? "on" : "off");
  } else if (app->work_pane_mode == TUI_RENDER_WORK_PANE_LIVE_PLAYER) {
    size_t active_count = 0;
    size_t fading_count = 0;
    size_t start_channel;
    size_t end_channel;
    uint64_t now_nanos = host_time_now_nanos();
    double timeout_seconds = fade_timeout_seconds(app->settings.fade_timeout);
    char scope_label[16];
    char note_format_label[16];
    const char *fade_label = fade_timeout_label(app->settings.fade_timeout);

    live_scope_channel_range(app, &start_channel, &end_channel);
    for (size_t channel = start_channel; channel < end_channel; ++channel) {
      for (size_t note = 0; note < 128; ++note) {
        TuiLiveNoteState *state = &app->live_notes[channel][note];
        if (!live_note_is_visible(state, now_nanos, timeout_seconds, NULL)) {
          continue;
        }
        if (state->active) {
          active_count++;
        } else {
          fading_count++;
        }
      }
    }
    format_channel_scope_label(app, scope_label, sizeof(scope_label));
    snprintf(note_format_label, sizeof(note_format_label), "%s",
             app->settings.note_format == TUI_NOTE_FORMAT_NAME ? "name"
                                                               : "number");

    snprintf(summary, summary_size,
             "%s  %s  %zu active  %zu fading  hold: %s  format: %s  fade: %s",
             app->status_message, scope_label, active_count, fading_count,
             app->live_hold ? "on" : "off", note_format_label, fade_label);
  } else if (app->work_pane_mode == TUI_RENDER_WORK_PANE_LIVE_DIAGNOSTIC) {
    snprintf(
        summary, summary_size,
        "Live Diagnostic  filters: %s  capture paused: %s  raw bytes visible",
        app->diagnostic_filter_midi_only ? "midi only" : "off",
        app->diagnostic_log_paused ? "yes" : "no");
  } else if (app->playback.active || app->playback.loaded) {
    tui_format_clock_time(clock_text, sizeof(clock_text),
                          playback_position_seconds(app));
    snprintf(summary, summary_size,
             "PLAY %s / %.1fs  event %zu/%zu  tempo %d BPM  %s  %s", clock_text,
             app->playback.total_seconds, app->playback.selected_event + 1,
             app->playback.events.count, app->settings.tempo_bpm,
             app->append_armed ? "append armed" : "append off",
             app->playback.active ? "active" : app->status_message);
  } else {
    snprintf(summary, summary_size, "%s  tempo %d BPM  metro %s",
             app->status_message, app->settings.tempo_bpm,
             app->settings.metronome_enabled ? "on" : "off");
  }
}

static int fill_sequence_render_row(void *context, size_t index,
                                    TuiRenderSequenceRow *row) {
  TuiApp *app = (TuiApp *)context;
  MidiSequenceEvent *event;
  MidiDescription description;
  TuiMidiDisplayFields display_fields;

  if (app == NULL || row == NULL || index >= app->playback.events.count) {
    return 0;
  }

  event = &app->playback.events.items[index];
  memset(row, 0, sizeof(*row));
  row->index = index;
  row->selected = index == app->playback.selected_event;
  tui_format_clock_time(row->clock_text, sizeof(row->clock_text),
                        event->seconds);
  tui_format_midi_bytes(row->byte_text, sizeof(row->byte_text), event->data,
                        event->length);
  midi_describe_bytes(event->data, event->length, &description);
  fill_midi_display_fields(&app->settings, event->data, event->length,
                           &description, &display_fields);
  snprintf(row->channel, sizeof(row->channel), "%s", display_fields.channel);
  snprintf(row->event, sizeof(row->event), "%s", display_fields.event);
  snprintf(row->target, sizeof(row->target), "%s", display_fields.target);
  snprintf(row->value, sizeof(row->value), "%s", display_fields.value);
  row->category = description.category;
  snprintf(row->description, sizeof(row->description), "%s", description.text);
  return 1;
}

static double fade_timeout_seconds(TuiFadeTimeout timeout) {
  switch (timeout) {
  case TUI_FADE_TIMEOUT_2S:
    return 2.0;
  case TUI_FADE_TIMEOUT_5S:
    return 5.0;
  case TUI_FADE_TIMEOUT_10S:
    return 10.0;
  case TUI_FADE_TIMEOUT_NEVER:
    return -1.0;
  }
  return 5.0;
}

static const char *fade_timeout_label(TuiFadeTimeout timeout) {
  switch (timeout) {
  case TUI_FADE_TIMEOUT_2S:
    return "2s";
  case TUI_FADE_TIMEOUT_5S:
    return "5s";
  case TUI_FADE_TIMEOUT_10S:
    return "10s";
  case TUI_FADE_TIMEOUT_NEVER:
    return "never";
  }
  return "5s";
}

static size_t fill_live_note_rows(TuiApp *app, TuiRenderLiveNoteRow *rows,
                                  size_t capacity, uint64_t now_nanos) {
  size_t count = 0;
  double timeout_seconds;
  size_t start_channel = 0;
  size_t end_channel = 16;

  if (app == NULL || rows == NULL || capacity == 0) {
    return 0;
  }

  live_scope_channel_range(app, &start_channel, &end_channel);

  timeout_seconds = fade_timeout_seconds(app->settings.fade_timeout);
  for (size_t channel = start_channel; channel < end_channel; ++channel) {
    for (size_t note = 0; note < 128; ++note) {
      TuiLiveNoteState *state = &app->live_notes[channel][note];
      TuiRenderLiveNoteRow *row;
      double age_seconds = 0.0;

      if (!live_note_is_visible(state, now_nanos, timeout_seconds,
                                &age_seconds)) {
        continue;
      }
      if (count >= capacity) {
        return count;
      }

      row = &rows[count];
      memset(row, 0, sizeof(*row));
      snprintf(row->number, sizeof(row->number), "%zu", count + 1);
      format_note_label(&app->settings, state->note, row->note,
                        sizeof(row->note));
      snprintf(row->state, sizeof(row->state), "%s",
               state->active ? "ON"
                             : (timeout_seconds < 0.0 ? "OFF" : "FADING"));
      snprintf(row->velocity, sizeof(row->velocity), "%u", state->velocity);
      snprintf(row->pressure, sizeof(row->pressure), "%u", state->pressure);
      snprintf(row->bend_mod, sizeof(row->bend_mod), "%+d/%u", state->bend,
               state->modulation);
      snprintf(row->age, sizeof(row->age), "%.1fs", age_seconds);
      row->active = state->active;
      row->category =
          state->active ? MIDI_DESCRIPTION_NOTE_ON : MIDI_DESCRIPTION_NOTE_OFF;
      row->raw_velocity = state->velocity;
      row->raw_pressure = state->pressure;
      count += 1;
    }
  }
  for (size_t i = 0; i < count; ++i) {
    char numbered[TUI_RENDER_FIELD_TEXT_LENGTH];
    snprintf(numbered, sizeof(numbered), "%zu/%zu", i + 1, count);
    snprintf(rows[i].number, sizeof(rows[i].number), "%s", numbered);
  }
  return count;
}

static void format_age_short(uint64_t now_nanos, uint64_t last_nanos, char *out,
                             size_t out_size) {
  if (out == NULL || out_size == 0) {
    return;
  }
  if (last_nanos == 0 || now_nanos < last_nanos) {
    snprintf(out, out_size, "--");
    return;
  }
  uint64_t delta = now_nanos - last_nanos;
  double seconds = (double)delta / 1000000000.0;
  if (seconds < 1.0) {
    snprintf(out, out_size, "%ums", (unsigned int)(seconds * 1000.0));
  } else if (seconds < 10.0) {
    snprintf(out, out_size, "%.1fs", seconds);
  } else {
    snprintf(out, out_size, "%us", (unsigned int)seconds);
  }
}

static void fill_live_controls_row(TuiApp *app, TuiRenderLiveControlRow *row,
                                   uint64_t now_nanos) {
  TuiLiveChannelState *best = NULL;

  if (app == NULL || row == NULL) {
    return;
  }
  memset(row, 0, sizeof(*row));

  if (app->channel_scope == 0) {
    for (size_t i = 0; i < 16; ++i) {
      TuiLiveChannelState *state = &app->live_channels[i];
      if (!state->seen) {
        continue;
      }
      if (best == NULL || state->last_seen_nanos > best->last_seen_nanos) {
        best = state;
      }
    }
  } else {
    best = &app->live_channels[app->channel_scope - 1];
  }

  if (best == NULL || (app->channel_scope > 0 && !best->seen)) {
    if (app->channel_scope > 0) {
      snprintf(row->scope, sizeof(row->scope), "CH %d", app->channel_scope);
    } else {
      snprintf(row->scope, sizeof(row->scope), "CH AUTO");
    }
    snprintf(row->modulation, sizeof(row->modulation), "0");
    snprintf(row->pressure, sizeof(row->pressure), "0");
    snprintf(row->pitch, sizeof(row->pitch), "+0");
    snprintf(row->last_rx, sizeof(row->last_rx), "RX --");
    row->active = false;
    row->raw_modulation = 0;
    row->raw_pressure = 0;
    row->raw_pitch = 0;
    return;
  }

  if (app->channel_scope > 0) {
    snprintf(row->scope, sizeof(row->scope), "CH %d", app->channel_scope);
  } else {
    snprintf(row->scope, sizeof(row->scope), "CH AUTO");
  }
  snprintf(row->modulation, sizeof(row->modulation), "%u",
           (unsigned int)best->modulation);
  snprintf(row->pressure, sizeof(row->pressure), "%u",
           (unsigned int)best->pressure);
  snprintf(row->pitch, sizeof(row->pitch), "%+d", best->bend);

  char age[TUI_RENDER_FIELD_TEXT_LENGTH];
  format_age_short(now_nanos, best->last_seen_nanos, age, sizeof(age));
  snprintf(row->last_rx, sizeof(row->last_rx), "RX %s", age);
  row->active = true;
  row->raw_modulation = (int)best->modulation;
  row->raw_pressure = (int)best->pressure;
  row->raw_pitch = (int)best->bend;
}

static void render_tui(TuiApp *app) {
  TuiRenderState state;
  TuiLogEntry log_snapshot[TUI_LOG_CAPACITY];
  TuiRenderLiveNoteRow live_note_rows[TUI_RENDER_LIVE_NOTE_MAX];
  char source_label[TUI_MAX_STATUS];
  char destination_label[TUI_MAX_STATUS];
  char source_name[TUI_MAX_NAME];
  char destination_name[TUI_MAX_NAME];
  char footer[TUI_MAX_STATUS];
  char middle_c_label[16];
  char tempo_label[24];
  char directory_overlay_message[TUI_MAX_STATUS];
  size_t log_count;
  uint64_t now_nanos = host_time_now_nanos();

  memset(&state, 0, sizeof(state));
  describe_endpoint(true, 0, source_name, sizeof(source_name));
  describe_endpoint(false, 0, destination_name, sizeof(destination_name));
  snprintf(source_label, sizeof(source_label), "SRC [0] %s", source_name);
  if (app->work_pane_mode == TUI_RENDER_WORK_PANE_LIVE_PLAYER) {
    format_channel_scope_label(app, destination_label,
                               sizeof(destination_label));
  } else {
    snprintf(destination_label, sizeof(destination_label), "DST [0] %s",
             destination_name);
  }
  format_footer_summary(app, footer, sizeof(footer));
  snprintf(middle_c_label, sizeof(middle_c_label), "C%d",
           app->settings.middle_c_octave);
  snprintf(tempo_label, sizeof(tempo_label), "%d BPM", app->settings.tempo_bpm);
  snprintf(directory_overlay_message, sizeof(directory_overlay_message),
           "Current recordings path: %s", app->recordings_dir);

  state.app_version = app_version();
  state.recordings_dir = app->recordings_dir;
  state.source_label = source_label;
  state.destination_label = destination_label;
  state.mode_label = current_mode_label(app);
  state.work_pane_mode = app->work_pane_mode;
  state.focus = app->focus;
  state.rx_active = activity_is_active(
      now_nanos,
      atomic_load_explicit(&app->monitor.last_rx_nanos, memory_order_relaxed));
  state.tx_active = activity_is_active(now_nanos, app->playback.last_tx_nanos);
  state.record_active = app->record.active;
  state.playback_active = app->playback.active;
  state.append_armed = app->append_armed;
  state.files = &app->files;
  state.sequence_loaded = app->playback.loaded;
  state.sequence_name = app->playback.loaded_name;
  state.sequence_event_count = app->playback.events.count;
  state.sequence_selected = app->playback.selected_event;
  state.sequence_row_provider = fill_sequence_render_row;
  state.sequence_context = app;
  log_count = tui_log_snapshot(app->log, log_snapshot, TUI_LOG_CAPACITY);
  log_count = filter_diagnostic_log_snapshot(log_snapshot, log_count,
                                             app->diagnostic_filter_midi_only);
  state.logs = log_snapshot;
  state.log_count = log_count;
  state.live_notes = live_note_rows;
  state.live_note_count = fill_live_note_rows(
      app, live_note_rows, TUI_RENDER_LIVE_NOTE_MAX, now_nanos);
  fill_live_controls_row(app, &state.live_controls, now_nanos);
  state.overlay = app->overlay;
  state.settings.selected_index = app->settings.selected_index;
  state.settings.recordings_dir = app->recordings_dir;
  state.settings.middle_c = middle_c_label;
  state.settings.note_format =
      app->settings.note_format == TUI_NOTE_FORMAT_NAME ? "name" : "number";
  state.settings.fade_timeout = fade_timeout_label(app->settings.fade_timeout);
  state.settings.tempo = tempo_label;
  state.settings.metronome = app->settings.metronome_enabled ? "on" : "off";
  state.directory_overlay_message = directory_overlay_message;
  state.footer = footer;

  tui_render(&state);
}

static void move_file_selection(TuiApp *app, int delta) {
  size_t selected;

  if (app->files.count == 0) {
    return;
  }

  selected = app->files.selected;
  if (delta < 0 && selected > 0) {
    selected -= 1;
  } else if (delta > 0 && selected + 1 < app->files.count) {
    selected += 1;
  }
  app->files.selected = selected;
}

static void move_event_selection(TuiApp *app, int delta) {
  size_t selected;

  if (!app->playback.loaded || app->playback.events.count == 0 ||
      app->record.active || app->playback.active) {
    return;
  }

  selected = app->playback.selected_event;
  if (delta < 0 && selected > 0) {
    selected -= 1;
  } else if (delta > 0 && selected + 1 < app->playback.events.count) {
    selected += 1;
  }
  app->playback.selected_event = selected;
}

static void prompt_for_output_directory(TuiApp *app) {
  char input[PATH_MAX];
  int rows;
  int cols;

  if (app->record.active || app->playback.active) {
    set_status(app, "Stop the current transport before changing the directory");
    return;
  }

  getmaxyx(stdscr, rows, cols);
  clear();
  echo();
  curs_set(1);
  timeout(-1);
  mvhline(rows - 2, 0, ' ', cols);
  mvprintw(rows - 2, 2, "Recordings directory: ");
  getnstr(input, (int)sizeof(input) - 1);
  noecho();
  curs_set(0);
  timeout(TUI_INPUT_TIMEOUT_MS);

  trim_ascii_whitespace(input);
  if (input[0] == '\0') {
    set_status(app, "Recordings directory unchanged");
    return;
  }
  if (!ensure_directory_exists(input)) {
    set_status(app, "Could not create or access %s", input);
    return;
  }

  snprintf(app->recordings_dir, sizeof(app->recordings_dir), "%s", input);
  playback_clear_loaded(&app->playback);
  app->append_armed = false;
  scan_recording_files(app);
  set_status(app, "Recordings directory set to %s", app->recordings_dir);
}

static bool has_mid_suffix(const char *name) {
  size_t length;

  if (name == NULL) {
    return false;
  }
  length = strlen(name);
  if (length < 4) {
    return false;
  }
  return tolower((unsigned char)name[length - 4]) == '.' &&
         tolower((unsigned char)name[length - 3]) == 'm' &&
         tolower((unsigned char)name[length - 2]) == 'i' &&
         tolower((unsigned char)name[length - 1]) == 'd';
}

static void unload_loaded_file(TuiApp *app) {
  if (app == NULL) {
    return;
  }
  if (app->record.active || app->playback.active) {
    set_status(app, "Stop the current transport before unloading");
    return;
  }
  if (!app->playback.loaded) {
    set_status(app, "No loaded recording to unload");
    return;
  }
  playback_clear_loaded(&app->playback);
  app->append_armed = false;
  set_status(app, "Unloaded recording");
}

static void arm_append_recording(TuiApp *app) {
  if (app == NULL) {
    return;
  }
  if (app->record.active || app->playback.active) {
    set_status(app, "Stop the current transport before changing append state");
    return;
  }
  if (!app->playback.loaded) {
    set_status(app, "Load a recording before appending");
    return;
  }
  app->append_armed = !app->append_armed;
  set_status(app, "%s append for %s", app->append_armed ? "Armed" : "Disarmed",
             app->playback.loaded_name);
}

static void prompt_for_rename(TuiApp *app) {
  const TuiFileEntry *selected_file;
  char input[NAME_MAX + 1];
  char new_name[NAME_MAX + 1];
  char new_path[PATH_MAX];
  int rows;
  int cols;
  int written;

  if (app == NULL) {
    return;
  }
  if (app->record.active || app->playback.active) {
    set_status(app, "Stop the current transport before renaming");
    return;
  }
  selected_file = tui_file_list_selected(&app->files);
  if (selected_file == NULL) {
    set_status(app, "No recording selected to rename");
    return;
  }

  getmaxyx(stdscr, rows, cols);
  echo();
  curs_set(1);
  timeout(-1);
  mvhline(rows - 2, 0, ' ', cols);
  mvprintw(rows - 2, 2, "Rename %s to: ", selected_file->name);
  getnstr(input, (int)sizeof(input) - 1);
  noecho();
  curs_set(0);
  timeout(TUI_INPUT_TIMEOUT_MS);

  trim_ascii_whitespace(input);
  if (input[0] == '\0') {
    set_status(app, "Rename cancelled");
    return;
  }
  if (strchr(input, '/') != NULL) {
    set_status(app, "Rename accepts a file name, not a path");
    return;
  }

  if (has_mid_suffix(input)) {
    written = snprintf(new_name, sizeof(new_name), "%s", input);
  } else {
    written = snprintf(new_name, sizeof(new_name), "%s.mid", input);
  }
  if (written < 0 || (size_t)written >= sizeof(new_name)) {
    set_status(app, "New name is too long");
    return;
  }
  if (!tui_join_path(new_path, sizeof(new_path), app->recordings_dir,
                     new_name)) {
    set_status(app, "New path is too long");
    return;
  }
  if (strcmp(selected_file->path, new_path) == 0) {
    set_status(app, "Rename unchanged");
    return;
  }
  if (access(new_path, F_OK) == 0) {
    set_status(app, "%s already exists", new_name);
    return;
  }
  if (rename(selected_file->path, new_path) != 0) {
    set_status(app, "Could not rename %s: %s", selected_file->name,
               strerror(errno));
    return;
  }

  if (app->playback.loaded &&
      strcmp(app->playback.loaded_path, selected_file->path) == 0) {
    snprintf(app->playback.loaded_path, sizeof(app->playback.loaded_path), "%s",
             new_path);
    snprintf(app->playback.loaded_name, sizeof(app->playback.loaded_name), "%s",
             new_name);
  }
  scan_recording_files(app);
  select_file_by_name(app, new_name);
  set_status(app, "Renamed recording to %s", new_name);
}

static void cycle_work_pane_mode(TuiApp *app) {
  if (app == NULL) {
    return;
  }
  app->work_pane_mode = (TuiRenderWorkPaneMode)((app->work_pane_mode + 1) % 3);
  app->focus = TUI_RENDER_FOCUS_WORK_PANE;
  set_status(app, "Work pane: %s",
             app->work_pane_mode == TUI_RENDER_WORK_PANE_SEQUENCE
                 ? "Sequence"
                 : (app->work_pane_mode == TUI_RENDER_WORK_PANE_LIVE_PLAYER
                        ? "Live Player"
                        : "Live Diagnostic"));
}

static void cycle_focus(TuiApp *app) {
  if (app == NULL) {
    return;
  }
  app->focus = app->focus == TUI_RENDER_FOCUS_FILES ? TUI_RENDER_FOCUS_WORK_PANE
                                                    : TUI_RENDER_FOCUS_FILES;
}

static void return_playhead_to_start(TuiApp *app) {
  if (app == NULL || !app->playback.loaded) {
    return;
  }
  if (app->playback.active) {
    stop_playback_session(app, false);
  }
  app->playback.selected_event = 0;
  app->playback.current_index = 0;
  set_status(app, "Playhead returned to start");
}

static void adjust_setting(TuiApp *app, int direction) {
  if (app == NULL || direction == 0) {
    return;
  }

  switch (app->settings.selected_index) {
  case 1:
    app->settings.middle_c_octave = app->settings.middle_c_octave == 4 ? 3 : 4;
    break;
  case 2:
    app->settings.note_format =
        app->settings.note_format == TUI_NOTE_FORMAT_NAME
            ? TUI_NOTE_FORMAT_NUMBER
            : TUI_NOTE_FORMAT_NAME;
    break;
  case 3:
    app->settings.fade_timeout = (TuiFadeTimeout)((app->settings.fade_timeout +
                                                   (direction > 0 ? 1 : 3)) %
                                                  4);
    break;
  case 4:
    app->settings.tempo_bpm += direction > 0 ? 5 : -5;
    if (app->settings.tempo_bpm < 40) {
      app->settings.tempo_bpm = 40;
    } else if (app->settings.tempo_bpm > 240) {
      app->settings.tempo_bpm = 240;
    }
    break;
  case 5:
    app->settings.metronome_enabled = !app->settings.metronome_enabled;
    break;
  default:
    break;
  }
}

static void handle_settings_key(TuiApp *app, int ch) {
  const int setting_count = 6;

  switch (ch) {
  case 27:
  case ',':
    app->overlay = TUI_RENDER_OVERLAY_NONE;
    break;
  case 'd':
    app->overlay = TUI_RENDER_OVERLAY_DIRECTORY;
    break;
  case KEY_UP:
    app->settings.selected_index = app->settings.selected_index == 0
                                        ? setting_count - 1
                                        : app->settings.selected_index - 1;
    break;
  case KEY_DOWN:
    app->settings.selected_index =
        (app->settings.selected_index + 1) % setting_count;
    break;
  case KEY_LEFT:
    adjust_setting(app, -1);
    break;
  case KEY_RIGHT:
    adjust_setting(app, 1);
    break;
  case '\n':
  case '\r':
  case KEY_ENTER:
    if (app->settings.selected_index == 0) {
      app->overlay = TUI_RENDER_OVERLAY_NONE;
      prompt_for_output_directory(app);
    } else {
      adjust_setting(app, 1);
    }
    break;
  default:
    break;
  }
}

static void handle_directory_overlay_key(TuiApp *app, int ch) {
  switch (ch) {
  case 27:
  case 'd':
    app->overlay = TUI_RENDER_OVERLAY_NONE;
    break;
  case '\n':
  case '\r':
  case KEY_ENTER:
  case 'o':
    app->overlay = TUI_RENDER_OVERLAY_NONE;
    prompt_for_output_directory(app);
    break;
  default:
    break;
  }
}

static void handle_overlay_key(TuiApp *app, int ch) {
  if (app->overlay == TUI_RENDER_OVERLAY_SETTINGS) {
    handle_settings_key(app, ch);
  } else if (app->overlay == TUI_RENDER_OVERLAY_DIRECTORY) {
    handle_directory_overlay_key(app, ch);
  }
}

static void metronome_tick(TuiApp *app) {
  double beat_seconds;
  double elapsed_seconds;

  if (app == NULL || !app->record.active || app->record.paused ||
      !app->settings.metronome_enabled || app->settings.tempo_bpm <= 0) {
    return;
  }

  beat_seconds = 60.0 / (double)app->settings.tempo_bpm;
  elapsed_seconds = record_elapsed_seconds(&app->record);
  while (elapsed_seconds >= beat_seconds * (double)app->metronome_next_beat) {
    beep();
    app->metronome_next_beat += 1;
  }
}

static void maybe_rescan_files(TuiApp *app) {
  time_t now_time = time(NULL);

  if (now_time != app->last_scan_time && !app->record.active &&
      !app->playback.active) {
    scan_recording_files(app);
  }
}

static void handle_keypress(TuiApp *app, int ch, bool *should_quit) {
  if (app->overlay != TUI_RENDER_OVERLAY_NONE && ch != 'q') {
    handle_overlay_key(app, ch);
    return;
  }

  switch (ch) {
  case 'q':
    if (app->record.active) {
      stop_record_session(app, true);
    }
    if (app->playback.active) {
      stop_playback_session(app, false);
    }
    *should_quit = true;
    break;
  case '\t':
    cycle_focus(app);
    break;
  case 'v':
    cycle_work_pane_mode(app);
    break;
  case '\n':
  case '\r':
  case KEY_ENTER:
    playback_load_selected_file(app);
    app->work_pane_mode = TUI_RENDER_WORK_PANE_SEQUENCE;
    app->focus = TUI_RENDER_FOCUS_WORK_PANE;
    break;
  case 'r':
    prompt_for_rename(app);
    break;
  case 'R':
    start_record_session(app);
    break;
  case 'p':
    app->work_pane_mode = TUI_RENDER_WORK_PANE_SEQUENCE;
    start_playback_session(app);
    break;
  case 's':
    if (app->record.active) {
      stop_record_session(app, true);
    } else if (app->playback.active) {
      stop_playback_session(app, false);
    }
    break;
  case ' ':
    if (app->work_pane_mode == TUI_RENDER_WORK_PANE_LIVE_PLAYER) {
      app->live_hold = !app->live_hold;
      set_status(app, "Live hold %s", app->live_hold ? "on" : "off");
    } else if (app->work_pane_mode == TUI_RENDER_WORK_PANE_LIVE_DIAGNOSTIC) {
      app->diagnostic_log_paused = !app->diagnostic_log_paused;
      set_status(app, "Diagnostic capture paused: %s",
                 app->diagnostic_log_paused ? "yes" : "no");
    } else if (app->record.active) {
      toggle_record_pause(app);
    } else if (app->playback.active) {
      toggle_playback_pause(app);
    } else {
      start_playback_session(app);
    }
    break;
  case '0':
  case KEY_HOME:
    return_playhead_to_start(app);
    break;
  case 'u':
    unload_loaded_file(app);
    break;
  case 'a':
    arm_append_recording(app);
    break;
  case ',':
    app->overlay = TUI_RENDER_OVERLAY_SETTINGS;
    break;
  case 'd':
    app->overlay = TUI_RENDER_OVERLAY_DIRECTORY;
    break;
  case 'c':
    if (app->work_pane_mode == TUI_RENDER_WORK_PANE_LIVE_PLAYER) {
      app->channel_scope = (app->channel_scope + 1) % 17;
      if (app->channel_scope == 0) {
        set_status(app, "Channel scope: AUTO");
      } else {
        set_status(app, "Channel scope: CH %d", app->channel_scope);
      }
    }
    break;
  case 'f':
    if (app->work_pane_mode == TUI_RENDER_WORK_PANE_LIVE_DIAGNOSTIC) {
      app->diagnostic_filter_midi_only = !app->diagnostic_filter_midi_only;
      set_status(app, "Diagnostic filter: %s",
                 app->diagnostic_filter_midi_only ? "midi only" : "off");
    }
    break;
  case 'm':
    app->settings.metronome_enabled = !app->settings.metronome_enabled;
    set_status(app, "Metronome %s",
               app->settings.metronome_enabled ? "on" : "off");
    break;
  case 'o':
    prompt_for_output_directory(app);
    break;
  case KEY_UP:
    if (app->focus == TUI_RENDER_FOCUS_WORK_PANE &&
        app->work_pane_mode == TUI_RENDER_WORK_PANE_SEQUENCE) {
      move_event_selection(app, -1);
    } else {
      move_file_selection(app, -1);
    }
    break;
  case KEY_DOWN:
    if (app->focus == TUI_RENDER_FOCUS_WORK_PANE &&
        app->work_pane_mode == TUI_RENDER_WORK_PANE_SEQUENCE) {
      move_event_selection(app, 1);
    } else {
      move_file_selection(app, 1);
    }
    break;
  case KEY_LEFT:
    move_event_selection(app, -1);
    break;
  case KEY_RIGHT:
    move_event_selection(app, 1);
    break;
  default:
    break;
  }
}

static int initialize_tui(TuiApp *app, const char *recordings_dir) {
  memset(app, 0, sizeof(*app));
  tui_file_list_init(&app->files);
  snprintf(app->recordings_dir, sizeof(app->recordings_dir), "%s",
           recordings_dir != NULL ? recordings_dir : ".");
  app->log = tui_log_create();
  if (app->log == NULL) {
    fputs("could not initialize TUI log mutex\n", stderr);
    return 0;
  }
  app->monitor.app = app;
  app->monitor.source_index = 0;
  midi_parser_state_init(&app->monitor.parser_state);
  app->record.app = app;
  app->playback.app = app;
  app->settings.middle_c_octave = TUI_MIDDLE_C_DEFAULT;
  app->settings.note_format = TUI_NOTE_FORMAT_NAME;
  app->settings.fade_timeout = TUI_FADE_TIMEOUT_5S;
  app->settings.tempo_bpm = TUI_TEMPO_DEFAULT_BPM;
  app->settings.metronome_enabled = false;
  app->settings.selected_index = 0;
  app->work_pane_mode = TUI_RENDER_WORK_PANE_SEQUENCE;
  app->focus = TUI_RENDER_FOCUS_FILES;
  app->overlay = TUI_RENDER_OVERLAY_NONE;
  app->channel_scope = 0;
  midi_output_init(&app->playback.output);
  set_status(app, "Ready");
  if (!ensure_directory_exists(app->recordings_dir)) {
    tui_log_destroy(app->log);
    app->log = NULL;
    fputs("could not access recordings directory\n", stderr);
    return 0;
  }
  maybe_refresh_monitor_input(app);
  scan_recording_files(app);
  return 1;
}

static void dispose_tui(TuiApp *app) {
  if (app->record.active) {
    stop_record_session(app, true);
  }
  if (app->playback.active) {
    stop_playback_session(app, false);
  }
  dispose_monitor_input(&app->monitor);
  playback_clear_loaded(&app->playback);
  tui_file_list_dispose(&app->files);
  tui_log_destroy(app->log);
  app->log = NULL;
}

int command_tui(const char *recordings_dir) {
  TuiApp app;
  bool should_quit = false;

  if (!initialize_tui(&app, recordings_dir)) {
    return 1;
  }

  signal(SIGINT, on_sigint);
  g_stop_requested = 0;

  setenv("ESCDELAY", "25", 1);
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  timeout(TUI_INPUT_TIMEOUT_MS);
  curs_set(0);
  tui_render_setup_colors();

  while (!should_quit && !g_stop_requested) {
    int ch;

    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, false);
    maybe_refresh_monitor_input(&app);
    playback_tick(&app);
    metronome_tick(&app);
    maybe_rescan_files(&app);
    render_tui(&app);

    ch = getch();
    if (ch != ERR) {
      handle_keypress(&app, ch, &should_quit);
    }
  }

  endwin();
  dispose_tui(&app);
  if (g_stop_requested) {
    fputs("Interrupted.\n", stderr);
    g_stop_requested = 0;
  }
  return 0;
}
