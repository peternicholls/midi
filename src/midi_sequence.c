#include "midi_sequence.h"

#include "midi_parser.h"

#include <CoreFoundation/CoreFoundation.h>

#include <stdlib.h>
#include <string.h>

void midi_sequence_event_list_init(MidiSequenceEventList *events) {
  if (events == NULL) {
    return;
  }
  memset(events, 0, sizeof(*events));
}

void midi_sequence_event_list_free(MidiSequenceEventList *events) {
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
  midi_sequence_event_list_init(events);
}

MidiResult midi_sequence_event_list_append(MidiSequenceEventList *events,
                                           double seconds,
                                           const uint8_t *data,
                                           size_t length) {
  if (events == NULL || data == NULL || length == 0) {
    return midi_result_invalid_argument("midi_sequence_event_list_append");
  }

  if (events->count == events->capacity) {
    size_t new_capacity = events->capacity == 0 ? 32 : events->capacity * 2;
    MidiSequenceEvent *resized = (MidiSequenceEvent *)realloc(
        events->items, new_capacity * sizeof(*resized));
    if (resized == NULL) {
      return midi_result_no_memory("midi_sequence_event_list_append");
    }
    events->items = resized;
    events->capacity = new_capacity;
  }

  MidiSequenceEvent *event = &events->items[events->count];
  memset(event, 0, sizeof(*event));
  event->seconds = seconds;
  event->length = length;

  if (length <= sizeof(event->inline_bytes)) {
    memcpy(event->inline_bytes, data, length);
    event->data = event->inline_bytes;
  } else {
    event->data = (uint8_t *)malloc(length);
    if (event->data == NULL) {
      return midi_result_no_memory("midi_sequence_event_list_append");
    }
    memcpy(event->data, data, length);
  }

  event->order = events->next_order++;
  events->count += 1;
  return midi_result_ok();
}

static int compare_sequence_events(const void *left, const void *right) {
  const MidiSequenceEvent *a = (const MidiSequenceEvent *)left;
  const MidiSequenceEvent *b = (const MidiSequenceEvent *)right;

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

static MidiResult sequence_length_in_beats(MusicSequence sequence,
                                           MusicTimeStamp *out_beats) {
  UInt32 track_count = 0;
  OSStatus status = MusicSequenceGetTrackCount(sequence, &track_count);
  if (status != noErr) {
    return midi_result_osstatus("MusicSequenceGetTrackCount", status);
  }

  MusicTimeStamp max_beats = 0;
  for (UInt32 i = 0; i < track_count; ++i) {
    MusicTrack track = NULL;
    MusicTimeStamp track_length = 0;
    UInt32 property_size = (UInt32)sizeof(track_length);

    status = MusicSequenceGetIndTrack(sequence, i, &track);
    if (status != noErr) {
      return midi_result_osstatus("MusicSequenceGetIndTrack", status);
    }

    status = MusicTrackGetProperty(track, kSequenceTrackProperty_TrackLength,
                                   &track_length, &property_size);
    if (status != noErr) {
      return midi_result_osstatus("MusicTrackGetProperty", status);
    }
    if (track_length > max_beats) {
      max_beats = track_length;
    }
  }

  *out_beats = max_beats;
  return midi_result_ok();
}

static MidiResult seconds_for_beats(MusicSequence sequence,
                                    MusicTimeStamp beats,
                                    double *out_seconds) {
  OSStatus status =
      MusicSequenceGetSecondsForBeats(sequence, beats, out_seconds);
  if (status != noErr) {
    return midi_result_osstatus("MusicSequenceGetSecondsForBeats", status);
  }
  return midi_result_ok();
}

static MidiResult collect_track_events(MusicSequence sequence,
                                       MusicTrack track,
                                       MidiSequenceEventList *events) {
  MusicEventIterator iterator = NULL;
  Boolean has_current = false;
  MidiResult result;
  OSStatus status = NewMusicEventIterator(track, &iterator);
  if (status != noErr) {
    return midi_result_osstatus("NewMusicEventIterator", status);
  }

  status = MusicEventIteratorHasCurrentEvent(iterator, &has_current);
  if (status != noErr) {
    DisposeMusicEventIterator(iterator);
    return midi_result_osstatus("MusicEventIteratorHasCurrentEvent", status);
  }

  while (has_current) {
    MusicTimeStamp beats = 0;
    MusicEventType event_type = 0;
    const void *event_data = NULL;
    UInt32 event_size = 0;
    double seconds = 0.0;

    status = MusicEventIteratorGetEventInfo(iterator, &beats, &event_type,
                                            &event_data, &event_size);
    if (status != noErr) {
      DisposeMusicEventIterator(iterator);
      return midi_result_osstatus("MusicEventIteratorGetEventInfo", status);
    }
    (void)event_size;

    result = seconds_for_beats(sequence, beats, &seconds);
    if (!midi_result_is_ok(result)) {
      DisposeMusicEventIterator(iterator);
      return result;
    }

    if (event_type == kMusicEventType_MIDIChannelMessage) {
      const MIDIChannelMessage *message =
          (const MIDIChannelMessage *)event_data;
      uint8_t bytes[3] = {message->status, message->data1, message->data2};
      size_t length = midi_channel_message_length(message->status);
      result =
          midi_sequence_event_list_append(events, seconds, bytes, length);
      if (!midi_result_is_ok(result)) {
        DisposeMusicEventIterator(iterator);
        return result;
      }
    } else if (event_type == kMusicEventType_MIDIRawData) {
      const MIDIRawData *raw = (const MIDIRawData *)event_data;
      result =
          midi_sequence_event_list_append(events, seconds, raw->data,
                                          raw->length);
      if (!midi_result_is_ok(result)) {
        DisposeMusicEventIterator(iterator);
        return result;
      }
    } else if (event_type == kMusicEventType_MIDINoteMessage) {
      const MIDINoteMessage *note = (const MIDINoteMessage *)event_data;
      uint8_t note_on[3] = {(uint8_t)(0x90 | (note->channel & 0x0F)),
                            note->note, note->velocity};
      uint8_t note_off[3] = {(uint8_t)(0x80 | (note->channel & 0x0F)),
                             note->note, note->releaseVelocity};
      double end_seconds = 0.0;

      result = midi_sequence_event_list_append(events, seconds, note_on,
                                               sizeof(note_on));
      if (!midi_result_is_ok(result)) {
        DisposeMusicEventIterator(iterator);
        return result;
      }

      result = seconds_for_beats(sequence, beats + note->duration,
                                 &end_seconds);
      if (!midi_result_is_ok(result)) {
        DisposeMusicEventIterator(iterator);
        return result;
      }

      result = midi_sequence_event_list_append(events, end_seconds, note_off,
                                               sizeof(note_off));
      if (!midi_result_is_ok(result)) {
        DisposeMusicEventIterator(iterator);
        return result;
      }
    }

    status = MusicEventIteratorNextEvent(iterator);
    if (status != noErr) {
      DisposeMusicEventIterator(iterator);
      return midi_result_osstatus("MusicEventIteratorNextEvent", status);
    }
    status = MusicEventIteratorHasCurrentEvent(iterator, &has_current);
    if (status != noErr) {
      DisposeMusicEventIterator(iterator);
      return midi_result_osstatus("MusicEventIteratorHasCurrentEvent", status);
    }
  }

  DisposeMusicEventIterator(iterator);
  return midi_result_ok();
}

MidiResult midi_sequence_load_file(const char *path, MusicSequence *sequence) {
  CFURLRef url = NULL;
  OSStatus status;

  if (path == NULL || sequence == NULL) {
    return midi_result_invalid_argument("midi_sequence_load_file");
  }

  *sequence = NULL;
  url = CFURLCreateFromFileSystemRepresentation(
      kCFAllocatorDefault, (const UInt8 *)path, (CFIndex)strlen(path), false);
  if (url == NULL) {
    return midi_result_invalid_argument("CFURLCreateFromFileSystemRepresentation");
  }

  status = NewMusicSequence(sequence);
  if (status != noErr) {
    CFRelease(url);
    return midi_result_osstatus("NewMusicSequence", status);
  }

  status = MusicSequenceFileLoad(*sequence, url, kMusicSequenceFile_AnyType, 0);
  CFRelease(url);
  if (status != noErr) {
    DisposeMusicSequence(*sequence);
    *sequence = NULL;
    return midi_result_osstatus("MusicSequenceFileLoad", status);
  }

  return midi_result_ok();
}

MidiResult midi_sequence_collect_events(MusicSequence sequence,
                                        MidiSequenceEventList *events,
                                        double *out_total_seconds) {
  UInt32 track_count = 0;
  MusicTimeStamp end_beats = 0;
  double total_seconds = 0.0;
  MidiResult result;
  OSStatus status;

  if (sequence == NULL || events == NULL || out_total_seconds == NULL) {
    return midi_result_invalid_argument("midi_sequence_collect_events");
  }

  *out_total_seconds = 0.0;

  status = MusicSequenceGetTrackCount(sequence, &track_count);
  if (status != noErr) {
    return midi_result_osstatus("MusicSequenceGetTrackCount", status);
  }

  for (UInt32 i = 0; i < track_count; ++i) {
    MusicTrack track = NULL;
    status = MusicSequenceGetIndTrack(sequence, i, &track);
    if (status != noErr) {
      return midi_result_osstatus("MusicSequenceGetIndTrack", status);
    }
    result = collect_track_events(sequence, track, events);
    if (!midi_result_is_ok(result)) {
      return result;
    }
  }

  if (events->count > 1) {
    qsort(events->items, events->count, sizeof(events->items[0]),
          compare_sequence_events);
  }

  result = sequence_length_in_beats(sequence, &end_beats);
  if (!midi_result_is_ok(result)) {
    return result;
  }

  result = seconds_for_beats(sequence, end_beats, &total_seconds);
  if (!midi_result_is_ok(result)) {
    return result;
  }
  if (events->count > 0 &&
      events->items[events->count - 1].seconds > total_seconds) {
    total_seconds = events->items[events->count - 1].seconds;
  }

  *out_total_seconds = total_seconds;
  return midi_result_ok();
}

MidiResult midi_sequence_load_events_from_file(const char *path,
                                               MidiSequenceEventList *events,
                                               double *out_total_seconds) {
  MusicSequence sequence = NULL;
  MidiResult result = midi_sequence_load_file(path, &sequence);
  if (!midi_result_is_ok(result)) {
    return result;
  }

  result = midi_sequence_collect_events(sequence, events, out_total_seconds);
  DisposeMusicSequence(sequence);
  return result;
}
