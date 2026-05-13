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

#include <curses.h>

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

typedef enum TuiColorPair {
  TUI_COLOR_NOTE_ON = 1,
  TUI_COLOR_NOTE_OFF = 2,
  TUI_COLOR_CONTROL = 3,
  TUI_COLOR_PROGRAM = 4,
  TUI_COLOR_BEND = 5,
  TUI_COLOR_SYSTEM = 6,
  TUI_COLOR_WARNING = 7
} TuiColorPair;

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
  char recordings_dir[PATH_MAX];
  char status_message[TUI_MAX_STATUS];
  time_t last_scan_time;
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

static void log_midi_bytes(TuiApp *app, const char *label, double seconds,
                           const uint8_t *bytes, size_t length) {
  char clock_text[32];
  char byte_text[64];
  MidiDescription description;

  tui_format_clock_time(clock_text, sizeof(clock_text), seconds);
  tui_format_midi_bytes(byte_text, sizeof(byte_text), bytes, length);
  midi_describe_bytes(bytes, length, &description);
  log_line(app, "%s %s  %s  %s", clock_text, label, byte_text,
           description.text);
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

static int playback_load_selected_file(TuiApp *app) {
  MidiSequenceEventList events;
  const TuiFileEntry *selected_file;
  double total_seconds = 0.0;
  MidiResult result;

  midi_sequence_event_list_init(&events);
  selected_file = tui_file_list_selected(&app->files);

  if (selected_file == NULL) {
    playback_clear_loaded(&app->playback);
    return 1;
  }

  if (app->playback.loaded &&
      strcmp(app->playback.loaded_path, selected_file->path) == 0) {
    return 1;
  }

  result = midi_sequence_load_events_from_file(selected_file->path, &events,
                                               &total_seconds);
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
           selected_file->path);
  snprintf(app->playback.loaded_name, sizeof(app->playback.loaded_name), "%s",
           selected_file->name);
  set_status(app, "Loaded %s with %zu event(s)", app->playback.loaded_name,
             app->playback.events.count);
  return 1;
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
  unsigned int captured;
  unsigned int ignored;

  if (!app->record.active) {
    return 1;
  }

  if (save_file) {
    status = save_sequence_to_file(
        midi_recorder_sequence(&app->record.recorder), app->record.output_path);
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

  if (save_file) {
    log_line(app, "Saved %u event(s) to %s", captured, app->record.output_path);
    if (ignored > 0) {
      log_line(app, "Ignored %u unsupported/incomplete message(s)", ignored);
    }
    set_status(app, "Saved %s", app->record.output_path);
    scan_recording_files(app);
    playback_load_selected_file(app);
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
  tui_join_path(record.output_path, sizeof(record.output_path),
                app->recordings_dir, file_name);
  app->record = record;
  set_status(app, "Recording to %s", app->record.output_path);
  log_line(app, "REC source[0] %s -> %s", app->record.source_name,
           app->record.output_path);
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

static double playback_position_seconds(const TuiPlaybackSession *playback) {
  if (playback == NULL) {
    return 0.0;
  }
  if (!playback->active) {
    if (playback->events.count == 0 ||
        playback->selected_event >= playback->events.count) {
      return 0.0;
    }
    return playback->events.items[playback->selected_event].seconds;
  }
  return playback->start_offset_seconds + playback_elapsed_seconds(playback);
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
    if (!playback_load_selected_file(app)) {
      return 0;
    }
  }
  if (!app->playback.loaded) {
    set_status(app, "Select a MIDI file first");
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

  current_position = playback_position_seconds(&app->playback);
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

static void draw_header(TuiApp *app, int cols) {
  char source_label[TUI_MAX_STATUS];
  char destination_label[TUI_MAX_STATUS];
  char transport_label[TUI_MAX_STATUS];
  char source_name[TUI_MAX_NAME];
  char destination_name[TUI_MAX_NAME];
  bool rx_active = false;
  bool tx_active = false;
  uint64_t now_nanos = host_time_now_nanos();

  describe_endpoint(true, 0, source_name, sizeof(source_name));
  describe_endpoint(false, 0, destination_name, sizeof(destination_name));
  snprintf(source_label, sizeof(source_label), "Source [0]: %s", source_name);
  snprintf(destination_label, sizeof(destination_label), "Destination [0]: %s",
           destination_name);
  rx_active = activity_is_active(
      now_nanos,
      atomic_load_explicit(&app->monitor.last_rx_nanos, memory_order_relaxed));
  tx_active = activity_is_active(now_nanos, app->playback.last_tx_nanos);

  snprintf(transport_label, sizeof(transport_label), "%s  RX%s TX%s",
           current_mode_label(app), rx_active ? "." : " ",
           tx_active ? "." : " ");

  attron(A_BOLD);
  mvaddnstr(0, 2, "MIDI Capture TUI", cols - 4);
  mvaddnstr(0, cols - (int)strlen(transport_label) - 2, transport_label,
            (int)strlen(transport_label));
  attroff(A_BOLD);
  mvaddnstr(1, 2, "Recordings: ", 12);
  mvaddnstr(1, 14, app->recordings_dir, cols - 16);
  mvaddnstr(2, 2, source_label, cols / 2 - 3);
  mvaddnstr(2, cols / 2, destination_label, cols - cols / 2 - 2);
  mvaddnstr(3, 2,
            "r record  p play  space pause/resume  s stop  o output dir  "
            "arrows navigate  q quit",
            cols - 4);
  mvhline(4, 0, ACS_HLINE, cols);
}

static void draw_files_panel(TuiApp *app, int top, int left, int width,
                             int height) {
  int visible = height - 2;
  size_t scroll = 0;

  mvaddnstr(top, left + 1, "Files", width - 2);
  if (visible <= 0) {
    return;
  }
  if (app->files.selected >= (size_t)visible) {
    scroll = app->files.selected - (size_t)visible + 1;
  }

  for (int row = 0; row < visible; ++row) {
    int screen_row = top + 1 + row;
    size_t index = scroll + (size_t)row;

    move(screen_row, left + 1);
    clrtoeol();
    if (index >= app->files.count) {
      continue;
    }

    if (index == app->files.selected) {
      attron(A_REVERSE);
    }
    mvaddnstr(screen_row, left + 1, app->files.items[index].name, width - 2);
    if (index == app->files.selected) {
      attroff(A_REVERSE);
    }
  }

  if (app->files.count == 0) {
    mvaddnstr(top + 1, left + 1, "No .mid files in destination", width - 2);
  }
}

static void draw_events_panel(TuiApp *app, int top, int left, int width,
                              int height) {
  int visible = height - 3;
  size_t scroll = 0;

  mvaddnstr(top, left + 1, "Sequence", width - 2);
  if (!app->playback.loaded) {
    mvaddnstr(top + 1, left + 1, "Select a MIDI file to inspect", width - 2);
    return;
  }

  mvprintw(top + 1, left + 1, "%s  %zu event(s)", app->playback.loaded_name,
           app->playback.events.count);
  if (visible <= 0 || app->playback.events.count == 0) {
    return;
  }

  if (app->playback.selected_event >= (size_t)visible) {
    scroll = app->playback.selected_event - (size_t)visible + 1;
  }

  for (int row = 0; row < visible; ++row) {
    size_t index = scroll + (size_t)row;
    int screen_row = top + 2 + row;
    char clock_text[32];
    char byte_text[64];
    char line[160];
    MidiDescription description;
    int attrs;

    move(screen_row, left + 1);
    clrtoeol();
    if (index >= app->playback.events.count) {
      continue;
    }

    tui_format_clock_time(clock_text, sizeof(clock_text),
                          app->playback.events.items[index].seconds);
    tui_format_midi_bytes(byte_text, sizeof(byte_text),
                          app->playback.events.items[index].data,
                          app->playback.events.items[index].length);
    midi_describe_bytes(app->playback.events.items[index].data,
                        app->playback.events.items[index].length, &description);
    snprintf(line, sizeof(line), "%c %4zu  %s  %-12s  %s",
             index == app->playback.selected_event ? '>' : ' ', index + 1,
             clock_text, byte_text, description.text);

    attrs = category_attrs(description.category);
    if (index == app->playback.selected_event) {
      attrs |= A_REVERSE;
    }
    attron(attrs);
    mvaddnstr(screen_row, left + 1, line, width - 2);
    attroff(attrs);
  }
}

static void draw_log_panel(TuiApp *app, int top, int left, int width,
                           int height) {
  TuiLogEntry snapshot[TUI_LOG_CAPACITY];
  size_t count = tui_log_snapshot(app->log, snapshot, TUI_LOG_CAPACITY);
  int visible = height - 2;
  size_t start = 0;

  mvaddnstr(top, left + 1, "Live Stream", width - 2);
  if (visible <= 0) {
    return;
  }
  if (count > (size_t)visible) {
    start = count - (size_t)visible;
  }

  for (int row = 0; row < visible; ++row) {
    int screen_row = top + 1 + row;
    size_t index = start + (size_t)row;

    move(screen_row, left + 1);
    clrtoeol();
    if (index >= count) {
      continue;
    }
    mvaddnstr(screen_row, left + 1, snapshot[index].line, width - 2);
  }
}

static void draw_footer(TuiApp *app, int row, int cols) {
  char summary[TUI_MAX_STATUS];
  char clock_text[32];

  if (app->record.active) {
    tui_format_clock_time(clock_text, sizeof(clock_text),
                          record_elapsed_seconds(&app->record));
    snprintf(summary, sizeof(summary),
             "REC %s  %u event(s)  %u ignored  output %s", clock_text,
             midi_recorder_captured_events(&app->record.recorder),
             midi_recorder_ignored_messages(&app->record.recorder),
             app->record.output_path);
  } else if (app->playback.active || app->playback.loaded) {
    tui_format_clock_time(clock_text, sizeof(clock_text),
                          playback_position_seconds(&app->playback));
    snprintf(summary, sizeof(summary), "PLAY %s / %.1fs  event %zu/%zu  %s",
             clock_text, app->playback.total_seconds,
             app->playback.selected_event + 1, app->playback.events.count,
             app->playback.active ? "play starts from selected event"
                                  : app->status_message);
  } else {
    snprintf(summary, sizeof(summary), "%s", app->status_message);
  }

  mvhline(row - 1, 0, ACS_HLINE, cols);
  draw_clipped_text(row, 2, cols - 4, summary, A_BOLD);
}

static void render_tui(TuiApp *app) {
  int rows;
  int cols;
  int top;
  int file_width;
  int event_left;
  int content_height;
  int log_top;
  int log_height = 8;

  erase();
  getmaxyx(stdscr, rows, cols);
  if (rows < 20 || cols < 90) {
    mvaddstr(1, 2, "Resize terminal to at least 90x20 for TUI mode.");
    refresh();
    return;
  }

  draw_header(app, cols);
  top = 5;
  content_height = rows - top - log_height - 2;
  file_width = cols / 3;
  if (file_width < 28) {
    file_width = 28;
  }
  event_left = file_width + 1;
  log_top = top + content_height + 1;

  mvvline(top, file_width, ACS_VLINE, content_height);
  draw_files_panel(app, top, 0, file_width, content_height);
  draw_events_panel(app, top, event_left, cols - event_left, content_height);
  draw_log_panel(app, log_top, 0, cols, log_height);
  draw_footer(app, rows - 1, cols);
  refresh();
}

static void move_file_selection(TuiApp *app, int delta) {
  size_t selected;

  if (app->files.count == 0 || app->record.active || app->playback.active) {
    return;
  }

  selected = app->files.selected;
  if (delta < 0 && selected > 0) {
    selected -= 1;
  } else if (delta > 0 && selected + 1 < app->files.count) {
    selected += 1;
  }
  if (selected != app->files.selected) {
    app->files.selected = selected;
    playback_load_selected_file(app);
  }
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
  scan_recording_files(app);
  playback_load_selected_file(app);
  set_status(app, "Recordings directory set to %s", app->recordings_dir);
}

static void maybe_rescan_files(TuiApp *app) {
  time_t now_time = time(NULL);

  if (now_time != app->last_scan_time && !app->record.active &&
      !app->playback.active) {
    scan_recording_files(app);
    playback_load_selected_file(app);
  }
}

static void handle_keypress(TuiApp *app, int ch, bool *should_quit) {
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
  case 'r':
    start_record_session(app);
    break;
  case 'p':
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
    if (app->record.active) {
      toggle_record_pause(app);
    } else if (app->playback.active) {
      toggle_playback_pause(app);
    }
    break;
  case 'o':
    prompt_for_output_directory(app);
    break;
  case KEY_UP:
    move_file_selection(app, -1);
    break;
  case KEY_DOWN:
    move_file_selection(app, 1);
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
  playback_load_selected_file(app);
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

  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  timeout(TUI_INPUT_TIMEOUT_MS);
  curs_set(0);
  if (has_colors()) {
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

  while (!should_quit && !g_stop_requested) {
    int ch;

    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, false);
    maybe_refresh_monitor_input(&app);
    playback_tick(&app);
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
