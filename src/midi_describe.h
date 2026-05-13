#ifndef MIDI_DESCRIBE_H
#define MIDI_DESCRIBE_H

#include "midi_parser.h"

#include <stddef.h>
#include <stdint.h>

#define MIDI_DESCRIPTION_TEXT_SIZE 64

typedef enum MidiDescriptionCategory {
  MIDI_DESCRIPTION_NOTE_ON = 0,
  MIDI_DESCRIPTION_NOTE_OFF = 1,
  MIDI_DESCRIPTION_CONTROL_CHANGE = 2,
  MIDI_DESCRIPTION_PROGRAM_CHANGE = 3,
  MIDI_DESCRIPTION_PITCH_BEND = 4,
  MIDI_DESCRIPTION_CHANNEL_PRESSURE = 5,
  MIDI_DESCRIPTION_POLY_PRESSURE = 6,
  MIDI_DESCRIPTION_SYSEX = 7,
  MIDI_DESCRIPTION_UNSUPPORTED = 8,
  MIDI_DESCRIPTION_INCOMPLETE = 9
} MidiDescriptionCategory;

typedef struct MidiDescription {
  MidiDescriptionCategory category;
  char text[MIDI_DESCRIPTION_TEXT_SIZE];
} MidiDescription;

void midi_note_name(uint8_t note, char *buffer, size_t buffer_size);
void midi_describe_parsed_message(const MidiParsedMessage *message,
                                  MidiDescription *description);
void midi_describe_bytes(const uint8_t *bytes, size_t length,
                         MidiDescription *description);

#endif
