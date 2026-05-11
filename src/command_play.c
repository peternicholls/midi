#include "command_play.h"

#include "app_support.h"
#include "midi_parser.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct PlaybackEvent {
  double seconds;
  size_t length;
  uint8_t inline_bytes[3];
  uint8_t *data;
  unsigned long order;
} PlaybackEvent;

typedef struct PlaybackEventList {
  PlaybackEvent *items;
  size_t count;
  size_t capacity;
  unsigned long next_order;
} PlaybackEventList;

static OSStatus sequence_length_in_beats(MusicSequence sequence,
                                         MusicTimeStamp *out_beats) {
  UInt32 track_count = 0;
  OSStatus status = MusicSequenceGetTrackCount(sequence, &track_count);
  if (status != noErr) {
    return status;
  }

  MusicTimeStamp max_beats = 0;
  for (UInt32 i = 0; i < track_count; ++i) {
    MusicTrack track = NULL;
    status = MusicSequenceGetIndTrack(sequence, i, &track);
    if (status != noErr) {
      return status;
    }

    MusicTimeStamp track_length = 0;
    UInt32 property_size = (UInt32)sizeof(track_length);
    status = MusicTrackGetProperty(track, kSequenceTrackProperty_TrackLength,
                                   &track_length, &property_size);
    if (status != noErr) {
      return status;
    }

    if (track_length > max_beats) {
      max_beats = track_length;
    }
  }

  *out_beats = max_beats;
  return noErr;
}

static void free_playback_events(PlaybackEventList *events) {
  if (events == NULL) {
    return;
  }

  for (size_t i = 0; i < events->count; ++i) {
    if (events->items[i].data != NULL &&
        events->items[i].data != events->items[i].inline_bytes) {
      free(events->items[i].data);
    }
  }

  free(events->items);
  events->items = NULL;
  events->count = 0;
  events->capacity = 0;
  events->next_order = 0;
}

static int append_playback_event(PlaybackEventList *events, double seconds,
                                 const uint8_t *data, size_t length) {
  if (events == NULL || data == NULL || length == 0) {
    return 0;
  }

  if (events->count == events->capacity) {
    size_t new_capacity = events->capacity == 0 ? 32 : events->capacity * 2;
    PlaybackEvent *resized = (PlaybackEvent *)realloc(
        events->items, new_capacity * sizeof(*resized));
    if (resized == NULL) {
      fputs("out of memory while preparing playback events\n", stderr);
      return 0;
    }
    events->items = resized;
    events->capacity = new_capacity;
  }

  PlaybackEvent *event = &events->items[events->count];
  memset(event, 0, sizeof(*event));
  event->seconds = seconds;
  event->length = length;

  if (length <= sizeof(event->inline_bytes)) {
    memcpy(event->inline_bytes, data, length);
    event->data = event->inline_bytes;
  } else {
    event->data = (uint8_t *)malloc(length);
    if (event->data == NULL) {
      fputs("out of memory while copying playback bytes\n", stderr);
      return 0;
    }
    memcpy(event->data, data, length);
  }

  event->order = events->next_order++;
  events->count += 1;
  return 1;
}

static int compare_playback_events(const void *left, const void *right) {
  const PlaybackEvent *a = (const PlaybackEvent *)left;
  const PlaybackEvent *b = (const PlaybackEvent *)right;

  if (a->seconds < b->seconds) {
    return -1;
  }
  if (a->seconds > b->seconds) {
    return 1;
  }
  if (a->order < b->order) {
    return -1;
  }
  if (a->order > b->order) {
    return 1;
  }
  return 0;
}

static int seconds_for_beats(MusicSequence sequence, MusicTimeStamp beats,
                             double *out_seconds) {
  OSStatus status =
      MusicSequenceGetSecondsForBeats(sequence, beats, out_seconds);
  if (status != noErr) {
    log_osstatus_error("MusicSequenceGetSecondsForBeats", status);
    return 0;
  }
  return 1;
}

static int collect_track_playback_events(MusicSequence sequence,
                                         MusicTrack track,
                                         PlaybackEventList *events) {
  MusicEventIterator iterator = NULL;
  OSStatus status = NewMusicEventIterator(track, &iterator);
  if (status != noErr) {
    log_osstatus_error("NewMusicEventIterator", status);
    return 0;
  }

  Boolean has_current = false;
  status = MusicEventIteratorHasCurrentEvent(iterator, &has_current);
  if (status != noErr) {
    log_osstatus_error("MusicEventIteratorHasCurrentEvent", status);
    DisposeMusicEventIterator(iterator);
    return 0;
  }

  while (has_current) {
    MusicTimeStamp beats = 0;
    MusicEventType event_type = 0;
    const void *event_data = NULL;
    UInt32 event_size = 0;
    status = MusicEventIteratorGetEventInfo(iterator, &beats, &event_type,
                                            &event_data, &event_size);
    if (status != noErr) {
      log_osstatus_error("MusicEventIteratorGetEventInfo", status);
      DisposeMusicEventIterator(iterator);
      return 0;
    }
    (void)event_size;

    double seconds = 0.0;
    if (!seconds_for_beats(sequence, beats, &seconds)) {
      DisposeMusicEventIterator(iterator);
      return 0;
    }

    if (event_type == kMusicEventType_MIDIChannelMessage) {
      const MIDIChannelMessage *message =
          (const MIDIChannelMessage *)event_data;
      uint8_t bytes[3] = {message->status, message->data1, message->data2};
      size_t length = midi_channel_message_length(message->status);
      if (!append_playback_event(events, seconds, bytes, length)) {
        DisposeMusicEventIterator(iterator);
        return 0;
      }
    } else if (event_type == kMusicEventType_MIDIRawData) {
      const MIDIRawData *raw = (const MIDIRawData *)event_data;
      if (!append_playback_event(events, seconds, raw->data, raw->length)) {
        DisposeMusicEventIterator(iterator);
        return 0;
      }
    } else if (event_type == kMusicEventType_MIDINoteMessage) {
      const MIDINoteMessage *note = (const MIDINoteMessage *)event_data;
      uint8_t note_on[3] = {(uint8_t)(0x90 | (note->channel & 0x0F)),
                            note->note, note->velocity};
      uint8_t note_off[3] = {(uint8_t)(0x80 | (note->channel & 0x0F)),
                             note->note, note->releaseVelocity};
      double end_seconds = seconds;

      if (!append_playback_event(events, seconds, note_on, sizeof(note_on))) {
        DisposeMusicEventIterator(iterator);
        return 0;
      }
      if (!seconds_for_beats(sequence, beats + note->duration, &end_seconds)) {
        DisposeMusicEventIterator(iterator);
        return 0;
      }
      if (!append_playback_event(events, end_seconds, note_off,
                                 sizeof(note_off))) {
        DisposeMusicEventIterator(iterator);
        return 0;
      }
    }

    status = MusicEventIteratorNextEvent(iterator);
    if (status != noErr) {
      log_osstatus_error("MusicEventIteratorNextEvent", status);
      DisposeMusicEventIterator(iterator);
      return 0;
    }
    status = MusicEventIteratorHasCurrentEvent(iterator, &has_current);
    if (status != noErr) {
      log_osstatus_error("MusicEventIteratorHasCurrentEvent", status);
      DisposeMusicEventIterator(iterator);
      return 0;
    }
  }

  DisposeMusicEventIterator(iterator);
  return 1;
}

static int collect_playback_events(MusicSequence sequence,
                                   PlaybackEventList *events,
                                   double *out_total_seconds) {
  UInt32 track_count = 0;
  OSStatus status = MusicSequenceGetTrackCount(sequence, &track_count);
  if (status != noErr) {
    log_osstatus_error("MusicSequenceGetTrackCount", status);
    return 0;
  }

  for (UInt32 i = 0; i < track_count; ++i) {
    MusicTrack track = NULL;
    status = MusicSequenceGetIndTrack(sequence, i, &track);
    if (status != noErr) {
      log_osstatus_error("MusicSequenceGetIndTrack", status);
      return 0;
    }
    if (!collect_track_playback_events(sequence, track, events)) {
      return 0;
    }
  }

  if (events->count > 1) {
    qsort(events->items, events->count, sizeof(events->items[0]),
          compare_playback_events);
  }

  MusicTimeStamp end_beats = 0;
  status = sequence_length_in_beats(sequence, &end_beats);
  if (status != noErr) {
    log_osstatus_error("sequence_length_in_beats", status);
    return 0;
  }

  double total_seconds = 0.0;
  if (!seconds_for_beats(sequence, end_beats, &total_seconds)) {
    return 0;
  }
  if (events->count > 0 &&
      events->items[events->count - 1].seconds > total_seconds) {
    total_seconds = events->items[events->count - 1].seconds;
  }
  *out_total_seconds = total_seconds;
  return 1;
}

static int send_midi_bytes(MIDIPortRef output_port, MIDIEndpointRef destination,
                           const uint8_t *bytes, size_t length) {
  if (bytes == NULL || length == 0) {
    return 0;
  }

  uint8_t stack_buffer[sizeof(MIDIPacketList) + 256];
  size_t packet_list_size = sizeof(MIDIPacketList) + length;
  MIDIPacketList *packet_list = (MIDIPacketList *)stack_buffer;
  bool heap_allocated = false;

  if (length > 256) {
    packet_list = (MIDIPacketList *)malloc(packet_list_size);
    if (packet_list == NULL) {
      fputs("out of memory while sending MIDI\n", stderr);
      return 0;
    }
    heap_allocated = true;
  }

  MIDIPacket *packet = MIDIPacketListInit(packet_list);
  packet = MIDIPacketListAdd(packet_list, packet_list_size, packet, 0, length,
                             bytes);
  if (packet == NULL) {
    if (heap_allocated) {
      free(packet_list);
    }
    fputs("failed to build MIDI packet list\n", stderr);
    return 0;
  }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  OSStatus status = MIDISend(output_port, destination, packet_list);
#pragma clang diagnostic pop

  if (heap_allocated) {
    free(packet_list);
  }
  if (status != noErr) {
    log_osstatus_error("MIDISend", status);
    return 0;
  }

  return 1;
}

static void wait_with_status(const char *mode, uint64_t started_nanos,
                             double target_seconds, bool has_total,
                             double total_seconds, uint64_t last_rx_nanos,
                             uint64_t last_tx_nanos) {
  while (!g_stop_requested) {
    uint64_t now_nanos = host_time_now_nanos();
    double elapsed_seconds = (double)(now_nanos - started_nanos) / 1000000000.0;
    if (elapsed_seconds >= target_seconds) {
      return;
    }

    draw_status_line(mode, elapsed_seconds, has_total, total_seconds,
                     activity_is_active(now_nanos, last_rx_nanos),
                     activity_is_active(now_nanos, last_tx_nanos));
    usleep(20000);
  }
}

static int parse_play_request(const char *destination_arg,
                              long *destination_index) {
  *destination_index = 0;

  if (destination_arg != NULL &&
      !parse_long_arg(destination_arg, destination_index)) {
    fputs("destination-index must be an integer\n", stderr);
    return 0;
  }

  return 1;
}

static int load_sequence_from_path(const char *input_path,
                                   MusicSequence *sequence) {
  CFURLRef url = NULL;
  if (!create_file_url(input_path, &url)) {
    fputs("could not create input URL\n", stderr);
    return 0;
  }

  OSStatus status = NewMusicSequence(sequence);
  if (status != noErr) {
    CFRelease(url);
    log_osstatus_error("NewMusicSequence", status);
    return 0;
  }

  status = MusicSequenceFileLoad(*sequence, url, kMusicSequenceFile_AnyType, 0);
  CFRelease(url);
  if (status != noErr) {
    log_osstatus_error("MusicSequenceFileLoad", status);
    DisposeMusicSequence(*sequence);
    *sequence = NULL;
    return 0;
  }

  return 1;
}

static int open_playback_output(MIDIClientRef *client,
                                MIDIPortRef *output_port) {
  OSStatus status = MIDIClientCreate(CFSTR("midi-capture-playback-client"),
                                     NULL, NULL, client);
  if (status != noErr) {
    log_osstatus_error("MIDIClientCreate", status);
    return 0;
  }

  status =
      MIDIOutputPortCreate(*client, CFSTR("midi-capture-output"), output_port);
  if (status != noErr) {
    log_osstatus_error("MIDIOutputPortCreate", status);
    MIDIClientDispose(*client);
    *client = 0;
    return 0;
  }

  return 1;
}

static void dispose_playback_resources(MIDIClientRef client,
                                       MIDIPortRef output_port,
                                       MusicSequence sequence,
                                       PlaybackEventList *events) {
  if (output_port != 0) {
    MIDIPortDispose(output_port);
  }
  if (client != 0) {
    MIDIClientDispose(client);
  }
  free_playback_events(events);
  if (sequence != NULL) {
    DisposeMusicSequence(sequence);
  }
}

static int play_events(MIDIPortRef output_port, MIDIEndpointRef destination,
                       PlaybackEventList *events, double total_seconds) {
  uint64_t started_nanos = host_time_now_nanos();
  uint64_t last_tx_nanos = 0;

  for (size_t i = 0; i < events->count && !g_stop_requested; ++i) {
    wait_with_status("PLAY", started_nanos, events->items[i].seconds, true,
                     total_seconds, 0, last_tx_nanos);
    if (g_stop_requested) {
      break;
    }
    if (!send_midi_bytes(output_port, destination, events->items[i].data,
                         events->items[i].length)) {
      return 0;
    }

    last_tx_nanos = host_time_now_nanos();
    draw_status_line("PLAY", events->items[i].seconds, true, total_seconds,
                     false, true);
  }

  wait_with_status("PLAY", started_nanos, total_seconds, true, total_seconds, 0,
                   last_tx_nanos);
  draw_status_line("PLAY", total_seconds, true, total_seconds, false, false);
  putchar('\n');
  return 1;
}

int command_play(const char *input_path, const char *destination_arg) {
  long destination_index;

  if (!parse_play_request(destination_arg, &destination_index)) {
    return 1;
  }

  MIDIEndpointRef destination = get_destination_by_index(destination_index);
  if (destination == 0) {
    fputs("invalid destination index or no MIDI destinations are available\n",
          stderr);
    return 1;
  }

  char destination_name[256];
  get_endpoint_name(destination, destination_name, sizeof(destination_name));

  MusicSequence sequence = NULL;
  MIDIClientRef client = 0;
  MIDIPortRef output_port = 0;
  PlaybackEventList events;
  memset(&events, 0, sizeof(events));

  if (!load_sequence_from_path(input_path, &sequence)) {
    return 1;
  }

  double total_seconds = 0.0;
  if (!collect_playback_events(sequence, &events, &total_seconds)) {
    dispose_playback_resources(client, output_port, sequence, &events);
    return 1;
  }

  if (!open_playback_output(&client, &output_port)) {
    dispose_playback_resources(client, output_port, sequence, &events);
    return 1;
  }

  g_stop_requested = 0;
  signal(SIGINT, on_sigint);

  printf("Playing %s to destination [%ld] %s...\n", input_path,
         destination_index, destination_name);

  int ok = play_events(output_port, destination, &events, total_seconds);
  dispose_playback_resources(client, output_port, sequence, &events);
  return ok ? 0 : 1;
}
