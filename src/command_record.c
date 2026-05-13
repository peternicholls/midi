#include "command_record.h"

#include "app_support.h"
#include "midi_recorder.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct AppRecorderState {
  MidiRecorder recorder;
  uint64_t first_host_time;
  bool has_first_host_time;
  atomic_ullong last_rx_nanos;
} AppRecorderState;

static int report_recorder_result(MidiResult result) {
  if (midi_result_is_ok(result)) {
    return 1;
  }

  switch (result.code) {
  case MIDI_RESULT_OK:
    return 1;
  case MIDI_RESULT_INVALID_ARGUMENT:
    fputs("invalid recorder input\n", stderr);
    return 0;
  case MIDI_RESULT_NO_MEMORY:
    fputs("out of memory while recording SysEx\n", stderr);
    return 0;
  case MIDI_RESULT_OSSTATUS:
    log_osstatus_error(result.operation, result.os_status);
    return 0;
  }

  return 0;
}

static void record_packet_bytes(AppRecorderState *state, const uint8_t *bytes,
                                size_t byte_count, uint64_t host_time) {
  double elapsed_seconds = 0.0;

  if (state == NULL || bytes == NULL || byte_count == 0) {
    return;
  }

  if (!state->has_first_host_time) {
    state->first_host_time = host_time;
    state->has_first_host_time = true;
  }

  atomic_store_explicit(&state->last_rx_nanos,
                        AudioConvertHostTimeToNanos(host_time),
                        memory_order_relaxed);

  if (host_time >= state->first_host_time) {
    elapsed_seconds = (double)AudioConvertHostTimeToNanos(
                          host_time - state->first_host_time) /
                      1000000000.0;
  }

  if (!report_recorder_result(midi_recorder_record_packet_bytes(
          &state->recorder, bytes, byte_count, elapsed_seconds))) {
    g_stop_requested = 1;
  }
}

static void midi_read_proc(const MIDIPacketList *packet_list,
                           void *read_proc_ref_con, void *src_conn_ref_con) {
  (void)src_conn_ref_con;

  AppRecorderState *state = (AppRecorderState *)read_proc_ref_con;
  if (state == NULL || packet_list == NULL) {
    return;
  }

  const MIDIPacket *packet = &packet_list->packet[0];
  for (UInt32 i = 0; i < packet_list->numPackets; ++i) {
    const uint64_t packet_time =
        packet->timeStamp != 0 ? packet->timeStamp : AudioGetCurrentHostTime();
    record_packet_bytes(state, packet->data, packet->length, packet_time);
    packet = MIDIPacketNext(packet);
  }
}

static OSStatus save_sequence_to_file(MusicSequence sequence,
                                      const char *output_path) {
  CFURLRef url = NULL;
  if (!create_file_url(output_path, &url)) {
    fputs("could not create output URL\n", stderr);
    return -50;
  }

  OSStatus status =
      MusicSequenceFileCreate(sequence, url, kMusicSequenceFile_MIDIType,
                              kMusicSequenceFileFlags_EraseFile, 480);
  CFRelease(url);
  return status;
}

static int parse_record_request(const char *seconds_arg, const char *source_arg,
                                long *requested_seconds, long *source_index) {
  *requested_seconds = -1;
  *source_index = 0;

  if (seconds_arg != NULL && !parse_long_arg(seconds_arg, requested_seconds)) {
    fputs("seconds must be an integer\n", stderr);
    return 0;
  }
  if (source_arg != NULL && !parse_long_arg(source_arg, source_index)) {
    fputs("source-index must be an integer\n", stderr);
    return 0;
  }
  if (*requested_seconds == 0 || *requested_seconds < -1) {
    fputs("seconds must be positive, or omit it to record until Ctrl-C\n",
          stderr);
    return 0;
  }

  return 1;
}

static int initialize_record_state(AppRecorderState *state) {
  memset(state, 0, sizeof(*state));

  return report_recorder_result(midi_recorder_init(&state->recorder));
}

static void dispose_record_resources(MIDIClientRef client,
                                     MIDIPortRef input_port,
                                     AppRecorderState *state) {
  if (input_port != 0) {
    MIDIPortDispose(input_port);
  }
  if (client != 0) {
    MIDIClientDispose(client);
  }
  if (state != NULL) {
    midi_recorder_dispose(&state->recorder);
  }
}

static int open_record_input(MIDIEndpointRef source, AppRecorderState *state,
                             MIDIClientRef *client, MIDIPortRef *input_port) {
  OSStatus status =
      MIDIClientCreate(CFSTR("midi-capture-client"), NULL, NULL, client);
  if (status != noErr) {
    log_osstatus_error("MIDIClientCreate", status);
    return 0;
  }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  status = MIDIInputPortCreate(*client, CFSTR("midi-capture-input"),
                               midi_read_proc, state, input_port);
#pragma clang diagnostic pop
  if (status != noErr) {
    log_osstatus_error("MIDIInputPortCreate", status);
    MIDIClientDispose(*client);
    *client = 0;
    return 0;
  }

  status = MIDIPortConnectSource(*input_port, source, NULL);
  if (status != noErr) {
    log_osstatus_error("MIDIPortConnectSource", status);
    MIDIPortDispose(*input_port);
    MIDIClientDispose(*client);
    *input_port = 0;
    *client = 0;
    return 0;
  }

  return 1;
}

static void print_record_start(long requested_seconds, long source_index,
                               const char *source_name) {
  if (requested_seconds > 0) {
    printf("Recording from source [%ld] %s for %ld second(s)...\n",
           source_index, source_name, requested_seconds);
  } else {
    printf("Recording from source [%ld] %s until Ctrl-C...\n", source_index,
           source_name);
  }
}

static void run_record_loop(AppRecorderState *state, long requested_seconds) {
  const uint64_t started_nanos = host_time_now_nanos();

  while (!g_stop_requested) {
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.05, false);

    uint64_t now_nanos = host_time_now_nanos();
    double elapsed_seconds = (double)(now_nanos - started_nanos) / 1000000000.0;

    draw_status_line("REC", elapsed_seconds, false, 0.0,
                     activity_is_active(
                         now_nanos, atomic_load_explicit(&state->last_rx_nanos,
                                                         memory_order_relaxed)),
                     false);

    if (requested_seconds > 0 && elapsed_seconds >= (double)requested_seconds) {
      return;
    }
  }
}

static void print_record_summary(const AppRecorderState *state,
                                 const char *output_path) {
  unsigned int captured_events =
      midi_recorder_captured_events(&state->recorder);
  unsigned int ignored_messages =
      midi_recorder_ignored_messages(&state->recorder);

  printf("Saved %u event(s) to %s", captured_events, output_path);
  if (ignored_messages > 0) {
    printf(" (%u unsupported/incomplete message(s) ignored)", ignored_messages);
  }
  putchar('\n');
}

int command_record(const char *output_path, const char *seconds_arg,
                   const char *source_arg) {
  long requested_seconds;
  long source_index;

  if (!parse_record_request(seconds_arg, source_arg, &requested_seconds,
                            &source_index)) {
    return 1;
  }

  MIDIEndpointRef source = get_source_by_index(source_index);
  if (source == 0) {
    fputs("invalid source index or no MIDI sources are available\n", stderr);
    return 1;
  }

  char source_name[256];
  get_endpoint_name(source, source_name, sizeof(source_name));

  MIDIClientRef client = 0;
  MIDIPortRef input_port = 0;
  AppRecorderState state;

  if (!initialize_record_state(&state)) {
    return 1;
  }

  if (!open_record_input(source, &state, &client, &input_port)) {
    dispose_record_resources(client, input_port, &state);
    return 1;
  }

  g_stop_requested = 0;
  signal(SIGINT, on_sigint);

  print_record_start(requested_seconds, source_index, source_name);
  run_record_loop(&state, requested_seconds);

  OSStatus status = save_sequence_to_file(
      midi_recorder_sequence(&state.recorder), output_path);
  putchar('\n');
  if (status != noErr) {
    dispose_record_resources(client, input_port, &state);
    log_osstatus_error("MusicSequenceFileCreate", status);
    return 1;
  }

  print_record_summary(&state, output_path);
  dispose_record_resources(client, input_port, &state);
  return 0;
}
