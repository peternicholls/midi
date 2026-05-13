#include "midi_recorder.h"
#include "midi_sequence.h"

#include <assert.h>
#include <string.h>

static void test_init_creates_sequence(void) {
  MidiRecorder recorder;

  MidiResult result = midi_recorder_init(&recorder);

  assert(midi_result_is_ok(result));
  assert(midi_recorder_sequence(&recorder) != NULL);

  midi_recorder_dispose(&recorder);
}

static void test_channel_event_is_inserted_and_counted(void) {
  MidiRecorder recorder;
  MidiSequenceEventList events;
  double total_seconds = 0.0;
  const uint8_t note_on[] = {0x90, 60, 100};

  assert(midi_result_is_ok(midi_recorder_init(&recorder)));

  MidiResult result =
      midi_recorder_add_channel_event(&recorder, note_on, sizeof(note_on), 0.5);

  assert(midi_result_is_ok(result));
  assert(midi_recorder_captured_events(&recorder) == 1);

  midi_sequence_event_list_init(&events);
  result = midi_sequence_collect_events(midi_recorder_sequence(&recorder),
                                        &events, &total_seconds);

  assert(midi_result_is_ok(result));
  assert(events.count == 1);
  assert(events.items[0].length == sizeof(note_on));
  assert(memcmp(events.items[0].data, note_on, sizeof(note_on)) == 0);
  assert(events.items[0].seconds >= 0.5);

  midi_sequence_event_list_free(&events);
  midi_recorder_dispose(&recorder);
}

static void test_packet_recording_counts_supported_and_ignored_messages(void) {
  MidiRecorder recorder;
  const uint8_t bytes[] = {0x90, 60, 100, 0xF8, 0xF0, 0x7D, 0x01, 0xF7};

  assert(midi_result_is_ok(midi_recorder_init(&recorder)));

  MidiResult result =
      midi_recorder_record_packet_bytes(&recorder, bytes, sizeof(bytes), 0.0);

  assert(midi_result_is_ok(result));
  assert(midi_recorder_captured_events(&recorder) == 2);
  assert(midi_recorder_ignored_messages(&recorder) == 1);

  midi_recorder_dispose(&recorder);
}

int main(void) {
  test_init_creates_sequence();
  test_channel_event_is_inserted_and_counted();
  test_packet_recording_counts_supported_and_ignored_messages();
  return 0;
}
