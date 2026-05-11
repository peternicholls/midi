#include "command_record.h"

#include "app_support.h"
#include "midi_parser.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct AppRecorderState {
  MusicSequence sequence;
  MusicTrack track;
  uint64_t first_host_time;
  bool has_first_host_time;
  atomic_uint captured_events;
  atomic_uint ignored_messages;
  atomic_ullong last_rx_nanos;
  MidiParserState parser_state;
} AppRecorderState;

static int add_channel_event(AppRecorderState *state, const uint8_t *bytes,
                             size_t length, MusicTimeStamp beats) {
  if (state == NULL || bytes == NULL || length < 2) {
    return 0;
  }

  MIDIChannelMessage message;
  memset(&message, 0, sizeof(message));
  message.status = bytes[0];
  message.data1 = length > 1 ? bytes[1] : 0;
  message.data2 = length > 2 ? bytes[2] : 0;

  OSStatus status =
      MusicTrackNewMIDIChannelEvent(state->track, beats, &message);
  if (status != noErr) {
    log_osstatus_error("MusicTrackNewMIDIChannelEvent", status);
    return 0;
  }

  atomic_fetch_add_explicit(&state->captured_events, 1u, memory_order_relaxed);
  return 1;
}

static int add_sysex_event(AppRecorderState *state, const uint8_t *bytes,
                           size_t length, MusicTimeStamp beats) {
  if (state == NULL || bytes == NULL || length == 0) {
    return 0;
  }

  MIDIRawData *raw = (MIDIRawData *)malloc(sizeof(MIDIRawData) + length - 1);
  if (raw == NULL) {
    fputs("out of memory while recording SysEx\n", stderr);
    return 0;
  }

  raw->length = (UInt32)length;
  memcpy(raw->data, bytes, length);

  OSStatus status = MusicTrackNewMIDIRawDataEvent(state->track, beats, raw);
  free(raw);
  if (status != noErr) {
    log_osstatus_error("MusicTrackNewMIDIRawDataEvent", status);
    return 0;
  }

  atomic_fetch_add_explicit(&state->captured_events, 1u, memory_order_relaxed);
  return 1;
}

static void record_packet_bytes(AppRecorderState *state, const uint8_t *bytes,
                                size_t byte_count, uint64_t host_time) {
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

  double elapsed_seconds = 0.0;
  if (host_time >= state->first_host_time) {
    elapsed_seconds = (double)AudioConvertHostTimeToNanos(
                          host_time - state->first_host_time) /
                      1000000000.0;
  }

  MusicTimeStamp beats = 0;
  OSStatus status =
      MusicSequenceGetBeatsForSeconds(state->sequence, elapsed_seconds, &beats);
  if (status != noErr) {
    log_osstatus_error("MusicSequenceGetBeatsForSeconds", status);
    return;
  }

  size_t offset = 0;
  while (offset < byte_count) {
    MidiParsedMessage parsed;
    size_t used = midi_next_stream_message(&state->parser_state, bytes + offset,
                                           byte_count - offset, &parsed);
    if (used == 0) {
      atomic_fetch_add_explicit(&state->ignored_messages, 1u,
                                memory_order_relaxed);
      break;
    }

    switch (parsed.kind) {
    case MIDI_PARSED_CHANNEL:
      if (!add_channel_event(state, parsed.channel_bytes, parsed.channel_length,
                             beats)) {
        g_stop_requested = 1;
        return;
      }
      break;
    case MIDI_PARSED_SYSEX:
      if (!add_sysex_event(state, bytes + offset, parsed.length, beats)) {
        g_stop_requested = 1;
        return;
      }
      break;
    case MIDI_PARSED_UNSUPPORTED:
      atomic_fetch_add_explicit(&state->ignored_messages, 1u,
                                memory_order_relaxed);
      break;
    case MIDI_PARSED_INCOMPLETE:
      atomic_fetch_add_explicit(&state->ignored_messages, 1u,
                                memory_order_relaxed);
      return;
    }

    offset += used;
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
  OSStatus status;

  memset(state, 0, sizeof(*state));
  midi_parser_state_init(&state->parser_state);

  status = NewMusicSequence(&state->sequence);
  if (status != noErr) {
    log_osstatus_error("NewMusicSequence", status);
    return 0;
  }

  status = MusicSequenceNewTrack(state->sequence, &state->track);
  if (status != noErr) {
    log_osstatus_error("MusicSequenceNewTrack", status);
    DisposeMusicSequence(state->sequence);
    state->sequence = NULL;
    return 0;
  }

  return 1;
}

static void dispose_record_resources(MIDIClientRef client,
                                     MIDIPortRef input_port,
                                     MusicSequence sequence) {
  if (input_port != 0) {
    MIDIPortDispose(input_port);
  }
  if (client != 0) {
    MIDIClientDispose(client);
  }
  if (sequence != NULL) {
    DisposeMusicSequence(sequence);
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
      atomic_load_explicit(&state->captured_events, memory_order_relaxed);
  unsigned int ignored_messages =
      atomic_load_explicit(&state->ignored_messages, memory_order_relaxed);

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
    dispose_record_resources(client, input_port, state.sequence);
    return 1;
  }

  g_stop_requested = 0;
  signal(SIGINT, on_sigint);

  print_record_start(requested_seconds, source_index, source_name);
  run_record_loop(&state, requested_seconds);

  OSStatus status = save_sequence_to_file(state.sequence, output_path);
  putchar('\n');
  dispose_record_resources(client, input_port, state.sequence);
  if (status != noErr) {
    log_osstatus_error("MusicSequenceFileCreate", status);
    return 1;
  }

  print_record_summary(&state, output_path);
  return 0;
}
