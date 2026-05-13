#include "midi_recorder.h"

#include <stdlib.h>
#include <string.h>

static MidiResult midi_recorder_seconds_to_beats(MidiRecorder *recorder,
                                                 double elapsed_seconds,
                                                 MusicTimeStamp *out_beats) {
  if (recorder == NULL || out_beats == NULL || elapsed_seconds < 0.0) {
    return midi_result_invalid_argument("midi_recorder_seconds_to_beats");
  }

  OSStatus status = MusicSequenceGetBeatsForSeconds(recorder->sequence,
                                                    elapsed_seconds, out_beats);
  if (status != noErr) {
    return midi_result_osstatus("MusicSequenceGetBeatsForSeconds", status);
  }

  return midi_result_ok();
}

MidiResult midi_recorder_init(MidiRecorder *recorder) {
  if (recorder == NULL) {
    return midi_result_invalid_argument("midi_recorder_init");
  }

  memset(recorder, 0, sizeof(*recorder));
  midi_parser_state_init(&recorder->parser_state);

  OSStatus status = NewMusicSequence(&recorder->sequence);
  if (status != noErr) {
    return midi_result_osstatus("NewMusicSequence", status);
  }

  status = MusicSequenceNewTrack(recorder->sequence, &recorder->track);
  if (status != noErr) {
    DisposeMusicSequence(recorder->sequence);
    recorder->sequence = NULL;
    return midi_result_osstatus("MusicSequenceNewTrack", status);
  }

  return midi_result_ok();
}

void midi_recorder_dispose(MidiRecorder *recorder) {
  if (recorder == NULL) {
    return;
  }

  if (recorder->sequence != NULL) {
    DisposeMusicSequence(recorder->sequence);
  }

  memset(recorder, 0, sizeof(*recorder));
}

MidiResult midi_recorder_add_channel_event(MidiRecorder *recorder,
                                           const uint8_t *bytes, size_t length,
                                           double elapsed_seconds) {
  if (recorder == NULL || bytes == NULL || length < 2) {
    return midi_result_invalid_argument("midi_recorder_add_channel_event");
  }

  MusicTimeStamp beats = 0;
  MidiResult result =
      midi_recorder_seconds_to_beats(recorder, elapsed_seconds, &beats);
  if (!midi_result_is_ok(result)) {
    return result;
  }

  MIDIChannelMessage message;
  memset(&message, 0, sizeof(message));
  message.status = bytes[0];
  message.data1 = length > 1 ? bytes[1] : 0;
  message.data2 = length > 2 ? bytes[2] : 0;

  OSStatus status =
      MusicTrackNewMIDIChannelEvent(recorder->track, beats, &message);
  if (status != noErr) {
    return midi_result_osstatus("MusicTrackNewMIDIChannelEvent", status);
  }

  atomic_fetch_add_explicit(&recorder->captured_events, 1u,
                            memory_order_relaxed);
  return midi_result_ok();
}

MidiResult midi_recorder_add_sysex_event(MidiRecorder *recorder,
                                         const uint8_t *bytes, size_t length,
                                         double elapsed_seconds) {
  if (recorder == NULL || bytes == NULL || length == 0) {
    return midi_result_invalid_argument("midi_recorder_add_sysex_event");
  }

  MusicTimeStamp beats = 0;
  MidiResult result =
      midi_recorder_seconds_to_beats(recorder, elapsed_seconds, &beats);
  if (!midi_result_is_ok(result)) {
    return result;
  }

  MIDIRawData *raw = (MIDIRawData *)malloc(sizeof(MIDIRawData) + length - 1);
  if (raw == NULL) {
    return midi_result_no_memory("midi_recorder_add_sysex_event");
  }

  raw->length = (UInt32)length;
  memcpy(raw->data, bytes, length);

  OSStatus status = MusicTrackNewMIDIRawDataEvent(recorder->track, beats, raw);
  free(raw);
  if (status != noErr) {
    return midi_result_osstatus("MusicTrackNewMIDIRawDataEvent", status);
  }

  atomic_fetch_add_explicit(&recorder->captured_events, 1u,
                            memory_order_relaxed);
  return midi_result_ok();
}

MidiResult midi_recorder_record_packet_bytes(MidiRecorder *recorder,
                                             const uint8_t *bytes,
                                             size_t byte_count,
                                             double elapsed_seconds) {
  if (recorder == NULL || bytes == NULL || byte_count == 0 ||
      elapsed_seconds < 0.0) {
    return midi_result_invalid_argument("midi_recorder_record_packet_bytes");
  }

  size_t offset = 0;
  while (offset < byte_count) {
    MidiParsedMessage parsed;
    size_t used = midi_next_stream_message(
        &recorder->parser_state, bytes + offset, byte_count - offset, &parsed);
    if (used == 0) {
      atomic_fetch_add_explicit(&recorder->ignored_messages, 1u,
                                memory_order_relaxed);
      break;
    }

    MidiResult result = midi_result_ok();
    switch (parsed.kind) {
    case MIDI_PARSED_CHANNEL:
      result = midi_recorder_add_channel_event(recorder, parsed.channel_bytes,
                                               parsed.channel_length,
                                               elapsed_seconds);
      break;
    case MIDI_PARSED_SYSEX:
      result = midi_recorder_add_sysex_event(recorder, bytes + offset,
                                             parsed.length, elapsed_seconds);
      break;
    case MIDI_PARSED_UNSUPPORTED:
      atomic_fetch_add_explicit(&recorder->ignored_messages, 1u,
                                memory_order_relaxed);
      break;
    case MIDI_PARSED_INCOMPLETE:
      atomic_fetch_add_explicit(&recorder->ignored_messages, 1u,
                                memory_order_relaxed);
      return midi_result_ok();
    }

    if (!midi_result_is_ok(result)) {
      return result;
    }

    offset += used;
  }

  return midi_result_ok();
}

MusicSequence midi_recorder_sequence(const MidiRecorder *recorder) {
  if (recorder == NULL) {
    return NULL;
  }

  return recorder->sequence;
}

unsigned int midi_recorder_captured_events(const MidiRecorder *recorder) {
  if (recorder == NULL) {
    return 0;
  }

  return atomic_load_explicit(&recorder->captured_events, memory_order_relaxed);
}

unsigned int midi_recorder_ignored_messages(const MidiRecorder *recorder) {
  if (recorder == NULL) {
    return 0;
  }

  return atomic_load_explicit(&recorder->ignored_messages,
                              memory_order_relaxed);
}
