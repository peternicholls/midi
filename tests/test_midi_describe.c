#include "midi_describe.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static void assert_description(const uint8_t *bytes, size_t length,
                               MidiDescriptionCategory category,
                               const char *text) {
  MidiDescription description;

  midi_describe_bytes(bytes, length, &description);

  assert(description.category == category);
  assert(strcmp(description.text, text) == 0);
}

static void test_note_names_use_scientific_pitch_octaves(void) {
  char text[8];

  midi_note_name(0, text, sizeof(text));
  assert(strcmp(text, "C-1") == 0);

  midi_note_name(60, text, sizeof(text));
  assert(strcmp(text, "C4") == 0);

  midi_note_name(61, text, sizeof(text));
  assert(strcmp(text, "C#4") == 0);

  midi_note_name(127, text, sizeof(text));
  assert(strcmp(text, "G9") == 0);
}

static void test_note_on_and_note_off_are_distinct_categories(void) {
  const uint8_t note_on[] = {0x90, 60, 100};
  const uint8_t note_off[] = {0x80, 60, 64};

  assert_description(note_on, sizeof(note_on), MIDI_DESCRIPTION_NOTE_ON,
                     "Note on C4 v100 ch1");
  assert_description(note_off, sizeof(note_off), MIDI_DESCRIPTION_NOTE_OFF,
                     "Note off C4 v64 ch1");
}

static void test_note_on_velocity_zero_is_note_off(void) {
  const uint8_t bytes[] = {0x91, 64, 0};

  assert_description(bytes, sizeof(bytes), MIDI_DESCRIPTION_NOTE_OFF,
                     "Note off E4 v0 ch2");
}

static void test_common_channel_messages_are_described(void) {
  const uint8_t poly_pressure[] = {0xA2, 67, 45};
  const uint8_t control_change[] = {0xB3, 64, 127};
  const uint8_t program_change[] = {0xC4, 10};
  const uint8_t channel_pressure[] = {0xD5, 70};
  const uint8_t pitch_bend[] = {0xE6, 0x00, 0x40};

  assert_description(poly_pressure, sizeof(poly_pressure),
                     MIDI_DESCRIPTION_POLY_PRESSURE,
                     "Poly pressure G4 v45 ch3");
  assert_description(control_change, sizeof(control_change),
                     MIDI_DESCRIPTION_CONTROL_CHANGE, "CC 64 = 127 ch4");
  assert_description(program_change, sizeof(program_change),
                     MIDI_DESCRIPTION_PROGRAM_CHANGE,
                     "Program change 10 ch5");
  assert_description(channel_pressure, sizeof(channel_pressure),
                     MIDI_DESCRIPTION_CHANNEL_PRESSURE,
                     "Channel pressure 70 ch6");
  assert_description(pitch_bend, sizeof(pitch_bend),
                     MIDI_DESCRIPTION_PITCH_BEND, "Pitch bend +0 ch7");
}

static void test_sysex_and_unsupported_messages_stay_general(void) {
  const uint8_t sysex[] = {0xF0, 0x7D, 0x01, 0xF7};
  const uint8_t realtime[] = {0xFE};

  assert_description(sysex, sizeof(sysex), MIDI_DESCRIPTION_SYSEX,
                     "SysEx 4 bytes");
  assert_description(realtime, sizeof(realtime),
                     MIDI_DESCRIPTION_UNSUPPORTED, "Unsupported 1 byte");
}

static void test_incomplete_and_null_inputs_are_described(void) {
  const uint8_t incomplete[] = {0x90, 60};
  MidiDescription description;

  assert_description(incomplete, sizeof(incomplete),
                     MIDI_DESCRIPTION_INCOMPLETE, "Incomplete message");

  midi_describe_bytes(NULL, 3, &description);
  assert(description.category == MIDI_DESCRIPTION_INCOMPLETE);
  assert(strcmp(description.text, "Incomplete message") == 0);
}

int main(void) {
  test_note_names_use_scientific_pitch_octaves();
  test_note_on_and_note_off_are_distinct_categories();
  test_note_on_velocity_zero_is_note_off();
  test_common_channel_messages_are_described();
  test_sysex_and_unsupported_messages_stay_general();
  test_incomplete_and_null_inputs_are_described();
  return 0;
}
