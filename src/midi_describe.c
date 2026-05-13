#include "midi_describe.h"

#include <stdarg.h>
#include <stdio.h>

static void set_description(MidiDescription *description,
                            MidiDescriptionCategory category,
                            const char *format, ...) {
  va_list args;

  if (description == NULL) {
    return;
  }

  description->category = category;
  description->text[0] = '\0';
  if (format == NULL) {
    return;
  }

  va_start(args, format);
  vsnprintf(description->text, sizeof(description->text), format, args);
  va_end(args);
}

static unsigned int channel_number(uint8_t status) {
  return (unsigned int)(status & 0x0F) + 1u;
}

static const char *byte_word(size_t length) {
  return length == 1 ? "byte" : "bytes";
}

void midi_note_name(uint8_t note, char *buffer, size_t buffer_size) {
  static const char *names[] = {"C",  "C#", "D",  "D#", "E",  "F",
                                "F#", "G",  "G#", "A",  "A#", "B"};
  int octave;

  if (buffer == NULL || buffer_size == 0) {
    return;
  }

  octave = (int)(note / 12u) - 1;
  snprintf(buffer, buffer_size, "%s%d", names[note % 12u], octave);
}

static void describe_channel_message(const MidiParsedMessage *message,
                                     MidiDescription *description) {
  uint8_t status = message->channel_bytes[0];
  uint8_t data1 = message->channel_length > 1 ? message->channel_bytes[1] : 0;
  uint8_t data2 = message->channel_length > 2 ? message->channel_bytes[2] : 0;
  unsigned int channel = channel_number(status);
  char note[8];

  switch (status & 0xF0) {
  case 0x80:
    midi_note_name(data1, note, sizeof(note));
    set_description(description, MIDI_DESCRIPTION_NOTE_OFF,
                    "Note off %s v%u ch%u", note, data2, channel);
    break;
  case 0x90:
    midi_note_name(data1, note, sizeof(note));
    if (data2 == 0) {
      set_description(description, MIDI_DESCRIPTION_NOTE_OFF,
                      "Note off %s v0 ch%u", note, channel);
    } else {
      set_description(description, MIDI_DESCRIPTION_NOTE_ON,
                      "Note on %s v%u ch%u", note, data2, channel);
    }
    break;
  case 0xA0:
    midi_note_name(data1, note, sizeof(note));
    set_description(description, MIDI_DESCRIPTION_POLY_PRESSURE,
                    "Poly pressure %s v%u ch%u", note, data2, channel);
    break;
  case 0xB0:
    set_description(description, MIDI_DESCRIPTION_CONTROL_CHANGE,
                    "CC %u = %u ch%u", data1, data2, channel);
    break;
  case 0xC0:
    set_description(description, MIDI_DESCRIPTION_PROGRAM_CHANGE,
                    "Program change %u ch%u", data1, channel);
    break;
  case 0xD0:
    set_description(description, MIDI_DESCRIPTION_CHANNEL_PRESSURE,
                    "Channel pressure %u ch%u", data1, channel);
    break;
  case 0xE0: {
    int value = (int)(((unsigned int)(data2 & 0x7F) << 7u) |
                      (unsigned int)(data1 & 0x7F)) -
                8192;
    set_description(description, MIDI_DESCRIPTION_PITCH_BEND,
                    "Pitch bend %+d ch%u", value, channel);
    break;
  }
  default:
    set_description(description, MIDI_DESCRIPTION_UNSUPPORTED,
                    "Unsupported %zu %s", message->length,
                    byte_word(message->length));
    break;
  }
}

void midi_describe_parsed_message(const MidiParsedMessage *message,
                                  MidiDescription *description) {
  if (description == NULL) {
    return;
  }

  set_description(description, MIDI_DESCRIPTION_INCOMPLETE,
                  "Incomplete message");

  if (message == NULL) {
    return;
  }

  switch (message->kind) {
  case MIDI_PARSED_CHANNEL:
    describe_channel_message(message, description);
    break;
  case MIDI_PARSED_SYSEX:
    set_description(description, MIDI_DESCRIPTION_SYSEX, "SysEx %zu %s",
                    message->length, byte_word(message->length));
    break;
  case MIDI_PARSED_UNSUPPORTED:
    set_description(description, MIDI_DESCRIPTION_UNSUPPORTED,
                    "Unsupported %zu %s", message->length,
                    byte_word(message->length));
    break;
  case MIDI_PARSED_INCOMPLETE:
    set_description(description, MIDI_DESCRIPTION_INCOMPLETE,
                    "Incomplete message");
    break;
  }
}

void midi_describe_bytes(const uint8_t *bytes, size_t length,
                         MidiDescription *description) {
  MidiParsedMessage message;
  size_t used;

  if (description == NULL) {
    return;
  }

  set_description(description, MIDI_DESCRIPTION_INCOMPLETE,
                  "Incomplete message");

  used = midi_next_message(bytes, length, &message);
  if (used == 0) {
    return;
  }

  midi_describe_parsed_message(&message, description);
}
