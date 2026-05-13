#include "midi_sequence.h"

#include <assert.h>
#include <string.h>

static MusicSequence new_sequence_with_track(MusicTrack *track) {
  MusicSequence sequence = NULL;
  assert(NewMusicSequence(&sequence) == noErr);
  assert(MusicSequenceNewTrack(sequence, track) == noErr);
  return sequence;
}

static void test_channel_events_are_sorted_by_seconds(void) {
  MusicTrack track = NULL;
  MusicSequence sequence = new_sequence_with_track(&track);
  MidiSequenceEventList events;
  double total_seconds = 0.0;

  MIDIChannelMessage later = {0x90, 60, 100, 0};
  MIDIChannelMessage earlier = {0xC0, 5, 0, 0};
  assert(MusicTrackNewMIDIChannelEvent(track, 1.0, &later) == noErr);
  assert(MusicTrackNewMIDIChannelEvent(track, 0.5, &earlier) == noErr);

  midi_sequence_event_list_init(&events);
  MidiResult result =
      midi_sequence_collect_events(sequence, &events, &total_seconds);

  assert(midi_result_is_ok(result));
  assert(events.count == 2);
  assert(events.items[0].seconds < events.items[1].seconds);
  assert(events.items[0].length == 2);
  assert(memcmp(events.items[0].data, (uint8_t[]){0xC0, 5}, 2) == 0);
  assert(events.items[1].length == 3);
  assert(memcmp(events.items[1].data, (uint8_t[]){0x90, 60, 100}, 3) == 0);
  assert(total_seconds >= events.items[1].seconds);

  midi_sequence_event_list_free(&events);
  DisposeMusicSequence(sequence);
}

static void test_note_message_expands_to_note_on_and_off(void) {
  MusicTrack track = NULL;
  MusicSequence sequence = new_sequence_with_track(&track);
  MidiSequenceEventList events;
  double total_seconds = 0.0;

  MIDINoteMessage note;
  memset(&note, 0, sizeof(note));
  note.channel = 0;
  note.note = 64;
  note.velocity = 90;
  note.releaseVelocity = 12;
  note.duration = 2.0;
  assert(MusicTrackNewMIDINoteEvent(track, 1.0, &note) == noErr);

  midi_sequence_event_list_init(&events);
  MidiResult result =
      midi_sequence_collect_events(sequence, &events, &total_seconds);

  assert(midi_result_is_ok(result));
  assert(events.count == 2);
  assert(events.items[0].length == 3);
  assert(memcmp(events.items[0].data, (uint8_t[]){0x90, 64, 90}, 3) == 0);
  assert(events.items[1].length == 3);
  assert(memcmp(events.items[1].data, (uint8_t[]){0x80, 64, 12}, 3) == 0);
  assert(events.items[1].seconds > events.items[0].seconds);
  assert(total_seconds >= events.items[1].seconds);

  midi_sequence_event_list_free(&events);
  DisposeMusicSequence(sequence);
}

static void test_raw_event_bytes_are_copied(void) {
  MusicTrack track = NULL;
  MusicSequence sequence = new_sequence_with_track(&track);
  MidiSequenceEventList events;
  double total_seconds = 0.0;
  const uint8_t raw_bytes[] = {0xF0, 0x7D, 0x01, 0xF7};
  MIDIRawData *raw =
      (MIDIRawData *)malloc(sizeof(MIDIRawData) + sizeof(raw_bytes) - 1);
  assert(raw != NULL);
  raw->length = (UInt32)sizeof(raw_bytes);
  memcpy(raw->data, raw_bytes, sizeof(raw_bytes));
  assert(MusicTrackNewMIDIRawDataEvent(track, 0.0, raw) == noErr);
  free(raw);

  midi_sequence_event_list_init(&events);
  MidiResult result =
      midi_sequence_collect_events(sequence, &events, &total_seconds);

  assert(midi_result_is_ok(result));
  assert(events.count == 1);
  assert(events.items[0].length == sizeof(raw_bytes));
  assert(events.items[0].data != events.items[0].inline_bytes);
  assert(memcmp(events.items[0].data, raw_bytes, sizeof(raw_bytes)) == 0);

  midi_sequence_event_list_free(&events);
  DisposeMusicSequence(sequence);
}

int main(void) {
  test_channel_events_are_sorted_by_seconds();
  test_note_message_expands_to_note_on_and_off();
  test_raw_event_bytes_are_copied();
  return 0;
}
